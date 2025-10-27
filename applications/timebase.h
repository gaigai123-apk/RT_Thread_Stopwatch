#ifndef APPLICATIONS_TIMEBASE_H_
#define APPLICATIONS_TIMEBASE_H_

#include <rtthread.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 初始化高精度计时基准（优先使用 DWT CYCCNT）。 */
rt_err_t timebase_init(void);

/* 获取自初始化以来的单调微秒时间（us）。 */
uint64_t timebase_get_us(void);

#ifdef __cplusplus
}
#endif

#endif /* APPLICATIONS_TIMEBASE_H_ */










