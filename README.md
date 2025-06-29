## AC_SPOT_WELDER - 기술 분석 및 펌웨어 리팩토링

- **프로젝트 명:** 자동 트리거 기능의 AC 스팟 용접기
- **분석 대상:** 하드웨어(회로, PCB) 및 펌웨어 코드
- **문서 목적:** 원본 프로젝트의 기술적 구현 상태를 분석하고, 식별된 문제점에 대한 개선 방안과 그에 따른 최종 개선 코드를 제시함.
- 단 현재로써는 적용시킬수 있는 환경 및 조건이 안됨, 코드 관점에서의 분석

---

### **Part A. 펌웨어 코드 주요 개선점**

원본 코드는 기능 구현에 집중한 '유기체적' 코드이며, 개선된 코드는 안정성, 확장성, 유지보수성을 확보하기 위한 설계 원칙을 적용한 '구조적' 코드입니다. 각 개선점을 원본 코드와 개선된 코드의 실제 예시를 통해 비교 분석합니다.

### **1. 타이밍 제어 방식의 변경: `delay`에서 `하드웨어 타이머`로**

- **이전 코드:** 모든 시간 제어를 `delay_ms()`에 의존하며, 특히 ISR(인터럽트 서비스 루틴) 컨텍스트에서 CPU를 정지시키는 블로킹(Blocking) 방식을 사용함. 이는 시스템 반응성 저하의 주요 원인이 되는 구조적 문제점이다.

```c
// [원본 코드] ISR 내부에서 직접 delay 호출
ISR(INT0_vect)
{
    if(isr_cn == 1 & vbl>=8){
        _delay_ms(0.5);
         PORTC = 0x20; // 트리거 ON
         // CPU가 여기서 멈춰서 다음 라인으로 넘어가지 않음
         delay_ms(spot_time_hz * halfcycle); 
         PORTC = 0x00; // 트리거 OFF
         delay_ms(nbl);
         isr_cn=0;
    } 
    // ...
}
```

- **개선된 코드:** 하드웨어 Timer1을 이용해 1ms 주기의 시스템 틱(Tick)을 생성하고, 모든 시간 계산을 논블로킹(Non-blocking) 방식으로 처리함. 이를 통해 시스템은 다른 입력에 항상 반응할 수 있는 상태를 유지한다.

```c
// [개선된 코드] ISR은 플래그만 설정하고, 타이머가 시간 관리
volatile uint16_t weld_pulse_timer_ms;

// 1ms 마다 실행되는 타이머 ISR
ISR(TIMER1_COMPA_vect) {
    if (weld_pulse_timer_ms > 0) {
        weld_pulse_timer_ms--; // 0이 될 때까지 1ms씩 감소
    }
}

// 상태 머신 내부의 논블로킹 로직
case STATE_WELD_PULSE_ACTIVE:
    // 타이머가 0이 되었는지 '확인'만 할 뿐, 기다리지 않음
    if (weld_pulse_timer_ms == 0) {
        TRIAC_PORT &= ~(1 << TRIAC_PIN); // 타이머가 끝나면 트리거 OFF
        // ... 다음 상태로 전환 ...
    }
    break;
```

---

### **2. 아키텍처의 재정의: '거대 함수'에서 '상태 머신'으로**

- **이전 코드:** 하나의 거대 함수가 UI 표시, 로직 처리, 하드웨어 제어 등 다수의 책임을 갖는 구조로, 코드 흐름 파악 및 디버깅의 복잡도가 높음.

```c
// [원본 코드] 하나의 함수에 모든 로직이 얽혀 있음
void spot_action(void) {
    LCD_initialize(); // UI 초기화
    isr_init();       // 하드웨어 초기화
    // ... 데이터 로딩, UI 업데이트 ...
    while(1) { // 이 안에서 또 다른 while 루프들이 중첩됨
        while(autospot_sw==1) { /* 자동 용접 로직... */ }
        while(autospot_sw ==0) { /* 수동 용접 로직... */ }
    }
}
```

- **개선된 코드:** `enum SystemState`와 `main()` 함수의 `while(1)` 루프 내 `switch` 구문을 이용한 명시적 상태 머신(State Machine)으로 재설계함. 이는 코드의 흐름을 명확하게 하고 기능 추가의 용이성을 확보한다.

```c
// [개선된 코드] 역할이 명확히 분리된 상태 머신
typedef enum {
    STATE_IDLE_DISPLAY, STATE_WELD_PULSE_ACTIVE, STATE_MENU_SET_TIME,
    // ...
} SystemState;

int main(void) {
    initialize_all(); // 초기화는 여기서 한번만
    load_settings();
    sei();

    g_app.current_state = STATE_IDLE_DISPLAY;
    while(1) {
        switch (g_app.current_state) { // 현재 상태에 따라 분기
            case STATE_IDLE_DISPLAY:   /* IDLE 로직 처리 */ break;
            case STATE_WELD_PULSE_ACTIVE: handle_welding_logic(); break;
            case STATE_MENU_SET_TIME:   handle_menu_logic(); break;
        }
        update_display_smart(); // UI 업데이트는 항상 마지막에
    }
}
```

---

### **3. 데이터 관리의 현대화: '전역 변수'에서 '구조체'로**

- **이전 코드:** 다수의 개별 전역 변수가 코드 전체에 흩어져 있어 데이터 흐름과 연관 관계의 파악이 어려움.

```c
// [원본 코드] 의미를 알기 어려운 전역 변수들의 나열
int static_hz = 60;
int spotac_tacle=0;
int stan_flag=0;
int en_flag=1;
// ... 수십 개의 전역 변수 ...
int val,bal,nal,vbl,bbl,nbl;
```

