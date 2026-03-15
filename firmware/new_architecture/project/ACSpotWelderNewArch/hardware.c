#include "hardware.h"

#include <avr/interrupt.h>
#include <avr/io.h>

#include "app_config.h"

#define TRIAC_DDR DDRC
#define TRIAC_PORT PORTC
#define TRIAC_PIN PC5

#define BUZZER_DDR DDRC
#define BUZZER_PORT PORTC
#define BUZZER_PIN PC2

#define SENSE_DDR DDRC
#define SENSE_PORT PORTC
#define SENSE_PIN PC6

#define ENC_DDR DDRB
#define ENC_PORT PORTB
#define ENC_PIN PINB

#define ENC_A PB4
#define ENC_B PB5
#define ENC_SW PB6
#define MANUAL_SW PB7

#define ZC_DDR DDRD
#define ZC_PORT PORTD
#define ZC_PIN PD2

static volatile uint32_t g_ms = 0;
static volatile bool g_tick_1ms = false;
static volatile bool g_zero_cross = false;
static volatile bool g_sample_tick = false;

ISR(TIMER1_COMPA_vect)
{
    g_ms++;
    g_tick_1ms = true;
}

ISR(TIMER0_COMP_vect)
{
    g_sample_tick = true;
}

ISR(INT0_vect)
{
    g_zero_cross = true;
}

static void adc_init(void)
{
    ADMUX = (1 << REFS0) | (1 << ADLAR) | (ADC_CHANNEL & 0x07);
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
}

void hardware_init(void)
{
    cli();

    TRIAC_DDR |= (1 << TRIAC_PIN);
    BUZZER_DDR |= (1 << BUZZER_PIN);
    SENSE_DDR |= (1 << SENSE_PIN);

    TRIAC_PORT &= ~(1 << TRIAC_PIN);
    BUZZER_PORT &= ~(1 << BUZZER_PIN);
    SENSE_PORT &= ~(1 << SENSE_PIN);

    ENC_DDR &= ~((1 << ENC_A) | (1 << ENC_B) | (1 << ENC_SW) | (1 << MANUAL_SW));
    ENC_PORT |= (1 << ENC_A) | (1 << ENC_B) | (1 << ENC_SW) | (1 << MANUAL_SW);

    ZC_DDR &= ~(1 << ZC_PIN);
    ZC_PORT |= (1 << ZC_PIN);

    TCCR1A = 0;
    TCCR1B = (1 << WGM12) | (1 << CS11) | (1 << CS10);
    OCR1A = 249;
    TIMSK |= (1 << OCIE1A);

    TCCR0 = (1 << WGM01) | (1 << CS01) | (1 << CS00);
    OCR0 = 24;
    TIMSK |= (1 << OCIE0);

    MCUCR |= (1 << ISC01) | (1 << ISC00);
    GICR |= (1 << INT0);

    adc_init();

    sei();
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
    if (enabled) {
        TRIAC_PORT |= (1 << TRIAC_PIN);
    } else {
        TRIAC_PORT &= ~(1 << TRIAC_PIN);
    }
}

void hardware_buzzer_set(bool enabled)
{
    if (enabled) {
        BUZZER_PORT |= (1 << BUZZER_PIN);
    } else {
        BUZZER_PORT &= ~(1 << BUZZER_PIN);
    }
}

void hardware_sense_set(bool enabled)
{
    if (enabled) {
        SENSE_PORT |= (1 << SENSE_PIN);
    } else {
        SENSE_PORT &= ~(1 << SENSE_PIN);
    }
}

uint8_t hardware_adc_read_8bit(void)
{
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC)) {
    }
    return ADCH;
}

uint8_t hardware_read_encoder_port(void)
{
    return ENC_PIN;
}
