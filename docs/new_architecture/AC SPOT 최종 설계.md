# 1. 전체 재설계 목표

## 예전 구조

- 메뉴마다 별도 함수
- `while(1)` 중첩
- `delay()`로 시간 맞춤
- UI / 용접 / 센서 / 출력이 서로 섞임
- 자동 감지 로직이 blocking
- 상태는 flag와 break로 암묵적으로 표현

## 재설계 목표

- **UI FSM**과 **Process FSM** 분리
- 시간 처리는 **논블로킹 타이머 기반**
- 자동 감지는 **zero-cross 동기 feature 기반**
- 메뉴는 **공통 편집 구조**로 일반화
- 렌더링은 **상태와 분리**
- 구조를 UML로 설명할 수 있게 만듦

즉 한 줄로 말하면:

> **“되게 만든 코드”에서 “설명 가능한 구조의 코드”로 바꾸는 것**
> 

---

# 2. 최상위 설계 구조

전체 시스템은 4층으로 나누는 게 가장 적절합니다.

```
Input Layer
 ├─ 엔코더 회전
 ├─ 엔코더 버튼
 ├─ 수동 버튼
 └─ Zero-cross 인터럽트

UI FSM
 ├─ Splash
 ├─ Idle
 ├─ Menu Main
 └─ Menu Edit

Process FSM
 ├─ Ready
 ├─ Auto Monitor
 ├─ Wait ZC
 ├─ Pulse On
 ├─ Rest
 ├─ Done
 └─ Lockout

Driver Layer
 ├─ LCD
 ├─ TRIAC
 ├─ Sense Enable
 ├─ Buzzer
 └─ ADC
```

이 구조가 중요한 이유는 역할이 분리되기 때문입니다.

- 입력층은 **이벤트 생성만**
- UI는 **화면/설정만**
- Process는 **용접 동작만**
- Driver는 **하드웨어 제어만**

---

# 3. 상태머신 재설계

## 3-1. UI FSM

UI는 화면과 메뉴만 담당합니다.

### 상태

- `UI_SPLASH`
- `UI_IDLE`
- `UI_MENU_MAIN`
- `UI_MENU_EDIT`

### 의미

### `UI_SPLASH`

- 부팅 직후 시작 화면
- 일정 시간 후 `UI_IDLE`

### `UI_IDLE`

- 기본 상태
- 현재 설정값 표시
- Process 상태 표시
- 엔코더 누르면 메뉴 진입
- 수동 버튼 누르면 manual weld 요청

### `UI_MENU_MAIN`

- TIME / MULT / REST / MODE 중 하나 선택
- 엔코더 회전으로 항목 이동
- 엔코더 버튼으로 편집 진입
- timeout 또는 cancel 입력 시 `UI_IDLE`

### `UI_MENU_EDIT`

- 선택한 항목의 값 수정
- 엔코더 회전으로 값 변경
- 버튼 누르면 저장
- 수동 버튼 또는 timeout이면 취소

---

## 3-2. Process FSM

용접 동작은 이 FSM이 전담합니다.

### 상태

- `PROC_READY`
- `PROC_AUTO_MONITOR`
- `PROC_WAIT_ZC`
- `PROC_PULSE_ON`
- `PROC_REST`
- `PROC_DONE`
- `PROC_LOCKOUT`

### 의미

### `PROC_READY`

- 용접 대기 상태
- manual request 또는 auto mode에 따라 전이

### `PROC_AUTO_MONITOR`

- 자동 감지 모드
- ADC feature 기반 접촉 감지 수행
- 감지 성공 시 용접 시퀀스 시작

### `PROC_WAIT_ZC`

- zero-cross 이벤트 대기
- 반주기 시작점에 펄스를 안정적으로 시작

### `PROC_PULSE_ON`

- TRIAC on
- 설정 시간만큼 펄스 유지

### `PROC_REST`

