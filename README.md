# damda — HW

담다(DAMDA) 프로젝트의 하드웨어 및 펌웨어 코드.
ESP32-CAM 기반 피부 스캐너로 수분·유분을 측정하고 이미지를 캡처하여 Supabase 클라우드 DB에 저장한다.

## 시스템 구성

```
ESP32-CAM (AP 모드)
├── DAMDA_SKIN 핫스팟 생성 → 웹페이지(192.168.4.1) 조작
└── sender.h → FastAPI(main.py) → Supabase 저장
```

| 부품 | 용도 |
|---|---|
| ESP32-CAM (AI Thinker) | 핵심 컨트롤러 + 카메라 |
| FDC2112 (Moisture Click) | 피부 수분 측정 (I2C 0x2A) |
| VEML7700 | 조도 측정 → 유분 추정 (I2C 0x10) |
| 백색 LED × 3 | 일반 조명 (IO13) |
| UV LED × 1 (395nm) | 자외선 조명 (IO12) |
| 택트 스위치 | 스캔 시작 / 딥슬립 (IO2) |
| TP4056 + LiPo | 전원 |

## 디렉터리

```
HW/
├── test_LED/
│   ├── test_LED.ino      # 메인 — WiFi AP모드, 스위치, 스캔 루프
│   ├── camInit.h         # 카메라 핀 정의 + 초기화
│   ├── aboutSensors.h    # FDC2112, VEML7700, 스캔 시퀀스
│   ├── analysis.h        # 수분/유분 분석 함수
│   ├── web.h             # 웹페이지 HTML, /scan, /status
│   └── sender.h          # FastAPI 서버로 데이터 전송
└── main.py               # FastAPI 서버 (Supabase 연동)
```

## 핀 배치

| ESP32-CAM 핀 | 용도 | 연결 대상 |
|---|---|---|
| 5V | 전원 | TP4056 VOUT+ |
| GND | 그라운드 | 공통 |
| 3.3V | 센서 전원 | FDC2112 + VEML7700 |
| IO12 | GPIO | UV LED (220Ω) |
| IO13 | GPIO | 백색 LED × 3 (각 220Ω) |
| IO14 | I2C SCL | FDC2112 + VEML7700 병렬 |
| IO15 | I2C SDA | FDC2112 + VEML7700 병렬 |
| IO2 | GPIO | 택트 스위치 (INPUT_PULLUP) |

## 스캔 시퀀스

```
IDLE
 └─ 스위치 단타 or 웹 버튼
     └─ AMBIENT  (0.5s)   — LED 끄고 주변광 측정
         └─ WHITE_LED (3s) — 백색 LED ON, 500ms 간격 6회 측정, 2초 시점 카메라 캡처
             └─ UV_LED (1.5s) — UV LED ON, 1초 시점 카메라 캡처
                 └─ DONE → 3초 대기 → FastAPI 전송 → IDLE
```

총 소요 시간: 약 6초

- **수분**: FDC2112 RAW값 6회 평균 → `calcMoisturePct()`
- **유분**: (백색 LED 조도 − 주변광) = 반사광 6회 평균 → `calcOilPct()`
- **스위치 5초 장누름**: 딥슬립 진입 (부팅 후 1초 이내 입력 무시)

## WiFi 구조

ESP32가 AP 모드로 핫스팟을 직접 생성한다.

```cpp
WiFi.softAP("DAMDA_SKIN", "12345678");
```

| 접속 방법 | 주소 |
|---|---|
| 웹페이지 | PC를 DAMDA_SKIN에 연결 후 `http://192.168.4.1` |
| FastAPI | PC에서 `python -m uvicorn main:app --host 0.0.0.0 --port 8000` |
| Supabase 대시보드 | 브라우저에서 `https://supabase.com` |

> **주의**: PC가 DAMDA_SKIN에 연결된 상태에서는 인터넷이 끊기므로 FastAPI 서버 실행 후 연결해야 함.

## 웹페이지 (web.h)

`http://192.168.4.1` 접속 시 표시되는 웹 UI.

- 팀원 선택: M1 ~ M5
- 부위 선택: 이마, 미간, 눈가(좌/우), 볼(좌/우), 턱
- 스캔 시작 버튼 → `/scan?member=&part=` 요청
- `/status` 1초 폴링으로 진행 상태 표시

