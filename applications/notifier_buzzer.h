#ifndef APPLICATIONS_NOTIFIER_BUZZER_H_
#define APPLICATIONS_NOTIFIER_BUZZER_H_

#include <rtthread.h>

#ifdef __cplusplus
extern "C" {
#endif

rt_err_t notifier_buzzer_init(void);
void notifier_beep_once(rt_uint16_t ms);
void notifier_beep_enable(rt_bool_t enable);
rt_bool_t notifier_beep_is_enabled(void);

#ifdef __cplusplus
}
#endif

#endif /* APPLICATIONS_NOTIFIER_BUZZER_H_ */










