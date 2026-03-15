#ifndef HARDWARE_H
#define HARDWARE_H

#include <stdbool.h>
#include <stdint.h>

void hardware_init(void);
bool hardware_consume_tick_1ms(void);
bool hardware_consume_zero_cross(void);
bool hardware_consume_sample_tick(void);
uint32_t hardware_millis(void);

void hardware_triac_set(bool enabled);
void hardware_buzzer_set(bool enabled);
void hardware_sense_set(bool enabled);
uint8_t hardware_adc_read_8bit(void);

uint8_t hardware_read_encoder_port(void);

#endif
