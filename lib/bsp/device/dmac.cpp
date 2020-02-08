/* Copyright 2018 Canaan Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <FreeRTOS.h>
#include <atomic.h>
#include <dmac.h>
#include <hal.h>
#include <kernel/driver_impl.hpp>
#include <plic.h>
#include <semphr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysctl.h>
#include <task.h>
#include <utility.h>
#include <iomem.h>
#include <printf.h>

using namespace sys;

/* DMAC */

class k_dmac_driver : public dmac_driver, public static_object, public free_object_access
{
public:
    k_dmac_driver(uintptr_t base_addr)
        : dmac_(*reinterpret_cast<volatile dmac_t *>(base_addr))
    {
    }

    virtual void install() override
    {
        uint64_t tmp;
        dmac_commonreg_intclear_u_t intclear;
        dmac_cfg_u_t dmac_cfg;
        dmac_reset_u_t dmac_reset;

        sysctl_clock_enable(SYSCTL_CLOCK_DMA);

        dmac_reset.data = readq(&dmac_.reset);
        dmac_reset.reset.rst = 1;
        writeq(dmac_reset.data, &dmac_.reset);
        while (dmac_reset.reset.rst)
            dmac_reset.data = readq(&dmac_.reset);

        intclear.data = readq(&dmac_.com_intclear);
        intclear.com_intclear.cear_slvif_dec_err_intstat = 1;
        intclear.com_intclear.clear_slvif_wr2ro_err_intstat = 1;
        intclear.com_intclear.clear_slvif_rd2wo_err_intstat = 1;
        intclear.com_intclear.clear_slvif_wronhold_err_intstat = 1;
        intclear.com_intclear.clear_slvif_undefinedreg_dec_err_intstat = 1;
        writeq(intclear.data, &dmac_.com_intclear);

        dmac_cfg.data = readq(&dmac_.cfg);
        dmac_cfg.cfg.dmac_en = 0;
        dmac_cfg.cfg.int_en = 0;
        writeq(dmac_cfg.data, &dmac_.cfg);

        while (readq(&dmac_.cfg))
            ;

        tmp = readq(&dmac_.chen);
        tmp &= ~0xf;
        writeq(tmp, &dmac_.chen);

        dmac_cfg.data = readq(&dmac_.cfg);
        dmac_cfg.cfg.dmac_en = 1;
        dmac_cfg.cfg.int_en = 1;
        writeq(dmac_cfg.data, &dmac_.cfg);
    }

    volatile dmac_t &dmac()
    {
        return dmac_;
    }

private:
    volatile dmac_t &dmac_;
};

static k_dmac_driver dev0_driver(DMAC_BASE_ADDR);

driver &g_dmac_driver_dmac0 = static_cast<driver &>(dev0_driver);

/* DMA Channel */

#define MAX_PING_PONG_SRCS 4
#define C_COMMON_ENTRY         \
    auto &dmac = dmac_.dmac(); \
    auto &dma = dmac.channel[channel_];

class k_dma_driver : public dma_driver, public static_object, public exclusive_object_access
{
public:
    k_dma_driver(k_dmac_driver &dmac, uint32_t channel)
        : dmac_(dmac), channel_(channel)
    {
    }

    virtual void install() override
    {
        pic_set_irq_handler(IRQN_DMA0_INTERRUPT + channel_, dma_completion_isr, this);
        pic_set_irq_priority(IRQN_DMA0_INTERRUPT + channel_, 1);
        pic_set_irq_enable(IRQN_DMA0_INTERRUPT + channel_, 1);
    }

