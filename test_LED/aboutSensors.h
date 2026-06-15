#ifndef ABOUT_SENSORS_H
#define ABOUT_SENSORS_H

#include <Wire.h>
#include "Adafruit_VEML7700.h"

#define FDC2112_ADDR 0x2A
#define BASELINE 30    // 테스트 후 조정
#define MIN_VAL  10    // 테스트 후 조정

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

Adafruit_VEML7700 veml = Adafruit_VEML7700();

// ===== FDC2112 수분 센서 =====

void writeRegister(uint8_t reg, uint16_t val) {
  Wire.beginTransmission(FDC2112_ADDR);
  Wire.write(reg);
  Wire.write(val >> 8);
  Wire.write(val & 0xFF);
  Wire.endTransmission();
}

uint16_t readRegister(uint8_t reg) {
  Wire.beginTransmission(FDC2112_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(FDC2112_ADDR, 2);
  return (Wire.read() << 8) | Wire.read();
}

void initFDC2112() {
  writeRegister(0x1C, 0x020D);
  writeRegister(0x08, 0x7FFF);
  writeRegister(0x0C, 0x0064);
  writeRegister(0x10, 0x047F);
  delay(10);
  writeRegister(0x1A, 0x1401);
  delay(100);
}

uint16_t readMoisture() {
  uint16_t val = readRegister(0x00);
  return val & 0x0FFF;
}

// ===== VEML7700 조도 센서 =====

void initVEML7700() {
  if (veml.begin()) {
    vemlConnected = true;
    veml.setGain(VEML7700_GAIN_1);
    veml.setIntegrationTime(VEML7700_IT_100MS);
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

  // 이전 프레임 버퍼 비우기 (캐시 제거)
  camera_fb_t *fb = esp_camera_fb_get();
  if (fb) esp_camera_fb_return(fb);
  delay(100);

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
      if (elapsed > 2000 && whiteCaptureLen == 0) {
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
      if (elapsed > 1000 && uvCaptureLen == 0) {
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