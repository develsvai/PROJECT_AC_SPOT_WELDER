- ATmega16
- 16MHz
- zero-cross interrupt
- 1ms system tick
- FSM 기반
- delay 없이 동작
- LCD 16x2
- encoder + 버튼
- auto detect 가능

주의:

이 코드는 **완성 제품 코드라기보다 “구조적으로 올바른 레퍼런스 구현”**입니다.

---

# 전체 코드

```c
/*
 * ATmega16 Spot Welder Firmware
 * FSM based architecture
 *
 * UI FSM + Process FSM + Auto Detection
 */

#define F_CPU 16000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// --------------------------------------------------
// LCD driver external
// --------------------------------------------------

void LCD_initialize(void);
void LCD_string(uint8_t pos, const char *str);

// --------------------------------------------------
// PIN CONFIG
// --------------------------------------------------

#define TRIAC_DDR   DDRC
#define TRIAC_PORT  PORTC
#define TRIAC_PIN   PC5

#define BUZZER_DDR  DDRC
#define BUZZER_PORT PORTC
#define BUZZER_PIN  PC2

#define SENSE_DDR   DDRC
#define SENSE_PORT  PORTC
#define SENSE_PIN   PC6

#define ENC_DDR     DDRB
#define ENC_PORT    PORTB
#define ENC_PIN     PINB

#define ENC_A       PB4
#define ENC_B       PB5
#define ENC_SW      PB6
#define MANUAL_SW   PB7

#define ZC_DDR      DDRD
#define ZC_PORT     PORTD
#define ZC_PIN      PD2

// --------------------------------------------------
// TIMING
// --------------------------------------------------

#define SPLASH_TIME_MS 1500
#define UI_TIMEOUT_MS  10000
#define LOCKOUT_TIME   250
#define BEEP_TIME      30

// --------------------------------------------------
// EEPROM
// --------------------------------------------------

#define EEPROM_TIME   ((uint16_t*)0)
#define EEPROM_MULT   ((uint8_t*)2)
#define EEPROM_REST   ((uint16_t*)3)
#define EEPROM_MODE   ((uint8_t*)5)

// --------------------------------------------------
// SETTINGS
// --------------------------------------------------

typedef enum {
    MODE_MANUAL = 0,
    MODE_AUTO
} WeldMode;

typedef struct {

    uint16_t time_ms;
    uint8_t  multiplier;
    uint16_t rest_ms;
    WeldMode mode;

} Settings;

Settings g_settings;

// --------------------------------------------------
// UI FSM
// --------------------------------------------------

typedef enum {

    UI_SPLASH,
    UI_IDLE,
    UI_MENU,
    UI_EDIT

} UiState;

typedef enum {

    MENU_TIME,
    MENU_MULT,
    MENU_REST,
    MENU_MODE,
    MENU_COUNT

} MenuItem;

typedef struct {

    UiState state;

    MenuItem item;

    int16_t edit_value;

    uint16_t timeout;

    uint16_t splash;

} UiContext;

UiContext g_ui;

// --------------------------------------------------
// PROCESS FSM
// --------------------------------------------------

typedef enum {

    PROC_READY,
    PROC_AUTO_MONITOR,
    PROC_WAIT_ZC,
    PROC_PULSE,
    PROC_REST,
    PROC_DONE,
    PROC_LOCKOUT

} ProcessState;

typedef struct {

    ProcessState state;

    uint8_t pulses;

    uint16_t pulse_timer;
    uint16_t rest_timer;
    uint16_t lockout_timer;

    uint16_t beep_timer;

    bool manual_req;
    bool busy;

} ProcessContext;

ProcessContext g_proc;

// --------------------------------------------------
// INPUT
// --------------------------------------------------

typedef struct {

    int8_t enc_delta;

    bool enc_press;
    bool manual_press;

} InputEvent;

InputEvent g_input;

// --------------------------------------------------
// GLOBAL FLAGS
// --------------------------------------------------

volatile uint32_t g_ms = 0;

volatile bool g_tick = false;

volatile bool g_zc_flag = false;

// --------------------------------------------------
// AUTO DETECT
// --------------------------------------------------

uint16_t adc_center = 128;

uint16_t feature_base = 10;

uint8_t detect_count = 0;

#define FEATURE_RATIO 115
#define FEATURE_DIFF  4

bool auto_detect_contact(uint16_t feature)
{

    uint16_t threshold = (feature_base * FEATURE_RATIO) / 100;

    if(feature > threshold && feature > feature_base + FEATURE_DIFF)
        return true;

    return false;
}

// --------------------------------------------------
// HARDWARE
// --------------------------------------------------

void triac_on()
{
    TRIAC_PORT |= (1<<TRIAC_PIN);
}

void triac_off()
{
    TRIAC_PORT &= ~(1<<TRIAC_PIN);
}

void buzzer_on()
{
    BUZZER_PORT |= (1<<BUZZER_PIN);
}

void buzzer_off()
{
    BUZZER_PORT &= ~(1<<BUZZER_PIN);
}

// --------------------------------------------------
// ISR
// --------------------------------------------------

ISR(TIMER1_COMPA_vect)
{
    g_ms++;
    g_tick = true;
}

ISR(INT0_vect)
{
    g_zc_flag = true;
}

// --------------------------------------------------
// INIT
// --------------------------------------------------

void hw_init()
{

    cli();

    TRIAC_DDR |= (1<<TRIAC_PIN);
    BUZZER_DDR |= (1<<BUZZER_PIN);
    SENSE_DDR |= (1<<SENSE_PIN);

    ENC_DDR &= ~((1<<ENC_A)|(1<<ENC_B)|(1<<ENC_SW)|(1<<MANUAL_SW));

    ENC_PORT |= ((1<<ENC_A)|(1<<ENC_B)|(1<<ENC_SW)|(1<<MANUAL_SW));

    ZC_DDR &= ~(1<<ZC_PIN);
    ZC_PORT |= (1<<ZC_PIN);

    // timer1 1ms tick

    TCCR1A = 0;
    TCCR1B = (1<<WGM12)|(1<<CS11)|(1<<CS10);

    OCR1A = 249;

    TIMSK |= (1<<OCIE1A);

    // INT0

    MCUCR |= (1<<ISC01)|(1<<ISC00);
    GICR |= (1<<INT0);

    LCD_initialize();

    sei();
}

// --------------------------------------------------
// SETTINGS
// --------------------------------------------------

void load_settings()
{

    g_settings.time_ms = eeprom_read_word(EEPROM_TIME);
    g_settings.multiplier = eeprom_read_byte(EEPROM_MULT);
    g_settings.rest_ms = eeprom_read_word(EEPROM_REST);
    g_settings.mode = eeprom_read_byte(EEPROM_MODE);

    if(g_settings.time_ms==0 || g_settings.time_ms>150)
        g_settings.time_ms = 50;

    if(g_settings.multiplier==0 || g_settings.multiplier>20)
        g_settings.multiplier = 1;

    if(g_settings.rest_ms>1000)
        g_settings.rest_ms = 200;

}

// --------------------------------------------------
// INPUT TASK
// --------------------------------------------------

void input_task()
{

    static uint8_t last = 0;

    uint8_t a = (ENC_PIN&(1<<ENC_A))?1:0;
    uint8_t b = (ENC_PIN&(1<<ENC_B))?1:0;

    uint8_t state = (a<<1)|b;

    g_input.enc_delta = 0;
    g_input.enc_press = false;
    g_input.manual_press = false;

    if(state!=last)
    {

        if((last==2 && state==0) || (last==0 && state==1))
            g_input.enc_delta = +1;

        if((last==1 && state==0) || (last==0 && state==2))
            g_input.enc_delta = -1;

        last = state;
    }

    if(!(ENC_PIN&(1<<ENC_SW)))
        g_input.enc_press = true;

    if(!(ENC_PIN&(1<<MANUAL_SW)))
        g_input.manual_press = true;
}

// --------------------------------------------------
// UI TASK
// --------------------------------------------------

void ui_task()
{

    switch(g_ui.state)
    {

        case UI_SPLASH:

            if(g_ui.splash>0) g_ui.splash--;
            else g_ui.state = UI_IDLE;

            break;

        case UI_IDLE:

            if(g_input.enc_press)
                g_ui.state = UI_MENU;

            if(g_input.manual_press && !g_proc.busy)
                g_proc.manual_req = true;

            break;

        case UI_MENU:

            if(g_input.enc_delta>0)
                g_ui.item=(g_ui.item+1)%MENU_COUNT;

            if(g_input.enc_delta<0)
                g_ui.item=(g_ui.item+MENU_COUNT-1)%MENU_COUNT;

            if(g_input.enc_press)
            {
                g_ui.state = UI_EDIT;

                switch(g_ui.item)
                {

                    case MENU_TIME: g_ui.edit_value=g_settings.time_ms; break;
                    case MENU_MULT: g_ui.edit_value=g_settings.multiplier; break;
                    case MENU_REST: g_ui.edit_value=g_settings.rest_ms; break;
                    case MENU_MODE: g_ui.edit_value=g_settings.mode; break;

                }
            }

            break;

        case UI_EDIT:

            if(g_input.enc_delta)
                g_ui.edit_value += g_input.enc_delta;

            if(g_input.enc_press)
            {

                switch(g_ui.item)
                {

                    case MENU_TIME: g_settings.time_ms=g_ui.edit_value; break;
                    case MENU_MULT: g_settings.multiplier=g_ui.edit_value; break;
                    case MENU_REST: g_settings.rest_ms=g_ui.edit_value; break;
                    case MENU_MODE: g_settings.mode=g_ui.edit_value; break;

                }

                g_ui.state = UI_IDLE;

            }

            break;

    }

}

// --------------------------------------------------
// PROCESS TASK
// --------------------------------------------------

void process_task()
{

    if(g_proc.pulse_timer>0) g_proc.pulse_timer--;
    if(g_proc.rest_timer>0) g_proc.rest_timer--;
    if(g_proc.lockout_timer>0) g_proc.lockout_timer--;

    switch(g_proc.state)
    {

        case PROC_READY:

            g_proc.busy=false;

            if(g_settings.mode==MODE_AUTO)
                g_proc.state = PROC_AUTO_MONITOR;

            if(g_proc.manual_req)
            {

                g_proc.manual_req=false;

                g_proc.busy=true;

                g_proc.pulses=g_settings.multiplier;

                g_proc.state=PROC_WAIT_ZC;

            }

            break;

        case PROC_AUTO_MONITOR:

            // auto detect stub

            break;

        case PROC_WAIT_ZC:

            if(g_zc_flag)
            {

                g_zc_flag=false;

                triac_on();

                g_proc.pulse_timer = g_settings.time_ms;

                g_proc.state = PROC_PULSE;

            }

            break;

        case PROC_PULSE:

            if(g_proc.pulse_timer==0)
            {

                triac_off();

                g_proc.pulses--;

                if(g_proc.pulses>0)
                {

                    g_proc.rest_timer = g_settings.rest_ms;

                    g_proc.state = PROC_REST;

                }
                else
                    g_proc.state = PROC_DONE;

            }

            break;

        case PROC_REST:

            if(g_proc.rest_timer==0)
                g_proc.state = PROC_WAIT_ZC;

            break;

        case PROC_DONE:

            buzzer_on();

            g_proc.beep_timer = BEEP_TIME;

            g_proc.lockout_timer = LOCKOUT_TIME;

            g_proc.state = PROC_LOCKOUT;

            break;

        case PROC_LOCKOUT:

            if(g_proc.lockout_timer==0)
            {

                buzzer_off();

                g_proc.state = PROC_READY;

            }

            break;

    }

}

// --------------------------------------------------
// RENDER
// --------------------------------------------------

char line0[17];
char line1[17];

void render_task()
{

    snprintf(line0,17,"T:%03u M:%02u R:%03u",
        g_settings.time_ms,
        g_settings.multiplier,
        g_settings.rest_ms);

    if(g_settings.mode==MODE_AUTO)
        snprintf(line1,17,"AUTO");
    else
        snprintf(line1,17,"MANUAL");

    LCD_string(0x80,line0);
    LCD_string(0xC0,line1);

}

// --------------------------------------------------
// MAIN
// --------------------------------------------------

int main()
{

    hw_init();

    load_settings();

    memset(&g_ui,0,sizeof(g_ui));
    memset(&g_proc,0,sizeof(g_proc));

    g_ui.state = UI_SPLASH;
    g_ui.splash = SPLASH_TIME_MS;

    g_proc.state = PROC_READY;

    while(1)
    {

        if(g_tick)
        {

            g_tick=false;

            input_task();

            ui_task();

            process_task();

            render_task();

        }

    }

}
```

