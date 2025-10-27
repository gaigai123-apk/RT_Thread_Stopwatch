#include <rtthread.h>
#include <finsh.h>
#include <stdlib.h>
#include <string.h>
#include "stopwatch.h"
#include "notifier_buzzer.h"
#include "sensor_light.h"
#include "ui_oled.h"

static void format_time(rt_uint32_t total_ms, char *buf, rt_size_t buf_len)
{
    rt_uint32_t ms = total_ms % 1000U;
    rt_uint32_t sec = (total_ms / 1000U) % 60U;
    rt_uint32_t min = (total_ms / 60000U) % 60U;
    rt_uint32_t hr  = (total_ms / 3600000U);
    if (hr > 0)
        rt_snprintf(buf, buf_len, "%02u:%02u:%02u.%03u", (unsigned)hr, (unsigned)min, (unsigned)sec, (unsigned)ms);
    else
        rt_snprintf(buf, buf_len, "%02u:%02u.%03u", (unsigned)min, (unsigned)sec, (unsigned)ms);
}

/* ================== 基本命令 ================== */
static int cmd_sw_start(int argc, char **argv)
{
    (void)argc; (void)argv;
    stopwatch_start();
    rt_kprintf("sw: start\n");
    notifier_beep_once(50);
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_sw_start, sw_start, Start_stopwatch);

static int cmd_sw_stop(int argc, char **argv)
{
    (void)argc; (void)argv;
    stopwatch_stop();
    rt_kprintf("sw: stop\n");
    notifier_beep_once(100);
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_sw_stop, sw_stop, Stop_stopwatch);

static int cmd_sw_reset(int argc, char **argv)
{
    (void)argc; (void)argv;
    stopwatch_reset();
    rt_kprintf("sw: reset\n");
    notifier_beep_once(30);
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_sw_reset, sw_reset, Reset_stopwatch);

static int cmd_sw_lap(int argc, char **argv)
{
    (void)argc; (void)argv;
    rt_uint32_t lap_ms = 0;
    if (stopwatch_lap(&lap_ms) == RT_EOK)
    {
        rt_uint32_t cs = (lap_ms / 10U) % 100U;      /* 厘秒 00-99 */
        rt_uint32_t ss = (lap_ms / 1000U) % 60U;    /* 秒 00-59 */
        rt_uint32_t mm = (lap_ms / 60000U) % 100U;  /* 分 00-99 */
        rt_kprintf("sw: lap=%02u:%02u.%02u\n", (unsigned)mm, (unsigned)ss, (unsigned)cs);
        notifier_beep_once(40);
    }
    else
    {
        rt_kprintf("sw: lap failed\n");
    }
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_sw_lap, sw_lap, Add_lap_record);

static int cmd_sw_status(int argc, char **argv)
{
    (void)argc; (void)argv;
    stopwatch_state_t s = stopwatch_get_state();
    rt_uint32_t total = stopwatch_get_total_ms();
    rt_uint16_t cnt = stopwatch_get_lap_count();
    rt_uint32_t min_ms = 0xFFFFFFFFu, max_ms = 0, sum_ms = 0;
    for (rt_uint16_t i = 0; i < cnt; i++)
    {
        rt_uint32_t v = stopwatch_get_lap_ms(i);
        if (v < min_ms) min_ms = v;
        if (v > max_ms) max_ms = v;
        sum_ms += v;
    }
    char totalbuf[24];
    format_time(total, totalbuf, sizeof(totalbuf));
    rt_kprintf("state: %s\n", s==STOPWATCH_STATE_RUNNING?"RUNNING":(s==STOPWATCH_STATE_PAUSED?"PAUSED":"IDLE"));
    rt_kprintf("total: %u ms (%s)\n", (unsigned)total, totalbuf);
    rt_kprintf("laps:  %u\n", (unsigned)cnt);
    if (cnt > 0)
    {
        char minbuf[24], maxbuf[24], avgbuf[24];
        format_time(min_ms, minbuf, sizeof(minbuf));
        format_time(max_ms, maxbuf, sizeof(maxbuf));
        format_time(sum_ms / cnt, avgbuf, sizeof(avgbuf));
        rt_kprintf("min:   %u ms (%s)\n", (unsigned)min_ms, minbuf);
        rt_kprintf("max:   %u ms (%s)\n", (unsigned)max_ms, maxbuf);
        rt_kprintf("avg:   %u ms (%s)\n", (unsigned)(sum_ms/cnt), avgbuf);
    }
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_sw_status, sw_status, Stopwatch_status_summary);

