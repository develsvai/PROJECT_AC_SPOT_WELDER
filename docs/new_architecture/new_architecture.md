
## 1. 재설계 목표

기존 코드의 문제는 대체로 아래와 같습니다.

- UI, 입력, 용접 시퀀스, 자동 감지, 하드웨어 제어가 한 흐름에 섞여 있음
- `while(1)` 중첩과 `delay()` 기반 타이밍 제어가 많음
- 메뉴별 함수가 분리되어 있지만 구조적으로는 중복이 큼
- 자동 감지가 blocking 방식이라 전체 흐름을 멈추게 만듦
- 상태가 enum보다 flag, break, 분기 조합으로 암묵적으로 표현됨

새 구조의 목표는 아래와 같습니다.

- UI와 실제 용접 제어를 분리
- `delay()` 제거, 논블로킹 타이머 기반 동작
- 메뉴/설정/입력 구조 단순화
- 자동/수동 용접 흐름 통합
- zero-cross 동기 자동 감지 구조 도입
- UML로 설명 가능한 상태머신 구조 정립

한 줄로 정리하면:

> "되게 만든 코드"를 "설명 가능한 FSM 기반 구조"로 바꾸는 것

---

## 2. 최상위 아키텍처

새 구조는 4개 레이어로 나눕니다.

```text
+----------------------+
|     Input Layer      |
| encoder / buttons    |
| zero-cross / adc     |
+----------------------+
           |
           v
+----------------------+
|       UI FSM         |
| splash / idle / menu |
+----------------------+
           |
           v
+----------------------+
|    Process FSM       |
| ready / weld / lock  |
+----------------------+
           |
           v
+----------------------+
|    Driver Layer      |
| lcd / triac / adc    |
+----------------------+
```

각 레이어의 역할은 다음과 같습니다.

- Input Layer: 핀을 읽고 이벤트를 생성
- UI FSM: 화면, 메뉴, 설정 편집 관리
- Process FSM: 실제 용접 시퀀스 관리
- Driver Layer: LCD, TRIAC, ADC, 센스 제어 등 하드웨어 접근

핵심은 UI가 TRIAC를 직접 만지지 않고, Process가 LCD 로직을 직접 갖지 않도록 분리하는 것입니다.

---

## 3. 시스템 흐름

상위 수준에서는 다음 흐름으로 볼 수 있습니다.

```text
[BOOT]
   |
   v
[UI_ACTIVE]
   |
   | manual/auto weld request
   v
[PROCESS_ACTIVE]
   |
   | process done
   v
[UI_ACTIVE]
```

실제 구현에서는 아래처럼 더 구체적인 상태로 나뉩니다.

```text
ROOT
├─ BOOT
├─ UI
│  ├─ SPLASH
│  ├─ IDLE
│  ├─ MENU_MAIN
│  └─ MENU_EDIT
└─ PROCESS
   ├─ READY
   ├─ AUTO_MONITOR
   ├─ WAIT_ZC
   ├─ PULSE_ON
   ├─ REST
   ├─ DONE
   └─ LOCKOUT
```

---

## 4. UI FSM

UI는 화면과 메뉴만 담당합니다.

### UI 상태

```c
typedef enum {
    UI_SPLASH = 0,
    UI_IDLE,
    UI_MENU_MAIN,
    UI_MENU_EDIT
} UiState;
```

### UI 메뉴 항목

```c
typedef enum {
    MENU_TIME = 0,
    MENU_MULT,
    MENU_REST,
    MENU_MODE,
    MENU_COUNT
} MenuItem;
```

### UI 상태 의미

- `UI_SPLASH`
  - 부팅 직후 시작 화면
  - 일정 시간 후 `UI_IDLE`

- `UI_IDLE`
  - 현재 설정값 표시
  - Process 상태 표시
  - 엔코더 버튼으로 메뉴 진입
  - 수동 버튼으로 manual weld 요청

- `UI_MENU_MAIN`
  - `TIME`, `MULT`, `REST`, `MODE` 중 하나 선택
  - 엔코더 회전으로 항목 이동
  - 버튼으로 편집 진입

- `UI_MENU_EDIT`
  - 선택 항목 값 수정
  - 엔코더 회전으로 값 변경
  - 버튼으로 저장
  - 취소/timeout 시 편집 종료

### UI 설계 핵심

기존처럼 `spottimeset()`, `spotmult()`, `spotrest()`, `auto_spot()`를 각각 두는 방식은 버립니다.

대신:

