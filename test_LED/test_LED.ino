#include <Wire.h>
#include <WiFi.h>

#define PIN_LED_WHITE 13
#define PIN_LED_UV 12
#define PIN_SWITCH 2

#include "camInit.h"
#include "aboutSensors.h"
#include "analysis.h"
#include "sender.h" // web.h 대신 교체

unsigned long lastSwitchPress = 0;
bool lastSwitchState = HIGH;

void setup() {
  Serial.begin(115200);
  pinMode(PIN_LED_WHITE, OUTPUT);
  pinMode(PIN_LED_UV, OUTPUT);
  pinMode(PIN_SWITCH, INPUT_PULLUP);

  initCamera();
  Wire.begin(15, 14);
  initFDC2112();
  initVEML7700();

  WiFi.begin("와이파이이름", "비밀번호"); // 공유기 연결
  Serial.println("Connecting to WiFi...");
}

void loop() {
  bool sw = digitalRead(PIN_SWITCH);
  if (sw == LOW && lastSwitchState == HIGH && millis() - lastSwitchPress > 300) {
    if (scanState == IDLE || scanState == DONE) {
      startScan();
      lastSwitchPress = millis();
    }
  }
  lastSwitchState = sw;

  if (scanState != IDLE && scanState != DONE) {
    processScan();
  }

  // 스캔 완료 시 자동으로 서버 전송
  if (scanState == DONE) {
    sendDataToServer(avgMoisture, avgReflectedLux, "M1", "FOREHEAD");
    scanState = IDLE;
  }
}