> **현재 이슈**: 웹페이지의 스캔 진행 상황 표시(백색/UV LED 단계 안내 텍스트)가 동작하지 않음. `/status` 폴링은 정상이나 상태 전환 시 UI 업데이트 미작동.

## Supabase 연동 (main.py)

FastAPI 서버가 ESP32에서 데이터를 수신하고 Supabase에 저장한다.

**scans 테이블 구조**

| 컬럼 | 타입 | 내용 |
|---|---|---|
| id | int8 | PK |
| created_at | timestamptz | 자동 생성 |
| timestamp | text | 촬영 시각 |
| member | text | 팀원 ID (M1~M5) |
| part | text | 부위 |
| moisture | float4 | 수분도 (%) |
| oil | float4 | 유분도 (%) |
| white_img | text | 백색 이미지 (base64) |
| uv_img | text | UV 이미지 (base64) |

**FastAPI 엔드포인트**

| 메서드 | 경로 | 내용 |
|---|---|---|
| POST | `/sensor` | 센서 데이터 수신 |
| POST | `/image/white` | 백색 이미지 수신 |
| POST | `/image/uv` | UV 이미지 수신 + DB 저장 |
| GET | `/scans` | 전체 조회 |
| GET | `/scans/{member}` | 팀원별 조회 |
| GET | `/view/{scan_id}` | 이미지 포함 웹 뷰 |

**서버 실행**

```bash
python -m uvicorn main:app --host 0.0.0.0 --port 8000
```

## Arduino IDE 설정

| 항목 | 값 |
|---|---|
| 보드 | AI Thinker ESP32-CAM |
| 보드 패키지 URL | `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json` |
| 시리얼 Baud Rate | 115200 |
| 업로드 보드 | ESP32-CAM MB (CH340 내장) |

**필수 라이브러리**

| 라이브러리 | 버전 | 설치 |
|---|---|---|
| Adafruit VEML7700 | 최신 | 라이브러리 관리자 |
| FDC2214 by Harijs Zablockis | 1.1.0 | 라이브러리 관리자 |

**Python 패키지**

```bash
pip install fastapi uvicorn supabase
```

## 센서 캘리브레이션

FDC2112는 납땜 상태에 따라 RAW값 범위가 달라진다. `aboutSensors.h`에서 조정.

```cpp
#define BASELINE 7   // 피부 미접촉 시 RAW값
#define MIN_VAL  2   // 피부 완전 접촉 시 RAW값
```

유분 캘리브레이션은 `analysis.h`에서 조정.

```cpp
float minRef = 5.0;   // 건조한 피부 반사광 (lux)
float maxRef = 50.0;  // 유분 많은 피부 반사광 (lux)
```

> 납땜 완료 후 실측값으로 재캘리브레이션 필요.

## 프로토타입 진행 현황

| 단계 | 상태 | 내용 |
|---|---|---|
| 브레드보드 조립 | ✅ 완료 | 전체 부품 연결 및 동작 확인 |
| 센서 데이터 수집 | ✅ 완료 | 수분·유분 측정 + Supabase 저장 확인 |
| 카메라 캡처 | ✅ 완료 | 백색/UV 이미지 캡처 및 전송 |
| Supabase 연동 | ✅ 완료 | DB 저장 및 조회 확인 |
| 스위치 Long press | ✅ 완료 | 극성 수정 후 정상 동작 |
| 웹 진행상황 표시 | 🔄 진행 중 | 스캔 단계별 안내 텍스트 미동작 |
| 3D 프린팅 케이스 | 🔄 진행 중 | 3차 프로토타입까지 제작 완료 |

## Phase 전략

| Phase | 내용 | 시점 |
|---|---|---|
| **Phase 1** (현재) | 브레드보드 → 소형화 프로토타입, 데이터 수집 | 진행 중 |
| **Phase 2** | 누적 데이터 → AI 모델 fine-tune 입력 | 데이터 충분 후 |
| **Phase 3** | 최종 케이스 + 캘리브레이션 + 데모 | 공모전 직전 |

## TODO

- [ ] 웹페이지 스캔 진행 상황 표시 수정 (백색/UV 단계 안내 텍스트)
- [ ] 3D 프린팅 케이스 4차 프로토타입 (카메라 초점거리 0.65cm 기준)
- [ ] FDC2112 납땜 후 BASELINE/MIN_VAL 재캘리브레이션
- [ ] 유분 minRef/maxRef 실측 데이터 기반 재설정
- [ ] 소형화 직접 납땜 연결 완성
