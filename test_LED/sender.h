#ifndef SENDER_H
#define SENDER_H

#include <HTTPClient.h>

// EC2 서버 IP
#define SERVER_IP "52.79.241.24"
#define SERVER_PORT 8001
#define DEVICE_ID "ESP32_1"  // 두 번째 기기는 "ESP32_2"로 변경

// 이미지는 백엔드가 /capture 로 직접 가져감 (pull 방식)
// 여기서는 센서값(moisture, oil)만 EC2로 전송
void sendDataToSupabase(float moisture, float oil, uint8_t* whiteData, size_t whiteLen, uint8_t* uvData, size_t uvLen) {

  String base = "http://" + String(SERVER_IP) + ":" + String(SERVER_PORT);
  HTTPClient http;

  // 센서 데이터 전송 (device_id 포함)
  http.begin(base + "/sensor");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  String data = "device_id=" + String(DEVICE_ID) +
                "&member=" + selectedMember + "&part=" + selectedPart +
                "&moisture=" + String(moisture) + "&oil=" + String(oil);
  int code = http.POST(data);
  Serial.printf("Sensor POST: %d\n", code);
  http.end();

  // white 이미지 전송
  if(whiteData && whiteLen > 0){
    http.begin(base + "/image/white");
    http.addHeader("Content-Type", "image/jpeg");
    code = http.POST(whiteData, whiteLen);
    Serial.printf("White POST: %d\n", code);
    http.end();
  }

  // uv 이미지 전송
  if(uvData && uvLen > 0){
    http.begin(base + "/image/uv");
    http.addHeader("Content-Type", "image/jpeg");
    code = http.POST(uvData, uvLen);
    Serial.printf("UV POST: %d\n", code);
    http.end();
  }
}

// EC2에서 스캔 명령 폴링
bool checkScanCommand(String &outMember, String &outPart) {
  HTTPClient http;
  String url = "http://" + String(SERVER_IP) + ":" + String(SERVER_PORT) +
               "/scan-command/" + String(DEVICE_ID);
  http.begin(url);
  int code = http.GET();
  if(code == 200) {
    String body = http.getString();
    // pending:true 이면 명령 있음
    if(body.indexOf("\"pending\":true") >= 0) {
      // member 파싱
      int mi = body.indexOf("\"member\":\"") + 10;
      int mj = body.indexOf("\"", mi);
      outMember = body.substring(mi, mj);
      // part 파싱
      int pi = body.indexOf("\"part\":\"") + 8;
      int pj = body.indexOf("\"", pi);
      outPart = body.substring(pi, pj);
      http.end();
      return true;
    }
  }
  http.end();
  return false;
}

// EC2에 ESP32 IP 등록 (registerToEC2는 test_LED.ino에서 호출)

// ─── 신규: 하드웨어 버튼 눌렸을 때 React 프론트에 '스캔 중' 알림 ───
// 백엔드가 상태를 'scanning'으로 변경 → 프론트가 GET /scans/status 폴링해서 UI 업데이트
void notifyScanTrigger() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi 미연결 - 프론트 알림 스킵");
    return;
  }
  HTTPClient http;
  String url = "http://" + String(SERVER_IP) + ":" + String(SERVER_PORT) + "/scans/trigger";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  int code = http.POST("{\"status\":\"scanning\",\"device_id\":\"" + String(DEVICE_ID) + "\"}");
  Serial.printf("→ POST /scans/trigger: %d\n", code);
  http.end();
}

#endif
