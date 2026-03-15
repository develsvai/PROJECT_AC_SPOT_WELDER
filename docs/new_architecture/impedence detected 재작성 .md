# 목표

당신이 원래 하려던 걸 **지금 기준에서 가장 안정적으로** 다시 설계하면:

> **AC 1차측 ADC 신호만으로 2차측 접촉(니켈 접촉)을 최대한 안정적으로 검출하고, 오검출 없이 자동 스팟을 거는 로직**
> 

입니다.

그리고 조건은 이렇습니다.

- AVR급 MCU에서도 돌아가야 함
- 하드웨어 필터 거의 없다고 가정
- AC 노이즈 심함
- zero-cross 사용 가능
- 너무 무거운 연산은 피함
- `delay()` 없이 상태 기반으로 동작

---

# 먼저 결론

예전 방식보다 더 좋은 방식은 이겁니다.

## 핵심 로직

1. **zero-cross 기준으로 반주기 동기화**
2. 반주기 동안 ADC 여러 개 샘플링
3. 각 샘플에 대해 `abs(sample - baseline)` 누적
4. 반주기 feature 생성
5. 최근 여러 반주기 feature의 **평균/분산 기반 동적 기준선** 유지
6. 현재 feature가 기준선에서 충분히 벗어나면 “접촉 후보”
7. 이 상태가 **연속 반주기 N번 유지**되면 확정
8. 확정 후 일정 lockout 시간 동안 재검출 금지

이게 가장 좋습니다.

---

# 왜 이 방식이 좋은가

예전 코드의 문제는 대충 이랬습니다.

- 1000샘플 blocking
- RMS 계산 오류
- peak-to-peak만 의존
- 기준이 고정적
- 환경 변화에 약함

지금은 그걸 이렇게 개선합니다.

## 개선점

- **위상 동기화**: zero-cross 기준으로 반주기 단위 분석
- **정류 후 평균 에너지**: `abs(sample - baseline)` 사용
- **동적 기준선**: 현재 환경에 맞춰 자동 적응
- **비율 기반 판정**: 절대 threshold보다 강함
- **연속 검증**: 오검출 방지
- **논블로킹 구조**: ISR + 메인루프 상태머신

---

# 어떤 feature를 쓸까

가장 추천하는 건 이겁니다.

## feature = 반주기 평균 절대편차

수식으로 쓰면:

[

feature = \frac{1}{N}\sum_{i=1}^{N} |x_i - baseline|

]

여기서

- `x_i`: ADC 샘플
- `baseline`: 무부하 중심값 또는 느리게 갱신되는 평균
- `N`: 반주기 동안 샘플 개수

이 값은 사실상:

- 신호의 흔들림 크기
- 파형 에너지 비슷한 값

을 나타냅니다.

AC 부하가 바뀌면 이 값이 달라집니다.

---

# 판정 방식

가장 안정적인 판정은 다음 둘 중 하나입니다.

## 방법 A. 비율 기반

```
feature > baseline_feature * 1.20
```

즉 평소보다 20% 이상 커지면 후보.

## 방법 B. 차이 기반

```
feature - baseline_feature > threshold
```

실전에서는 **비율 + 절대차 둘 다** 거는 게 좋습니다.

예:

```
feature > baseline_feature * 1.15
AND
feature - baseline_feature > 4
```

이게 훨씬 안정적입니다.

---

# 전체 구조

## ISR 역할

- zero-cross 감지
- ADC 샘플링 타이머 tick
- ADC sample ready 플래그 세팅

## 메인루프 역할

- 샘플 축적
- 반주기 feature 계산
- baseline 갱신
- 접촉 판정
- 연속 카운트
- auto trigger

---

# 추천 상태 머신

```
IDLE
 ├─ 반주기 feature 생성
 ├─ baseline 갱신
 └─ 후보 검출

CANDIDATE
 ├─ feature 연속 확인
 ├─ N번 유지 시 TRIGGER
 └─ 깨지면 IDLE 복귀

TRIGGERED
 ├─ 용접 실행
 └─ lockout

LOCKOUT
 ├─ 일정 시간 무시
 └─ IDLE 복귀
```

