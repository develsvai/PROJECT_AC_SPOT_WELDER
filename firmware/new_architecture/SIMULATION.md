# New Architecture Simulation Structure

이 문서는 `firmware/new_architecture/sim/` 과
`firmware/new_architecture/sim_ui/` 가 어떻게 연결되어 있는지 설명합니다.

핵심은:

- 펌웨어 코어는 실제 `project/ACSpotWelderNewArch/` 소스를 그대로 사용하고
- 하드웨어만 시뮬레이터용 구현으로 바꿔서
- CLI와 Web UI가 같은 코어를 호출한다는 점입니다.

## 전체 흐름

```text
Browser UI
   |
   v
server.py
   |
   v
libacspot_sim.dylib (C API)
   |
   v
sim_api.c
   |
   v
app.c + ui.c + process.c + auto_detect.c + render.c
   |
   v
hardware_sim.c / settings_sim.c / render_sim.c
```

즉 웹이 별도 JS 상태머신을 갖는 구조가 아니라, 실제 C 시뮬레이터 상태를 받아서
화면에 표시하는 구조입니다.

## 시뮬레이션 폴더 구성

```text
sim/
├── Makefile
├── README.md
├── sim_main.c
├── sim_api.c / sim_api.h
├── hardware_sim.c / hardware_sim.h
├── settings_sim.c / settings_sim.h
├── render_sim.c / render_sim.h
├── acspot_sim
└── libacspot_sim.dylib

sim_ui/
├── index.html
├── server.py
└── README.md
```

## `sim/` 역할

### `Makefile`

- CLI 실행 파일과 공유 라이브러리를 빌드
- 출력:
  - `acspot_sim`
  - `libacspot_sim.dylib`

### `sim_main.c`

- 콘솔 시뮬레이터 진입점
- 기본 시나리오:
  - manual weld scenario
  - auto detect scenario

상태 전이를 텍스트 로그로 빠르게 검증하는 용도입니다.

### `sim_api.c`, `sim_api.h`

- 시뮬레이터를 외부에서 호출하기 위한 C API
- Web UI와 Python 서버가 이 계층을 사용함

주요 기능:
- 초기화
- 시간 진행
- manual input
- contact on/off
- touch pulse
- 설정 적용
- 현재 상태 snapshot 반환

### `hardware_sim.c`, `hardware_sim.h`

- 실제 AVR 하드웨어 대신 동작하는 가짜 하드웨어
- 제공 기능:
  - virtual time
  - 1ms tick
  - zero-cross 이벤트
  - ADC 파형 생성
  - manual/encoder 입력 포트 상태
  - TRIAC/buzzer/sense 상태 저장

즉, `hardware.c` 의 PC용 대체 구현입니다.

### `settings_sim.c`, `settings_sim.h`

- EEPROM 대신 기본 설정값을 메모리에서 제공

### `render_sim.c`, `render_sim.h`

- 실제 LCD 대신 line0/line1 문자열을 저장
- Web UI와 CLI가 현재 LCD 내용을 읽을 수 있게 해줌

## `sim_ui/` 역할

### `server.py`

- Python HTTP 서버
- `libacspot_sim.dylib` 를 ctypes로 로드
- `/api/state` 와 `/api/action` 제공
- C 시뮬레이터 호출 시 lock을 사용해 동시 요청 꼬임 방지

즉, 브라우저와 C 시뮬레이터 사이의 브리지입니다.

### `index.html`

- 브라우저용 UI
- 역할:
  - 현재 LCD 표시
  - 현재 상태 카드 표시
  - 설정값 입력
  - manual/touch/reset/warm-up 조작
  - 이벤트 로그와 상태 설명 표시

중요한 점:
- 이 파일은 "가짜 펌웨어 로직"을 갖지 않음
- 상태는 전부 `/api/state` 에서 읽어옴

## 상태가 전달되는 방식

`server.py` 는 `sim_api_get_snapshot()` 으로 아래 항목들을 읽습니다.

- 현재 시간
- 설정값
- UI 상태
- Process 상태
- Auto Detect 상태
- pulse count
- weld count
- contact / triac / buzzer 상태
- LCD 2줄 문자열

브라우저는 이 snapshot을 받아 그대로 렌더링합니다.

## 조작이 전달되는 방식

### 웹 버튼

- `Manual Weld`
- `Touch Pulse`
- `Contact On/Off`
- `Apply Settings`
- `Warm Up 1s`
- `Reset`

### API 호출

웹 버튼은 `/api/action?...` 으로 서버에 전달됩니다.

예:

- `manual_press`
- `touch_pulse`
- `contact_toggle`
- `set_settings`
- `step`
- `reset`

### C 시뮬레이터 호출

서버는 이 요청을 받아 `sim_api.c` 함수를 호출합니다.

예:

- `sim_api_manual_cycle()`
- `sim_api_touch_pulse()`
- `sim_api_set_settings()`
- `sim_api_step_ms()`

## 실제 펌웨어와 무엇이 같고 다른가

### 같은 것

- `app.c`
- `ui.c`
- `process.c`
- `auto_detect.c`
- `render.c`
- `types.h`
- 설정 구조와 상태머신 구조

즉, 핵심 FSM과 로직은 실제 펌웨어와 같습니다.

### 다른 것

- AVR 레지스터 직접 접근 대신 `hardware_sim.c`
- EEPROM 대신 `settings_sim.c`
- LCD 대신 `render_sim.c`
- 웹용 편의 입력:
  - `Touch Pulse`
  - 직접 설정 적용
  - warm-up step

즉:

> 코어 로직은 실제 펌웨어와 같고,
> 주변 하드웨어와 조작 UI만 시뮬레이터용으로 바뀐 구조입니다.

## 추천 사용 순서

### 수동 용접 확인

1. `MANUAL` 모드
2. `Manual Weld`
3. `Pulse Count`, `Weld Count`, `Spot Progress`, LCD 확인

### 자동 용접 확인

1. `AUTO` 모드
2. `Warm Up 1s`
3. `Touch Pulse`
4. `Spot Progress`, `Event Log`, `Weld Count` 확인

## 함께 보면 좋은 문서

- `firmware/new_architecture/README.md`
- `docs/new_architecture/new_architecture.md`
- `docs/project_overview.md`