/* 清空圈速 */
static int cmd_sw_clear_laps(int argc, char **argv)
{
    (void)argc; (void)argv;
    stopwatch_clear_laps();
    rt_kprintf("sw: clear laps\n");
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_sw_clear_laps, sw_clear_laps, Clear_lap_records);

/* ================== CSV 输出 ================== */
static rt_timer_t csv_timer = RT_NULL;
static rt_uint32_t csv_period_ms = 200;
static rt_bool_t csv_header = 0;
static rt_bool_t csv_human = 0; /* 0: ms, 1: human mm:ss.mmm */

static void csv_timer_cb(void *parameter)
{
    (void)parameter;
    rt_uint32_t t = stopwatch_get_total_ms();
    rt_uint16_t n = stopwatch_get_lap_count();
    rt_uint32_t lap = (n > 0) ? stopwatch_get_latest_lap_ms() : 0;
    if (csv_header)
    {
        rt_kprintf("t,lap_idx,lap_ms,total\n");
        csv_header = 0; /* 只打一遍 */
    }
    if (!csv_human)
    {
        rt_kprintf("%u,%u,%u,%u\n", (unsigned)t, (unsigned)n, (unsigned)lap, (unsigned)t);
    }
    else
    {
        char tb[24], lb[24];
        /* 复用已有 format_time */
        extern void format_time(rt_uint32_t total_ms, char *buf, rt_size_t buf_len);
        format_time(t, tb, sizeof(tb));
        format_time(lap, lb, sizeof(lb));
        rt_kprintf("%s,%u,%s,%s\n", tb, (unsigned)n, lb, tb);
    }
}

static rt_tick_t ms_to_ticks(rt_uint32_t ms)
{
    /* 计算向上取整，避免 0 tick */
    rt_uint64_t ticks = ((rt_uint64_t)ms * RT_TICK_PER_SECOND + 999ULL) / 1000ULL;
    if (ticks == 0) ticks = 1;
    return (rt_tick_t)ticks;
}

static int cmd_sw_csv(int argc, char **argv)
{
    if (argc < 2)
    {
        rt_kprintf("usage: sw.csv on [period_ms]|off\n");
        return -RT_ERROR;
    }
    if (!strcmp(argv[1], "on"))
    {
        if (argc >= 3)
        {
            csv_period_ms = (rt_uint32_t)atoi(argv[2]);
            if (csv_period_ms < 10) csv_period_ms = 10;
        }
        if (csv_timer == RT_NULL)
        {
            csv_timer = rt_timer_create("swcsv", csv_timer_cb, RT_NULL, ms_to_ticks(csv_period_ms), RT_TIMER_FLAG_PERIODIC | RT_TIMER_FLAG_SOFT_TIMER);
            if (csv_timer == RT_NULL)
            {
                rt_kprintf("sw.csv: create timer failed\n");
                return -RT_ENOMEM;
            }
        }
        else
        {
            rt_tick_t t = ms_to_ticks(csv_period_ms);
            rt_timer_control(csv_timer, RT_TIMER_CTRL_SET_TIME, &t);
        }
        rt_timer_start(csv_timer);
        rt_kprintf("sw.csv: on, %u ms\n", (unsigned)csv_period_ms);
        return 0;
    }
    else if (!strcmp(argv[1], "off"))
    {
        if (csv_timer)
        {
            rt_timer_stop(csv_timer);
        }
        rt_kprintf("sw.csv: off\n");
        return 0;
    }
    else
    {
        rt_kprintf("usage: sw.csv on [period_ms]|off\n");
        return -RT_ERROR;
    }
}
MSH_CMD_EXPORT_ALIAS(cmd_sw_csv, sw_csv, Periodic_CSV_stream);

static int cmd_sw_csv_header(int argc, char **argv)
{
    if (argc < 2) { rt_kprintf("usage: sw_csv_header on|off\n"); return -RT_ERROR; }
    if (!strcmp(argv[1], "on")) { csv_header = 1; rt_kprintf("sw_csv_header: on\n"); return 0; }
    if (!strcmp(argv[1], "off")) { csv_header = 0; rt_kprintf("sw_csv_header: off\n"); return 0; }
    rt_kprintf("usage: sw_csv_header on|off\n"); return -RT_ERROR;
}
MSH_CMD_EXPORT_ALIAS(cmd_sw_csv_header, sw_csv_header, CSV_header_once_switch);

static int cmd_sw_timefmt(int argc, char **argv)
{
    if (argc < 2) { rt_kprintf("usage: sw_timefmt human|ms\n"); return -RT_ERROR; }
    if (!strcmp(argv[1], "human")) { csv_human = 1; rt_kprintf("sw_timefmt: human\n"); return 0; }
    if (!strcmp(argv[1], "ms")) { csv_human = 0; rt_kprintf("sw_timefmt: ms\n"); return 0; }
    rt_kprintf("usage: sw_timefmt human|ms\n"); return -RT_ERROR;
}
MSH_CMD_EXPORT_ALIAS(cmd_sw_timefmt, sw_timefmt, CSV_time_format_switch);

