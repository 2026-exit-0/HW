#include <Wire.h>
#include <WiFi.h>

const char* ap_ssid = "DAMDA_SKIN";
const char* ap_password = "12345678";

#define PIN_LED_WHITE 13
#define PIN_LED_UV 12
#define PIN_SWITCH 2

#include "camInit.h"
#include "aboutSensors.h"
#include "analysis.h"
#include "web.h"
#include "sender.h"

unsigned long lastSwitchPress = 0;
unsigned long switchHoldStart = 0;
bool lastSwitchState = HIGH;
bool holdProcessed = false;

void setup() {
  Serial.begin(115200);
  pinMode(PIN_LED_WHITE, OUTPUT);
  pinMode(PIN_LED_UV, OUTPUT);
  pinMode(PIN_SWITCH, INPUT_PULLUP);

  initCamera();
  Serial.println(cameraReady ? "Camera OK" : "Camera FAIL");

  Wire.begin(15, 14);
  initFDC2112();
  Serial.println("FDC2112 init done");
  initVEML7700();

  WiFi.softAP(ap_ssid, ap_password);
  Serial.println("AP Mode IP: " + WiFi.softAPIP().toString());

  initWebServer();

  digitalWrite(PIN_LED_WHITE, LOW);
  digitalWrite(PIN_LED_UV, LOW);
}

void loop() {
  server.handleClient();

  bool sw = digitalRead(PIN_SWITCH);

  if(sw == LOW && lastSwitchState == HIGH){
    switchHoldStart = millis();
    holdProcessed = false;
  }

  if(sw == LOW){
    if(!holdProcessed && millis() - switchHoldStart > 5000){
      holdProcessed = true;
      Serial.println("Long press - shutting down...");
      digitalWrite(PIN_LED_WHITE, LOW);
      digitalWrite(PIN_LED_UV, LOW);
      // esp_deep_sleep_start();
    }
  } else if(sw == HIGH && lastSwitchState == LOW){
    if(!holdProcessed && millis() - switchHoldStart < 5000
        && millis() - lastSwitchPress > 300){
      if(scanState == IDLE || scanState == DONE){
        startScan();
        lastSwitchPress = millis();
        Serial.println("Switch pressed - scan started");
      }
    }
  }

  lastSwitchState = sw;

  if(scanState != IDLE && scanState != DONE){
    processScan();
  }

  if(scanState == DONE){
    Serial.println("Sending data to server...");
    sendDataToServer(avgMoisture, avgReflectedLux,
                     whiteCaptureData, whiteCaptureLen,
                     uvCaptureData, uvCaptureLen);
    scanState = IDLE;  // sendDataToServer 완료 후 IDLE로 변경
    // 웹UI는 /status 폴링으로 done → idle 순서로 감지함
  }

  if(scanState == IDLE){
    digitalWrite(PIN_LED_WHITE, LOW);
    digitalWrite(PIN_LED_UV, LOW);
  }
}


// main.py 실행: uvicorn main:app --host 0.0.0.0 --port 8000
// ESP32 업로드
// 노트북을 DAMDA_SKIN 와이파이에 연결
// 브라우저에서 http://192.168.4.1 접속
// 팀원/부위 선택 → 스캔 시작