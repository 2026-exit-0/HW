#ifndef SENDER_H
#define SENDER_H

#include <HTTPClient.h>

void sendDataToServer(float moisture, float oil, String member, String part) {
  HTTPClient http;
  // PC의 실제 IP 주소를 넣어주세요 (예: 192.168.0.10)
  http.begin("http://192.168.0.X:8000/upload");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  
  String data = "member=" + member + "&part=" + part + 
                "&moisture=" + String(moisture) + "&oil=" + String(oil);
  
  http.POST(data);
  http.end();
}
#endif