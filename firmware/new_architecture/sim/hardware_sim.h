#ifndef HARDWARE_SIM_H
#define HARDWARE_SIM_H

#include <stdbool.h>
#include <stdint.h>

void sim_hardware_step_us(uint32_t us);
void sim_hardware_set_manual_pressed(bool pressed);
void sim_hardware_set_encoder_pressed(bool pressed);
void sim_hardware_set_contact(bool active);
void sim_hardware_reset_inputs(void);

bool sim_hardware_triac_is_on(void);
bool sim_hardware_buzzer_is_on(void);
bool sim_hardware_sense_is_on(void);
bool sim_hardware_contact_is_on(void);
uint32_t sim_hardware_pulse_count(void);

#endif
