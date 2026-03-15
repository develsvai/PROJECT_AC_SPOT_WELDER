#ifndef PROCESS_H
#define PROCESS_H

#include "types.h"

void process_init(ProcessContext *process);
void process_request_manual(ProcessContext *process);
void process_request_auto(ProcessContext *process);
void process_tick_1ms(ProcessContext *process, const Settings *settings);
void process_handle_zero_cross(ProcessContext *process, const Settings *settings);

#endif