    virtual void set_select_request(uint32_t request) override
    {
        if (channel_ == SYSCTL_DMA_CHANNEL_5)
        {
            sysctl->dma_sel1.dma_sel5 = request;
        }
        else
        {
            auto &dma_sel = sysctl->dma_sel0;

            switch (channel_)
            {
            case SYSCTL_DMA_CHANNEL_0:
                dma_sel.dma_sel0 = request;
                break;

            case SYSCTL_DMA_CHANNEL_1:
                dma_sel.dma_sel1 = request;
                break;

            case SYSCTL_DMA_CHANNEL_2:
                dma_sel.dma_sel2 = request;
                break;

            case SYSCTL_DMA_CHANNEL_3:
                dma_sel.dma_sel3 = request;
                break;

            case SYSCTL_DMA_CHANNEL_4:
                dma_sel.dma_sel4 = request;
                break;

            default:
                configASSERT(!"Invalid dma channel_");
            }
        }
    }

    virtual void config(uint32_t priority) override
    {
        C_COMMON_ENTRY;
        configASSERT((dmac_.dmac().chen & (1 << channel_)) == 0);
        configASSERT(priority <= 7);

        dmac_ch_cfg_u_t cfg_u;

        cfg_u.data = readq(&dma.cfg);
        cfg_u.ch_cfg.ch_prior = priority;
        writeq(cfg_u.data, &dma.cfg);
    }

