#ifndef SIM_API_H
#define SIM_API_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t time_ms;
    uint16_t setting_time_ms;
    uint8_t setting_multiplier;
    uint16_t setting_rest_ms;
    uint8_t setting_mode;
    uint8_t ui_state;
    uint8_t menu_item;
    int16_t edit_value;
    uint8_t process_state;
    uint8_t process_pulses_remaining;
    uint8_t auto_state;
    uint16_t auto_baseline_feature;
    uint16_t auto_half_feature;
    bool triac_on;
    bool buzzer_on;
    bool sense_on;
    bool contact_on;
    uint32_t pulse_count;
    uint32_t weld_count;
    char lcd_line0[17];
    char lcd_line1[17];
} SimSnapshot;

void sim_api_init(void);
void sim_api_step_ms(uint32_t duration_ms);
void sim_api_press_manual(void);
void sim_api_manual_cycle(void);
void sim_api_press_encoder(void);
void sim_api_rotate_encoder(int8_t delta);
void sim_api_set_contact(bool active);
void sim_api_toggle_contact(void);
void sim_api_touch_pulse(uint32_t contact_ms, uint32_t settle_ms);
void sim_api_set_settings(uint16_t time_ms, uint8_t multiplier, uint16_t rest_ms, uint8_t mode);
void sim_api_get_snapshot(SimSnapshot *snapshot);

#endif
