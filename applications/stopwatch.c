#include "stopwatch.h"
#include <rtdevice.h>
#include <stdlib.h>
#include <string.h>
#include "timebase.h"

#define DBG_TAG "sw"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

typedef struct
{
    stopwatch_state_t state;
    rt_tick_t         state_start_tick;   /* 最近一次 start 的 tick */
    rt_uint32_t       accumulated_ms;     /* 历史累计毫秒（不含本次运行段） */

    rt_uint64_t       state_start_us;     /* 最近一次 start 的 us 基准（高精度） */

    rt_uint32_t       last_lap_total_ms;  /* 上一次 lap 时的累计毫秒 */
    rt_uint32_t       lap_durations_ms[STOPWATCH_MAX_LAPS];
    rt_uint16_t       lap_count;

    rt_mutex_t        lock;
} stopwatch_ctx_t;

static stopwatch_ctx_t g_sw;
static rt_uint8_t g_inited = 0;

static rt_uint32_t get_now_total_ms_unsafe(void)
{
    if (g_sw.state == STOPWATCH_STATE_RUNNING && g_sw.state_start_us != 0)
    {
        uint64_t now_us = timebase_get_us();
        uint64_t delta_us = now_us - g_sw.state_start_us;
        return g_sw.accumulated_ms + (rt_uint32_t)(delta_us / 1000ULL);
    }
    return g_sw.accumulated_ms;
}

rt_err_t stopwatch_init(void)
{
    if (g_inited)
    {
        return RT_EOK;
    }
    memset(&g_sw, 0, sizeof(g_sw));
    g_sw.state = STOPWATCH_STATE_IDLE;
    g_sw.lock = rt_mutex_create("swlock", RT_IPC_FLAG_PRIO);
    if (!g_sw.lock)
    {
        return -RT_ENOMEM;
    }

    timebase_init();
    g_inited = 1;
    /* 初始化日志可去除以节省ROM */
    return RT_EOK;
}

void stopwatch_start(void)
{
    if (!g_inited) { if (stopwatch_init() != RT_EOK) return; }
    rt_mutex_take(g_sw.lock, RT_WAITING_FOREVER);
    if (g_sw.state == STOPWATCH_STATE_RUNNING)
    {
        rt_mutex_release(g_sw.lock);
        return;
    }
    g_sw.state_start_tick = rt_tick_get();
    g_sw.state_start_us = timebase_get_us();
    g_sw.state = STOPWATCH_STATE_RUNNING;
    rt_mutex_release(g_sw.lock);
}

void stopwatch_stop(void)
{
    if (!g_inited) { if (stopwatch_init() != RT_EOK) return; }
    rt_mutex_take(g_sw.lock, RT_WAITING_FOREVER);
    if (g_sw.state == STOPWATCH_STATE_RUNNING)
    {
        if (g_sw.state_start_us != 0)
        {
            uint64_t now_us = timebase_get_us();
            g_sw.accumulated_ms += (rt_uint32_t)((now_us - g_sw.state_start_us) / 1000ULL);
            g_sw.state_start_us = 0;
        }
        g_sw.state = STOPWATCH_STATE_PAUSED;
    }
    rt_mutex_release(g_sw.lock);
}

void stopwatch_reset(void)
{
    if (!g_inited) { if (stopwatch_init() != RT_EOK) return; }
    rt_mutex_take(g_sw.lock, RT_WAITING_FOREVER);
    g_sw.accumulated_ms = 0;
    g_sw.last_lap_total_ms = 0;
    g_sw.lap_count = 0;
    memset(g_sw.lap_durations_ms, 0, sizeof(g_sw.lap_durations_ms));
    if (g_sw.state == STOPWATCH_STATE_RUNNING)
    {
        g_sw.state_start_tick = rt_tick_get();
        g_sw.state_start_us = timebase_get_us();
    }
    else
    {
        g_sw.state = STOPWATCH_STATE_IDLE;
        g_sw.state_start_us = 0;
    }
    rt_mutex_release(g_sw.lock);
}

rt_err_t stopwatch_lap(rt_uint32_t *out_lap_ms)
{
    if (!g_inited) { rt_err_t r = stopwatch_init(); if (r != RT_EOK) return r; }
    rt_mutex_take(g_sw.lock, RT_WAITING_FOREVER);
    rt_uint32_t total_ms = get_now_total_ms_unsafe();
    rt_uint32_t lap_ms = total_ms - g_sw.last_lap_total_ms;

    if (g_sw.lap_count < STOPWATCH_MAX_LAPS)
    {
        g_sw.lap_durations_ms[g_sw.lap_count] = lap_ms;
        g_sw.lap_count++;
    }
    else
    {
        /* 达到上限，丢弃最早一圈，右移 */
        memmove(&g_sw.lap_durations_ms[0], &g_sw.lap_durations_ms[1], sizeof(rt_uint32_t) * (STOPWATCH_MAX_LAPS - 1));
        g_sw.lap_durations_ms[STOPWATCH_MAX_LAPS - 1] = lap_ms;
    }
    g_sw.last_lap_total_ms = total_ms;

    if (out_lap_ms) { *out_lap_ms = lap_ms; }
    rt_mutex_release(g_sw.lock);
    return RT_EOK;
}

stopwatch_state_t stopwatch_get_state(void)
{
    if (!g_inited) { return STOPWATCH_STATE_IDLE; }
    rt_mutex_take(g_sw.lock, RT_WAITING_FOREVER);
    stopwatch_state_t s = g_sw.state;
    rt_mutex_release(g_sw.lock);
    return s;
}

rt_uint32_t stopwatch_get_total_ms(void)
{
    if (!g_inited) { return 0; }
    rt_mutex_take(g_sw.lock, RT_WAITING_FOREVER);
    rt_uint32_t v = get_now_total_ms_unsafe();
    rt_mutex_release(g_sw.lock);
    return v;
}

rt_uint16_t stopwatch_get_lap_count(void)
{
    if (!g_inited) { return 0; }
    rt_mutex_take(g_sw.lock, RT_WAITING_FOREVER);
    rt_uint16_t c = g_sw.lap_count;
    rt_mutex_release(g_sw.lock);
    return c;
}

rt_uint32_t stopwatch_get_lap_ms(rt_uint16_t index)
{
    if (!g_inited) { return 0; }
    rt_mutex_take(g_sw.lock, RT_WAITING_FOREVER);
    rt_uint32_t v = 0;
    if (index < g_sw.lap_count)
    {
        v = g_sw.lap_durations_ms[index];
    }
    rt_mutex_release(g_sw.lock);
    return v;
}

rt_uint32_t stopwatch_get_latest_lap_ms(void)
{
    if (!g_inited) { return 0; }
    rt_mutex_take(g_sw.lock, RT_WAITING_FOREVER);
    rt_uint32_t v = 0;
    if (g_sw.lap_count > 0)
    {
        v = g_sw.lap_durations_ms[g_sw.lap_count - 1];
    }
    rt_mutex_release(g_sw.lock);
    return v;
}

void stopwatch_clear_laps(void)
{
    if (!g_inited) return;
    rt_mutex_take(g_sw.lock, RT_WAITING_FOREVER);
    g_sw.lap_count = 0;
    g_sw.last_lap_total_ms = get_now_total_ms_unsafe();
    memset(g_sw.lap_durations_ms, 0, sizeof(g_sw.lap_durations_ms));
    rt_mutex_release(g_sw.lock);
}