    virtual void transmit_async(const volatile void *src, volatile void *dest, bool src_inc, bool dest_inc, size_t element_size, size_t count, size_t burst_size, SemaphoreHandle_t completion_event) override
    {
        C_COMMON_ENTRY;
#if FIX_CACHE
        //iomem_free(session_.alloc_mem);
        //session_.alloc_mem = NULL;
#else
        free(session_.alloc_mem);
        session_.alloc_mem = NULL;
#endif
        if (count == 0)
        {
            xSemaphoreGive(completion_event);
            return;
        }

        src_inc = !src_inc;
        dest_inc = !dest_inc;
        configASSERT(count > 0 && count <= 0x3fffff);
        configASSERT((dmac.chen & (1 << channel_)) == 0);

        int mem_type_src = is_memory((uintptr_t)src), mem_type_dest = is_memory((uintptr_t)dest);

        dmac_ch_cfg_u_t cfg_u;

        dmac_transfer_flow_t flow_control = DMAC_MEM2MEM_DMA;
        if (mem_type_src == 0 && mem_type_dest == 0)
        {
            configASSERT(!"Periph to periph dma is not supported.");
        }
        else if (mem_type_src == 1 && mem_type_dest == 0)
            flow_control = DMAC_MEM2PRF_DMA;
        else if (mem_type_src == 0 && mem_type_dest == 1)
            flow_control = DMAC_PRF2MEM_DMA;
        else if (mem_type_src == 1 && mem_type_dest == 1)
            flow_control = DMAC_MEM2MEM_DMA;

        configASSERT(flow_control == DMAC_MEM2MEM_DMA || element_size <= 8);

        cfg_u.data = readq(&dma.cfg);
        cfg_u.ch_cfg.tt_fc = flow_control;
        cfg_u.ch_cfg.hs_sel_src = mem_type_src ? DMAC_HS_SOFTWARE : DMAC_HS_HARDWARE;
        cfg_u.ch_cfg.hs_sel_dst = mem_type_dest ? DMAC_HS_SOFTWARE : DMAC_HS_HARDWARE;
        cfg_u.ch_cfg.src_per = channel_;
        cfg_u.ch_cfg.dst_per = channel_;
        cfg_u.ch_cfg.src_multblk_type = 0;
        cfg_u.ch_cfg.dst_multblk_type = 0;

        writeq(cfg_u.data, &dma.cfg);

        session_.is_loop = 0;
        session_.flow_control = flow_control;

        size_t old_elm_size = element_size;
        session_.element_size = old_elm_size;
        session_.count = count;
        session_.dest = dest;
        session_.alloc_mem = NULL;

        if (flow_control != DMAC_MEM2MEM_DMA && old_elm_size < 4)
        {
#if FIX_CACHE
            void *alloc_mem = iomem_malloc(sizeof(uint32_t) * count + 128);
#else
            void *alloc_mem = malloc(sizeof(uint32_t) * count + 128);
#endif
            session_.alloc_mem = alloc_mem;
            element_size = sizeof(uint32_t);

            if (!mem_type_src)
            {
                dma.sar = (uint64_t)src;
                dma.dar = (uint64_t)alloc_mem;
            }
            else if (!mem_type_dest)
            {
                if (old_elm_size == 1)
                {
                    size_t i;
                    const uint8_t *p_src = (const uint8_t *)src;
                    uint32_t *p_dst = reinterpret_cast<uint32_t *>(alloc_mem);
                    for (i = 0; i < count; i++)
                        p_dst[i] = p_src[i];
                }
                else if (old_elm_size == 2)
                {
                    size_t i;
                    const uint16_t *p_src = (const uint16_t *)src;
                    uint32_t *p_dst = reinterpret_cast<uint32_t *>(alloc_mem);
                    for (i = 0; i < count; i++)
                        p_dst[i] = p_src[i];
                }
                else
                {
                    configASSERT(!"invalid element size");
                }

                dma.sar = (uint64_t)alloc_mem;
                dma.dar = (uint64_t)dest;
            }
            else
            {
                configASSERT(!"Impossible");
            }
        }
        else
        {
#if FIX_CACHE
            //iomem_free(session_.dest_malloc);
            //iomem_free(session_.src_malloc);
            //session_.dest_malloc = NULL;
            //session_.src_malloc = NULL;
            uint8_t *src_io = (uint8_t *)src;
            uint8_t *dest_io = (uint8_t *)dest;
            if(is_memory_cache((uintptr_t)src))
            {
                if(src_inc == 0)
                {
                    src_io = (uint8_t *)iomem_malloc(element_size * count+128);
                    memcpy(src_io, (uint8_t *)src, element_size * count);
                }
                else
                {
                    src_io = (uint8_t *)iomem_malloc(element_size+128);
                    memcpy(src_io, (uint8_t *)src, element_size);
                }
                session_.src_malloc = src_io;
            }
            if(is_memory_cache((uintptr_t)dest))
            {
                if(dest_inc == 0)
                {
                    dest_io = (uint8_t *)iomem_malloc(element_size * count+128);
                    session_.buf_len = element_size * count;
                }
                else
                {
                    dest_io = (uint8_t *)iomem_malloc(element_size+128);
                    session_.buf_len = element_size;
                }
                session_.dest_malloc = dest_io;
                session_.dest_buffer = (uint8_t *)dest;
            }
            dma.sar = (uint64_t)src_io;
            dma.dar = (uint64_t)dest_io;
#else
            dma.sar = (uint64_t)src;
            dma.dar = (uint64_t)dest;
#endif
        }

        dma.block_ts = count - 1;

        uint32_t tr_width = 0;
        switch (element_size)
        {
        case 1:
            tr_width = 0;
            break;
        case 2:
            tr_width = 1;
            break;
        case 4:
            tr_width = 2;
            break;
        case 8:
            tr_width = 3;
            break;
        case 16:
            tr_width = 4;
            break;
        default:
            configASSERT(!"Invalid element size.");
            break;
        }

        uint32_t msize = 0;
        switch (burst_size)
        {
        case 1:
            msize = 0;
            break;
        case 4:
            msize = 1;
            break;
        case 8:
            msize = 2;
            break;
        case 16:
            msize = 3;
            break;
        case 32:
            msize = 4;
            break;
        default:
            configASSERT(!"Invalid busrt size.");
            break;
        }

        dma.intstatus_en = 0xFFFFFFE2;
        dma.intclear = 0xFFFFFFFF;

        dmac_ch_ctl_u_t ctl_u;

        ctl_u.data = readq(&dma.ctl);
        ctl_u.ch_ctl.sinc = src_inc;
        ctl_u.ch_ctl.src_tr_width = tr_width;
        ctl_u.ch_ctl.src_msize = msize;
        ctl_u.ch_ctl.dinc = dest_inc;
        ctl_u.ch_ctl.dst_tr_width = tr_width;
        ctl_u.ch_ctl.dst_msize = msize;

        ctl_u.ch_ctl.sms = DMAC_MASTER1;
        ctl_u.ch_ctl.dms = DMAC_MASTER2;

        writeq(ctl_u.data, &dma.ctl);

        session_.completion_event = completion_event;
        dmac.chen |= 0x101 << channel_;
    }

