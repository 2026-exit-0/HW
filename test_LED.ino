#include <Wire.h>
#include <WiFi.h>

// ===== Wi-Fi 설정 =====
const char* ssid = "minaong309";
const char* password = "lunaeong46&!)";

IPAddress local_IP(10, 174, 185, 100);
IPAddress gateway(10, 174, 185, 132);
IPAddress subnet(255, 255, 255, 0);

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

bool lastSwitchState = HIGH;

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

  // Wi-Fi 연결
  WiFi.config(local_IP, gateway, subnet);
  WiFi.begin(ssid, password);

  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 20) {
    delay(500);
    Serial.print(".");
    retry++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi: " + WiFi.localIP().toString());
  } else {
    Serial.println("\nWiFi FAILED");
  }

  // 웹 서버 시작
  initWebServer();

  // LED 초기 상태 끄기
  digitalWrite(PIN_LED_WHITE, LOW);
  digitalWrite(PIN_LED_UV, LOW);
}

void loop() {
  server.handleClient();

  // 스위치 눌림 감지 (LOW = 눌림, INPUT_PULLUP 사용)
  bool sw = digitalRead(PIN_SWITCH);
  if (sw == LOW && lastSwitchState == HIGH && millis() - lastSwitchPress > 300) {
    if (scanState == IDLE || scanState == DONE) {
      startScan();
      lastSwitchPress = millis();
      Serial.println("Switch pressed - scan started");
    }
  }
  lastSwitchState = sw;

  // 스캔 진행 중이면 처리
  if (scanState != IDLE && scanState != DONE) {
    processScan();
  }

  // 대기 상태에서는 LED 끄기
  if (scanState == IDLE) {
    digitalWrite(PIN_LED_WHITE, LOW);
    digitalWrite(PIN_LED_UV, LOW);
  }
}