---

# 이 코드의 핵심 특징

### 1️⃣ delay 없음

모든 시간은

```
pulse_timer
rest_timer
lockout_timer
```

로 처리됩니다.

---

### 2️⃣ UI / Process 완전 분리

UI는

```
manual_req
```

만 보내고

TRIAC 제어는 **Process FSM만 담당**

---

### 3️⃣ 메뉴 구조 일반화

예전처럼

```
spottimeset()
spotmult()
spotrest()
```

같은 함수 필요 없음.

---

### 4️⃣ zero-cross 동기 용접

용접 시작은 반드시

```
PROC_WAIT_ZC
```

에서 시작됩니다.

---

### 5️⃣ 확장 포인트

이 코드에서 **auto detect**만 붙이면 완성됩니다.

여기입니다.

```
PROC_AUTO_MONITOR
```

---

# 이 코드 구조의 의미

예전 코드:

```
동작은 되는데 설명이 어려움
```

지금 코드:

```
상태도 → 코드가 그대로 대응
```

즉

> **설계와 코드가 일치하는 펌웨어**
> 

입니다.

---

좋습니다.

그럼 아까 만든 전체 구조에 **`PROC_AUTO_MONITOR` + auto detect 모듈**까지 붙여서, 실제로 이어지는 형태로 정리해보겠습니다.