    virtual void loop_async(const volatile void **srcs, size_t src_num, volatile void **dests, size_t dest_num, bool src_inc, bool dest_inc, size_t element_size, size_t count, size_t burst_size, dma_stage_completion_handler_t stage_completion_handler, void *stage_completion_handler_data, SemaphoreHandle_t completion_event, int *stop_signal) override
    {
        C_COMMON_ENTRY;
#if FIX_CACHE
        //iomem_free(session_.alloc_mem);
#else
        free(session_.alloc_mem);
#endif

        //session_.alloc_mem = NULL;
        if (count == 0)
        {
            xSemaphoreGive(completion_event);
            return;
        }

        src_inc = !src_inc;
        dest_inc = !dest_inc;
        configASSERT(count > 0 && count <= 0x3fffff);
        configASSERT((dmac.chen & (1 << channel_)) == 0);
        configASSERT(element_size >= 4);
        configASSERT(src_num > 0 && src_num <= MAX_PING_PONG_SRCS);
        configASSERT(dest_num > 0 && dest_num <= MAX_PING_PONG_SRCS);

        int mem_type_src = is_memory((uintptr_t)srcs[0]), mem_type_dest = is_memory((uintptr_t)dests[0]);

        dmac_ch_cfg_u_t cfg_u;

        dmac_transfer_flow_t flow_control = DMAC_MEM2MEM_DMA;
        if (mem_type_src == 0 && mem_type_dest == 0)
        {
            configASSERT(!"Periph to periph dma is not supported.");
        }
        else if (mem_type_src == 1 && mem_type_dest == 0)
            flow_control = DMAC_MEM2PRF_DMA;
        else if (mem_type_src == 0 && mem_type_dest == 1)
            flow_control = DMAC_PRF2MEM_DMA;
        else if (mem_type_src == 1 && mem_type_dest == 1)
            flow_control = DMAC_MEM2MEM_DMA;

        configASSERT(flow_control == DMAC_MEM2MEM_DMA || element_size <= 8);

        cfg_u.data = readq(&dma.cfg);
        cfg_u.ch_cfg.tt_fc = flow_control;
        cfg_u.ch_cfg.hs_sel_src = mem_type_src ? DMAC_HS_SOFTWARE : DMAC_HS_HARDWARE;
        cfg_u.ch_cfg.hs_sel_dst = mem_type_dest ? DMAC_HS_SOFTWARE : DMAC_HS_HARDWARE;
        cfg_u.ch_cfg.src_per = channel_;
        cfg_u.ch_cfg.dst_per = channel_;
        cfg_u.ch_cfg.src_multblk_type = 0;
        cfg_u.ch_cfg.dst_multblk_type = 0;

        writeq(cfg_u.data, &dma.cfg);

        session_.is_loop = 1;
        session_.flow_control = flow_control;

        dma.sar = (uint64_t)srcs[0];
        dma.dar = (uint64_t)dests[0];

        dma.block_ts = count - 1;

        uint32_t tr_width = 0;
        switch (element_size)
        {
        case 1:
            tr_width = 0;
            break;
        case 2:
            tr_width = 1;
            break;
        case 4:
            tr_width = 2;
            break;
        case 8:
            tr_width = 3;
            break;
        case 16:
            tr_width = 4;
            break;
        default:
            configASSERT(!"Invalid element size.");
            break;
        }

        uint32_t msize = 0;
        switch (burst_size)
        {
        case 1:
            msize = 0;
            break;
        case 4:
            msize = 1;
            break;
        case 8:
            msize = 2;
            break;
        case 16:
            msize = 3;
            break;
        case 32:
            msize = 4;
            break;
        default:
            configASSERT(!"Invalid busrt size.");
            break;
        }

        dma.intstatus_en = 0xFFFFFFE2;
        dma.intclear = 0xFFFFFFFF;

        dmac_ch_ctl_u_t ctl_u;

        ctl_u.data = readq(&dma.ctl);
        ctl_u.ch_ctl.sinc = src_inc;
        ctl_u.ch_ctl.src_tr_width = tr_width;
        ctl_u.ch_ctl.src_msize = msize;
        ctl_u.ch_ctl.dinc = dest_inc;
        ctl_u.ch_ctl.dst_tr_width = tr_width;
        ctl_u.ch_ctl.dst_msize = msize;

        ctl_u.ch_ctl.sms = DMAC_MASTER1;
        ctl_u.ch_ctl.dms = DMAC_MASTER2;

        writeq(ctl_u.data, &dma.ctl);

        session_.completion_event = completion_event;
        session_.stage_completion_handler_data = stage_completion_handler_data;
        session_.stage_completion_handler = stage_completion_handler;
        session_.stop_signal = stop_signal;
        session_.src_num = src_num;
        session_.dest_num = dest_num;
        session_.next_src_id = 0;
        session_.next_dest_id = 0;
        size_t i = 0;
        for (i = 0; i < src_num; i++)
            session_.srcs[i] = srcs[i];
        for (i = 0; i < dest_num; i++)
            session_.dests[i] = dests[i];

        dmac.chen |= 0x101 << channel_;
    }

