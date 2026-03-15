# Simulator

이 폴더는 `firmware/new_architecture/project/ACSpotWelderNewArch/` 코어 로직을
하드웨어 없이 호스트 환경에서 실행해보는 시뮬레이터입니다.

## 시뮬레이션 가능한 것

- UI FSM
- Process FSM
- zero-cross 동기 용접 흐름
- 자동 감지 로직
- LCD 표시 변경
- TRIAC / buzzer 상태 변화

## 실행

```sh
make -C firmware/new_architecture/sim
./firmware/new_architecture/sim/acspot_sim
```

## 포함된 기본 시나리오

- Manual weld scenario
- Auto detect scenario

필요하면 `sim_main.c`에 시나리오를 추가해서 입력, zero-cross, ADC 파형을
더 세밀하게 검증할 수 있습니다.
