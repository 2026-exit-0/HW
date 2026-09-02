#ifndef ABOUT_SENSORS_H
#define ABOUT_SENSORS_H

#include <Wire.h>
#include "Adafruit_VEML7700.h"

#define FDC2112_ADDR 0x2A
#define BASELINE 130  // 아무것도 안 댔을 때
#define MIN_VAL  60  // 가장 촉촉할 때

// 센서 실시간 변수
uint16_t currentRaw = 7;
uint16_t prevRaw = 7;
float currentLux = 0;
float currentWhite = 0;
float currentALS = 0;
bool vemlConnected = false;

// 스캔 결과 평균값
float avgMoisture = 0;
float avgLux = 0;
float avgWhite = 0;
float avgALS = 0;

// 유분 측정용
float ambientLux = 0;      // 주변광 (LED 끈 상태)
float reflectedLux = 0;    // 순수 반사광 (LED 켠 상태 - 주변광)
float avgReflectedLux = 0; // 반사광 평균
float sumReflectedLux = 0;
float sumAmbientLux = 0;
int ambientMeasureCount = 0;
const int AMBIENT_SAMPLES = 2;

// 유분 측정용 (카메라 하이라이트 블록 분석)
// 픽셀 단위로 세면 흩어진 잔반짝임이 오히려 카운트가 높게 나오는 문제가 있어서
// 픽셀 하나하나가 아니라 블록(BLOCK_SIZE x BLOCK_SIZE) 평균으로 판정함
// -> 실측해보니 유분이 많아도 하이라이트가 자잘한 점/가는 줄기 형태라, 20x20은 너무 커서
//    진짜 신호까지 주변 어두운 픽셀에 묻혀버림(3케이스 다 ratio=0) -> 블록을 4x4로 축소
#define HIGHLIGHT_THRESHOLD 200
#define HIGHLIGHT_BLOCK_SIZE 4
float avgHighlightRatio = 0;
bool highlightAnalyzed = false;

// 스캔 상태
enum ScanState { IDLE, AMBIENT, WHITE_LED, UV_LED, MEASURING, DONE, SENT };
ScanState scanState = IDLE;
unsigned long scanStartTime = 0;
int measureCount = 0;
float sumMoisture = 0;
float sumLux = 0;
float sumWhite = 0;
float sumALS = 0;

// 캡처 이미지 저장
uint8_t* whiteCaptureData = NULL;
size_t whiteCaptureLen = 0;
uint8_t* uvCaptureData = NULL;
size_t uvCaptureLen = 0;
bool whiteCaptureStarted = false;
bool uvCaptureStarted = false;

Adafruit_VEML7700 veml = Adafruit_VEML7700();

// ===== FDC2112 수분 센서 =====

void writeRegister(uint8_t reg, uint16_t val) {
  uint8_t err = 1;
  for (int attempt = 0; attempt < 5 && err != 0; attempt++) {
    Wire.beginTransmission(FDC2112_ADDR);
    Wire.write(reg);
    Wire.write(val >> 8);
    Wire.write(val & 0xFF);
    err = Wire.endTransmission();
    if (err != 0) delay(5);  // 실패 시 잠깐 쉬고 재시도
  }
  Serial.printf("  writeReg 0x%02X <= 0x%04X (err=%d)\n", reg, val, err);
}

