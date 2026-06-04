#ifndef SENDER_H
#define SENDER_H

#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#define SUPABASE_URL "https://roghtpxrhxkicleukrfz.supabase.co"
#define SUPABASE_KEY "sb_publishable_i6hDxJVq5ikMLhcVz2KioA_S1f-tQEn"

// 이미지 없이 센서 데이터만 먼저 테스트
void sendDataToSupabase(float moisture, float oil,
                        uint8_t* whiteData, size_t whiteLen,
                        uint8_t* uvData, size_t uvLen) {

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(30);

  HTTPClient http;
  http.setTimeout(30000);

  // 1. 센서 데이터만 먼저 전송
  if(!http.begin(client, String(SUPABASE_URL) + "/rest/v1/scans")){
    Serial.println("HTTP begin failed");
    return;
  }

  http.addHeader("apikey", SUPABASE_KEY);
  http.addHeader("Authorization", "Bearer " + String(SUPABASE_KEY));
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Prefer", "return=minimal");

  String json = "{";
  json += "\"member\":\"" + selectedMember + "\"";
  json += ",\"part\":\"" + selectedPart + "\"";
  json += ",\"moisture\":" + String(moisture);
  json += ",\"oil\":" + String(oil);
  json += "}";

  int code = http.POST(json);
  Serial.printf("DB insert: %d\n", code);
  Serial.println("Response: " + http.getString());
  http.end();

  // 2. white 이미지 전송
  if(whiteData && whiteLen > 0){
    WiFiClientSecure c2;
    c2.setInsecure();
    c2.setTimeout(30);
    HTTPClient h2;
    h2.setTimeout(30000);

    String url = String(SUPABASE_URL) + "/storage/v1/object/images/" +
                 selectedMember + "_" + selectedPart + "_" +
                 String(millis()) + "_white.jpg";

    if(h2.begin(c2, url)){
      h2.addHeader("Authorization", "Bearer " + String(SUPABASE_KEY));
      h2.addHeader("Content-Type", "image/jpeg");
      code = h2.PUT(whiteData, whiteLen);
      Serial.printf("White upload: %d\n", code);
      Serial.println("Response: " + h2.getString());
      h2.end();
    }
  }

  // 3. uv 이미지 전송
  if(uvData && uvLen > 0){
    WiFiClientSecure c3;
    c3.setInsecure();
    c3.setTimeout(30);
    HTTPClient h3;
    h3.setTimeout(30000);

    String url = String(SUPABASE_URL) + "/storage/v1/object/images/" +
                 selectedMember + "_" + selectedPart + "_" +
                 String(millis()) + "_uv.jpg";

    if(h3.begin(c3, url)){
      h3.addHeader("Authorization", "Bearer " + String(SUPABASE_KEY));
      h3.addHeader("Content-Type", "image/jpeg");
      code = h3.PUT(uvData, uvLen);
      Serial.printf("UV upload: %d\n", code);
      Serial.println("Response: " + h3.getString());
      h3.end();
    }
  }
}

#endif