- `MENU_MAIN`에서 항목 선택
- `MENU_EDIT`에서 값 편집

형태로 일반화합니다.

장점은 다음과 같습니다.

- 중복 코드 감소
- 설정 항목 추가 용이
- UI 일관성 증가
- 상태 수 감소

---

## 5. Process FSM

Process FSM은 실제 용접 동작만 담당합니다.

### Process 상태

```c
typedef enum {
    PROC_READY = 0,
    PROC_AUTO_MONITOR,
    PROC_WAIT_ZC,
    PROC_PULSE_ON,
    PROC_REST,
    PROC_DONE,
    PROC_LOCKOUT
} ProcessState;
```

### 상태 의미

- `PROC_READY`
  - 용접 대기
  - manual 요청 또는 auto mode에 따라 다음 상태 전이

- `PROC_AUTO_MONITOR`
  - 자동 감지 수행
  - 접촉 감지 성공 시 `PROC_WAIT_ZC`

- `PROC_WAIT_ZC`
  - zero-cross 이벤트 대기
  - 반주기 시작점에서 펄스 시작

- `PROC_PULSE_ON`
  - TRIAC on
  - 설정 시간 동안 펄스 유지

- `PROC_REST`
  - 멀티 펄스 간 휴지 시간

- `PROC_DONE`
  - 용접 완료
  - beep / 완료 표시 가능

- `PROC_LOCKOUT`
  - 재트리거 방지용 무시 시간

### 상태 흐름

```text
[PROC_READY]
    -> auto mode on -> [PROC_AUTO_MONITOR]
    -> manual request -> [PROC_WAIT_ZC]

[PROC_AUTO_MONITOR]
    -> contact detected -> [PROC_WAIT_ZC]
    -> auto off -> [PROC_READY]

[PROC_WAIT_ZC]
    -> zero-cross -> [PROC_PULSE_ON]

[PROC_PULSE_ON]
    -> pulse end && more pulses -> [PROC_REST]
    -> pulse end && no more pulses -> [PROC_DONE]

[PROC_REST]
    -> rest end -> [PROC_WAIT_ZC]

[PROC_DONE]
    -> [PROC_LOCKOUT]

[PROC_LOCKOUT]
    -> timeout -> [PROC_READY] or [PROC_AUTO_MONITOR]
```

---

## 6. 입력 처리 구조

입력은 각 FSM이 핀을 직접 읽는 방식이 아니라, 입력 스캐너가 이벤트를 만들어 전달하는 구조로 갑니다.

### 입력 이벤트 예시

```c
typedef struct {
    int8_t encoder_delta;
    bool enc_press;
    bool manual_press;
} InputEvent;
```

### 입력 처리 단계

```text
[RAW PIN READ]
    |
    v
[DEBOUNCE]
    |
    v
[EDGE DETECT]
    |
    v
[EVENT GENERATE]
```

즉:

- 핀 읽기
- 디바운스
- 엔코더 방향 판별
- 버튼 이벤트 생성
- UI / Process에 전달

---

## 7. 렌더링 분리

기존 코드는 상태 로직 안에서 LCD를 직접 갱신하고 있었지만, 새 구조에서는 렌더링을 별도 단계로 분리합니다.

### 권장 구조

```c
void input_task(void);
void ui_task(void);
void process_task(void);
void render_task(void);
```

### 렌더링 원칙

- `ui_task()`는 상태만 변경
- `process_task()`는 동작 상태만 변경
- `render_task()`가 최종 화면 문자열을 생성

### 화면 예시

Idle:

```text
T:050 M:02 R:100
AUTO READY
```

Menu Main:

```text
> TIME
PRESS TO EDIT
```

Menu Edit:

```text
SET TIME
< 050 ms >
```

Welding:

```text
T:019 M:01 R:200
AUTO WELD
```

---

## 8. 자동 감지 재설계

이 부분이 기존 코드와 가장 크게 달라지는 부분입니다.

목표는:

> AC 1차측 ADC 신호만으로 2차측 접촉(니켈 접촉)을 최대한 안정적으로 검출하고, 오검출 없이 자동 스팟을 거는 것

### 조건

- AVR급 MCU에서 동작 가능해야 함
- zero-cross 사용 가능
- 하드웨어 필터 거의 없다고 가정
- AC 노이즈가 심함
- 무거운 연산은 피함
- `delay()` 없이 상태 기반으로 동작

### 핵심 아이디어

