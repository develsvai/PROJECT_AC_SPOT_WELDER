# 설계 목표

- **UI와 용접 제어를 분리**
- **blocking delay 제거**
- **메뉴/설정/입력 구조 단순화**
- **자동/수동 용접 공통화**
- **UML로 그렸을 때도 구조가 설명 가능하도록**

---

# 1. 전체 아키텍처

기존 코드는 거의 모든 게 한 흐름에 섞여 있었는데,

새 구조는 **4개 레이어**로 나누는 게 제일 좋습니다.

## 권장 구조

```
+----------------------+
|     Input Layer      |
| 버튼 / 엔코더 / ZC   |
+----------------------+
           |
           v
+----------------------+
|      UI FSM          |
| 화면 / 메뉴 / 편집   |
+----------------------+
           |
           v
+----------------------+
|   Process FSM        |
| Ready / Weld / Lock  |
+----------------------+
           |
           v
+----------------------+
|   Hardware Driver    |
| LCD / TRIAC / ADC    |
+----------------------+
```

즉,

- **입력층**: 이벤트만 생성
- **UI FSM**: 사용자가 뭘 선택하는지 관리
- **Process FSM**: 실제 용접 진행 관리
- **Driver**: 하드웨어 접근

---

# 2. 가장 중요한 분리

## UI FSM

담당:

- 기본 화면
- 메뉴 진입
- TIME/MULT/REST/AUTO 편집
- 저장/취소
- 상태 표시

## Process FSM

담당:

- 대기
- 자동 감지
- manual trigger
- zero-cross 대기
- pulse on/off
- rest
- done
- lockout

이 둘이 섞이면 다시 원래 코드처럼 됩니다.

---

# 3. 전체 UML 상위 구조

## 상위 상태도

```
[BOOT]
   |
   v
[UI_ACTIVE] <------------------------------+
   |                                       |
   | manual/auto weld request              |
   v                                       |
[PROCESS_ACTIVE]                           |
   |                                       |
   | process done                          |
   +---------------------------------------+
```

하지만 실제 구현은 이렇게 더 명확합니다.

```
ROOT
├─ BOOT
├─ UI
│  ├─ SPLASH
│  ├─ IDLE
│  ├─ MENU_MAIN
│  └─ MENU_EDIT
└─ PROCESS
   ├─ READY
   ├─ ARMED
   ├─ WAIT_ZC
   ├─ PULSE_ON
   ├─ REST
   ├─ DONE
   └─ LOCKOUT
```

---

# 4. UI FSM 재설계

## UI 상태 정의

```c
typedef enum {
    UI_SPLASH = 0,
    UI_IDLE,
    UI_MENU_MAIN,
    UI_MENU_EDIT
} UiState;
```

### 왜 이 정도만 두는가

기존처럼:

- TIME 상태
- MULT 상태
- REST 상태
- AUTO 상태

를 각각 따로 두면 메뉴가 늘어날수록 중복이 많아집니다.

대신:

- **MENU_MAIN**: 어떤 항목을 고를지
- **MENU_EDIT**: 고른 항목의 값을 수정

이렇게 통합합니다.

---

## 메뉴 항목 정의

```c
typedef enum {
    MENU_TIME = 0,
    MENU_MULT,
    MENU_REST,
    MENU_MODE,
    MENU_COUNT
} MenuItem;
```

---

## UI 상태 UML

```
+----------------+
|   UI_SPLASH    |
+----------------+
        |
        | splash timeout
        v
+----------------+
|    UI_IDLE     |
+----------------+
   |         |
   | enc/sw  | weld request
   v         |
+----------------+   process busy? no
|  UI_MENU_MAIN  |----------------------+
+----------------+                      |
   |    |    |                          |
   |    | enc press                     |
   |    v                               |
   |  +----------------+                |
   |  | UI_MENU_EDIT   |                |
   |  +----------------+                |
   |      |      |                      |
   |      |save  |cancel                |
   +------+------+
          |
          v
      UI_IDLE
```

---

# 5. UI 이벤트 정의

UI는 핀을 직접 읽지 말고 **이벤트**를 받아야 합니다.

## 이벤트 구조

