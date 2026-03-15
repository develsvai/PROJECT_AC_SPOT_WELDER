#ifndef TYPES_H
#define TYPES_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    MODE_MANUAL = 0,
    MODE_AUTO = 1
} WeldMode;

typedef enum {
    UI_SPLASH = 0,
    UI_IDLE,
    UI_MENU_MAIN,
    UI_MENU_EDIT
} UiState;

typedef enum {
    MENU_TIME = 0,
    MENU_MULT,
    MENU_REST,
    MENU_MODE,
    MENU_COUNT
} MenuItem;

typedef enum {
    PROC_READY = 0,
    PROC_AUTO_MONITOR,
    PROC_WAIT_ZC,
    PROC_PULSE_ON,
    PROC_REST,
    PROC_DONE,
    PROC_LOCKOUT
} ProcessState;

typedef enum {
    AUTO_IDLE = 0,
    AUTO_CANDIDATE,
    AUTO_LOCKOUT
} AutoState;

typedef struct {
    uint16_t time_ms;
    uint8_t multiplier;
    uint16_t rest_ms;
    WeldMode mode;
} Settings;

typedef struct {
    int8_t encoder_delta;
    bool enc_pressed;
    bool manual_pressed;
} InputEvent;

typedef struct {
    UiState state;
    MenuItem selected_item;
    int16_t edit_value;
    uint16_t timeout_ms;
    uint16_t splash_ms;
    bool dirty;
} UiContext;

typedef struct {
    ProcessState state;
    uint8_t pulses_remaining;
    uint16_t pulse_timer_ms;
    uint16_t rest_timer_ms;
    uint16_t lockout_timer_ms;
    uint16_t beep_timer_ms;
    bool manual_request;
    bool auto_trigger_request;
    bool busy;
    bool dirty;
} ProcessContext;

typedef struct {
    AutoState state;
    uint16_t adc_center;
    uint16_t baseline_feature;
    uint32_t abs_sum;
    uint16_t sample_count;
    uint16_t half_feature;
    uint8_t detect_count;
    uint8_t release_count;
    bool half_active;
    bool feature_ready;
    bool trigger_request;
} AutoDetectContext;

typedef struct {
    Settings settings;
    InputEvent input;
    UiContext ui;
    ProcessContext process;
    AutoDetectContext auto_detect;
} AppContext;

#endif