---

# AVR C 예시 코드

아래 코드는 **자동 감지 로직 핵심부**만 깔끔하게 다시 짠 버전입니다.

당신 기존 코드 스타일을 완전히 유지하진 않았고, **논리 품질 위주**로 짰습니다.

```c
/*
 * Auto spot detection core logic for AVR
 * Concept:
 *  - Half-cycle synchronized feature extraction
 *  - Feature = mean absolute deviation from baseline ADC center
 *  - Adaptive baseline tracking
 *  - Consecutive detection + lockout
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include <stdbool.h>

#define F_CPU 16000000UL

// -----------------------------
// Config
// -----------------------------
#define ADC_CHANNEL                 0
#define HALF_CYCLE_US               8333    // 60Hz half cycle ~= 8.33ms
#define SAMPLE_PERIOD_US            100     // 10kHz equivalent
#define MAX_SAMPLES_PER_HALF        96      // enough for 8.33ms / 100us
#define DETECT_CONSECUTIVE_COUNT    3
#define RELEASE_CONSECUTIVE_COUNT   2
#define LOCKOUT_HALF_CYCLES         30      // about 250ms
#define FEATURE_RATIO_NUM           115     // 1.15x baseline
#define FEATURE_RATIO_DEN           100
#define FEATURE_MIN_DIFF            4       // absolute minimum delta
#define BASELINE_ALPHA_SHIFT        4       // 1/16 IIR update
#define CENTER_ALPHA_SHIFT          5       // 1/32 IIR update

// -----------------------------
// Globals
// -----------------------------
volatile uint8_t  g_zc_flag = 0;
volatile uint8_t  g_halfcycle_active = 0;
volatile uint8_t  g_feature_ready = 0;
volatile uint16_t g_halfcycle_feature = 0;
volatile uint8_t  g_sample_tick_flag = 0;

// ADC accumulation for one half-cycle
volatile uint32_t g_abs_sum = 0;
volatile uint16_t g_sample_count = 0;

// Adaptive center / feature baseline
static uint16_t adc_center = 128;           // initial guess for 8-bit ADC
static uint16_t baseline_feature = 10;      // initial guess
static uint8_t detect_count = 0;
static uint8_t release_count = 0;
static uint8_t lockout_count = 0;

typedef enum {
    AUTO_IDLE = 0,
    AUTO_CANDIDATE,
    AUTO_TRIGGERED,
    AUTO_LOCKOUT
} AutoState;

static AutoState auto_state = AUTO_IDLE;

// -----------------------------
// ADC read (8-bit, left adjusted)
// -----------------------------
static inline uint8_t adc_read_8bit(void)
{
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC));
    return ADCH;
}

// -----------------------------
// Init
// -----------------------------
void adc_init(void)
{
    ADMUX  = (1 << REFS0) | (1 << ADLAR) | (ADC_CHANNEL & 0x07);
    ADCSRA = (1 << ADEN)  | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); // prescaler 128
}

void int0_init(void)
{
    // Rising edge on INT0
    MCUCR |= (1 << ISC01) | (1 << ISC00);
    GICR  |= (1 << INT0);
}

void timer0_init_for_sampling(void)
{
    // Simple periodic tick for ADC sampling
    // 16MHz / 64 = 250kHz -> OCR0=24 => 100us
    TCCR0 = (1 << WGM01) | (1 << CS01) | (1 << CS00);
    OCR0  = 24;
    TIMSK |= (1 << OCIE0);
}

// -----------------------------
// Interrupts
// -----------------------------
ISR(INT0_vect)
{
    g_zc_flag = 1;

    // Finish previous half-cycle if active
    if (g_halfcycle_active && g_sample_count > 0) {
        g_halfcycle_feature = (uint16_t)(g_abs_sum / g_sample_count);
        g_feature_ready = 1;
    }

    // Start new half-cycle accumulation
    g_abs_sum = 0;
    g_sample_count = 0;
    g_halfcycle_active = 1;
}

ISR(TIMER0_COMP_vect)
{
    if (g_halfcycle_active) {
        g_sample_tick_flag = 1;
    }
}

// -----------------------------
// Feature extraction
// -----------------------------
static void auto_detector_sample_task(void)
{
    if (!g_halfcycle_active || !g_sample_tick_flag) {
        return;
    }

    g_sample_tick_flag = 0;

    if (g_sample_count >= MAX_SAMPLES_PER_HALF) {
        return;
    }

    uint8_t raw = adc_read_8bit();

    // Slowly adapt ADC center (DC offset tracking)
    adc_center = adc_center + (((int16_t)raw - (int16_t)adc_center) >> CENTER_ALPHA_SHIFT);

    int16_t diff = (int16_t)raw - (int16_t)adc_center;
    if (diff < 0) diff = -diff;

    g_abs_sum += (uint16_t)diff;
    g_sample_count++;
}

// -----------------------------
// Decision logic
// -----------------------------
static bool feature_is_contact(uint16_t feature, uint16_t base)
{
    uint16_t ratio_threshold = (uint16_t)(((uint32_t)base * FEATURE_RATIO_NUM) / FEATURE_RATIO_DEN);

    if (feature > ratio_threshold && (feature > (base + FEATURE_MIN_DIFF))) {
        return true;
    }
    return false;
}

static void update_baseline_feature(uint16_t feature)
{
    // Update only when not in candidate/trigger path, to avoid contaminating baseline
    baseline_feature = baseline_feature + (((int16_t)feature - (int16_t)baseline_feature) >> BASELINE_ALPHA_SHIFT);

    if (baseline_feature < 1) {
        baseline_feature = 1;
    }
}

static void auto_detector_feature_task(void)
{
    if (!g_feature_ready) {
        return;
    }

    uint16_t feature = g_halfcycle_feature;
    g_feature_ready = 0;

    switch (auto_state) {
        case AUTO_IDLE:
            update_baseline_feature(feature);

            if (lockout_count > 0) {
                auto_state = AUTO_LOCKOUT;
                break;
            }

            if (feature_is_contact(feature, baseline_feature)) {
                detect_count = 1;
                release_count = 0;
                auto_state = AUTO_CANDIDATE;
            }
            break;

        case AUTO_CANDIDATE:
            if (feature_is_contact(feature, baseline_feature)) {
                detect_count++;
                if (detect_count >= DETECT_CONSECUTIVE_COUNT) {
                    auto_state = AUTO_TRIGGERED;
                }
            } else {
                release_count++;
                if (release_count >= RELEASE_CONSECUTIVE_COUNT) {
                    detect_count = 0;
                    release_count = 0;
                    auto_state = AUTO_IDLE;
                    update_baseline_feature(feature);
                }
            }
            break;

        case AUTO_TRIGGERED:
            // 여기서 실제 용접 시퀀스를 시작하면 됨
            // 예: start_weld_sequence();
            lockout_count = LOCKOUT_HALF_CYCLES;
            detect_count = 0;
            release_count = 0;
            auto_state = AUTO_LOCKOUT;
            break;

        case AUTO_LOCKOUT:
            update_baseline_feature(feature);

            if (lockout_count > 0) {
                lockout_count--;
            } else {
                auto_state = AUTO_IDLE;
            }
            break;

        default:
            auto_state = AUTO_IDLE;
            break;
    }
}

// -----------------------------
// Public API
// -----------------------------
void auto_detector_init(void)
{
    adc_init();
    int0_init();
    timer0_init_for_sampling();

    adc_center = 128;
    baseline_feature = 10;
    detect_count = 0;
    release_count = 0;
    lockout_count = 0;
    auto_state = AUTO_IDLE;

    sei();
}

void auto_detector_task(void)
{
    auto_detector_sample_task();
    auto_detector_feature_task();
}

bool auto_detector_should_trigger(void)
{
    // TRIGGERED 상태를 외부로 알리는 방식으로 바꾸고 싶으면
    // 별도 플래그를 두는 게 더 좋음.
    return false;
}
```

