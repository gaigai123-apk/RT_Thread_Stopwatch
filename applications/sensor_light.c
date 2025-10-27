#include "sensor_light.h"
#include <rtdevice.h>
#include "board.h"
#include "notifier_buzzer.h"
#include "ui_oled.h"
#define DBG_TAG "light"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#ifndef LIGHT_DO_PIN
#define LIGHT_DO_PIN   GET_PIN(B, 13)
#endif

static rt_thread_t s_light_thread;
static rt_bool_t   s_enabled = 1;
static rt_bool_t   s_dark = 0;
static rt_bool_t   s_invert = 1; /* 默认反相：多数模块 DO=1=暗，按现场反馈修正 */

static rt_bool_t read_do(void)
{
    /* 模块 DO 高/低由模块阈值决定；此处认为 0=黑暗，1=明亮（若相反可取反） */
    int v = rt_pin_read(LIGHT_DO_PIN);
    rt_bool_t dark = v ? 0 : 1; /* 默认 DO=0 为黑暗 */
    if (s_invert) dark = !dark;
    return dark;
}

static void apply_state(rt_bool_t dark)
{
    s_dark = dark ? 1 : 0;
    if (s_dark)
    {
        notifier_beep_enable(0);
        ui_oled_set_refresh_ms(300);
        LOG_I("env=dark -> beep=off, oled=300ms");
    }
    else
    {
        notifier_beep_enable(1);
        ui_oled_set_refresh_ms(10);
        LOG_I("env=light -> beep=on, oled=10ms");
    }
}

static void light_thread_entry(void *parameter)
{
    (void)parameter;
    const rt_uint16_t sample_ms = 50;
    rt_uint8_t dark_cnt = 0, light_cnt = 0;
    while (1)
    {
        if (s_enabled)
        {
            rt_bool_t dark_now = read_do();
            if (dark_now)
            {
                dark_cnt++;
                light_cnt = 0;
            }
            else
            {
                light_cnt++;
                dark_cnt = 0;
            }
            /* 迟滞：连续3次判定才切换状态 */
            if (!s_dark && dark_cnt >= 3)
            {
                apply_state(1);
            }
            else if (s_dark && light_cnt >= 3)
            {
                apply_state(0);
            }
        }
        rt_thread_mdelay(sample_ms);
    }
}

rt_err_t sensor_light_init(void)
{
    rt_pin_mode(LIGHT_DO_PIN, PIN_MODE_INPUT_PULLUP);
    /* 启动即读取当前环境并直接应用，避免上电时与真实环境不符 */
    apply_state(read_do());
    s_light_thread = rt_thread_create("light", light_thread_entry, RT_NULL, 768, RT_THREAD_PRIORITY_MAX - 3, 10);
    if (!s_light_thread) return -RT_ENOMEM;
    rt_thread_startup(s_light_thread);
    return RT_EOK;
}

void sensor_light_enable(rt_bool_t enable)
{
    s_enabled = enable ? 1 : 0;
}

rt_bool_t sensor_light_is_enabled(void)
{
    return s_enabled;
}

void sensor_light_set_invert(rt_bool_t invert)
{
    s_invert = invert ? 1 : 0;
}

rt_bool_t sensor_light_get_invert(void)
{
    return s_invert;
}


