#ifndef APPLICATIONS_STOPWATCH_H_
#define APPLICATIONS_STOPWATCH_H_

#include <rtthread.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef STOPWATCH_MAX_LAPS
#define STOPWATCH_MAX_LAPS  20
#endif

typedef enum
{
    STOPWATCH_STATE_IDLE = 0,
    STOPWATCH_STATE_RUNNING = 1,
    STOPWATCH_STATE_PAUSED = 2,
} stopwatch_state_t;

rt_err_t stopwatch_init(void);

void stopwatch_start(void);
void stopwatch_stop(void);
void stopwatch_reset(void);

/* 记录一圈；如 out_lap_ms 非空返回本圈用时 */
rt_err_t stopwatch_lap(rt_uint32_t *out_lap_ms);
void     stopwatch_clear_laps(void);

/* 查询接口（线程安全，快照） */
stopwatch_state_t stopwatch_get_state(void);
rt_uint32_t       stopwatch_get_total_ms(void);
rt_uint16_t       stopwatch_get_lap_count(void);
rt_uint32_t       stopwatch_get_lap_ms(rt_uint16_t index);
rt_uint32_t       stopwatch_get_latest_lap_ms(void);

#ifdef __cplusplus
}
#endif

#endif /* APPLICATIONS_STOPWATCH_H_ */