/* 提示音开关命令 */
static int cmd_sw_beep(int argc, char **argv)
{
    if (argc < 2)
    {
        rt_kprintf("usage: sw_beep on|off\n");
        return -RT_ERROR;
    }
    if (!strcmp(argv[1], "on"))
    {
        notifier_beep_enable(1);
        rt_kprintf("sw_beep: on\n");
        return 0;
    }
    else if (!strcmp(argv[1], "off"))
    {
        notifier_beep_enable(0);
        rt_kprintf("sw_beep: off\n");
        return 0;
    }
    else
    {
        rt_kprintf("usage: sw_beep on|off\n");
        return -RT_ERROR;
    }
}
MSH_CMD_EXPORT_ALIAS(cmd_sw_beep, sw_beep, Enable_or_disable_beep);

/* 光敏联动开关 */
static int cmd_sw_light(int argc, char **argv)
{
    if (argc < 2)
    {
        rt_kprintf("usage: sw_light on|off\n");
        return -RT_ERROR;
    }
    if (!strcmp(argv[1], "on"))
    {
        sensor_light_enable(1);
        rt_kprintf("sw_light: on\n");
        return 0;
    }
    else if (!strcmp(argv[1], "off"))
    {
        sensor_light_enable(0);
        rt_kprintf("sw_light: off\n");
        return 0;
    }
    else
    {
        rt_kprintf("usage: sw_light on|off\n");
        return -RT_ERROR;
    }
}
MSH_CMD_EXPORT_ALIAS(cmd_sw_light, sw_light, Enable_or_disable_light_linkage);

/* 光敏极性反转 */
static int cmd_sw_light_invert(int argc, char **argv)
{
    if (argc < 2)
    {
        rt_kprintf("usage: sw_light_invert on|off\n");
        return -RT_ERROR;
    }
    if (!strcmp(argv[1], "on"))
    {
        sensor_light_set_invert(1);
        rt_kprintf("sw_light_invert: on\n");
        return 0;
    }
    else if (!strcmp(argv[1], "off"))
    {
        sensor_light_set_invert(0);
        rt_kprintf("sw_light_invert: off\n");
        return 0;
    }
    else
    {
        rt_kprintf("usage: sw_light_invert on|off\n");
        return -RT_ERROR;
    }
}
MSH_CMD_EXPORT_ALIAS(cmd_sw_light_invert, sw_light_invert, Invert_light_polarity);

/* OLED 刷新周期设置（ms） */
static int cmd_sw_oled_rate(int argc, char **argv)
{
    if (argc < 2)
    {
        rt_kprintf("usage: sw_oled_rate <ms>\n");
        return -RT_ERROR;
    }
    int ms = atoi(argv[1]);
    if (ms < 10) ms = 10;
    ui_oled_set_refresh_ms((rt_uint16_t)ms);
    rt_kprintf("sw_oled_rate: %d ms\n", ms);
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_sw_oled_rate, sw_oled_rate, Set_OLED_refresh_period_ms);

/* OLED 页面切换 */
static int cmd_sw_page(int argc, char **argv)
{
    if (argc < 2)
    {
        rt_kprintf("usage: sw_page main|laps\n");
        return -RT_ERROR;
    }
    if (!strcmp(argv[1], "main"))
    {
        ui_oled_set_page(0);
        rt_kprintf("sw_page: main\n");
        return 0;
    }
    else if (!strcmp(argv[1], "laps"))
    {
        ui_oled_set_page(1);
        rt_kprintf("sw_page: laps\n");
        return 0;
    }
    else
    {
        rt_kprintf("usage: sw_page main|laps\n");
        return -RT_ERROR;
    }
}
MSH_CMD_EXPORT_ALIAS(cmd_sw_page, sw_page, Switch_OLED_page);

/* 圈速页翻页 */
static int cmd_sw_laps_prev(int argc, char **argv)
{
    (void)argc; (void)argv;
    ui_oled_laps_prev();
    rt_kprintf("sw_laps_prev\n");
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_sw_laps_prev, sw_laps_prev, Laps_page_previous);

static int cmd_sw_laps_next(int argc, char **argv)
{
    (void)argc; (void)argv;
    ui_oled_laps_next();
    rt_kprintf("sw_laps_next\n");
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_sw_laps_next, sw_laps_next, Laps_page_next);


