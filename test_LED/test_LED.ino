#include <Wire.h>
#include <WiFi.h>

const char* ssid = "chimin";
const char* password = "iiii0070";

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

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected! IP: " + WiFi.localIP().toString());

  initWebServer();

  digitalWrite(PIN_LED_WHITE, LOW);
  digitalWrite(PIN_LED_UV, LOW);

  switchHoldStart = millis();
  lastSwitchState = digitalRead(PIN_SWITCH);

  
  Serial.println("I2C Scan:");
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  Found: 0x%02X\n", addr);
    }
  }
}

void loop() {
  server.handleClient();

  bool sw = digitalRead(PIN_SWITCH);

  if(sw == LOW && lastSwitchState == HIGH){
    if(millis() > 1000){
      switchHoldStart = millis();
      holdProcessed = false;
    }
  }

  if(sw == LOW && !holdProcessed && switchHoldStart > 0){
    if(millis() - switchHoldStart > 5000){
      holdProcessed = true;
      Serial.println("Long press - shutting down...");
      digitalWrite(PIN_LED_WHITE, LOW);
      digitalWrite(PIN_LED_UV, LOW);
      esp_deep_sleep_start();
    }
  } else if(sw == HIGH && lastSwitchState == LOW){
    if(!holdProcessed && switchHoldStart > 0
        && millis() - switchHoldStart < 5000
        && millis() - lastSwitchPress > 300){
      if(scanState == IDLE || scanState == DONE){
        startScan();
        lastSwitchPress = millis();
        Serial.println("Switch pressed - scan started");
      }
    }
    switchHoldStart = 0;
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
    Serial.println("Sending data to server...");
    int moisturePct = calcMoisturePct((uint16_t)avgMoisture);
    int oilPct = calcOilPct(avgReflectedLux);
    sendDataToSupabase(moisturePct, oilPct,
                       whiteCaptureData, whiteCaptureLen,
                       uvCaptureData, uvCaptureLen);
    needSend = false;      
    scanState = IDLE;      
  }                        

  if(scanState == IDLE){
    digitalWrite(PIN_LED_WHITE, LOW);
    digitalWrite(PIN_LED_UV, LOW);
  }

  if (Serial.available()) {
    Serial.read();
  
    camera_fb_t *fb = esp_camera_fb_get();
    if (fb) {
      // 선명도 계산
      long sum = 0;
      for (int i = 0; i < fb->len; i++) sum += fb->buf[i];
      long mean = sum / fb->len;
      long sumSq = 0;
      for (int i = 0; i < fb->len; i++) {
        long diff = fb->buf[i] - mean;
        sumSq += diff * diff;
      }
      Serial.printf("선명도 점수: %ld\n", sumSq / fb->len);
      esp_camera_fb_return(fb);
    }
  }
}