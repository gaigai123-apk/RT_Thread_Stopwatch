#ifndef APPLICATIONS_UI_OLED_H_
#define APPLICATIONS_UI_OLED_H_

#include <rtthread.h>

#ifdef __cplusplus
extern "C" {
#endif

rt_err_t ui_oled_init(void);
void ui_oled_set_refresh_ms(rt_uint16_t ms);
void ui_oled_set_enabled(rt_bool_t enabled);
void ui_oled_set_page(rt_uint8_t page); /* 0: main, 1: laps */
void ui_oled_laps_prev(void);
void ui_oled_laps_next(void);
void ui_oled_laps_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* APPLICATIONS_UI_OLED_H_ */


