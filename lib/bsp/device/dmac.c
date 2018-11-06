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
#include <driver.h>
#include <hal.h>
#include <plic.h>
#include <semphr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysctl.h>
#include <task.h>
#include <utility.h>

/* DMAC */

#define COMMON_ENTRY                          \
    dmac_data *data = (dmac_data *)userdata;  \
    volatile dmac_t *dmac = (volatile dmac_t *)data->base_addr;

typedef struct
{
    uintptr_t base_addr;
    int32_t axi_master1_use;
    int32_t axi_master2_use;
} dmac_data;

static void dmac_install(void *userdata)
{
    COMMON_ENTRY;

    uint64_t tmp;
    dmac_commonreg_intclear_u_t intclear;
    dmac_cfg_u_t dmac_cfg;
    dmac_reset_u_t dmac_reset;

    sysctl_clock_enable(SYSCTL_CLOCK_DMA);

    dmac_reset.data = readq(&dmac->reset);
    dmac_reset.reset.rst = 1;
    writeq(dmac_reset.data, &dmac->reset);
    while (dmac_reset.reset.rst)
        dmac_reset.data = readq(&dmac->reset);

    intclear.data = readq(&dmac->com_intclear);
    intclear.com_intclear.cear_slvif_dec_err_intstat = 1;
    intclear.com_intclear.clear_slvif_wr2ro_err_intstat = 1;
    intclear.com_intclear.clear_slvif_rd2wo_err_intstat = 1;
    intclear.com_intclear.clear_slvif_wronhold_err_intstat = 1;
    intclear.com_intclear.clear_slvif_undefinedreg_dec_err_intstat = 1;
    writeq(intclear.data, &dmac->com_intclear);

    dmac_cfg.data = readq(&dmac->cfg);
    dmac_cfg.cfg.dmac_en = 0;
    dmac_cfg.cfg.int_en = 0;
    writeq(dmac_cfg.data, &dmac->cfg);

    while (readq(&dmac->cfg))
        ;

    tmp = readq(&dmac->chen);
    tmp &= ~0xf;
    writeq(tmp, &dmac->chen);

    dmac_cfg.data = readq(&dmac->cfg);
    dmac_cfg.cfg.dmac_en = 1;
    dmac_cfg.cfg.int_en = 1;
    writeq(dmac_cfg.data, &dmac->cfg);
}

static int dmac_open(void *userdata)
{
    return 1;
}

static void dmac_close(void *userdata)
{
}

static uint32_t add_lru_axi_master(dmac_data *data)
{
    uint32_t axi1 = atomic_read(&data->axi_master1_use);
    uint32_t axi2 = atomic_read(&data->axi_master2_use);

    if (axi1 < axi2)
    {
        atomic_add(&data->axi_master1_use, 1);
        return 0;
    }
    else
    {
        atomic_add(&data->axi_master2_use, 1);
        return 1;
    }
}

static void release_axi_master(dmac_data *data, uint32_t axi)
{
    if (axi == 0)
        atomic_add(&data->axi_master1_use, -1);
    else
        atomic_add(&data->axi_master2_use, -1);
}

static dmac_data dev0_data = {DMAC_BASE_ADDR, 0, 0};

const dmac_driver_t g_dmac_driver_dmac0 = {{&dev0_data, dmac_install, dmac_open, dmac_close}};

/* DMA Channel */

#define MAX_PING_PONG_SRCS 4
#define C_COMMON_ENTRY                                                                                    \
    dma_data *data = (dma_data *)userdata;                                                                \
    dmac_data *dmacdata = data->dmac_data;                                                                \
    volatile dmac_t *dmac = (volatile dmac_t *)dmacdata->base_addr;                                       \
    (void)dmac;                                                                                           \
    volatile dmac_channel_t *dma = (volatile dmac_channel_t *)dmac->channel + data->channel;              \
    (void)dma;

