#include "indicator_led.h"
#include <rtdevice.h>
#include "board.h"
#include "stopwatch.h"

/* 推荐引脚：PC13 运行闪烁；PB0 暂停常亮；PB1 错误（预留）
 * 如与实际连线不符，请在此调整或通过 Kconfig/宏重定义。 */
#ifndef LED_RUN_PIN
#define LED_RUN_PIN     GET_PIN(C, 13)
#endif
#ifndef LED_PAUSE_PIN
#define LED_PAUSE_PIN   GET_PIN(B, 0)
#endif
#ifndef LED_ERR_PIN
#define LED_ERR_PIN     GET_PIN(B, 1)
#endif

static rt_thread_t s_led_thread;

static void led_set(rt_base_t pin, int on)
{
    /* 若为低电平点亮，请在此按需取反 */
    rt_pin_write(pin, on ? PIN_HIGH : PIN_LOW);
}

static void indicator_led_entry(void *parameter)
{
    (void)parameter;
    int blink = 0;
    while (1)
    {
        stopwatch_state_t s = stopwatch_get_state();
        switch (s)
        {
        case STOPWATCH_STATE_RUNNING:
            blink = !blink;
            led_set(LED_RUN_PIN, blink);
            led_set(LED_PAUSE_PIN, 0);
            led_set(LED_ERR_PIN, 0);
            rt_thread_mdelay(500);
            break;
        case STOPWATCH_STATE_PAUSED:
            led_set(LED_RUN_PIN, 0);
            led_set(LED_PAUSE_PIN, 1);
            led_set(LED_ERR_PIN, 0);
            rt_thread_mdelay(200);
            break;
        default:
            led_set(LED_RUN_PIN, 0);
            led_set(LED_PAUSE_PIN, 0);
            led_set(LED_ERR_PIN, 0);
            rt_thread_mdelay(200);
            break;
        }
    }
}

rt_err_t indicator_led_init(void)
{
    rt_pin_mode(LED_RUN_PIN, PIN_MODE_OUTPUT);
    rt_pin_mode(LED_PAUSE_PIN, PIN_MODE_OUTPUT);
    rt_pin_mode(LED_ERR_PIN, PIN_MODE_OUTPUT);
    led_set(LED_RUN_PIN, 0);
    led_set(LED_PAUSE_PIN, 0);
    led_set(LED_ERR_PIN, 0);

    s_led_thread = rt_thread_create("led_ind", indicator_led_entry, RT_NULL, 768, RT_THREAD_PRIORITY_MAX - 3, 10);
    if (!s_led_thread)
    {
        return -RT_ENOMEM;
    }
    rt_thread_startup(s_led_thread);
    return RT_EOK;
}


