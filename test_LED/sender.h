#ifndef SENDER_H
#define SENDER_H

#include <HTTPClient.h>
#include <base64.h>

#define SUPABASE_URL "https://roghtpxrhxkicleukrfz.supabase.co"
#define SUPABASE_KEY "sb_publishable_i6hDxJVq5ikMLhcVz2KioA_S1f-tQEn"

String uploadImage(uint8_t* data, size_t len, String filename) {
  if (!data || len == 0) return "";

  HTTPClient http;
  String url = String(SUPABASE_URL) + "/storage/v1/object/images/" + filename;

  http.begin(url);
  http.addHeader("Authorization", "Bearer " + String(SUPABASE_KEY));
  http.addHeader("Content-Type", "image/jpeg");

  int code = http.PUT(data, len);
  Serial.printf("Image upload %s: %d\n", filename.c_str(), code);
  http.end();

  if (code == 200 || code == 201) {
    return String(SUPABASE_URL) + "/storage/v1/object/public/images/" + filename;
  }
  return "";
}

void sendDataToSupabase(float moisture, float oil,
                        uint8_t* whiteData, size_t whiteLen,
                        uint8_t* uvData, size_t uvLen) {

  String ts = String(millis());

  // 이미지 업로드
  String whiteUrl = uploadImage(whiteData, whiteLen,
    selectedMember + "_" + selectedPart + "_" + ts + "_white.jpg");
  String uvUrl = uploadImage(uvData, uvLen,
    selectedMember + "_" + selectedPart + "_" + ts + "_uv.jpg");

  // DB 저장
  HTTPClient http;
  http.begin(String(SUPABASE_URL) + "/rest/v1/scans");
  http.addHeader("apikey", SUPABASE_KEY);
  http.addHeader("Authorization", "Bearer " + String(SUPABASE_KEY));
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Prefer", "return=minimal");

  String json = "{";
  json += "\"member\":\"" + selectedMember + "\"";
  json += ",\"part\":\"" + selectedPart + "\"";
  json += ",\"moisture\":" + String(moisture);
  json += ",\"oil\":" + String(oil);
  json += ",\"white_img\":\"" + whiteUrl + "\"";
  json += ",\"uv_img\":\"" + uvUrl + "\"";
  json += "}";

  int code = http.POST(json);
  Serial.printf("DB insert: %d\n", code);
  http.end();
}

#endif