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
#include <stdio.h>
#include <stdlib.h>
#include <FreeRTOS.h>
#include <task.h>
#include <devices.h>
#include "dvp_camera.h"
#include "lcd.h"
#include "ov5640.h"
#include "region_layer.h"
#include "image_process.h"
#include "w25qxx.h"
#define INCBIN_STYLE INCBIN_STYLE_SNAKE
#define INCBIN_PREFIX
#include "incbin.h"
#include "printf.h"
#include "iomem.h"
#include <semphr.h>
#include <filesystem.h>
#include <storage/sdcard.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/lock.h>

handle_t sd0;
FILE *stream;
char buffer[320];
char msg[]="11k233333333333333333k233333333333333333k233333333333333333k233333333333333333k233333333333333333k233333333333333333k2333333333333333300";

image_t kpu_image, display_image0, display_image1;
handle_t model_context;
camera_context_t camera_ctx;
static region_layer_t face_detect_rl;
static obj_info_t face_detect_info;
#define ANCHOR_NUM 5
static float anchor[ANCHOR_NUM * 2] = {1.889,2.5245,  2.9465,3.94056, 3.99987,5.3658, 5.155437,6.92275, 6.718375,9.01025};
static volatile int task1_flag;
static volatile int task2_flag;
static char display[32];
static _lock_t flash_lock;

#define LOAD_KMODEL_FROM_FLASH 1

#if LOAD_KMODEL_FROM_FLASH
#define KMODEL_SIZE (380 * 1024)
uint8_t *model_data;
#else
INCBIN(model, "../src/face_detect/detect.kmodel");
#endif
static void draw_edge(uint32_t *gram, obj_info_t *obj_info, uint32_t index, uint16_t color)
{
    uint32_t data = ((uint32_t)color << 16) | (uint32_t)color;
    volatile uint32_t *addr1, *addr2, *addr3, *addr4, x1, y1, x2, y2;

    x1 = obj_info->obj[index].x1;
    y1 = obj_info->obj[index].y1;
    x2 = obj_info->obj[index].x2;
    y2 = obj_info->obj[index].y2;

    if (x1 <= 0)
        x1 = 1;
    if (x2 >= 319)
        x2 = 318;
    if (y1 <= 0)
        y1 = 1;
    if (y2 >= 239)
        y2 = 238;

    addr1 = gram + (320 * y1 + x1) / 2;
    addr2 = gram + (320 * y1 + x2 - 8) / 2;
    addr3 = gram + (320 * (y2 - 1) + x1) / 2;
    addr4 = gram + (320 * (y2 - 1) + x2 - 8) / 2;
    for (uint32_t i = 0; i < 4; i++)
    {
        *addr1 = data;
        *(addr1 + 160) = data;
        *addr2 = data;
        *(addr2 + 160) = data;
        *addr3 = data;
        *(addr3 + 160) = data;
        *addr4 = data;
        *(addr4 + 160) = data;
        addr1++;
        addr2++;
        addr3++;
        addr4++;
    }
    addr1 = gram + (320 * y1 + x1) / 2;
    addr2 = gram + (320 * y1 + x2 - 2) / 2;
    addr3 = gram + (320 * (y2 - 8) + x1) / 2;
    addr4 = gram + (320 * (y2 - 8) + x2 - 2) / 2;
    for (uint32_t i = 0; i < 8; i++)
    {
        *addr1 = data;
        *addr2 = data;
        *addr3 = data;
        *addr4 = data;
        addr1 += 160;
        addr2 += 160;
        addr3 += 160;
        addr4 += 160;
    }
}

#define TEST_START_ADDR (0xB00000U)
#define TEST_START_ADDR2 (0x100000U)

#define TEST_NUMBER (0x1000U)
uint8_t data_buf_send[TEST_NUMBER];
uint8_t *data_buf_recv;
handle_t spi3;
static SemaphoreHandle_t event_read;

void task_list(void *arg)
{
    char buffer[2048];

    for(;;)
    {
        vTaskDelay(10000 / portTICK_RATE_MS);
        vTaskList((char *)&buffer);
        printk("task_name   task_state  priority   stack  task_num\n");
        printk("%s ", buffer);
        printk("FreeHeapSize:%ld Byte\n", xPortGetFreeHeapSize());
        printk("MinimumEverFreeHeapSize:%ld Byte\n", xPortGetMinimumEverFreeHeapSize());
        printk("unused:%d\n", iomem_unused());
    }
    vTaskDelete(NULL);
    return;
}

