#ifndef APPLICATIONS_SENSOR_LIGHT_H_
#define APPLICATIONS_SENSOR_LIGHT_H_

#include <rtthread.h>

#ifdef __cplusplus
extern "C" {
#endif

rt_err_t sensor_light_init(void);
void sensor_light_enable(rt_bool_t enable);
rt_bool_t sensor_light_is_enabled(void);
void sensor_light_set_invert(rt_bool_t invert);
rt_bool_t sensor_light_get_invert(void);

#ifdef __cplusplus
}
#endif

#endif /* APPLICATIONS_SENSOR_LIGHT_H_ */