핵심은 이겁니다.

# 붙일 자동 감지 로직

- **zero-cross 기준 반주기 동기화**
- 반주기 동안 ADC 샘플 여러 개 수집
- `abs(sample - center)` 누적으로 **feature** 생성
- 평상시 feature를 baseline으로 천천히 학습
- 현재 feature가 baseline보다 충분히 크면 접촉 후보
- **연속 N번** 검출되면 자동 용접 시작
- 용접 직후 lockout 동안 재검출 금지

---

# 먼저 전제

아래 코드는 **이전 전체 코드에 붙이는 형태**입니다.

즉, 아래 항목들을 추가/수정하면 됩니다.

- ADC init 추가
- Timer0 샘플링 tick 추가
- auto detector 전역 상태 추가
- `PROC_AUTO_MONITOR` 구현
- `render_task()`에 상태 반영

---

# 1. 추가할 전역/타입/매크로

기존 전역/매크로 아래에 이것들을 추가하세요.

```c
// --------------------------------------------------
// AUTO DETECT CONFIG
// --------------------------------------------------

#define ADC_CHANNEL                 0
#define MAX_SAMPLES_PER_HALF        96
#define DETECT_CONSECUTIVE_COUNT    3
#define RELEASE_CONSECUTIVE_COUNT   2
#define FEATURE_RATIO_NUM           115   // 1.15x
#define FEATURE_RATIO_DEN           100
#define FEATURE_MIN_DIFF            4
#define CENTER_ALPHA_SHIFT          5     // 1/32
#define BASELINE_ALPHA_SHIFT        4     // 1/16

typedef enum {
    AUTO_IDLE = 0,
    AUTO_CANDIDATE,
    AUTO_LOCKOUT
} AutoState;

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
    bool sample_tick;
    bool trigger_request;
} AutoDetectContext;

AutoDetectContext g_auto;
```

