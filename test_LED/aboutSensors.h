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

  Serial.printf("raw=0x%04X data=%u errWD=%d errAW=%d | STATUS=0x%04X DRDY=%d CH0_UNREAD=%d\n",
                raw, data, errWD, errAW, status, drdy, ch0Unread);
  return data;
}
// ===== VEML7700 조도 센서 =====

void initVEML7700() {
  if (veml.begin()) {
    vemlConnected = true;
    veml.setGain(VEML7700_GAIN_2);
    veml.setIntegrationTime(VEML7700_IT_200MS);
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
      if (elapsed > 500) {
        readVEML7700();
        ambientLux = currentLux;
        Serial.printf("Ambient lux: %.1f\n", ambientLux);
        scanState = WHITE_LED;
        scanStartTime = millis();
        measureCount = 0;
        Serial.println("Scan: WHITE_LED");
      }
      break;

    case WHITE_LED:
      digitalWrite(PIN_LED_WHITE, HIGH);
      digitalWrite(PIN_LED_UV, LOW);

      // 500ms 간격 6회 측정 (3초 동안)
      if (elapsed > (unsigned long)(measureCount + 1) * 500 && measureCount < 6) {
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
        Serial.printf("Measure %d/6: raw=%d, lux=%.1f, reflected=%.1f\n", measureCount, currentRaw, currentLux, reflected);
      }

      // 2초에 캡처
      if (elapsed > 2000 && whiteCaptureLen == 0 && !whiteCaptureStarted) {
        whiteCaptureStarted = true;
        captureAndStore(&whiteCaptureData, &whiteCaptureLen);
        Serial.println("White capture done");
      }
    
      // 3초 후 UV로 전환
      if (elapsed > 3000) {
        digitalWrite(PIN_LED_WHITE, LOW);
        // 결과 산출
        avgMoisture = sumMoisture / 6.0;
        avgLux = sumLux / 6.0;
        avgWhite = sumWhite / 6.0;
        avgALS = sumALS / 6.0;
        avgReflectedLux = sumReflectedLux / 6.0;
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
        Serial.printf("Scan DONE: avgReflected=%.1f, avgMoisture=%.0f\n", avgReflectedLux, avgMoisture);
      }
      break;

    case DONE:
    case IDLE:
      break;
  }
}

#endif