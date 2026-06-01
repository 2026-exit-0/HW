#include <Wire.h>
#include <WiFi.h>

// ===== Wi-Fi 설정 =====
const char* ap_ssid = "DAMDA_SKIN";
const char* ap_password = "12345678";

// ===== 핀 설정 =====
#define PIN_LED_WHITE 13
#define PIN_LED_UV 12
#define PIN_SWITCH 2

// ===== 모듈 불러오기 =====
#include "camInit.h"
#include "aboutSensors.h"
#include "analysis.h"
#include "web.h"

// ===== 스위치 디바운스 변수 =====
unsigned long lastSwitchPress = 0;
unsigned long switchHoldStart = 0;
bool lastSwitchState = HIGH;
bool holdProcessed = false;

void setup() {
  Serial.begin(115200);

  // 핀 설정
  pinMode(PIN_LED_WHITE, OUTPUT);
  pinMode(PIN_LED_UV, OUTPUT);
  pinMode(PIN_SWITCH, INPUT_PULLUP);  // 내부 풀업 사용

  // 카메라 초기화
  initCamera();
  Serial.println(cameraReady ? "Camera OK" : "Camera FAIL");

  // I2C 초기화
  Wire.begin(15, 14);

  // 센서 초기화
  initFDC2112();
  Serial.println("FDC2112 init done");
  initVEML7700();

  // I2C 스캔
  Serial.println("I2C Scan:");
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  Found: 0x%02X\n", addr);
    }
  }

  // AP 모드 시작
  WiFi.softAP(ap_ssid, ap_password);
  Serial.println("AP Mode IP: " + WiFi.softAPIP().toString());

  // 웹 서버 시작
  initWebServer();

  // LED 초기 상태 끄기
  digitalWrite(PIN_LED_WHITE, LOW);
  digitalWrite(PIN_LED_UV, LOW);
}

void loop() {
  server.handleClient();

  // ===== 스위치 처리 =====
  bool sw = digitalRead(PIN_SWITCH);

  if (sw == LOW && lastSwitchState == HIGH) {
    // 누르기 시작
    switchHoldStart = millis();
    holdProcessed = false;
  }

  if (sw == LOW) {
    // 누르고 있는 중 - 3초 이상이면 전원 끄기
    if (!holdProcessed && millis() - switchHoldStart > 3000) {
      holdProcessed = true;
      Serial.println("Long press - shutting down...");
      digitalWrite(PIN_LED_WHITE, LOW);
      digitalWrite(PIN_LED_UV, LOW);
      esp_deep_sleep_start();
    }
  } else if (sw == HIGH && lastSwitchState == LOW) {
    // 짧게 눌렀다 뗀 경우 - 스캔 시작
    if (!holdProcessed && millis() - switchHoldStart < 3000
        && millis() - lastSwitchPress > 300) {
      if (scanState == IDLE || scanState == DONE) {
        startScan();
        lastSwitchPress = millis();
        Serial.println("Switch pressed - scan started");
      }
    }
  }

  lastSwitchState = sw;

  // ===== 스캔 처리 =====
  if (scanState != IDLE && scanState != DONE) {
    processScan();
  }

  // ===== 대기 상태에서는 LED 끄기 =====
  if (scanState == IDLE) {
    digitalWrite(PIN_LED_WHITE, LOW);
    digitalWrite(PIN_LED_UV, LOW);
  }
}