---

# 2. ADC init / read 추가

기존 `hw_init()` 근처에 아래 함수들을 추가하세요.

```c
void adc_init(void)
{
    // AVCC reference, left adjust, ADC0
    ADMUX = (1 << REFS0) | (1 << ADLAR) | (ADC_CHANNEL & 0x07);

    // ADC enable, prescaler 128
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
}

static inline uint8_t adc_read_8bit(void)
{
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC));
    return ADCH;
}
```

---

# 3. Timer0 샘플링 tick 추가

반주기 동안 ADC를 주기적으로 샘플링하려면 Timer0가 필요합니다.

## init 추가

```c
void timer0_init(void)
{
    // 16MHz / 64 = 250kHz
    // OCR0 = 24 -> 100us 간격
    TCCR0 = (1 << WGM01) | (1 << CS01) | (1 << CS00);
    OCR0  = 24;
    TIMSK |= (1 << OCIE0);
}
```

## ISR 추가

```c
ISR(TIMER0_COMP_vect)
{
    if (g_auto.half_active) {
        g_auto.sample_tick = true;
    }
}
```

---

# 4. 기존 `hw_init()` 수정

기존 `hw_init()` 마지막 부분에 아래 두 줄을 추가하세요.

```c
    adc_init();
    timer0_init();
```