```c
typedef struct {
    int8_t encoder_delta;   // -1, 0, +1
    uint8_t enc_press;      // 1 when pressed
    uint8_t manual_press;   // 1 when pressed
    uint8_t timeout_1ms;    // optional
} InputEvent;
```

---

# 6. UI 동작 규칙

## UI_IDLE

표시:

- Time / Mult / Rest / Mode
- Process 상태(Ready / Auto / Lockout / Welding)

동작:

- 엔코더 회전 → 메뉴 선택 인덱스 이동만 하거나 바로 MENU_MAIN 진입
- 엔코더 버튼 → MENU_MAIN 진입
- manual 버튼 → Process에 manual weld 요청
- auto mode면 Process가 자동 감지 수행

---

## UI_MENU_MAIN

표시:

- `> TIME`
- `MULT`
- `REST`
- `MODE`

동작:

- 엔코더 회전 → 항목 이동
- 엔코더 버튼 → MENU_EDIT 진입
- manual 버튼 또는 timeout → UI_IDLE 복귀

---

## UI_MENU_EDIT

표시 예:

- `SET TIME`
- `< 50 ms >`

동작:

- 엔코더 회전 → 값 변경
- 엔코더 버튼 → 저장 후 IDLE
- manual 버튼 → 취소 후 IDLE
- timeout → 취소 후 IDLE

---

# 7. UI 설계 핵심 포인트

## 포인트 1: 메뉴 상태를 일반화

TIME/MULT/REST/AUTO를 각각 별도 상태로 두지 않음.

```c
typedef struct {
    MenuItem selected_item;
    int16_t edit_value;
    uint8_t dirty;
} UiContext;
```

이렇게 하나의 편집기처럼 씀.

---

## 포인트 2: UI는 절대 용접 실행을 직접 제어하지 않음

예를 들어 UI는 이렇게만 말함:

- `process_request_manual_weld = 1`
- `settings.mode = AUTO`

실제 TRIAC on/off는 Process FSM이 전담.

---

# 8. Process FSM 재설계

이 부분이 실제 품질을 많이 좌우합니다.

## Process 상태 정의

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

---

## Process 상태 UML

```
+------------------+
|    PROC_READY    |
+------------------+
   |           |
   | auto on   | manual request
   v           v
+------------------+
| PROC_AUTO_MONITOR|
+------------------+
   | contact_detected
   v
+------------------+
|   PROC_WAIT_ZC   |
+------------------+
   | zero-cross
   v
+------------------+
|   PROC_PULSE_ON  |
+------------------+
   | pulse timer expired
   v
+------------------+
|    PROC_REST     |
+------------------+
   | more pulses? yes ---> PROC_WAIT_ZC
   | no
   v
+------------------+
|    PROC_DONE     |
+------------------+
   |
   v
+------------------+
|   PROC_LOCKOUT   |
+------------------+
   | timeout
   v
PROC_READY or PROC_AUTO_MONITOR
```

---

# 9. Process 상태 의미

## PROC_READY

- 아무 용접도 하지 않음
- 수동 버튼 기다림
- auto mode면 `PROC_AUTO_MONITOR`로 감

---

## PROC_AUTO_MONITOR

- ADC 기반 접촉 감지 수행
- 감지 성공하면 `PROC_WAIT_ZC`
- menu 진입, mode off, cancel 등으로 빠져나올 수 있음

---

## PROC_WAIT_ZC

- zero-cross 이벤트 대기
- 다음 반주기 시작점에서 펄스 시작

---

## PROC_PULSE_ON

- TRIAC on
- pulse timer 감소
- timer 0되면 TRIAC off
- 남은 pulse 수 있으면 REST로
- 없으면 DONE으로

---

## PROC_REST

- mult weld일 때 휴지기
- rest 끝나면 WAIT_ZC

---

## PROC_DONE

- 짧은 완료 상태
- beep 요청 가능
- lockout 진입

---

## PROC_LOCKOUT

- auto spot 재발사 방지
- 일정 시간 감지 무시
- 끝나면 READY 또는 AUTO_MONITOR

---

# 10. 가장 좋은 입력 처리 방식

지금 코드의 문제는 버튼을 여기저기서 직접 읽는 겁니다.