- **개선된 코드:** `AppState_t`라는 단일 구조체(struct)를 통해 시스템의 모든 상태와 설정값을 중앙에서 관리함. `g_app.settings.time_ms`처럼 데이터의 소속과 의미가 명확해져 코드의 예측 가능성과 안정성을 높인다.

```c
// [개선된 코드] 연관 데이터끼리 묶어놓은 구조체
typedef struct {
    uint16_t time_ms;
    uint8_t  multiplier;
    // ...
} Settings_t;

typedef struct {
    SystemState current_state;
    Settings_t  settings;
    volatile uint16_t weld_pulse_timer_ms;
    // ...
} AppState_t;

volatile AppState_t g_app; // 모든 상태를 담는 유일한 전역 변수
```

---

### **4. 코드 품질과 가독성: '매직 넘버'에서 '정의된 상수'로**

- **이전 코드:** `PORTC = 0x20`과 같이 의미를 알 수 없는 숫자(매직 넘버)가 많아 가독성이 낮음.

```c
// [원본 코드] 매직 넘버 사용
#**define** S4 0x80&PINB
// ...
PORTC = 0x20;
// ...
GICR  = 0x40;
```

- **개선된 코드:** `#define`을 통해 모든 상수와 핀에 의미 있는 이름을 부여함. 이는 코드 자체를 문서처럼 만들어 가독성과 유지보수성을 극적으로 향상시킨다.

```c
// [개선된 코드] 의미 있는 이름을 가진 상수 정의
#**define** TRIAC_PORT      PORTC
#**define** TRIAC_PIN       PC5

#**define** MANUAL_SW_PIN   PB7
// ...

// 코드 내 사용 예시
TRIAC_PORT |= (1 << TRIAC_PIN); // "TRIAC 포트의 TRIAC 핀을 켠다"고 명확히 읽힘
if (!(ENCODER_PIN & (1 << MANUAL_SW_PIN))) { /* ... */ }
GICR |= (1 << INT0); // GICR 레지스터의 INT0 비트를 켠다고 명확히 읽힘
```

| 항목 | 이전 코드 (Before) | 개선된 코드 (After) |
| --- | --- | --- |
| **타이밍 제어** | `delay_ms()` 블로킹 방식 | 하드웨어 타이머 논블로킹 방식 |
| **프로그램 구조** | 거대 함수, 전역 플래그 조합 | 명확한 상태 머신 (Enum + Switch) |
| **데이터 관리** | 수많은 개별 전역 변수 | 단일 상태 구조체 (struct) |
| **가독성** | 매직 넘버 사용 | `#define`을 이용한 명명된 상수 |

---

### **Part B. 회로 및 하드웨어 분석**

### **1.1. 종합 평가**

핵심 기능 구현에 필요한 회로는 포함되어 있으나, 고전압(220V AC)을 다루는 장비로서 필수적인 안전 및 신뢰성 관련 설계가 누락되어 있음.

### **1.2. 구현된 주요 기능**

- **자동 트리거 시스템:** 풋 스위치 대신 트랜스포머 1차측의 임피던스 변화를 감지하여 용접을 시작하는 방식을 채택함.
- **TRIAC 구동부:** 유도성 부하(트랜스포머) 제어를 위해 스너버 회로(R-C)와 게이트 저항을 회로에 포함함.
- **자립형 시스템:** AC-DC 컨버터를 내장하여 외부 어댑터 없이 주 전원으로 동작하도록 설계됨.

### **1.3. 식별된 문제점 및 개선 방안**

- **문제점: 절연(Isolation) 설계 미흡**
    - **현상:** 고전압 회로와 저전압 디지털 회로 사이의 PCB 패턴 간격(연면/공간 거리)이 전기 안전 규격의 최소 요구치를 만족하지 못함. 옵토커플러 등 절연 소자 하단에 절연 성능을 높이기 위한 슬롯(Slot)이 없음.
    - **리스크:** 습기, 먼지, 전압 서지 발생 시 절연이 파괴되어 저전압 회로가 손상되거나 사용자에게 감전 위험을 초래할 수 있음.
    - **개선 방안:** 고전압부와 저전압부의 물리적 이격 거리를 최소 4mm 이상 확보하고, 절연 소자 경계에 절연 슬롯을 적용해야 함.
- **문제점: 전원 보호 회로 부재**
    - **현상:** AC 입력단에 과전류나 과전압을 방지하기 위한 부품이 설계에 포함되지 않음.
    - **리스크:** 내부 회로의 단락(쇼트)이나 외부의 전기적 충격 발생 시 회로 및 부품이 영구적으로 손상될 수 있음.
    - **개선 방안:** AC 입력단에 퓨즈(Fuse)를 직렬로 연결하여 과전류를 차단하고, 바리스터(Varistor)를 병렬로 연결하여 과전압 서지로부터 회로를 보호해야 함.
- **문제점: 전원 인가 시 간헐적 초기화 불안정**
    - **현상:** 전원을 켤 때, 간헐적으로 LCD가 정상적으로 초기화되지 않음.
    - **원인 분석:** 16MHz 크리스탈 발진자의 기동 시간 및 전원 공급 안정화 시간보다 MCU의 코드 실행 시작이 빠름. 이로 인해 불안정한 시스템 클럭 상태에서 I2C 통신이 시도되어 실패하는 것으로 분석됨.
    - **개선 방안:** AVR 퓨즈 비트(SUT, CKSEL) 설정을 변경하여 하드웨어적으로 충분한 시작 지연 시간을 확보하거나, `main()` 함수 시작부에 100ms 이상의 소프트웨어 지연 시간을 추가해야 함.