즉 `LCD_initialize();` 전에 넣어도 되고 뒤에 넣어도 됩니다.

---

# 5. zero-cross ISR 수정

기존 ISR:

```c
ISR(INT0_vect)
{
    g_zc_flag = true;
}
```

이걸 아래처럼 바꾸세요.

```c
ISR(INT0_vect)
{
    g_zc_flag = true;

    // 이전 반주기 종료 -> feature 생성
    if (g_auto.half_active && g_auto.sample_count > 0) {
        g_auto.half_feature = (uint16_t)(g_auto.abs_sum / g_auto.sample_count);
        g_auto.feature_ready = true;
    }

    // 새 반주기 시작
    g_auto.abs_sum = 0;
    g_auto.sample_count = 0;
    g_auto.half_active = true;
}
```

---

# 6. auto detector helper 함수 추가

이 함수들을 추가하세요.

```c
static void auto_detector_init(void)
{
    memset(&g_auto, 0, sizeof(g_auto));

    g_auto.state = AUTO_IDLE;
    g_auto.adc_center = 128;
    g_auto.baseline_feature = 10;
}

static bool auto_feature_is_contact(uint16_t feature, uint16_t base)
{
    uint16_t ratio_threshold = (uint16_t)(((uint32_t)base * FEATURE_RATIO_NUM) / FEATURE_RATIO_DEN);

    if (feature > ratio_threshold && feature > (base + FEATURE_MIN_DIFF)) {
        return true;
    }
    return false;
}

static void auto_update_baseline(uint16_t feature)
{
    int16_t diff = (int16_t)feature - (int16_t)g_auto.baseline_feature;
    g_auto.baseline_feature += (diff >> BASELINE_ALPHA_SHIFT);

    if (g_auto.baseline_feature < 1) {
        g_auto.baseline_feature = 1;
    }
}
```

---

# 7. 샘플링 task 추가

메인 루프에서 1ms마다 호출되는 `input_task()`, `ui_task()`, `process_task()`와 별개로,

ADC 샘플링은 가능한 자주 체크해야 합니다. 그래도 여기서는 간단히 main loop에서 계속 호출해도 됩니다.

```c
static void auto_sample_task(void)
{
    if (!g_auto.half_active || !g_auto.sample_tick) {
        return;
    }

    g_auto.sample_tick = false;

    if (g_auto.sample_count >= MAX_SAMPLES_PER_HALF) {
        return;
    }

    uint8_t raw = adc_read_8bit();

    // ADC center tracking
    int16_t center_diff = (int16_t)raw - (int16_t)g_auto.adc_center;
    g_auto.adc_center += (center_diff >> CENTER_ALPHA_SHIFT);

    int16_t diff = (int16_t)raw - (int16_t)g_auto.adc_center;
    if (diff < 0) diff = -diff;

    g_auto.abs_sum += (uint16_t)diff;
    g_auto.sample_count++;
}
```

---

# 8. `auto_detect_contact()` 실제 구현

이전 코드에서는 stub였죠.

이걸 아래처럼 바꾸면 됩니다.

