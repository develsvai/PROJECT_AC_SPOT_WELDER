#include "process.h"

#include "app_config.h"
#include "hardware.h"

void process_init(ProcessContext *process)
{
    process->state = PROC_READY;
    process->pulses_remaining = 0;
    process->pulse_timer_ms = 0;
    process->rest_timer_ms = 0;
    process->lockout_timer_ms = 0;
    process->beep_timer_ms = 0;
    process->manual_request = false;
    process->auto_trigger_request = false;
    process->busy = false;
    process->dirty = true;
}

void process_request_manual(ProcessContext *process)
{
    process->manual_request = true;
}

void process_request_auto(ProcessContext *process)
{
    process->auto_trigger_request = true;
}

void process_handle_zero_cross(ProcessContext *process, const Settings *settings)
{
    if (process->state == PROC_WAIT_ZC) {
        hardware_triac_set(true);
        process->pulse_timer_ms = settings->time_ms;
        process->state = PROC_PULSE_ON;
        process->dirty = true;
    }
}

void process_tick_1ms(ProcessContext *process, const Settings *settings)
{
    if (process->pulse_timer_ms > 0U) {
        process->pulse_timer_ms--;
    }
    if (process->rest_timer_ms > 0U) {
        process->rest_timer_ms--;
    }
    if (process->lockout_timer_ms > 0U) {
        process->lockout_timer_ms--;
    }
    if (process->beep_timer_ms > 0U) {
        process->beep_timer_ms--;
        if (process->beep_timer_ms == 0U) {
            hardware_buzzer_set(false);
        }
    }

    switch (process->state) {
        case PROC_READY:
            process->busy = false;

            if (settings->mode == MODE_AUTO) {
                process->state = PROC_AUTO_MONITOR;
                process->dirty = true;
                break;
            }

            if (process->manual_request) {
                process->manual_request = false;
                process->busy = true;
                process->pulses_remaining = settings->multiplier;
                process->state = PROC_WAIT_ZC;
                process->dirty = true;
            }
            break;

        case PROC_AUTO_MONITOR:
            process->busy = false;

            if (settings->mode == MODE_MANUAL) {
                process->state = PROC_READY;
                process->dirty = true;
                break;
            }

            if (process->auto_trigger_request) {
                process->auto_trigger_request = false;
                process->busy = true;
                process->pulses_remaining = settings->multiplier;
                process->state = PROC_WAIT_ZC;
                process->dirty = true;
            }
            break;

        case PROC_WAIT_ZC:
            process->busy = true;
            break;

        case PROC_PULSE_ON:
            process->busy = true;
            if (process->pulse_timer_ms == 0U) {
                hardware_triac_set(false);

                if (process->pulses_remaining > 0U) {
                    process->pulses_remaining--;
                }

                if (process->pulses_remaining > 0U) {
                    process->rest_timer_ms = settings->rest_ms;
                    process->state = PROC_REST;
                } else {
                    process->state = PROC_DONE;
                }
                process->dirty = true;
            }
            break;

        case PROC_REST:
            process->busy = true;
            if (process->rest_timer_ms == 0U) {
                process->state = PROC_WAIT_ZC;
                process->dirty = true;
            }
            break;

        case PROC_DONE:
            hardware_buzzer_set(true);
            process->beep_timer_ms = BEEP_TIME_MS;
            process->lockout_timer_ms = LOCKOUT_TIME_MS;
            process->state = PROC_LOCKOUT;
            process->dirty = true;
            break;

        case PROC_LOCKOUT:
            process->busy = false;
            if (process->lockout_timer_ms == 0U) {
                process->state = (settings->mode == MODE_AUTO) ? PROC_AUTO_MONITOR : PROC_READY;
                process->dirty = true;
            }
            break;
    }
}
