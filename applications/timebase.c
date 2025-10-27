#include "timebase.h"
#include <rthw.h>
#include "stm32f1xx.h"
#include "core_cm3.h"

/* 优先使用 DWT CYCCNT，失败则回退到 rt_tick_get() 插值（精度略低）。 */
static uint32_t cpu_hz = 72000000; /* 默认 72MHz */
static uint8_t  dwt_ok = 0;
static uint32_t last_cyc = 0;
static uint64_t total_cyc = 0; /* 64位累计cycles，避免~59s溢出问题 */

static inline void dwt_init(void)
{
    /* 使能 DWT CYCCNT */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    dwt_ok = (DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) ? 1 : 0;
}

rt_err_t timebase_init(void)
{
    /* 从 SystemCoreClock 取频率 */
    extern uint32_t SystemCoreClock;
    if (SystemCoreClock) cpu_hz = SystemCoreClock;
    dwt_init();
    if (dwt_ok)
    {
        last_cyc = DWT->CYCCNT;
        total_cyc = 0;
    }
    return RT_EOK;
}

uint64_t timebase_get_us(void)
{
    if (dwt_ok)
    {
        uint32_t cur = DWT->CYCCNT;
        uint32_t delta = (uint32_t)(cur - last_cyc); /* 包含回绕 */
        last_cyc = cur;
        total_cyc += delta;
        return (total_cyc * 1000000ULL) / cpu_hz;
    }
    /* 回退：tick 转换为 us */
    uint32_t ticks = rt_tick_get();
    return ((uint64_t)ticks * 1000000ULL) / RT_TICK_PER_SECOND;
}