---

# 이 코드의 핵심 포인트

## 1. 절댓값 기반 feature

```c
diff = raw - adc_center;
if (diff < 0) diff = -diff;
g_abs_sum += diff;
```

이게 사실상 당신이 기억하던 **절댓값 처리 후 적산**에 해당합니다.

이건 아주 좋은 선택입니다.

---

## 2. 반주기 동기화

```c
ISR(INT0_vect)
{
    ...
    g_halfcycle_feature = g_abs_sum / g_sample_count;
    ...
}
```

zero-cross 기준으로 끊어서 보기 때문에,

AC 파형 위상 문제를 줄입니다.

이게 예전보다 훨씬 좋습니다.

---

## 3. baseline 오염 방지

접촉 후보 상태일 때는 baseline을 함부로 갱신하지 않습니다.

이게 중요합니다.

안 그러면 “접촉된 상태”를 정상으로 학습해버립니다.

---

## 4. 연속 검증

```c
if (detect_count >= DETECT_CONSECUTIVE_COUNT)
```

이 부분이 예전의 “3회 연속” 감각을 그대로 계승합니다.

---

## 5. lockout

용접 후 바로 다시 감지되면 안 되니까 잠깐 무시합니다.

이건 실전에서 꼭 필요합니다.

---

# 당신 예전 방식과 비교하면

