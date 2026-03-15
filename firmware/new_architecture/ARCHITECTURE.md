# New Architecture Firmware Structure

이 문서는 `firmware/new_architecture/project/ACSpotWelderNewArch/` 코드베이스가
어떤 레이어로 나뉘고, 각 파일이 어떤 책임을 가지는지 정리한 문서입니다.

## 전체 구조

펌웨어는 크게 4개 레이어로 구성됩니다.

```text
Input / Hardware events
        |
        v
      UI FSM
        |
        v
    Process FSM
        |
        v
Render / Driver / Settings
```

핵심 아이디어는 `UI`, `용접 시퀀스`, `자동 감지`, `하드웨어 접근`, `렌더링`을
서로 직접 섞지 않고 `app.c`에서 연결하는 것입니다.

## 파일 트리

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

## 파일별 역할

### `main.c`

- 펌웨어 진입점
- `app_init()` 호출 후 `while(1)`에서 `app_run()` 반복

### `app.c`, `app.h`

- 전체 앱의 루트 컨트롤러
- 하드웨어 이벤트 소비, 입력 처리, UI FSM, Process FSM, Render를 순서대로 연결
- 시뮬레이터에서도 같은 파일이 그대로 사용됨
- 시뮬레이터 편의를 위한 입력 주입 함수도 제공

이 파일이 사실상 "상태머신 오케스트레이터"입니다.

### `app_config.h`

- 타이밍 상수와 한계값 정의
- 예:
  - splash 시간
  - lockout 시간
  - beep 시간
  - time/mult/rest 허용 범위
  - 자동 감지 threshold 관련 상수

### `types.h`

- 프로젝트 전체에서 공유하는 enum/struct 정의
- 포함 내용:
  - `WeldMode`
  - `UiState`
  - `MenuItem`
  - `ProcessState`
  - `AutoState`
  - `Settings`
  - `InputEvent`
  - `UiContext`
  - `ProcessContext`
  - `AutoDetectContext`
  - `AppContext`

새 아키텍처의 상태 정의를 한 곳에 모아둔 파일입니다.

### `hardware.c`, `hardware.h`

- 실제 AVR 하드웨어 접근층
- 역할:
  - 1ms tick
  - zero-cross 이벤트
  - ADC 샘플 읽기
  - encoder/manual switch 포트 읽기
  - TRIAC, buzzer, sense 출력

상위 로직은 이 파일 안의 실제 레지스터/포트 구현을 직접 알지 않습니다.

### `input.c`, `input.h`

- 하드웨어 포트 값을 `입력 이벤트`로 변환
- 역할:
  - rotary encoder delta 계산
  - encoder switch edge 감지
  - manual switch edge 감지

즉, "핀 값"을 "의미 있는 입력"으로 바꾸는 스캐너입니다.

### `ui.c`, `ui.h`

- UI FSM 전담
- 상태:
  - `UI_SPLASH`
  - `UI_IDLE`
  - `UI_MENU_MAIN`
  - `UI_MENU_EDIT`

역할:
- 메뉴 진입/이동/편집
- 설정값 수정
- timeout 처리
- 저장 요청 여부 반환

중요한 점은 UI가 직접 TRIAC를 켜지 않는다는 것입니다.

### `process.c`, `process.h`

- 실제 용접 시퀀스를 담당하는 Process FSM
- 상태:
  - `PROC_READY`
  - `PROC_AUTO_MONITOR`
  - `PROC_WAIT_ZC`
  - `PROC_PULSE_ON`
  - `PROC_REST`
  - `PROC_DONE`
  - `PROC_LOCKOUT`

역할:
- manual request 처리
- auto trigger request 처리
- zero-cross에 맞춰 TRIAC on
- pulse/rest/lockout 타이머 관리
- 완료 후 buzzer 처리

이 파일이 실제 용접 타이밍 로직의 중심입니다.

### `auto_detect.c`, `auto_detect.h`

- 자동 접촉 감지 전담 모듈
- zero-cross에 동기화된 반주기 feature를 계산
- feature와 baseline 비교로 접촉 여부 판단
- 상태:
  - `AUTO_IDLE`
  - `AUTO_CANDIDATE`
  - `AUTO_LOCKOUT`

역할:
- ADC 샘플 누적
- half-cycle feature 계산
- baseline 업데이트
- trigger request 생성

즉, "자동 모드에서 언제 용접을 시작할지"를 결정하는 감지기입니다.

### `render.c`, `render.h`

- LCD 출력 전담
- 현재 `UiState`, `ProcessState`, `Settings`를 보고 16x2 LCD 문자열 생성
- 이전 문자열과 비교해서 바뀐 경우만 LCD에 반영

상태 로직과 표시 로직을 분리하는 역할을 합니다.

### `settings.c`, `settings.h`

- 설정값 로드/저장
- EEPROM 또는 시뮬레이터 기본값 레이어와 연결

### `lcd_i2c.h`

- LCD 드라이버 추상화 헤더
- 실제 AVR 빌드에서 I2C LCD 제어에 사용

## 실행 흐름

부팅 후 반복 흐름은 아래와 같습니다.

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

## 상태머신 관계

### UI FSM

- 화면과 메뉴를 담당
- 설정을 바꿀 수 있지만 용접 출력은 직접 제어하지 않음

### Process FSM

- 실제 용접 단계 진행
- UI 상태와 독립적으로 동작

### Auto Detect

- AUTO 모드에서만 의미 있음
- 접촉 검출 시 `process_request_auto()`를 유도

즉 관계는 다음과 같습니다.

```text
Input -> UI FSM -> Settings
Input/ADC -> Auto Detect -> Process FSM
Settings -> Process FSM
UI + Process + Settings -> Render
```

## 레거시와의 차이

- 레거시: 한 파일에 UI/용접/입력이 더 많이 섞여 있음
- new_architecture: 상태머신과 레이어로 분리됨
- 시뮬레이터와 웹 UI가 같은 코어를 재사용할 수 있음

## 함께 보면 좋은 문서

- `firmware/new_architecture/SIMULATION.md`
- `docs/new_architecture/new_architecture.md`