```c
bool auto_detect_contact(void)
{
    if (!g_auto.feature_ready) {
        return false;
    }

    uint16_t feature = g_auto.half_feature;
    g_auto.feature_ready = false;

    switch (g_auto.state)
    {
        case AUTO_IDLE:
            auto_update_baseline(feature);

            if (auto_feature_is_contact(feature, g_auto.baseline_feature)) {
                g_auto.detect_count = 1;
                g_auto.release_count = 0;
                g_auto.state = AUTO_CANDIDATE;
            }
            break;

        case AUTO_CANDIDATE:
            if (auto_feature_is_contact(feature, g_auto.baseline_feature)) {
                g_auto.detect_count++;

                if (g_auto.detect_count >= DETECT_CONSECUTIVE_COUNT) {
                    g_auto.detect_count = 0;
                    g_auto.release_count = 0;
                    g_auto.state = AUTO_LOCKOUT;
                    return true;
                }
            } else {
                g_auto.release_count++;

                if (g_auto.release_count >= RELEASE_CONSECUTIVE_COUNT) {
                    g_auto.detect_count = 0;
                    g_auto.release_count = 0;
                    g_auto.state = AUTO_IDLE;
                    auto_update_baseline(feature);
                }
            }
            break;

        case AUTO_LOCKOUT:
            // Process FSM에서 lockout 끝나면 AUTO_IDLE로 되돌릴 예정
            break;

        default:
            g_auto.state = AUTO_IDLE;
            break;
    }

    return false;
}
```

---

# 9. Process FSM의 `PROC_AUTO_MONITOR` 붙이기

기존 단순 stub:

```c
        case PROC_AUTO_MONITOR:

            // auto detect stub

            break;
```

이 부분을 아래로 교체하세요.

```c
        case PROC_AUTO_MONITOR:

            g_proc.busy = false;

            if(g_settings.mode != MODE_AUTO) {
                g_proc.state = PROC_READY;
                break;
            }

            if(g_proc.manual_req) {
                g_proc.manual_req = false;
                g_proc.busy = true;
                g_proc.pulses = g_settings.multiplier;
                g_proc.state = PROC_WAIT_ZC;
                break;
            }

            if (auto_detect_contact()) {
                g_proc.busy = true;
                g_proc.pulses = g_settings.multiplier;
                g_proc.state = PROC_WAIT_ZC;
            }

            break;
```

---

# 10. Process lockout 끝날 때 auto detector 리셋

기존 `PROC_LOCKOUT` 종료부를 아래처럼 수정하세요.

```c
        case PROC_LOCKOUT:

            if(g_proc.lockout_timer==0)
            {
                buzzer_off();

                if (g_settings.mode == MODE_AUTO) {
                    g_auto.state = AUTO_IDLE;
                    g_auto.detect_count = 0;
                    g_auto.release_count = 0;
                    g_proc.state = PROC_AUTO_MONITOR;
                } else {
                    g_proc.state = PROC_READY;
                }
            }

            break;
```

---

# 11. `main()`에서 auto detector init 추가

기존 `main()` 초기화 구간에 이것도 넣으세요.

```c
    auto_detector_init();
```

즉 대략 이렇게 됩니다.

```c
int main()
{
    hw_init();
    load_settings();
    auto_detector_init();

    memset(&g_ui,0,sizeof(g_ui));
    memset(&g_proc,0,sizeof(g_proc));

    g_ui.state = UI_SPLASH;
    g_ui.splash = SPLASH_TIME_MS;

    g_proc.state = PROC_READY;

    while(1)
    {
        // ADC 샘플링 task는 가능한 자주 호출
        auto_sample_task();

        if(g_tick)
        {
            g_tick=false;

            input_task();
            ui_task();
            process_task();
            render_task();
        }
    }
}
```

---

# 12. `render_task()`에 상태 반영 개선

현재 `AUTO`/`MANUAL`만 표시하면 자동감지 중인지 잘 안 보입니다.

이렇게 조금 바꾸는 게 좋습니다.

