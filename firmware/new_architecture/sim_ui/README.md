# Interactive UI Simulator

브라우저에서 클릭으로 확인할 수 있는 간단한 UI 시뮬레이터입니다.

## 열기

```sh
cd /Users/hongyongjae/Desktop/ACSpot_Project/firmware/new_architecture/sim
make

cd /Users/hongyongjae/Desktop/ACSpot_Project/firmware/new_architecture/sim_ui
python3 server.py
```

그 다음 브라우저에서 아래 주소를 엽니다.

- [http://127.0.0.1:8000](http://127.0.0.1:8000)

## 할 수 있는 것

- 엔코더 좌/우 회전
- 엔코더 버튼 클릭
- 수동 용접 버튼 클릭
- 접촉 신호 on/off
- 시간 진행 (`10ms`, `100ms`, `1s`)
- LCD 16x2 표시 확인
- UI 상태 / Process 상태 / Auto Detect 상태 확인

## 참고

이 시뮬레이터는 실제 전력회로가 아니라 FSM/자동감지 로직을 눈으로 확인하기 위한 도구입니다.

이제 웹 UI는 자체 로직이 아니라 `firmware/new_architecture/sim/`의 실제 C 시뮬레이터와 연동됩니다.
