#include <Wire.h>
#include <WiFi.h>
#include <WiFiMulti.h>

WiFiMulti wifiMulti;

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
unsigned long sentTime = 0;
bool needSend = false;

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

  // AP 모드 (웹페이지용)
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(ap_ssid, ap_password);
  Serial.println("AP Mode IP: " + WiFi.softAPIP().toString());

  // STA 모드 (Supabase 전송용) - 여기에 핫스팟 입력
  wifiMulti.addAP("minaong309", "lunaeong46&!)");
  // wifiMulti.addAP("핫스팟이름2", "비밀번호2");
  // wifiMulti.addAP("핫스팟이름3", "비밀번호3");

  Serial.println("Connecting to WiFi...");
  if(wifiMulti.run() == WL_CONNECTED) {
    Serial.println("STA IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("STA connect failed - Supabase unavailable");
  }

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

  if(scanState == DONE && !needSend){
    needSend = true;
    sentTime = millis();
  }

  if(scanState == DONE && needSend && millis() - sentTime > 3000){
    // Supabase 전송 전 WiFi 재연결 시도
    if(wifiMulti.run() != WL_CONNECTED){
      Serial.println("No internet - skip Supabase");
    } else {
      Serial.println("Sending data to Supabase...");
      sendDataToSupabase(avgMoisture, avgReflectedLux,
                         whiteCaptureData, whiteCaptureLen,
                         uvCaptureData, uvCaptureLen);
    }
    needSend = false;
    scanState = IDLE;
  }

  if(scanState == IDLE){
    digitalWrite(PIN_LED_WHITE, LOW);
    digitalWrite(PIN_LED_UV, LOW);
  }
}