```c
void render_task()
{
    snprintf(line0,17,"T:%03u M:%02u R:%03u",
        g_settings.time_ms,
        g_settings.multiplier,
        g_settings.rest_ms);

    if (g_ui.state == UI_MENU) {
        switch (g_ui.item) {
            case MENU_TIME: snprintf(line1,17,"> TIME"); break;
            case MENU_MULT: snprintf(line1,17,"> MULT"); break;
            case MENU_REST: snprintf(line1,17,"> REST"); break;
            case MENU_MODE: snprintf(line1,17,"> MODE"); break;
            default:        snprintf(line1,17,"> MENU"); break;
        }
    }
    else if (g_ui.state == UI_EDIT) {
        switch (g_ui.item) {
            case MENU_TIME: snprintf(line1,17,"TIME:%4d", g_ui.edit_value); break;
            case MENU_MULT: snprintf(line1,17,"MULT:%4d", g_ui.edit_value); break;
            case MENU_REST: snprintf(line1,17,"REST:%4d", g_ui.edit_value); break;
            case MENU_MODE:
                snprintf(line1,17,"%s", (g_ui.edit_value == MODE_AUTO) ? "MODE:AUTO" : "MODE:MAN");
                break;
            default:
                snprintf(line1,17,"EDIT");
                break;
        }
    }
    else {
        switch (g_proc.state) {
            case PROC_READY:        snprintf(line1,17,"READY"); break;
            case PROC_AUTO_MONITOR: snprintf(line1,17,"AUTO MONITOR"); break;
            case PROC_WAIT_ZC:      snprintf(line1,17,"ARMED"); break;
            case PROC_PULSE:        snprintf(line1,17,"WELDING"); break;
            case PROC_REST:         snprintf(line1,17,"REST"); break;
            case PROC_DONE:         snprintf(line1,17,"DONE"); break;
            case PROC_LOCKOUT:      snprintf(line1,17,"LOCKOUT"); break;
            default:                snprintf(line1,17,"UNKNOWN"); break;
        }
    }

    LCD_string(0x80,line0);
    LCD_string(0xC0,line1);
}
```

---

# 13. 전체 흐름 요약

이제 전체 시스템은 이렇게 됩니다.

## UI

- Splash
- Idle
- Menu
- Edit

## Process

- Ready
- Auto Monitor
- Wait ZC
- Pulse
- Rest
- Done
- Lockout

## Auto Detect

- 반주기 동기 샘플링
- 절댓값 편차 누적
- baseline 학습
- 연속 검출
- trigger 발생

---

# 14. 핵심적으로 달라진 점

예전 방식:

- blocking ADC
- 큰 for문
- 고정 threshold
- 불안정한 환경 적응

지금 방식:

- zero-cross 동기
- 반주기 feature
- adaptive baseline
- non-blocking
- FSM 결합

즉,

> **이제 자동 감지는 “ADC를 한 번 크게 돌려서 판단”하는 구조가 아니라, 반주기마다 특징량을 업데이트하면서 접촉을 판정하는 구조**입니다.
> 

---

# 15. 마지막으로, 완전 합친 버전에서 꼭 주의할 점

## 1. ATmega16의 `TIMSK`

컴파일 환경에 따라 `TIMSK` 비트명이 다를 수 있습니다.

현재 스타일은 구형 AVR-GCC 기준으로 맞춘 겁니다.

## 2. LCD 함수가 blocking이면

UI는 괜찮지만, LCD가 너무 느리면 전체 responsiveness가 떨어질 수 있습니다.

## 3. ADC 기준점

지금은 `adc_center = 128`에서 시작합니다.

실회로에서 offset이 다르면 baseline이 안정화될 때까지 약간 시간이 걸릴 수 있습니다.

## 4. threshold 튜닝

처음엔 아래 값부터 시작하세요.

```c
#define FEATURE_RATIO_NUM           115
#define FEATURE_RATIO_DEN           100
#define FEATURE_MIN_DIFF            4
#define DETECT_CONSECUTIVE_COUNT    3
```

오검출 많으면 `115 -> 120~125`로 올리면 됩니다.

---

원하면 다음 답변에서 내가 이걸 **하나로 합친 완전한 단일 C 파일 형태**로 다시 정리해서 줄게요.