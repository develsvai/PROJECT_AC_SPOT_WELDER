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
    bool enc_pressed;
    bool manual_pressed;
} InputEvent;
```

입력 스캐너는 하드웨어 포트 상태를 읽고, 이를 UI/Process가 이해할 수 있는 이벤트로 바꿉니다.

### 입력 처리 단계

```text
[RAW PIN READ]
    |
    v
[EDGE DETECT]
    |
    v
[EVENT GENERATE]
```

즉 입력층은 아래 역할을 담당합니다.

- 핀 읽기
- 엔코더 방향 판별
- 버튼 에지 감지
- `InputEvent` 생성
- UI / Process 에 이벤트 전달

현재 구현은 `input.c` 에서 이 역할을 수행합니다. 하드웨어 디바운스가 더 필요하면
이 계층에서 확장하는 것이 가장 자연스럽습니다.

---

## 7. 구현 코드 구조

현재 구현은 `firmware/new_architecture/project/ACSpotWelderNewArch/` 기준으로 아래처럼 구성됩니다.

```text
project/
└── ACSpotWelderNewArch/
    ├── main.c
    ├── app.c / app.h
    ├── app_config.h
    ├── types.h
    ├── hardware.c / hardware.h
    ├── input.c / input.h
    ├── ui.c / ui.h
    ├── process.c / process.h
    ├── auto_detect.c / auto_detect.h
    ├── render.c / render.h
    ├── settings.c / settings.h
    └── lcd_i2c.h
```

### 파일별 역할

- `main.c`
  - 펌웨어 진입점
  - `app_init()` 호출 후 `app_run()` 반복

- `app.c`, `app.h`
  - 전체 앱 루프와 모듈 연결
  - 하드웨어 이벤트 소비, UI FSM, Process FSM, Render 연결

- `app_config.h`
  - splash, lockout, beep, auto detect threshold 같은 상수 정의

- `types.h`
  - enum / context / settings 구조를 한 곳에 모은 공용 정의 파일

- `hardware.c`, `hardware.h`
  - AVR 실제 하드웨어 접근층
  - tick, zero-cross, ADC, 입력 포트, TRIAC, buzzer, sense 담당

- `input.c`, `input.h`
  - 포트 입력을 `InputEvent` 로 변환
  - encoder/manual switch edge 감지 담당

- `ui.c`, `ui.h`
  - UI FSM 전담
  - 메뉴 이동, 편집, timeout, 저장 요청 담당

- `process.c`, `process.h`
  - 용접 프로세스 FSM 전담
  - WAIT_ZC / PULSE / REST / LOCKOUT 같은 실제 시퀀스 담당

- `auto_detect.c`, `auto_detect.h`
  - 반주기 동기 자동 감지
  - ADC 샘플 누적, baseline 관리, trigger request 생성

- `render.c`, `render.h`
  - LCD 문자열 생성과 갱신

- `settings.c`, `settings.h`
  - EEPROM 설정 로드/저장

- `lcd_i2c.h`
  - LCD 드라이버 추상화 헤더

---

## 8. 실행 흐름

실제 루프는 아래 흐름으로 이해하면 됩니다.

```text
main.c
  -> app_init()
  -> app_run() 반복

app_run()
  -> hardware 이벤트 처리
  -> input_update()
  -> ui_tick()
  -> process_tick_1ms()
  -> auto detect와 process 연결
  -> render_tick()
```

핵심은 `app.c` 가 루트 컨트롤러가 되어 각 모듈을 순서 있게 호출한다는 점입니다.

---

## 9. 상태머신 관계

세 모듈의 관계는 아래처럼 볼 수 있습니다.

```text
Input -> UI FSM -> Settings
Input/ADC -> Auto Detect -> Process FSM
Settings -> Process FSM
UI + Process + Settings -> Render
```

정리하면:

- UI FSM은 화면과 설정을 담당
- Process FSM은 실제 용접 단계를 담당
- Auto Detect는 AUTO 모드에서 Process FSM을 트리거
- Render는 현재 상태를 LCD로 표현

---

## 10. 레거시와의 차이

- 레거시는 UI/입력/용접/출력이 더 많이 섞여 있음
- new_architecture는 상태머신과 레이어로 분리됨
- 동일 코어를 시뮬레이터와 웹 UI에서 재사용 가능함

---

## 11. 렌더링 분리

기존 코드는 상태 로직 안에서 LCD를 직접 갱신하는 흐름이 강했지만, 새 구조에서는
렌더링을 별도 단계로 분리합니다.

### 렌더링 원칙

- `ui.c` 는 화면 상태만 관리
- `process.c` 는 동작 상태만 관리
- `render.c` 가 현재 상태를 바탕으로 LCD 문자열을 생성

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
T:050 M:01 R:200
WELDING
```

---

## 12. 자동 감지 구조

자동 감지는 기존 구조와 가장 다르게 바뀌는 부분입니다.

목표는 아래와 같습니다.

> AC 1차측 ADC 신호만으로 2차측 접촉을 최대한 안정적으로 검출하고,
> 오검출 없이 자동 스팟을 거는 것

### 설계 조건

- AVR급 MCU에서 동작 가능해야 함
- zero-cross 사용 가능
- 하드웨어 필터가 거의 없다고 가정
- AC 노이즈가 심함
- 무거운 연산은 피해야 함
- `delay()` 없이 상태 기반으로 동작해야 함

### 핵심 아이디어

1. zero-cross 기준으로 반주기 동기화
2. 반주기 동안 ADC 여러 개 샘플링
3. 각 샘플에 대해 `abs(sample - baseline)` 누적
4. 반주기 feature 생성
5. 최근 반주기 feature의 baseline 유지
6. 현재 feature가 baseline 대비 충분히 커지면 접촉 후보
7. 연속 N회 유지되면 접촉 확정
8. 용접 후 lockout으로 재트리거 방지

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
AUTO_LOCKOUT
```

현재 구현도 이 기본 구조를 따릅니다.

---

## 13. 메인 루프 기준

권장하는 메인 루프 개념은 아래와 같습니다.

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

실제 구현은 `app.c` 안에서 이 개념을 `app_run()` 형태로 정리해 둔 구조입니다.

장점은 아래와 같습니다.

- 모든 로직이 짧게 실행되고 즉시 복귀
- 중첩 `while(1)` 제거
- `delay()` 제거
- 상태도와 코드 대응이 쉬움

---

## 14. 함께 보면 좋은 문서

- `docs/project_overview.md`
- `firmware/new_architecture/SIMULATION.md`