새 구조에서는 **입력 스캐너가 이벤트를 큐처럼 만들어야** 합니다.

## 입력 스캐너 역할

```
핀 읽기
→ 디바운싱
→ 엔코더 방향 판별
→ 버튼 눌림 이벤트 생성
→ UI/Process에 전달
```

---

## 입력 스캐너 UML 느낌

```
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

---

# 11. 렌더링도 분리

기존은 상태 안에서 LCD를 직접 만졌는데,

지금은 **렌더러 분리**가 좋습니다.

## 권장 구조

```c
void ui_update(void);
void process_update(void);
void render_update(void);
```

### render_update 역할

- 현재 `ui_state`, `process_state`, `settings`를 보고 화면 문자열 생성
- 이전 버퍼와 비교해서 바뀐 줄만 업데이트

---

## 렌더링 UML 개념

```
(UI state + Process state + Settings)
                |
                v
         [Render Model]
                |
                v
           [LCD Driver]
```

---

# 12. 화면 설계 예시

## UI_IDLE 화면

```
T:050 M:02 R:100
AUTO READY
```

또는 용접 중이면:

```
T:050 M:02 R:100
WELD 1/2
```

---

## UI_MENU_MAIN 화면

```
> TIME
  MULT
```

두 줄 LCD면 페이지 방식으로 돌려도 됩니다.

---

## UI_MENU_EDIT 화면

```
SET TIME
< 050 ms >
```

---

# 13. 설정 모델 구조

기존 EEPROM 값은 그대로 유지하되 구조화합니다.

```c
typedef struct {
    uint16_t time_ms;
    uint8_t  multiplier;
    uint16_t rest_ms;
    uint8_t  auto_mode;
} Settings;
```

---

# 14. 전체 컨텍스트 구조 예시

```c
typedef struct {
    UiState ui_state;
    MenuItem menu_item;
    int16_t edit_value;
    uint16_t ui_timeout_ms;
} UiContext;

typedef struct {
    ProcessState state;
    uint8_t pulses_remaining;
    uint16_t pulse_timer_ms;
    uint16_t rest_timer_ms;
    uint16_t lockout_timer_ms;
    uint8_t manual_request;
    uint8_t auto_trigger_request;
} ProcessContext;
```

---

# 15. 메인 루프 구조

가장 중요한 건 이 구조입니다.

## 권장 main loop

```c
while (1) {
    input_scan_task();      // 이벤트 생성
    auto_detect_task();     // ADC feature 처리
    process_fsm_task();     // 용접 상태 진행
    ui_fsm_task();          // 메뉴/화면 상태 진행
    render_task();          // LCD 출력
}
```

이게 핵심입니다.

---

# 16. 상태 전이 테이블식으로 보면 더 명확함

## UI FSM 전이표

| 현재 상태 | 이벤트 | 다음 상태 | 액션 |
| --- | --- | --- | --- |
| UI_SPLASH | splash timeout | UI_IDLE | none |
| UI_IDLE | enc_press | UI_MENU_MAIN | menu index init |
| UI_IDLE | manual_press | UI_IDLE | process manual request |
| UI_MENU_MAIN | encoder_delta | UI_MENU_MAIN | menu move |
| UI_MENU_MAIN | enc_press | UI_MENU_EDIT | edit value load |
| UI_MENU_MAIN | timeout/cancel | UI_IDLE | none |
| UI_MENU_EDIT | encoder_delta | UI_MENU_EDIT | value change |
| UI_MENU_EDIT | enc_press | UI_IDLE | save |
| UI_MENU_EDIT | cancel/timeout | UI_IDLE | discard |

---

## Process FSM 전이표

| 현재 상태 | 이벤트 | 다음 상태 | 액션 |
| --- | --- | --- | --- |
| PROC_READY | auto_mode on | PROC_AUTO_MONITOR | none |
| PROC_READY | manual_request | PROC_WAIT_ZC | pulse count init |
| PROC_AUTO_MONITOR | contact detected | PROC_WAIT_ZC | pulse count init |
| PROC_WAIT_ZC | zero-cross | PROC_PULSE_ON | triac on |
| PROC_PULSE_ON | pulse timeout | PROC_REST or PROC_DONE | triac off |
| PROC_REST | rest timeout | PROC_WAIT_ZC | none |
| PROC_DONE | immediate | PROC_LOCKOUT | beep |
| PROC_LOCKOUT | lockout timeout | PROC_READY or PROC_AUTO_MONITOR | none |

---

# 17. 기존 구조와 비교한 개선 포인트

## 기존

- 메뉴마다 별도 함수
- 내부 while(1)
- delay 다수
- menu와 weld 결합
- LCD 출력이 상태에 박혀 있음

## 개선

- 공통 메뉴 editor
- 단일 UI FSM
- 단일 Process FSM
- non-blocking
- renderer 분리
- 이벤트 기반

---

# 18. UML 관점에서 제일 예쁜 구조

가장 추천하는 UML은 **병렬 상태(orthogonal regions)** 느낌입니다.

즉 논리적으로는 이렇게 볼 수 있습니다.

```
SYSTEM
├─ UI REGION
│  ├─ SPLASH
│  ├─ IDLE
│  ├─ MENU_MAIN
│  └─ MENU_EDIT
└─ PROCESS REGION
   ├─ READY
   ├─ AUTO_MONITOR
   ├─ WAIT_ZC
   ├─ PULSE_ON
   ├─ REST
   ├─ DONE
   └─ LOCKOUT
