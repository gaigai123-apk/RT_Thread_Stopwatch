#include "notifier_buzzer.h"
#include <rtdevice.h>
#include "board.h"

/* 有源蜂鸣器，低电平触发；默认 PB12，可按需在编译宏重定义 */
#ifndef BUZZER_PIN
#define BUZZER_PIN  GET_PIN(B, 12)
#endif

static rt_bool_t s_beep_enabled = 1;

static void buz_set(int on)
{
    /* 低电平响，如果你的模块为高电平响，请取反 */
    rt_pin_write(BUZZER_PIN, on ? PIN_LOW : PIN_HIGH);
}

rt_err_t notifier_buzzer_init(void)
{
    rt_pin_mode(BUZZER_PIN, PIN_MODE_OUTPUT);
    buz_set(0);
    return RT_EOK;
}

void notifier_beep_enable(rt_bool_t enable)
{
    s_beep_enabled = enable ? 1 : 0;
    if (!s_beep_enabled) buz_set(0);
}

rt_bool_t notifier_beep_is_enabled(void)
{
    return s_beep_enabled;
}

void notifier_beep_once(rt_uint16_t ms)
{
    if (!s_beep_enabled) return;
    buz_set(1);
    rt_thread_mdelay(ms);
    buz_set(0);
}