typedef struct
{
    dmac_data *dmac_data;
    size_t channel;
    int used;

    struct
    {
        SemaphoreHandle_t completion_event;
        uint32_t axi_master;
        int is_loop;
        union
        {
            struct
            {
                dmac_transfer_flow_t flow_control;
                size_t element_size;
                size_t count;
                void *alloc_mem;
                volatile void *dest;
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
    } session;
} dma_data;

static void dma_completion_isr(void *userdata)
{
    C_COMMON_ENTRY;

    configASSERT(dma->intstatus & 0x2);
    dma->intclear = 0xFFFFFFFF;

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (data->session.is_loop)
    {
        if (atomic_read(data->session.stop_signal))
        {
            release_axi_master(dmacdata, data->session.axi_master);
            if (data->session.stage_completion_handler)
                data->session.stage_completion_handler(data->session.stage_completion_handler_data);
            xSemaphoreGiveFromISR(data->session.completion_event, &xHigherPriorityTaskWoken);
        }
        else
        {
            size_t next_src_id = data->session.next_src_id + 1;
            if (next_src_id == data->session.src_num)
                next_src_id = 0;
            data->session.next_src_id = next_src_id;
            dma->sar = (uint64_t)data->session.srcs[next_src_id];

            size_t next_dest_id = data->session.next_dest_id + 1;
            if (next_dest_id == data->session.dest_num)
                next_dest_id = 0;
            data->session.next_dest_id = next_dest_id;
            dma->dar = (uint64_t)data->session.dests[next_dest_id];

            if (data->session.stage_completion_handler)
                data->session.stage_completion_handler(data->session.stage_completion_handler_data);
            dmac->chen |= 0x101 << data->channel;
        }
    }
    else
    {
        release_axi_master(dmacdata, data->session.axi_master);

        if (data->session.flow_control != DMAC_MEM2MEM_DMA && data->session.element_size < 4)
        {
            if (data->session.flow_control == DMAC_PRF2MEM_DMA)
            {
                if (data->session.element_size == 1)
                {
                    size_t i;
                    uint32_t *p_src = data->session.alloc_mem;
                    uint8_t *p_dst = (uint8_t *)data->session.dest;
                    for (i = 0; i < data->session.count; i++)
                        p_dst[i] = p_src[i];
                }
                else if (data->session.element_size == 2)
                {
                    size_t i;
                    uint32_t *p_src = data->session.alloc_mem;
                    uint16_t *p_dst = (uint16_t *)data->session.dest;
                    for (i = 0; i < data->session.count; i++)
                        p_dst[i] = p_src[i];
                }
                else
                {
                    configASSERT(!"invalid element size");
                }
            }
            else if (data->session.flow_control == DMAC_MEM2PRF_DMA)
                ;
            else
            {
                configASSERT(!"Impossible");
            }

            free(data->session.alloc_mem);
        }

        xSemaphoreGiveFromISR(data->session.completion_event, &xHigherPriorityTaskWoken);
    }
}

static void dma_install(void *userdata)
{
    C_COMMON_ENTRY;

    pic_set_irq_handler(IRQN_DMA0_INTERRUPT + data->channel, dma_completion_isr, userdata);
    pic_set_irq_priority(IRQN_DMA0_INTERRUPT + data->channel, 1);
    pic_set_irq_enable(IRQN_DMA0_INTERRUPT + data->channel, 1);
}

static int dma_open(void *userdata)
{
    C_COMMON_ENTRY;
    return atomic_cas(&data->used, 0, 1) == 0;
}

static void dma_close_imp(void *userdata)
{
    C_COMMON_ENTRY;
    atomic_set(&data->used, 0);
}

static void dma_set_select_request_imp(uint32_t request, void *userdata)
{
    C_COMMON_ENTRY;

    if (data->channel == SYSCTL_DMA_CHANNEL_5)
    {
        sysctl->dma_sel1.dma_sel5 = request;
    }
    else
    {
        sysctl_dma_sel0_t dma_sel;
        dma_sel = sysctl->dma_sel0;

        switch (data->channel)
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
            configASSERT(!"Invalid dma channel");
        }

        /* Write register back to bus */
        sysctl->dma_sel0 = dma_sel;
    }
}

static void dma_config_imp(uint32_t priority, void *userdata)
{
    C_COMMON_ENTRY;
    configASSERT((dmac->chen & (1 << data->channel)) == 0);
    configASSERT(priority <= 7);

    dmac_ch_cfg_u_t cfg_u;

    cfg_u.data = readq(&dma->cfg);
    cfg_u.ch_cfg.ch_prior = priority;
    writeq(cfg_u.data, &dma->cfg);
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

static void dma_loop_async_imp(const volatile void **srcs, size_t src_num, volatile void **dests, size_t dest_num, bool src_inc, bool dest_inc, size_t element_size, size_t count, size_t burst_size, dma_stage_completion_handler_t stage_completion_handler, void *stage_completion_handler_data, SemaphoreHandle_t completion_event, int *stop_signal, void *userdata)
{
    C_COMMON_ENTRY;

    if (count == 0)
    {
        xSemaphoreGive(completion_event);
        return;
    }

    src_inc = !src_inc;
    dest_inc = !dest_inc;
    configASSERT(count > 0 && count <= 0x3fffff);
    configASSERT((dmac->chen & (1 << data->channel)) == 0);
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

    cfg_u.data = readq(&dma->cfg);
    cfg_u.ch_cfg.tt_fc = flow_control;
    cfg_u.ch_cfg.hs_sel_src = mem_type_src ? DMAC_HS_SOFTWARE : DMAC_HS_HARDWARE;
    cfg_u.ch_cfg.hs_sel_dst = mem_type_dest ? DMAC_HS_SOFTWARE : DMAC_HS_HARDWARE;
    cfg_u.ch_cfg.src_per = data->channel;
    cfg_u.ch_cfg.dst_per = data->channel;
    cfg_u.ch_cfg.src_multblk_type = 0;
    cfg_u.ch_cfg.dst_multblk_type = 0;

    writeq(cfg_u.data, &dma->cfg);

    data->session.is_loop = 1;
    data->session.flow_control = flow_control;

    dma->sar = (uint64_t)srcs[0];
    dma->dar = (uint64_t)dests[0];

    dma->block_ts = count - 1;

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

    dma->intstatus_en = 0xFFFFFFE2;
    dma->intclear = 0xFFFFFFFF;

    dmac_ch_ctl_u_t ctl_u;

    ctl_u.data = readq(&dma->ctl);
    ctl_u.ch_ctl.sinc = src_inc;
    ctl_u.ch_ctl.src_tr_width = tr_width;
    ctl_u.ch_ctl.src_msize = msize;
    ctl_u.ch_ctl.dinc = dest_inc;
    ctl_u.ch_ctl.dst_tr_width = tr_width;
    ctl_u.ch_ctl.dst_msize = msize;

    uint32_t axi_master = add_lru_axi_master(dmacdata);
    data->session.axi_master = axi_master;

    ctl_u.ch_ctl.sms = axi_master;
    ctl_u.ch_ctl.dms = axi_master;

    writeq(ctl_u.data, &dma->ctl);

    data->session.completion_event = completion_event;
    data->session.stage_completion_handler_data = stage_completion_handler_data;
    data->session.stage_completion_handler = stage_completion_handler;
    data->session.stop_signal = stop_signal;
    data->session.src_num = src_num;
    data->session.dest_num = dest_num;
    data->session.next_src_id = 0;
    data->session.next_dest_id = 0;
    size_t i = 0;
    for (i = 0; i < src_num; i++)
        data->session.srcs[i] = srcs[i];
    for (i = 0; i < dest_num; i++)
        data->session.dests[i] = dests[i];

    dmac->chen |= 0x101 << data->channel;
}

static void dma_transmit_async_imp(const volatile void *src, volatile void *dest, bool src_inc, bool dest_inc, size_t element_size, size_t count, size_t burst_size, SemaphoreHandle_t completion_event, void *userdata)
{
    C_COMMON_ENTRY;

    if (count == 0)
    {
        xSemaphoreGive(completion_event);
        return;
    }

    src_inc = !src_inc;
    dest_inc = !dest_inc;
    configASSERT(count > 0 && count <= 0x3fffff);
    configASSERT((dmac->chen & (1 << data->channel)) == 0);

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

    cfg_u.data = readq(&dma->cfg);
    cfg_u.ch_cfg.tt_fc = flow_control;
    cfg_u.ch_cfg.hs_sel_src = mem_type_src ? DMAC_HS_SOFTWARE : DMAC_HS_HARDWARE;
    cfg_u.ch_cfg.hs_sel_dst = mem_type_dest ? DMAC_HS_SOFTWARE : DMAC_HS_HARDWARE;
    cfg_u.ch_cfg.src_per = data->channel;
    cfg_u.ch_cfg.dst_per = data->channel;
    cfg_u.ch_cfg.src_multblk_type = 0;
    cfg_u.ch_cfg.dst_multblk_type = 0;

    writeq(cfg_u.data, &dma->cfg);

    data->session.is_loop = 0;
    data->session.flow_control = flow_control;

    size_t old_elm_size = element_size;
    data->session.element_size = old_elm_size;
    data->session.count = count;
    data->session.dest = dest;
    data->session.alloc_mem = NULL;

    if (flow_control != DMAC_MEM2MEM_DMA && old_elm_size < 4)
    {
        void *alloc_mem = malloc(sizeof(uint32_t) * count);
        data->session.alloc_mem = alloc_mem;
        element_size = sizeof(uint32_t);

        if (!mem_type_src)
        {
            dma->sar = (uint64_t)src;
            dma->dar = (uint64_t)alloc_mem;
        }
        else if (!mem_type_dest)
        {
            if (old_elm_size == 1)
            {
                size_t i;
                const uint8_t *p_src = (const uint8_t *)src;
                uint32_t *p_dst = alloc_mem;
                for (i = 0; i < count; i++)
                    p_dst[i] = p_src[i];
            }
            else if (old_elm_size == 2)
            {
                size_t i;
                const uint16_t *p_src = (const uint16_t *)src;
                uint32_t* p_dst = alloc_mem;
                for (i = 0; i < count; i++)
                    p_dst[i] = p_src[i];
            }
            else
            {
                configASSERT(!"invalid element size");
            }

            dma->sar = (uint64_t)alloc_mem;
            dma->dar = (uint64_t)dest;
        }
        else
        {
            configASSERT(!"Impossible");
        }
    }
    else
    {
        dma->sar = (uint64_t)src;
        dma->dar = (uint64_t)dest;
    }

    dma->block_ts = count - 1;

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

    dma->intstatus_en = 0xFFFFFFE2;
    dma->intclear = 0xFFFFFFFF;

    dmac_ch_ctl_u_t ctl_u;

    ctl_u.data = readq(&dma->ctl);
    ctl_u.ch_ctl.sinc = src_inc;
    ctl_u.ch_ctl.src_tr_width = tr_width;
    ctl_u.ch_ctl.src_msize = msize;
    ctl_u.ch_ctl.dinc = dest_inc;
    ctl_u.ch_ctl.dst_tr_width = tr_width;
    ctl_u.ch_ctl.dst_msize = msize;

    uint32_t axi_master = add_lru_axi_master(dmacdata);
    data->session.axi_master = axi_master;

    ctl_u.ch_ctl.sms = axi_master;
    ctl_u.ch_ctl.dms = axi_master;

    writeq(ctl_u.data, &dma->ctl);

    data->session.completion_event = completion_event;
    dmac->chen |= 0x101 << data->channel;
}

static dma_data dev0_c0_data = {&dev0_data, 0, 0, {0}};
static dma_data dev0_c1_data = {&dev0_data, 1, 0, {0}};
static dma_data dev0_c2_data = {&dev0_data, 2, 0, {0}};
static dma_data dev0_c3_data = {&dev0_data, 3, 0, {0}};
static dma_data dev0_c4_data = {&dev0_data, 4, 0, {0}};
static dma_data dev0_c5_data = {&dev0_data, 5, 0, {0}};

const dma_driver_t g_dma_driver_dma0 = {{&dev0_c0_data, dma_install, dma_open, dma_close_imp}, dma_set_select_request_imp, dma_config_imp, dma_transmit_async_imp, dma_loop_async_imp};
const dma_driver_t g_dma_driver_dma1 = {{&dev0_c1_data, dma_install, dma_open, dma_close_imp}, dma_set_select_request_imp, dma_config_imp, dma_transmit_async_imp, dma_loop_async_imp};
const dma_driver_t g_dma_driver_dma2 = {{&dev0_c2_data, dma_install, dma_open, dma_close_imp}, dma_set_select_request_imp, dma_config_imp, dma_transmit_async_imp, dma_loop_async_imp};
const dma_driver_t g_dma_driver_dma3 = {{&dev0_c3_data, dma_install, dma_open, dma_close_imp}, dma_set_select_request_imp, dma_config_imp, dma_transmit_async_imp, dma_loop_async_imp};
const dma_driver_t g_dma_driver_dma4 = {{&dev0_c4_data, dma_install, dma_open, dma_close_imp}, dma_set_select_request_imp, dma_config_imp, dma_transmit_async_imp, dma_loop_async_imp};
const dma_driver_t g_dma_driver_dma5 = {{&dev0_c5_data, dma_install, dma_open, dma_close_imp}, dma_set_select_request_imp, dma_config_imp, dma_transmit_async_imp, dma_loop_async_imp};
