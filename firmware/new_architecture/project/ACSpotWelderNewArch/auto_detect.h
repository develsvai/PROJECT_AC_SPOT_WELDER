#ifndef AUTO_DETECT_H
#define AUTO_DETECT_H

#include "types.h"

void auto_detect_init(AutoDetectContext *auto_detect);
void auto_detect_reset(AutoDetectContext *auto_detect);
void auto_detect_handle_zero_cross(AutoDetectContext *auto_detect);
void auto_detect_handle_sample_tick(AutoDetectContext *auto_detect);
bool auto_detect_consume_trigger(AutoDetectContext *auto_detect);

#endif
