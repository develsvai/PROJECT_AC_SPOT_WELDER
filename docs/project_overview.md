# Project Overview

이 문서는 현재 프로젝트 전체가 어디까지 진행되었는지, 어떤 산출물이 있고,
다음에 무엇을 보면 되는지 한 번에 파악하기 위한 overview 문서입니다.

## 1. 프로젝트 목적

이 프로젝트는 AC 스팟 용접기 제어 보드를 직접 설계하고, 펌웨어를 작성하고,
제작/검증 자료까지 함께 관리하기 위한 통합 작업 폴더입니다.

구성 요소는 크게 4개입니다.

- `hardware/`
  - 회로도, PCB, 제조 원본과 보존 자료
- `firmware/legacy/`
  - 기존 동작 펌웨어
- `firmware/new_architecture/`
  - 상태머신 기반으로 재구성한 새 펌웨어와 시뮬레이터
- `docs/`
  - 매뉴얼, BOM, 설계 문서, 프로젝트 설명 자료

## 2. 현재 폴더 기준 상태

### `hardware/`

- 실보드 원본 데이터 보존용으로 유지 중
- 바로 열기 어려운 파일이 많아서 현재는 "보존"이 우선
- 관련 이미지 자료가 별도로 있어 제작 기록 추적 가능

### `firmware/legacy/`

- 기존 AVR 펌웨어 보존 완료
- 원본 동작 기준선 역할
- 새 구조 이행 전 reference implementation 역할

### `firmware/new_architecture/`

- 새 구조 코드베이스 생성 완료
- `UI FSM`, `Process FSM`, `Auto Detect`, `Render`, `Settings`, `Hardware Abstraction`
  로 분리 완료
- 호스트용 C 시뮬레이터 추가 완료
- Web UI 시뮬레이터 추가 완료

### `docs/`

- 매뉴얼/가이드/BOM 문서 정리 완료
- `docs/new_architecture/new_architecture.md`
  - 설계 의도와 현재 구현 매핑용 기준 문서

## 3. 펌웨어 진행 상황

### 현재 구현 반영 상태

현재 `firmware/new_architecture/project/ACSpotWelderNewArch/` 기준으로 보면,
`docs/new_architecture/new_architecture.md` 에 정의된 주요 구조는 대부분 실제 코드에
반영되어 있습니다.

#### 이미 반영된 항목

- `UI FSM` / `Process FSM` 분리
- `types.h` 중심 상태 정의
- `app.c` 에서 루트 오케스트레이션 수행
- `input.c` 로 입력 스캔 분리
- `render.c` 로 LCD 렌더링 분리
- `settings.c` 로 설정 저장 분리
- `auto_detect.c` 로 자동 감지 분리
- `hardware.c` 추상화 및 `sim/hardware_sim.c` 대체 구현
- 웹 UI가 실제 C 시뮬레이터 snapshot을 읽는 구조

#### 구현되었지만 시뮬레이터에서 사용 방식이 다른 항목

- 엔코더 기반 메뉴 편집
  - 펌웨어 코어에는 남아 있음
  - Web UI에서는 시뮬레이션 사용성을 위해 직접 설정 입력을 더 많이 사용

- manual / auto 입력
  - 실제 코어는 버튼 이벤트 기반
  - Web UI에서는 `Manual Weld`, `Touch Pulse` 같은 시뮬레이터용 편의 액션도 함께 제공

#### 아직 주의해서 봐야 하는 항목

- 자동 감지 threshold / baseline / release 조건은 실제 하드웨어 측정값으로 추가 튜닝이 필요
- 현재 시뮬레이터 ADC 파형은 실제 전력 회로의 완전한 물리 모델이 아니라 로직 검증용 모델임

즉, 구조 설계는 상당 부분 구현되었고, 남은 핵심 과제는 실기 기준 파라미터 튜닝과 최종 검증입니다.

### 완료된 것

- 기존 펌웨어 legacy 보존
- 새 펌웨어 프로젝트 생성
- `GccApplication` 계열 이름을 프로젝트 의미에 맞게 정리
- 상태머신 기반 구조로 모듈 분리
- `ATmega16` 프로젝트 파일 정리
- 시뮬레이터와 Web UI 추가

### 현재 가능한 것

- manual weld 시퀀스 시뮬레이션
- auto detect 흐름 시뮬레이션
- LCD 상태 확인
- pulse / weld count 추적
- 현재 상태 snapshot을 브라우저에서 확인

### 아직 남은 것

- 실제 하드웨어 업로드/실기 검증
- 자동 감지 threshold 실측 튜닝
- 실전 전원/노이즈 조건에서 검증
- 필요시 Web UI와 실제 메뉴 UX를 더 가깝게 맞추기

## 4. 시뮬레이터 진행 상황

현재 시뮬레이터는 아래 구조로 동작합니다.

```text
firmware core
  -> sim_api
  -> Python server
  -> Web UI
```

의미:

- 핵심 펌웨어 로직은 실제 C 코드가 그대로 실행됨
- 하드웨어만 simulator layer로 치환됨
- Web UI는 snapshot을 표시하고 action을 전달하는 리모컨 역할

즉, 지금 Web 화면은 단순 mockup이 아니라 실제 시뮬레이터 상태를 보고 있습니다.

## 5. 문서 진행 상황

### 정리 완료

- manual 관련 markdown 정리
- guides / manuals / bom 문서 정리
- excel 기반 자료 markdown 변환
- `new_architecture` 통합 문서 생성
- 펌웨어 구조 / 시뮬레이션 구조 문서화

### 현재 기준 문서

- `docs/new_architecture/new_architecture.md`
- `firmware/new_architecture/SIMULATION.md`

## 6. 지금 프로젝트를 따라가는 추천 순서

처음 보는 사람이면 아래 순서가 가장 이해하기 쉽습니다.

1. `docs/project_overview.md`
2. `docs/new_architecture/new_architecture.md`
3. `firmware/new_architecture/SIMULATION.md`
4. `firmware/new_architecture/project/ACSpotWelderNewArch/`
5. `firmware/new_architecture/sim_ui/`

## 7. 현재 한 줄 요약

현재 프로젝트는:

> 회로 원본과 legacy 펌웨어를 보존한 상태에서,
> 새 상태머신 기반 펌웨어와 시뮬레이션 환경까지 갖춘 "다음 단계 검증 가능 상태"

까지 와 있습니다.
