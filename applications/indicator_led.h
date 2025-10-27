#ifndef APPLICATIONS_INDICATOR_LED_H_
#define APPLICATIONS_INDICATOR_LED_H_

#include <rtthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 初始化 LED 指示模块（根据秒表状态自动控制） */
rt_err_t indicator_led_init(void);

#ifdef __cplusplus
}
#endif

#endif /* APPLICATIONS_INDICATOR_LED_H_ */