    virtual void stop() override
    {
        atomic_set(session_.stop_signal, 1);
    }
private:
    static void dma_completion_isr(void *userdata)
    {
        auto &driver = *reinterpret_cast<k_dma_driver *>(userdata);
        auto &dmac = driver.dmac_.dmac();
        volatile dmac_channel_t &dma = dmac.channel[driver.channel_];

        configASSERT(dma.intstatus & 0x2);
        dma.intclear = 0xFFFFFFFF;

        BaseType_t xHigherPriorityTaskWoken = pdFALSE;

        if (driver.session_.is_loop)
        {
            if (atomic_read(driver.session_.stop_signal))
            {
                if (driver.session_.stage_completion_handler)
                    driver.session_.stage_completion_handler(driver.session_.stage_completion_handler_data);
                xSemaphoreGiveFromISR(driver.session_.completion_event, &xHigherPriorityTaskWoken);
            }
            else
            {
                size_t next_src_id = driver.session_.next_src_id + 1;
                if (next_src_id == driver.session_.src_num)
                    next_src_id = 0;
                driver.session_.next_src_id = next_src_id;
                dma.sar = (uint64_t)driver.session_.srcs[next_src_id];

                size_t next_dest_id = driver.session_.next_dest_id + 1;
                if (next_dest_id == driver.session_.dest_num)
                    next_dest_id = 0;
                driver.session_.next_dest_id = next_dest_id;
                dma.dar = (uint64_t)driver.session_.dests[next_dest_id];

                if (driver.session_.stage_completion_handler)
                    driver.session_.stage_completion_handler(driver.session_.stage_completion_handler_data);
                dmac.chen |= 0x101 << driver.channel_;
            }
        }
        else
        {
            if (driver.session_.flow_control != DMAC_MEM2MEM_DMA && driver.session_.element_size < 4)
            {
                if (driver.session_.flow_control == DMAC_PRF2MEM_DMA)
                {
                    if (driver.session_.element_size == 1)
                    {
                        size_t i;
                        uint32_t *p_src = reinterpret_cast<uint32_t *>(driver.session_.alloc_mem);
                        uint8_t *p_dst = (uint8_t *)driver.session_.dest;
                        for (i = 0; i < driver.session_.count; i++)
                            p_dst[i] = p_src[i];
                    }
                    else if (driver.session_.element_size == 2)
                    {
                        size_t i;
                        uint32_t *p_src = reinterpret_cast<uint32_t *>(driver.session_.alloc_mem);
                        uint16_t *p_dst = (uint16_t *)driver.session_.dest;
                        for (i = 0; i < driver.session_.count; i++)
                            p_dst[i] = p_src[i];
                    }
                    else
                    {
                        configASSERT(!"invalid element size");
                    }
                }
                else if (driver.session_.flow_control == DMAC_MEM2PRF_DMA)
                    ;
                else
                {
                    configASSERT(!"Impossible");
                }
                iomem_free_isr(driver.session_.alloc_mem);
                driver.session_.alloc_mem = NULL;
            }
#if FIX_CACHE
            else
            {
                if(driver.session_.buf_len)
                {
                    memcpy(driver.session_.dest_buffer, driver.session_.dest_malloc, driver.session_.buf_len);
                    iomem_free_isr(driver.session_.dest_malloc);
                    driver.session_.dest_malloc = NULL;
                    driver.session_.dest_buffer = NULL;
                    driver.session_.buf_len = 0;
                }
                if(driver.session_.src_malloc)
                {
                    iomem_free_isr(driver.session_.src_malloc);
                    driver.session_.src_malloc = NULL;
                }
            }
#endif
            xSemaphoreGiveFromISR(driver.session_.completion_event, &xHigherPriorityTaskWoken);
        }

        if (xHigherPriorityTaskWoken)
            portYIELD_FROM_ISR();
    }

