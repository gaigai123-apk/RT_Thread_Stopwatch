/*
 * Copyright (c) 2006-2025, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-10-26     RT-Thread    first version
 */

#include <rtthread.h>

#define DBG_TAG "main"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>
#include "stopwatch.h"
#include "indicator_led.h"
#include "notifier_buzzer.h"
#include "ui_oled.h"
#include "sensor_light.h"

int main(void)
{
    int count = 1;

    /* 初始化秒表服务 */
    stopwatch_init();
    /* 初始化 LED 指示 */
    indicator_led_init();
    /* 初始化 蜂鸣器 */
    notifier_buzzer_init();
    /* 初始化 OLED UI */
    ui_oled_init();
    /* 初始化 光敏联动 */
    sensor_light_init();

    while (count++)
    {
//        LOG_D("Hello RT-Thread! Use sw.* commands to control stopwatch.");
        rt_thread_mdelay(1000);
    }

    return RT_EOK;
}
