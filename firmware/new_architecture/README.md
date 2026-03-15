# New Architecture Firmware

이 폴더는 `docs/new_architecture/` 설계 문서를 바탕으로 다시 구성한 새 펌웨어와
그 시뮬레이션 스택을 담고 있습니다.

## 구성

- `project/`
  - 실제 AVR 펌웨어 코어
- `sim/`
  - 같은 코어를 PC에서 돌리는 C 시뮬레이터
- `sim_ui/`
  - 시뮬레이터를 브라우저에서 조작하는 Web UI

## 문서

- `ARCHITECTURE.md`
  - 실제 펌웨어 구조와 파일 책임 정리
- `SIMULATION.md`
  - 시뮬레이터와 Web UI 연결 구조 정리

## 빠른 진입점

- 펌웨어 솔루션:
  - `project/ACSpotWelderNewArch.atsln`
- 콘솔 시뮬레이터:
  - `sim/acspot_sim`
- Web UI:
  - `sim_ui/server.py`
  - `sim_ui/index.html`

## 참고

기존 동작 코드는 `firmware/legacy/avr-atmega16/`에 그대로 보존되어 있습니다.