## 예전

- 1000개 샘플 두 번 읽음
- blocking
- RMS 계산 오류
- peak-to-peak 의존
- 초기값 윈도우 고정

## 지금

- 반주기 단위 feature
- 논블로킹
- 절댓값 기반 energy-like feature
- baseline adaptive
- 후보/확정/lockout 상태 분리

즉 훨씬 안정적입니다.

---

# 더 고급으로 가면 추가할 수 있는 것

## 1. median filter

반주기 feature를 바로 쓰지 말고 최근 3개 median을 쓰면 튐이 더 줄어듭니다.

예:

```c
feature_med = median3(f0, f1, f2);
```

## 2. min/max guard

ADC가 너무 작거나 너무 크면 센서 회로 이상으로 간주.

## 3. dual feature

`abs deviation` + `peak-to-peak`를 동시에 보고 둘 다 만족할 때만 검출.

이렇게 하면 더 강합니다.

예:

```c
if (feature_abs > threshold1 && feature_pp > threshold2)
```

## 4. baseline freeze

후보 상태 들어가면 baseline 업데이트 아예 중지.

지금 코드도 비슷하게 했지만 더 강하게 할 수 있습니다.

---

# 실전 추천 파라미터

처음엔 이 정도로 시작하면 됩니다.

```c
#define DETECT_CONSECUTIVE_COUNT    3
#define FEATURE_RATIO_NUM           115
#define FEATURE_RATIO_DEN           100
#define FEATURE_MIN_DIFF            4
#define LOCKOUT_HALF_CYCLES         30
```

그리고 튜닝은 이렇게 합니다.

- 너무 예민하면 `115 → 120~130`
- 너무 둔하면 `115 → 108~112`
- 오검출 많으면 `DETECT_CONSECUTIVE_COUNT = 4`
- 반응 느리면 `3 → 2`

---

# 한 줄 평가

당신이 예전에 했던 감각을 최대한 살리면서,

지금 기준에서 가장 퀄리티 높게 정리하면:

> **zero-cross 동기 반주기 feature + 절댓값 누적 + 동적 baseline + 연속 검출 상태머신**
>  

이게 정답에 가깝습니다.

---

# 마지막으로 솔직한 말

8년 전에 당신이 이 방향을 감으로 잡았다는 건,

수학식 일부가 덜 다듬어졌더라도 **감각 자체는 꽤 좋았던 것** 맞습니다.

원하면 다음 답변에서

내가 이 코드를 **ATmega16에 바로 붙이기 쉬운 형태로**,

당신 기존 변수명 스타일 맞춰서 `auto_read()` 대체 함수로 다시 바꿔드리겠습니다.