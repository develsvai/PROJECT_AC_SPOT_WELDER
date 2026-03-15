#include "hardware_sim.h"

#include <stdbool.h>
#include <stdint.h>

#include "../project/ACSpotWelderNewArch/hardware.h"

enum {
    ENC_A_BIT = 4,
    ENC_B_BIT = 5,
    ENC_SW_BIT = 6,
    MANUAL_SW_BIT = 7
};

static uint32_t g_ms;
static uint32_t g_us_accum_1ms;
static uint32_t g_us_accum_sample;
static uint32_t g_us_half_cycle;
static bool g_tick_1ms;
static bool g_zero_cross;
static bool g_sample_tick;

static bool g_triac_on;
static bool g_buzzer_on;
static bool g_sense_on;
static uint32_t g_pulse_count;

static uint8_t g_encoder_port;
static bool g_contact_active;
static bool g_half_positive;

static uint8_t sim_triangle_sample(void)
{
    uint32_t phase = g_us_half_cycle;
    uint32_t folded = phase <= 4166U ? phase : (8333U - phase);
    uint8_t amplitude = g_contact_active ? 42U : 2U;
    uint8_t delta = (uint8_t)((folded * amplitude) / 4166U);

    if (g_half_positive) {
        return (uint8_t)(128U + delta);
    }
    return (uint8_t)(128U - delta);
}

void hardware_init(void)
{
    g_ms = 0U;
    g_us_accum_1ms = 0U;
    g_us_accum_sample = 0U;
    g_us_half_cycle = 0U;
    g_tick_1ms = false;
    g_zero_cross = false;
    g_sample_tick = false;
    g_triac_on = false;
    g_buzzer_on = false;
    g_sense_on = false;
    g_pulse_count = 0U;
    g_contact_active = false;
    g_half_positive = true;
    g_encoder_port = (uint8_t)((1U << ENC_A_BIT) | (1U << ENC_B_BIT) | (1U << ENC_SW_BIT) |
                               (1U << MANUAL_SW_BIT));
}

bool hardware_consume_tick_1ms(void)
{
    bool state = g_tick_1ms;
    g_tick_1ms = false;
    return state;
}

bool hardware_consume_zero_cross(void)
{
    bool state = g_zero_cross;
    g_zero_cross = false;
    return state;
}

bool hardware_consume_sample_tick(void)
{
    bool state = g_sample_tick;
    g_sample_tick = false;
    return state;
}

uint32_t hardware_millis(void)
{
    return g_ms;
}

void hardware_triac_set(bool enabled)
{
    if (!g_triac_on && enabled) {
        g_pulse_count++;
    }
    g_triac_on = enabled;
}

void hardware_buzzer_set(bool enabled)
{
    g_buzzer_on = enabled;
}

void hardware_sense_set(bool enabled)
{
    g_sense_on = enabled;
}

uint8_t hardware_adc_read_8bit(void)
{
    return sim_triangle_sample();
}

uint8_t hardware_read_encoder_port(void)
{
    return g_encoder_port;
}

void sim_hardware_step_us(uint32_t us)
{
    uint32_t remaining = us;

    while (remaining > 0U) {
        g_us_accum_1ms += 100U;
        g_us_accum_sample += 100U;
        g_us_half_cycle += 100U;
        remaining = remaining >= 100U ? (remaining - 100U) : 0U;

        if (g_us_accum_sample >= 100U) {
            g_us_accum_sample -= 100U;
            g_sample_tick = true;
        }

        if (g_us_accum_1ms >= 1000U) {
            g_us_accum_1ms -= 1000U;
            g_ms++;
            g_tick_1ms = true;
        }

        if (g_us_half_cycle >= 8333U) {
            g_us_half_cycle -= 8333U;
            g_half_positive = !g_half_positive;
            g_zero_cross = true;
        }
    }
}

void sim_hardware_set_manual_pressed(bool pressed)
{
    if (pressed) {
        g_encoder_port &= (uint8_t)~(1U << MANUAL_SW_BIT);
    } else {
        g_encoder_port |= (uint8_t)(1U << MANUAL_SW_BIT);
    }
}

void sim_hardware_set_encoder_pressed(bool pressed)
{
    if (pressed) {
        g_encoder_port &= (uint8_t)~(1U << ENC_SW_BIT);
    } else {
        g_encoder_port |= (uint8_t)(1U << ENC_SW_BIT);
    }
}

void sim_hardware_set_contact(bool active)
{
    g_contact_active = active;
}

void sim_hardware_reset_inputs(void)
{
    sim_hardware_set_manual_pressed(false);
    sim_hardware_set_encoder_pressed(false);
}

bool sim_hardware_triac_is_on(void)
{
    return g_triac_on;
}

bool sim_hardware_buzzer_is_on(void)
{
    return g_buzzer_on;
}

bool sim_hardware_sense_is_on(void)
{
    return g_sense_on;
}

bool sim_hardware_contact_is_on(void)
{
    return g_contact_active;
}

uint32_t sim_hardware_pulse_count(void)
{
    return g_pulse_count;
}