- 멀티 펄스일 때 휴지 시간

### `PROC_DONE`

- 펄스 완료
- 짧은 완료 상태
- 부저/완료 표시 가능

### `PROC_LOCKOUT`

- 연속 재트리거 방지
- 자동감지 재진입 전에 잠깐 무시 시간

---

# 4. UML 기준 최종 상태 흐름

## UI FSM 흐름

```
[UI_SPLASH]
    -> timeout
[UI_IDLE]
    -> encoder press -> [UI_MENU_MAIN]
    -> manual press -> process manual request
[UI_MENU_MAIN]
    -> encoder rotate -> 항목 이동
    -> encoder press -> [UI_MENU_EDIT]
    -> cancel/timeout -> [UI_IDLE]
[UI_MENU_EDIT]
    -> encoder rotate -> 값 수정
    -> save -> [UI_IDLE]
    -> cancel/timeout -> [UI_IDLE]
```

## Process FSM 흐름

```
[PROC_READY]
    -> auto mode on -> [PROC_AUTO_MONITOR]
    -> manual request -> [PROC_WAIT_ZC]

[PROC_AUTO_MONITOR]
    -> contact detected -> [PROC_WAIT_ZC]
    -> auto off -> [PROC_READY]

[PROC_WAIT_ZC]
    -> zero-cross -> [PROC_PULSE_ON]

[PROC_PULSE_ON]
    -> pulse time end && more pulses -> [PROC_REST]
    -> pulse time end && no more pulses -> [PROC_DONE]

[PROC_REST]
    -> rest time end -> [PROC_WAIT_ZC]

[PROC_DONE]
    -> [PROC_LOCKOUT]

[PROC_LOCKOUT]
    -> timeout -> [PROC_READY] or [PROC_AUTO_MONITOR]
```

---

# 5. UI 설계 원칙

예전 코드와 달리, UI는 개별 함수 기반이 아니라 **일반화된 편집기 구조**로 갑니다.

## 메뉴 항목

- `MENU_TIME`
- `MENU_MULT`
- `MENU_REST`
- `MENU_MODE`

## 핵심 포인트

예전처럼

- `spottimeset()`
- `spotmult()`
- `spotrest()`
- `auto_spot()`

를 따로 두지 않습니다.

대신

- `MENU_MAIN`에서 항목 선택
- `MENU_EDIT`에서 값 수정

이렇게 단일 흐름으로 통합합니다.

## 장점

- 중복 제거
- 설정 항목 추가 쉬움
- UX 일관성 증가
- 상태 수 감소

---

# 6. 렌더링 구조

렌더링은 상태머신과 분리합니다.

## 왜 분리하는가

예전 코드는 상태 안에서 LCD를 직접 찍고 있었기 때문에

- 로직이 복잡해지고
- 화면 변경과 상태 전이가 섞이고
- 나중에 UI 수정이 어렵습니다.

## 새 구조

- `ui_task()`는 상태만 바꿈
- `process_task()`는 동작 상태만 바꿈
- `render_task()`가 최종적으로 화면을 그림

## 렌더링 예시

### Idle

```
T:050 M:02 R:100
AUTO READY
```

### Menu Main

```
> TIME
PRESS TO EDIT
```

### Menu Edit

```
SET TIME
< 050 ms >
```

### 용접 중

```
T:019 M:01 R:200
AUTO WELD
```

---

# 7. 자동 감지 로직 재설계

이 부분이 예전과 가장 큰 차이입니다.

예전에는:

- blocking 1000샘플
- 평균 / max / min / 유사 RMS
- 고정 윈도우 판정

이런 식이었다면,

지금은 아래 구조가 가장 안정적입니다.

---

## 7-1. 자동 감지 목표

> **AC 1차측 ADC 신호만으로 2차측 접촉(니켈 접촉)을 안정적으로 감지**
> 

조건:

- AVR에서도 돌아가야 함
- zero-cross 사용 가능
- 하드웨어 필터 거의 없음
- 노이즈 심함
- delay 없이 동작

---

## 7-2. 핵심 아이디어

### 1) zero-cross 동기화

반주기 기준으로 샘플을 모읍니다.

### 2) feature 추출

각 샘플에 대해

```c
abs(sample - center)
```

를 계산하고 누적합니다.

즉 feature는 사실상:

> **반주기 평균 절대편차**
> 

입니다.

### 3) baseline 유지

평소 반주기 feature를 느리게 학습해서 baseline으로 둡니다.

### 4) 접촉 판정

현재 feature가 baseline보다 충분히 크면 접촉 후보로 봅니다.

예:

```
feature > baseline * 1.15
AND
feature > baseline + 4
```

### 5) 연속 검증

한 번 튀는 값이 아니라 **연속 N회** 조건 만족해야 확정

### 6) lockout

용접 직후 재검출 방지

---

## 7-3. 자동 감지 상태

자동 감지 내부적으로는 이렇게 볼 수 있습니다.

```
AUTO_IDLE
AUTO_CANDIDATE
AUTO_TRIGGERED
AUTO_LOCKOUT
```

실제로는 Process FSM의 `PROC_AUTO_MONITOR` 안에서 운영해도 되고,

독립 모듈로 둬도 됩니다.

---

# 8. 코드 구조 총정리

## 8-1. 핵심 컨텍스트

### Settings

```c
typedef struct {
    uint16_t time_ms;
    uint8_t  multiplier;
    uint16_t rest_ms;
    WeldMode mode;
} Settings;
```

### UI

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

### Process

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

### Input

```c
typedef struct {
    int8_t encoder_delta;
    bool enc_pressed;
    bool manual_pressed;
} InputEvent;
```

---

## 8-2. 메인 루프 구조

최종 구조는 이 흐름입니다.

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

이 구조의 장점:

- 모든 로직이 **짧게 실행되고 즉시 복귀**
- 무한루프 중첩 없음
- delay 없음
- 상태도와 코드가 대응됨

---

# 9. 예전 구조와 비교한 개선 요약

## 예전

- 함수 중첩 구조
- `while(1)` 다수
- `delay()`로 타이밍 봉합
- 메뉴별 중복 코드 많음
- ADC 자동 감지 blocking
- UI와 용접 로직 혼재

## 새 구조

- FSM 기반
- 입력 / UI / Process / Render 분리
- 타이머 기반 논블로킹
- 공통 메뉴 에디터
- zero-cross 동기 센서 처리
- baseline + feature + 연속검출

---

# 10. 지금 기준에서 가장 좋은 최종 형태

이 프로젝트를 다시 정리하면 기술적으로는 이렇게 표현할 수 있습니다.

> **ATmega16 기반 AC spot welder controller를 UI FSM / Process FSM 기반의 논블로킹 구조로 재설계하고, zero-cross 동기 반주기 ADC feature 추출을 통해 1차측 신호만으로 2차측 접촉을 자동 감지하도록 개선**
> 

조금 더 쉽게 말하면:

- UI는 메뉴만 담당
- Process는 용접만 담당
- 센서는 feature 기반으로 자동 감지
- 모든 시간은 상태+타이머로 처리

---

# 11. 앞으로 붙이면 좋은 것

## 우선순위 1

`auto_detect_contact()` 실제 구현 붙이기

- half-cycle feature
- adaptive baseline
- ratio + diff threshold
- consecutive detect

## 우선순위 2

`render_task()` UX 개선

- 저장 시 `SAVED`
- 메뉴 스크롤
- 현재 Process 상태 더 명확히 표시

## 우선순위 3

입력 이벤트 큐화

- 엔코더 빠르게 돌릴 때 안정성 증가

## 우선순위 4

부저 FSM 분리

- 완료음 / 오류음 구분 가능

---