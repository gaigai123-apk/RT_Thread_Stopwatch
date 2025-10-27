#include "ui_oled.h"
#include <rtdevice.h>
#include <rtdbg.h>
#include "stopwatch.h"
#include "board.h"
#include <string.h>

#include "qu_dong/OLED/OLED.h"

/* 使用 qu_dong 的 OLED 显示 API，但总线由其内部位带实现。
 * 若需改为 RT-Thread I2C 设备，可替换为 u8g2/ssd1306 驱动。 */

static rt_thread_t s_ui_thread;
static rt_bool_t s_oled_enabled = 1;
static rt_uint16_t s_refresh_ms = 10; /* 默认 10ms 尝试，若不稳可改为 20ms */
static rt_uint8_t s_page_drawn = 0; /* 页面静态元素是否已绘制 */

static void format_time_ms(rt_uint32_t total_ms, char *buf, rt_size_t buf_len)
{
    rt_uint32_t cs = (total_ms / 10U) % 100U;          /* 厘秒，两位 00-99 */
    rt_uint32_t sec = (total_ms / 1000U) % 60U;
    rt_uint32_t min = (total_ms / 60000U);             /* 分钟不取模 */
    rt_snprintf(buf, buf_len, "%02u:%02u.%02u", (unsigned)min, (unsigned)sec, (unsigned)cs);
}

/* 仅秒与厘秒，用于窄宽度显示（ss.cc，共 5 字符） */
static void format_time_sscc(rt_uint32_t total_ms, char *buf, rt_size_t buf_len)
{
    rt_uint32_t cs = (total_ms / 10U) % 100U;
    rt_uint32_t sec = (total_ms / 1000U) % 60U;
    rt_snprintf(buf, buf_len, "%02u.%02u", (unsigned)sec, (unsigned)cs);
}

static void draw_main_page(void)
{
    char buf[24];
    rt_uint32_t total = stopwatch_get_total_ms();
    format_time_ms(total, buf, sizeof(buf));
    /* 首次进入页面时绘制静态元素 */
    if (!s_page_drawn)
    {
        OLED_Clear();
        OLED_ShowString(0, 0, "Stopwatch", OLED_6X8);
        OLED_ShowString(0, 36, "Lap:", OLED_6X8);
        OLED_Update();
        s_page_drawn = 1;
    }
    /* 动态区域1：主时间（约占 Y=16~31） - 固定矩形每帧刷新 */
    OLED_ClearArea(0, 16, 128, 16);
    OLED_ShowString(0, 16, buf, OLED_8X16);
    OLED_UpdateArea(0, 16, 128, 16);

    /* 动态区域2：最近一圈（Y=36~43）——仅清除数值区域，保留左侧标签 "Lap:"，固定矩形每帧刷新 */
    rt_uint16_t cnt = stopwatch_get_lap_count();
    OLED_ClearArea(30, 36, 98, 8);
    if (cnt > 0)
    {
        rt_uint32_t last = stopwatch_get_latest_lap_ms();
        format_time_ms(last, buf, sizeof(buf));
        OLED_ShowString(30, 36, buf, OLED_6X8);
    }
    OLED_UpdateArea(30, 36, 98, 8);
}

static rt_uint16_t s_laps_offset = 0; /* 从第几条开始显示 */

static void draw_laps_page(void)
{
    OLED_Clear();
    OLED_ShowString(0, 0, "Laps", OLED_6X8);
    rt_uint16_t cnt = stopwatch_get_lap_count();
    /* 保底 */
    if (s_laps_offset >= cnt) s_laps_offset = 0;
    /* 每页显示 6 条（行高 8），保留标题一行 */
    for (rt_uint16_t i = 0; i < 6; i++)
    {
        rt_uint16_t idx = s_laps_offset + i;
        if (idx >= cnt) break;
        char line[24];
        char tbuf[16];
        format_time_ms(stopwatch_get_lap_ms(idx), tbuf, sizeof(tbuf));
        rt_snprintf(line, sizeof(line), "#%u %s", (unsigned)(idx+1), tbuf);
        OLED_ShowString(0, 8*(i+1), line, OLED_6X8);
    }
    /* 底部显示统计：min/max/avg（若有数据） */
    if (cnt > 0)
    {
        rt_uint32_t min_ms = 0xFFFFFFFFu, max_ms = 0, sum_ms = 0;
        for (rt_uint16_t i = 0; i < cnt; i++)
        {
            rt_uint32_t v = stopwatch_get_lap_ms(i);
            if (v < min_ms) min_ms = v;
            if (v > max_ms) max_ms = v;
            sum_ms += v;
        }
        char stat[24];
        rt_uint32_t avg_ms = sum_ms / cnt;
        rt_snprintf(stat, sizeof(stat), "m%u M%u a%u", (unsigned)min_ms, (unsigned)max_ms, (unsigned)avg_ms);
        OLED_ShowString(0, 56, stat, OLED_6X8);
    }
    OLED_Update();
}

static rt_uint8_t s_page = 0; /* 0: main, 1: laps */

static void ui_entry(void *parameter)
{
    (void)parameter;
    while (1)
    {
        if (s_oled_enabled)
        {
            if (s_page == 0) draw_main_page(); else draw_laps_page();
        }
        rt_thread_mdelay(s_refresh_ms);
    }
}

rt_err_t ui_oled_init(void)
{
    /* 使用默认地址 0x3C（0x78） */
    OLED_SetI2CAddress(0x78);
    OLED_Init();
    OLED_Clear();
    OLED_ShowString(0, 0, "RT-Thread", OLED_6X8);
    OLED_ShowString(0, 16, "Stopwatch", OLED_8X16);
    OLED_Update();

    s_ui_thread = rt_thread_create("ui_oled", ui_entry, RT_NULL, 1024, RT_THREAD_PRIORITY_MAX - 4, 10);
    if (!s_ui_thread)
    {
        return -RT_ENOMEM;
    }
    rt_thread_startup(s_ui_thread);
    return RT_EOK;
}

void ui_oled_set_refresh_ms(rt_uint16_t ms)
{
    if (ms < 10) ms = 10;
    s_refresh_ms = ms;
}

void ui_oled_set_enabled(rt_bool_t enabled)
{
    s_oled_enabled = enabled ? 1 : 0;
}

void ui_oled_set_page(rt_uint8_t page)
{
    s_page = (page != 0) ? 1 : 0;
    s_page_drawn = 0; /* 切页后触发静态区域重绘 */
}

void ui_oled_laps_prev(void)
{
    if (s_laps_offset >= 6) s_laps_offset -= 6; else s_laps_offset = 0;
}

void ui_oled_laps_next(void)
{
    rt_uint16_t cnt = stopwatch_get_lap_count();
    if (s_laps_offset + 6 < cnt) s_laps_offset += 6;
}

void ui_oled_laps_reset(void)
{
    s_laps_offset = 0;
}