void vTask1()
{
    int32_t index = 0;
    int32_t page_addr = TEST_START_ADDR;
    struct timeval get_time[2];
    while (1) 
    {
        task1_flag = 1;
        //configASSERT(xSemaphoreTake(event_read, 200) == pdTRUE);
        _lock_acquire_recursive(&flash_lock);
        gettimeofday(&get_time[0], NULL);
#if 0
        fseek(stream,0,SEEK_SET);
        fwrite(msg, 1, strlen(msg)+1, stream);
#else
        if(page_addr >= 0x1000000 - TEST_NUMBER)
        {
            page_addr = TEST_START_ADDR;
        }
        task1_flag = 2;
        w25qxx_write_data(page_addr, data_buf_send, TEST_NUMBER);

        w25qxx_read_data(page_addr, data_buf_recv, TEST_NUMBER);

        for (index = 0; index < TEST_NUMBER; index++)
        {
            if (data_buf_recv[index] != (uint8_t)index) {
                printk("task1 Read err:0x%x 0x%x\n", data_buf_recv[index], index);
                index += 0x100;
            }
        }
        page_addr += TEST_NUMBER;
        task1_flag = 3;
#endif

        gettimeofday(&get_time[1], NULL);;
        //printf("vtask1:%f ms \n", ((get_time[1].tv_sec - get_time[0].tv_sec)*1000*1000 + (get_time[1].tv_usec - get_time[0].tv_usec))/1000.0);
        //xSemaphoreGive(event_read);
        _lock_release_recursive(&flash_lock);

        task1_flag = 4;
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

void vTask2()
{
    int32_t index = 0;
    int32_t page_addr = TEST_START_ADDR2;
    struct timeval get_time[2];

    while (1) 
    {
        task2_flag = 1;
        //configASSERT(xSemaphoreTake(event_read, 200) == pdTRUE);
        _lock_acquire_recursive(&flash_lock);
        gettimeofday(&get_time[0], NULL);
#if 0
        fseek(stream,0,SEEK_SET);
        fwrite(msg, 1, strlen(msg)+1, stream);
#else
        if(page_addr >= 0xA00000 - TEST_NUMBER)
        {
            page_addr = TEST_START_ADDR2;
        }
        task2_flag = 2;
        w25qxx_write_data(page_addr, data_buf_send, TEST_NUMBER);

        w25qxx_read_data(page_addr, data_buf_recv, TEST_NUMBER);

        for (index = 0; index < TEST_NUMBER; index++)
        {
            if (data_buf_recv[index] != (uint8_t)index) {
                printk("task2 Read err:0x%x 0x%x\n", data_buf_recv[index], index);
                index += 0x100;
            }
        }
        page_addr += TEST_NUMBER;
        task2_flag = 3;
#endif

        gettimeofday(&get_time[1], NULL);;
        //printf("vtask2:%f ms \n", ((get_time[1].tv_sec - get_time[0].tv_sec)*1000*1000 + (get_time[1].tv_usec - get_time[0].tv_usec))/1000.0);
        //xSemaphoreGive(event_read);
        _lock_release_recursive(&flash_lock);
        task2_flag = 4;
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

void vTask3()
{
    int32_t index = 0;
    struct timeval get_time[2];
    int32_t addr = 0;

    while (1) 
    {
        //_lock_acquire_recursive(&flash_lock);
        gettimeofday(&get_time[0], NULL);
        if(addr < 1024*1024*10)
            addr = 0;
        fseek(stream,addr,SEEK_SET);
        fwrite(msg, 1, strlen(msg)+1, stream);
        fseek(stream,addr,SEEK_SET);
        fread(buffer, 1, strlen(msg)+1, stream);
        gettimeofday(&get_time[1], NULL);
        for(index=0; index < strlen(msg); index++)
        {
            if(buffer[index] != msg[index])
            {
                printk("task2 sd err:0x%x 0x%x\n", buffer[index], msg[index]);
                break;
            }
        }
        addr += 100;
        //printf("vtask2:%f ms \n", ((get_time[1].tv_sec - get_time[0].tv_sec)*1000*1000 + (get_time[1].tv_usec - get_time[0].tv_usec))/1000.0);
        //_lock_release_recursive(&flash_lock);
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

void detect()
{
    int time_count = 0;
    struct timeval get_time[2];
    uint64_t time_last,time_now;
    gettimeofday(&get_time[0], NULL);

    while (1) {
        while (camera_ctx.dvp_finish_flag == 0)
            ;
        camera_ctx.dvp_finish_flag = 0;
        uint32_t *lcd_gram = camera_ctx.gram_mux ? (uint32_t *)camera_ctx.lcd_image1->addr : (uint32_t *)camera_ctx.lcd_image0->addr;

#if 1
        if (kpu_run(model_context, camera_ctx.ai_image->addr) != 0) {
            printf("Cannot run kmodel.\n");
            exit(-1);
        }

        float *output;
        size_t output_size;
        kpu_get_output(model_context, 0, (uint8_t **)&output, &output_size);

        face_detect_rl.input = output;
        region_layer_run(&face_detect_rl, &face_detect_info);
        //for (uint32_t face_cnt = 0; face_cnt < face_detect_info.obj_number; face_cnt++) {
        //    draw_edge(lcd_gram, &face_detect_info, face_cnt, RED);
        //}
#endif
        if(face_detect_info.obj_number)
        {
            printk("=====>face detect  %d \n", face_detect_info.obj_number);
            if(!task1_flag)
                printk("==========>%d %d \n", task1_flag, task2_flag);
            if(!task2_flag)
                printk("==========>>%d %d \n", task1_flag, task2_flag);
        }
        
        //sprintf(display, "task1 = %d task2 = %d", task1_flag, task2_flag);
        task1_flag = 0;
        task2_flag = 0;

        //lcd_draw_string(50, 50, display, RED);

        lcd_draw_picture(0, 0, 320, 240, lcd_gram);
        camera_ctx.gram_mux ^= 0x01;
        time_count ++;
        if(time_count == 100)
        {
            gettimeofday(&get_time[1], NULL);;
            printf("SPF:%fms Byte\n", ((get_time[1].tv_sec - get_time[0].tv_sec)*1000*1000 + (get_time[1].tv_usec - get_time[0].tv_usec))/1000.0/100);
            memcpy(&get_time[0], &get_time[1], sizeof(struct timeval));
            time_count = 0;
        }
    }
    io_close(model_context);
    image_deinit(&kpu_image);
    image_deinit(&display_image0);
    image_deinit(&display_image1);
}

handle_t install_sdcard()
{
    handle_t spi, gpio;
    configASSERT(spi = io_open("/dev/spi1"));
    configASSERT(gpio = io_open("/dev/gpio0"));
    handle_t sd0 = spi_sdcard_driver_install(spi, gpio, 7);
    io_close(spi);
    io_close(gpio);
    return sd0;
}

int main(void)
{
    struct timeval get_time[2];
    gettimeofday(&get_time[0], NULL);
    //event_read = xSemaphoreCreateMutex();
    data_buf_recv = iomem_malloc(TEST_NUMBER);
    for (uint32_t index = 0; index < TEST_NUMBER; index++)
    {
        data_buf_send[index] = (uint8_t)(index);
    }
#if LOAD_KMODEL_FROM_FLASH
    model_data = (uint8_t *)iomem_malloc(KMODEL_SIZE);
#endif
#if 0
    printf("Hello sd\n");

    sd0 = install_sdcard();
    configASSERT(sd0);
    configASSERT(filesystem_mount("/fs/0/", sd0) == 0);
    io_close(sd0);
    if((stream=fopen("/fs/0/test_syscalls.txt","w+"))==NULL)
    {
        fprintf(stderr,"Can not open file.\n");
        exit(-1);
    }
#endif
    uint64_t time;
    kpu_image.pixel = 3;
    kpu_image.width = 320;
    kpu_image.height = 240;
    image_init(&kpu_image);
    
    display_image0.pixel = 2;
    display_image0.width = 320;
    display_image0.height = 240;
    image_init(&display_image0);

    display_image1.pixel = 2;
    display_image1.width = 320;
    display_image1.height = 240;
    image_init(&display_image1);

    camera_ctx.dvp_finish_flag = 0;
    camera_ctx.ai_image = &kpu_image;
    camera_ctx.lcd_image0 = &display_image0;
    camera_ctx.lcd_image1 = &display_image1;
    camera_ctx.gram_mux = 0;

    face_detect_rl.anchor_number = ANCHOR_NUM;
    face_detect_rl.anchor = anchor;
    face_detect_rl.threshold = 0.7;
    face_detect_rl.nms_value = 0.3;
    region_layer_init(&face_detect_rl, 20, 15, 30, camera_ctx.ai_image->width, camera_ctx.ai_image->height);

    printf("lcd init\n");
    lcd_init();
    printf("DVP init\n");
    dvp_init(&camera_ctx);
    ov5640_init();

    spi3 = io_open("/dev/spi3");
    configASSERT(spi3);
    w25qxx_init(spi3);
#if LOAD_KMODEL_FROM_FLASH
    w25qxx_read_data(0xA00000, model_data, KMODEL_SIZE);
#endif
    model_context = kpu_model_load_from_buffer(model_data);
    gettimeofday(&get_time[1], NULL);
    printf("Start time:%fms\n", ((get_time[1].tv_sec - get_time[0].tv_sec)*1000*1000 + (get_time[1].tv_usec - get_time[0].tv_usec))/1000.0);
    printf("xTaskCreate\n");
    printf("xTaskCreate\n");
    printf("xTaskCreate\n");
    printf("xTaskCreate\n");
    xTaskCreate(detect, "detect", 2048*2, NULL, 3, NULL);
    xTaskCreate(vTask1, "vTask1", 2048, NULL, 3, NULL);
    xTaskCreate(vTask2, "vTask2", 2048, NULL, 3, NULL);
    //xTaskCreate(vTask3, "vTask3", 2048, NULL, 3, NULL);
    xTaskCreate(task_list, "task_list", 2048, NULL, 2, NULL);
    vTaskDelete(NULL);
}
