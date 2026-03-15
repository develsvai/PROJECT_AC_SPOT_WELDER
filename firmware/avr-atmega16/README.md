# AVR ATmega16 Firmware

이 폴더는 펌웨어를 아래 기준으로 정리했습니다.

## 꼭 필요한 파일

- `project/ACSpotWelder.atsln`
  - Atmel Studio에서 여는 솔루션 파일
- `project/ACSpotWelder/ACSpotWelder.cproj`
  - AVR 프로젝트 설정 파일
- `project/ACSpotWelder/ACSpotWelder.c`
  - 메인 펌웨어 소스
- `project/ACSpotWelder/12c-lcd.h`
  - LCD 제어 코드

## 정리하면서 제거한 파일

- `.DS_Store`
- `.vs/`
- `.idea/`
- `.atsuo`
- `ACSpotWelder.componentinfo.xml`
- `artifacts/`
- 이전 버전 프로젝트 백업 파일

위 파일들은 개인 환경 설정 또는 재생성 가능한 메타데이터라서 프로젝트 유지에 필수는 아닙니다.

## 현재 권장 경로

- 소스 수정: `project/ACSpotWelder/`
- Atmel Studio 열기: `project/ACSpotWelder.atsln`