1. zero-cross 기준으로 반주기 동기화
2. 반주기 동안 ADC 여러 개 샘플링
3. 각 샘플에 대해 `abs(sample - baseline)` 누적
4. 반주기 feature 생성
5. 최근 반주기 feature의 baseline 유지
6. 현재 feature가 baseline 대비 충분히 커지면 접촉 후보
7. 연속 N회 유지되면 접촉 확정
8. 용접 후 일정 시간 lockout

### 추천 feature

가장 추천하는 값은 반주기 평균 절대편차입니다.

```text
feature = (1/N) * Σ |x_i - baseline|
```

여기서:

- `x_i`: ADC 샘플
- `baseline`: 무부하 중심값 또는 천천히 갱신되는 기준
- `N`: 반주기 동안 샘플 개수

이 값은 신호 흔들림 크기, 즉 파형 에너지에 가까운 지표입니다.

### 판정 방식

비율 기준과 절대차 기준을 동시에 쓰는 것이 가장 안정적입니다.

```c
feature > baseline_feature * 1.15
AND
feature > baseline_feature + 4
```

### 자동 감지 내부 상태

```text
AUTO_IDLE
AUTO_CANDIDATE
AUTO_TRIGGERED
AUTO_LOCKOUT
```

실제 구현에서는 `PROC_AUTO_MONITOR` 내부 모듈로 넣거나, 독립 모듈로 분리할 수 있습니다.

---

## 9. 코드 구조 기준안

새 구조의 핵심 컨텍스트는 대략 다음과 같습니다.

### Settings

```c
typedef struct {
    uint16_t time_ms;
    uint8_t  multiplier;
    uint16_t rest_ms;
    WeldMode mode;
} Settings;
```

### UI Context

```c
typedef struct {
    UiState state;
    MenuItem selected_item;
    int16_t edit_value;
    uint16_t timeout_ms;
    uint16_t splash_ms;
    bool dirty;
} UiContext;
```

### Process Context

```c
typedef struct {
    ProcessState state;
    uint8_t pulses_remaining;
    uint16_t pulse_timer_ms;
    uint16_t rest_timer_ms;
    uint16_t lockout_timer_ms;
    uint16_t beep_timer_ms;
    bool manual_request;
    bool auto_request;
    bool trigger_latched;
    bool busy;
    bool dirty;
} ProcessContext;
```

### Input Event

```c
typedef struct {
    int8_t encoder_delta;
    bool enc_pressed;
    bool manual_pressed;
} InputEvent;
```

---

## 10. 메인 루프 기준

최종적으로 권장하는 메인 루프는 아래 구조입니다.

```c
while (1) {
    if (g_tick_1ms) {
        g_tick_1ms = false;

        input_task();
        ui_task();
        process_task();
        render_task();
    }
}
```

이 구조의 장점은 명확합니다.

- 모든 로직이 짧게 실행되고 즉시 복귀
- 중첩 `while(1)` 제거
- `delay()` 제거
- 상태도와 코드 대응이 쉬움

---

## 11. 현재 `code.md` 구현 수준

`code.md`는 실제 구조 예시 코드로서 가치가 큽니다.

현재 포함된 핵심 요소:

- `ATmega16`, `16MHz`
- `INT0` zero-cross 인터럽트
- `Timer1` 1ms tick
- EEPROM 설정 로드
- 입력 스캔
- UI FSM 기본 구조
- Process FSM 기본 구조
- TRIAC / Buzzer / Sense 제어 함수

다만 현재 기준으로는 아직 완성품 코드는 아닙니다.

대표적으로:

- `PROC_AUTO_MONITOR`는 아직 stub 상태
- 렌더링 최적화 구조는 단순화되어 있음
- 설정값 범위 제한 / 저장 타이밍 / debounce는 보강 필요
- 자동 감지 핵심은 `impedence detected 재작성 .md` 쪽 설계를 결합해야 완성됨

즉 `code.md`는 "바로 제품에 넣는 최종본"이라기보다:

> 구조를 실제 코드로 옮겨보기 위한 레퍼런스 구현

으로 보는 것이 정확합니다.

---

## 12. 기존 구조와 새 구조 비교

### 기존

- 함수 중첩 구조
- `delay()` 중심 타이밍
- 메뉴별 중복 함수
- ADC 자동 감지 blocking
- UI / 용접 / 센서 / 출력 혼재

### 새 구조

- FSM 기반
- 입력 / UI / Process / Render 분리
- 타이머 기반 논블로킹
- 공통 메뉴 편집 구조
- zero-cross 동기 자동 감지
- baseline + feature + 연속 검출 + lockout

---
