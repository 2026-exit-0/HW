---
name: 기능 추가 (Feature)
about: 펌웨어 / Wi-Fi 통신 / 사용자 trigger / BE 연동
title: 'feat: '
labels: ['feature']
assignees: ''
---

## 배경 / 동기

<!-- BE 요구 / 사용자 시나리오 / 시연 시나리오 -->

## 영향 받는 컴포넌트

- [ ] ESP32-CAM (`damdaPrj.ino`)
- [ ] OV2640 카메라 (`camInit.h`)
- [ ] FDC2112 수분 센서 (`aboutSensors.h`)
- [ ] VEML7700 조도 센서 (`aboutSensors.h`)
- [ ] LED (백색 / UV)
- [ ] 택트스위치 / 전원
- [ ] Wi-Fi 웹서버 (`web.h`)

## 작업 내용

<!-- 펌웨어 변경 / 회로 변경 / 통신 흐름 -->

## 측정 / 검증

```
시리얼 baud: 115200
기대 출력:
  ...
```

## 안전 / 전원 영향

- [ ] 추가 전류 소비
- [ ] LiPo 출력 한계
- [ ] LED 동시 점등 시 발열

## BE 통신 영향

- 필요 BE 변경: 있음 / 없음
- API 응답 형식 변경: ...

## 작업 체크리스트

- [ ] 펌웨어 코드
- [ ] 시리얼 모니터 검증
- [ ] 회로 / 배선 사진 (해당 시)
- [ ] BE 통합 테스트