    static int is_memory(uintptr_t address)
    {
        enum
        {
            mem_len = 6 * 1024 * 1024,
            mem_no_cache_len = 8 * 1024 * 1024,
        };
        return ((address >= 0x80000000) && (address < 0x80000000 + mem_len)) || ((address >= 0x40000000) && (address < 0x40000000 + mem_no_cache_len)) || (address == 0x50450040);
    }

private:
    k_dmac_driver &dmac_;
    uint32_t channel_;

    struct
    {
        SemaphoreHandle_t completion_event;
        int is_loop;
        union {
            struct
            {
                dmac_transfer_flow_t flow_control;
                size_t element_size;
                size_t count;
                void *alloc_mem;
                volatile void *dest;
#if FIX_CACHE
                uint8_t *dest_buffer;
                uint8_t *src_malloc;
                uint8_t *dest_malloc;
                size_t buf_len;
#endif
            };

            struct
            {
                const volatile void *srcs[MAX_PING_PONG_SRCS];
                size_t src_num;
                volatile void *dests[MAX_PING_PONG_SRCS];
                size_t dest_num;
                size_t next_src_id;
                size_t next_dest_id;
                dma_stage_completion_handler_t stage_completion_handler;
                void *stage_completion_handler_data;
                int *stop_signal;
            };
        };
    } session_;
};

static k_dma_driver dev0_c0_driver(dev0_driver, 0);
static k_dma_driver dev0_c1_driver(dev0_driver, 1);
static k_dma_driver dev0_c2_driver(dev0_driver, 2);
static k_dma_driver dev0_c3_driver(dev0_driver, 3);
static k_dma_driver dev0_c4_driver(dev0_driver, 4);
static k_dma_driver dev0_c5_driver(dev0_driver, 5);

driver &g_dma_driver_dma0 = dev0_c0_driver;
driver &g_dma_driver_dma1 = dev0_c1_driver;
driver &g_dma_driver_dma2 = dev0_c2_driver;
driver &g_dma_driver_dma3 = dev0_c3_driver;
driver &g_dma_driver_dma4 = dev0_c4_driver;
driver &g_dma_driver_dma5 = dev0_c5_driver;
