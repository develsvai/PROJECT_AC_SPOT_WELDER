#ifndef APP_H
#define APP_H

#include "types.h"

void app_init(void);
void app_run(void);
const AppContext *app_get_context(void);
void app_inject_encoder_delta(int8_t delta);
void app_inject_encoder_press(void);
void app_inject_manual_press(void);
void app_apply_settings(const Settings *settings);

#endif