```

이렇게 하면 “UI는 메뉴 중인데 process는 lockout 상태” 같은 것도 표현 가능합니다.

실제 시스템은 이런 병렬성이 더 맞습니다.

---

# 19. 지금 기준으로 가장 현실적인 구현 순서

완전히 갈아엎지 않고 가려면 순서를 이렇게 가는 게 좋습니다.

## 1단계

`Settings`, `UiContext`, `ProcessContext` 구조체 분리

## 2단계

`spottimeset`, `spotmult`, `spotrest`, `auto_spot` 제거

→ `UI_MENU_MAIN`, `UI_MENU_EDIT`로 통합

## 3단계

`spot_action()` 쪼개서 Process FSM로 이동

## 4단계

모든 `_delay_ms()` 제거

→ 타이머 카운터 기반으로 변경

## 5단계

LCD 렌더링 별도 함수 분리

---

# 20. 내가 추천하는 최종 설계 한 줄 요약

> **상태를 “화면 상태”와 “용접 진행 상태”로 분리하고, 입력은 이벤트화하며, 메뉴는 공통 편집기로 일반화하고, 모든 시간 처리는 논블로킹 타이머로 바꾸는 구조가 가장 좋습니다.**
> 

---

# 21. 텍스트 UML 최종본

아래처럼 문서에 바로 넣어도 됩니다.

## UI FSM UML

```
[UI_SPLASH]
    --(startup timeout)-->
[UI_IDLE]
    --(encoder press)-->
[UI_MENU_MAIN]
    --(encoder rotate)--> [UI_MENU_MAIN]
    --(encoder press)-->
[UI_MENU_EDIT]
    --(encoder rotate)--> [UI_MENU_EDIT]
    --(save)--> [UI_IDLE]
    --(cancel/timeout)--> [UI_IDLE]
```

## PROCESS FSM UML

```
[PROC_READY]
    --(auto enabled)--> [PROC_AUTO_MONITOR]
    --(manual request)--> [PROC_WAIT_ZC]

[PROC_AUTO_MONITOR]
    --(contact detected)--> [PROC_WAIT_ZC]
    --(auto disabled)--> [PROC_READY]

[PROC_WAIT_ZC]
    --(zero-cross event)--> [PROC_PULSE_ON]

[PROC_PULSE_ON]
    --(pulse timer expired && more pulses)--> [PROC_REST]
    --(pulse timer expired && done)--> [PROC_DONE]

[PROC_REST]
    --(rest timer expired)--> [PROC_WAIT_ZC]

[PROC_DONE]
    --> [PROC_LOCKOUT]

[PROC_LOCKOUT]
    --(lockout expired && auto enabled)--> [PROC_AUTO_MONITOR]
    --(lockout expired && auto disabled)--> [PROC_READY]
```

---

원하면 다음 단계로 이 설계를 바탕으로 **실제 C enum/구조체/전이 코드 뼈대까지** 바로 짜드리겠습니다.