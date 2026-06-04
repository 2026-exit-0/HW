#ifndef SENDER_H
#define SENDER_H

#include <HTTPClient.h>

// 노트북 IP (DAMDA_SKIN 연결 시 보통 192.168.4.2)
#define SERVER_IP "192.168.4.2"
#define SERVER_PORT 8000

void sendDataToSupabase(float moisture, float oil,
                        uint8_t* whiteData, size_t whiteLen,
                        uint8_t* uvData, size_t uvLen) {

  String base = "http://" + String(SERVER_IP) + ":" + String(SERVER_PORT);
  HTTPClient http;

  // 센서 데이터 전송
  http.begin(base + "/sensor");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  String data = "member=" + selectedMember + "&part=" + selectedPart +
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

#endif