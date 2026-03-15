#include "input.h"

#include "hardware.h"

#define ENC_A_BIT 4
#define ENC_B_BIT 5
#define ENC_SW_BIT 6
#define MANUAL_SW_BIT 7

void input_init(InputEvent *input)
{
    input->encoder_delta = 0;
    input->enc_pressed = false;
    input->manual_pressed = false;
}

void input_update(InputEvent *input)
{
    static uint8_t last_state = 0;
    static uint8_t last_enc_sw = 1;
    static uint8_t last_manual_sw = 1;

    uint8_t pins = hardware_read_encoder_port();
    uint8_t a = (pins & (1U << ENC_A_BIT)) ? 1U : 0U;
    uint8_t b = (pins & (1U << ENC_B_BIT)) ? 1U : 0U;
    uint8_t state = (uint8_t)((a << 1) | b);
    uint8_t enc_sw = (pins & (1U << ENC_SW_BIT)) ? 1U : 0U;
    uint8_t manual_sw = (pins & (1U << MANUAL_SW_BIT)) ? 1U : 0U;

    input->encoder_delta = 0;
    input->enc_pressed = false;
    input->manual_pressed = false;

    if (state != last_state) {
        if ((last_state == 2U && state == 0U) || (last_state == 0U && state == 1U)) {
            input->encoder_delta = 1;
        } else if ((last_state == 1U && state == 0U) || (last_state == 0U && state == 2U)) {
            input->encoder_delta = -1;
        }
        last_state = state;
    }

    if (last_enc_sw && !enc_sw) {
        input->enc_pressed = true;
    }
    if (last_manual_sw && !manual_sw) {
        input->manual_pressed = true;
    }

    last_enc_sw = enc_sw;
    last_manual_sw = manual_sw;
}