uint16_t readRegister16(uint8_t reg) {
  Wire.beginTransmission(FDC2112_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(FDC2112_ADDR, 2);  // FDC2112는 레지스터가 16비트(2바이트)뿐
  uint8_t high = Wire.read();
  uint8_t low = Wire.read();
  return ((uint16_t)high << 8) | low;
}

void initFDC2112() {
  writeRegister(0x1A, 0x1C01);  // Sleep mode 진입 (설정 변경 위해)
  delay(10);
  writeRegister(0x1C, 0x0600);  // RESET_DEV — OUTPUT_GAIN=x16 (MikroE 공식값)
  writeRegister(0x10, 0x0064);  // SETTLECOUNT_CH0 — MikroE 공식값
  writeRegister(0x08, 0x010F);  // RCOUNT_CH0 — MikroE 공식값
  writeRegister(0x0C, 0x0000);  // OFFSET_CH0 — 테스트: 0으로 낮춰서 클램프 가설 검증
  writeRegister(0x14, 0x2002);  // CLOCK_DIVIDERS_CH0 — FREF_DIVIDER 1→2 감도 실험 (raw 대역을 넓혀보는 테스트)
  writeRegister(0x1E, 0x7C00);  // DRIVE_CURRENT_CH0 — MikroE 공식값 (약 0.146mA)
  writeRegister(0x19, 0xFFFF);  // ERROR_CONFIG — MikroE 공식값
  writeRegister(0x1A, 0x0000);  // CONFIG — Active, Full-current mode (MikroE 공식값)
  writeRegister(0x1B, 0x0007);  // MUX_CONFIG — MikroE 공식값
  delay(100);

  // 상태 확인
  uint16_t status = readRegister16(0x18);
  Serial.printf("FDC2112 STATUS: 0x%04X\n", status);

  // 칩 ID 확인 (I2C 읽기 신뢰성 검증용) — 정답: MANUFACTURER_ID=0x5449, DEVICE_ID=0x3054
  uint16_t manuId = readRegister16(0x7E);
  uint16_t devId = readRegister16(0x7F);
  Serial.printf("MANUFACTURER_ID: 0x%04X (정답 0x5449)  DEVICE_ID: 0x%04X (정답 0x3054)\n", manuId, devId);
}

uint16_t readMoisture() {
  uint16_t status = readRegister16(0x18);
  bool drdy = (status >> 6) & 0x01;
  bool ch0Unread = (status >> 3) & 0x01;

  uint16_t raw = readRegister16(0x00);
  bool errWD = (raw >> 13) & 0x01;
  bool errAW = (raw >> 12) & 0x01;
  uint16_t data = raw & 0x0FFF;

  return data;
}
// ===== VEML7700 조도 센서 =====

void initVEML7700() {
  if (veml.begin()) {
    vemlConnected = true;
    veml.setGain(VEML7700_GAIN_2);
    veml.setIntegrationTime(VEML7700_IT_400MS);  // 200MS → 400MS: 해상도 확보하면서 스캔 시간 절충
    Serial.println("VEML7700 connected! (0x10)");
  } else {
    vemlConnected = false;
    Serial.println("VEML7700 NOT found!");
  }
}

void readVEML7700() {
  if (vemlConnected) {
    currentLux = veml.readLux();
    currentWhite = veml.readWhite();
    currentALS = veml.readALS();
  }
}

// ===== 카메라 캡처 저장 =====

void captureAndStore(uint8_t** dest, size_t* destLen) {
  if (*dest != NULL) {
    free(*dest);
    *dest = NULL;
    *destLen = 0;
  }
  if (!cameraReady) return;

  // 이전 프레임 버퍼 2개 버리기
  camera_fb_t *fb = esp_camera_fb_get();
  if (fb) esp_camera_fb_return(fb);
  fb = esp_camera_fb_get();
  if (fb) esp_camera_fb_return(fb);

  // 새 프레임 캡처
  fb = esp_camera_fb_get();
  if (!fb) return;

  *dest = (uint8_t*)malloc(fb->len);
  if (*dest) {
    memcpy(*dest, fb->buf, fb->len);
    *destLen = fb->len;
  }
  esp_camera_fb_return(fb);
}

// ===== 카메라 하이라이트 블록 분석 (유분) =====
// 카메라 포맷을 런타임에 GRAYSCALE로 전환하는 방식은 esp32-camera 드라이버 자체의
// 알려진 버그(포맷 전환 시 프레임 버퍼가 제대로 갱신/동기화 안 됨)로 매번 같은 값이 나와서 폐기.
// 대신 이미 저장용으로 찍어둔 whiteCaptureData(JPEG)를 fmt2rgb888()로 소프트웨어 디코딩해서
// 밝기를 계산 — 카메라를 다시 찍지 않아도 되고, 포맷 전환 버그 자체를 완전히 피함.
void analyzeHighlights() {
  if (whiteCaptureData == NULL || whiteCaptureLen == 0) {
    Serial.println("Highlight analysis skipped: no white capture");
    return;
  }

  size_t rgbLen = (size_t)CAM_FRAME_WIDTH * CAM_FRAME_HEIGHT * 3;
  uint8_t* rgbBuf = (uint8_t*)malloc(rgbLen);
  if (!rgbBuf) {
    Serial.println("Highlight analysis: RGB888 malloc failed");
    return;
  }

  if (!fmt2rgb888(whiteCaptureData, whiteCaptureLen, PIXFORMAT_JPEG, rgbBuf)) {
    Serial.println("Highlight analysis: JPEG decode failed");
    free(rgbBuf);
    return;
  }

  int width = CAM_FRAME_WIDTH;
  int height = CAM_FRAME_HEIGHT;
  int blockCols = width / HIGHLIGHT_BLOCK_SIZE;
  int blockRows = height / HIGHLIGHT_BLOCK_SIZE;
  int highlightBlocks = 0;
  int totalBlocks = blockCols * blockRows;
  int maxBlockAvg = 0;  // 진단용: 이번 프레임에서 실제로 나온 블록 평균 최댓값 (threshold 보정 근거)

  for (int by = 0; by < blockRows; by++) {
    for (int bx = 0; bx < blockCols; bx++) {
      long sum = 0;
      int baseY = by * HIGHLIGHT_BLOCK_SIZE;
      int baseX = bx * HIGHLIGHT_BLOCK_SIZE;
      for (int y = 0; y < HIGHLIGHT_BLOCK_SIZE; y++) {
        int rowBase = ((baseY + y) * width + baseX) * 3;
        for (int x = 0; x < HIGHLIGHT_BLOCK_SIZE; x++) {
          int idx = rowBase + x * 3;
          sum += (rgbBuf[idx] + rgbBuf[idx + 1] + rgbBuf[idx + 2]) / 3;  // R,G,B 평균 = 밝기
        }
      }
      int blockAvg = sum / (HIGHLIGHT_BLOCK_SIZE * HIGHLIGHT_BLOCK_SIZE);
      if (blockAvg > maxBlockAvg) maxBlockAvg = blockAvg;
      if (blockAvg > HIGHLIGHT_THRESHOLD) highlightBlocks++;
    }
  }

  avgHighlightRatio = (totalBlocks > 0) ? (float)highlightBlocks / totalBlocks : 0;
  Serial.printf("Highlight blocks: %d/%d, ratio=%.3f, maxBlockAvg=%d\n",
                highlightBlocks, totalBlocks, avgHighlightRatio, maxBlockAvg);

  free(rgbBuf);
}

// ===== 스캔 시작 =====

void startScan() {
  // 이전 캡처 메모리 해제
  if (whiteCaptureData != NULL) {
    free(whiteCaptureData);
    whiteCaptureData = NULL;
  }
  if (uvCaptureData != NULL) {
    free(uvCaptureData);
    uvCaptureData = NULL;
  }
  whiteCaptureLen = 0;
  uvCaptureLen = 0;

  // 결과 초기화
  avgMoisture = 0;
  avgLux = 0;
  avgWhite = 0;
  avgALS = 0;
  avgReflectedLux = 0;
  ambientLux = 0;
  reflectedLux = 0;
  sumReflectedLux = 0;
  sumAmbientLux = 0;
  ambientMeasureCount = 0;
  avgHighlightRatio = 0;
  highlightAnalyzed = false;

  scanState = AMBIENT;
  scanStartTime = millis();
  measureCount = 0;
  sumMoisture = 0;
  sumLux = 0;
  sumWhite = 0;
  sumALS = 0;
  Serial.println("Scan started: AMBIENT");

  whiteCaptureStarted = false;
  uvCaptureStarted = false;
}

// ===== 스캔 루프 처리 =====

void processScan() {
  unsigned long elapsed = millis() - scanStartTime;

  switch (scanState) {
    case AMBIENT:
      digitalWrite(PIN_LED_WHITE, LOW);
      digitalWrite(PIN_LED_UV, LOW);

      // 400ms 간격 AMBIENT_SAMPLES회 측정 후 평균 (1회 측정 노이즈 제거)
      // +2를 곱해서 진입 직후 1사이클(400ms)은 settling(안정화) 구간으로 버림
      // — VEML7700은 연속 변환 방식이라 LED 전환 직후 첫 사이클엔 이전 상태 빛이 섞여 있을 수 있음
      if (elapsed > (unsigned long)(ambientMeasureCount + 2) * 400
          && ambientMeasureCount < AMBIENT_SAMPLES) {
        readVEML7700();
        sumAmbientLux += currentLux;
        ambientMeasureCount++;
      }

      if (ambientMeasureCount >= AMBIENT_SAMPLES) {
        ambientLux = sumAmbientLux / AMBIENT_SAMPLES;
        Serial.printf("Ambient lux (avg of %d): %.1f\n", AMBIENT_SAMPLES, ambientLux);
        scanState = WHITE_LED;
        scanStartTime = millis();
        measureCount = 0;
        sumAmbientLux = 0;
        ambientMeasureCount = 0;
        Serial.println("Scan: WHITE_LED");
      }
      break;

    case WHITE_LED:
      digitalWrite(PIN_LED_WHITE, HIGH);
      digitalWrite(PIN_LED_UV, LOW);

      // 400ms 간격 6회 측정 (IT_400MS에 맞춰 간격 확장)
      // +2를 곱해서 WHITE_LED 켜진 직후 1사이클(400ms)은 settling 구간으로 버림
      // — LED ON 직전에 진행 중이던 변환 사이클엔 AMBIENT 상태의 빛이 섞여 있을 수 있음
      if (elapsed > (unsigned long)(measureCount + 2) * 400 && measureCount < 6) {
        currentRaw = readMoisture();
        readVEML7700();
        sumMoisture += currentRaw;
        sumLux += currentLux;
        sumWhite += currentWhite;
        sumALS += currentALS;
        float reflected = currentLux - ambientLux;
        if (reflected < 0) reflected = 0;
        sumReflectedLux += reflected;

        measureCount++;
        Serial.printf("Measure %d/6: raw=%d, lux=%.1f, reflected=%.1f\n",
                      measureCount, currentRaw, currentLux, reflected);
      }

      // 캡처 타이밍: settling 1사이클 추가로 측정 구간이 늘어난 만큼(800~2800ms) 밀어서 조정
      if (elapsed > 2000 && whiteCaptureLen == 0 && !whiteCaptureStarted) {
        whiteCaptureStarted = true;
        captureAndStore(&whiteCaptureData, &whiteCaptureLen);
        Serial.println("White capture done");
      }

      // 저장용 JPEG 캡처(whiteCaptureData)가 끝나면 그걸 그대로 디코딩해서 분석
      // (카메라를 다시 찍는 게 아니라 소프트웨어 디코딩이라 별도 settling 시간 불필요)
      // (검증 단계: 아직 서버 전송/calcOilPct 반영 없이 로그로만 값 확인)
      if (whiteCaptureLen > 0 && !highlightAnalyzed) {
        highlightAnalyzed = true;
        analyzeHighlights();
      }

      // 시간(2400ms)이 아니라 "6회 측정 + 하이라이트 분석까지 실제로 다 끝났는지"로 전환 판단
      // (카메라 캡처가 루프를 오래 막으면 elapsed만 앞서가서 덜 끝난 채로 넘어가버리는 버그 방지
      //  — 이전에 측정 횟수에서 겪었던 것과 같은 종류의 문제라 하이라이트 분석도 동일하게 가드함)
      // 단, 센서 이상 등으로 못 채우는 상황을 대비해 10초 타임아웃 시 있는 데이터로 강제 진행
      if ((measureCount >= 6 && highlightAnalyzed) || elapsed > 10000) {
        digitalWrite(PIN_LED_WHITE, LOW);
        // 결과 산출 (실제로 채워진 개수로 나눔, 0 나누기 방지)
        int n = (measureCount > 0) ? measureCount : 1;
        avgMoisture = sumMoisture / n;
        avgLux = sumLux / n;
        avgWhite = sumWhite / n;
        avgALS = sumALS / n;
        avgReflectedLux = sumReflectedLux / n;
        scanState = UV_LED;
        scanStartTime = millis();
        Serial.println("Scan: UV_LED");
      }
      break;

    case UV_LED:
      digitalWrite(PIN_LED_UV, HIGH);
      digitalWrite(PIN_LED_WHITE, LOW);

      // 0.5초에 캡처
      if (elapsed > 1000 && uvCaptureLen == 0 && !uvCaptureStarted) {
        uvCaptureStarted = true;
        captureAndStore(&uvCaptureData, &uvCaptureLen);
        Serial.println("UV capture done");
      }

      // 캡처 완료 확인 후 DONE
      if (elapsed > 1500) {
        digitalWrite(PIN_LED_UV, LOW);
        digitalWrite(PIN_LED_WHITE, LOW);
        scanState = DONE;
        Serial.printf("Scan DONE: avgReflected=%.1f, avgMoisture=%.0f, avgHighlightRatio=%.3f\n",
                      avgReflectedLux, avgMoisture, avgHighlightRatio);
      }
      break;

    case DONE:
    case IDLE:
      break;
  }
}

#endif
