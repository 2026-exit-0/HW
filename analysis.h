#ifndef ANALYSIS_H
#define ANALYSIS_H

#include <Arduino.h>

// ===== 수분 분석 =====

int calcMoisturePct(uint16_t raw) {
  if (raw >= BASELINE) return 0;
  if (raw <= MIN_VAL) return 100;
  return map(raw, BASELINE, MIN_VAL, 0, 100);
}

String getSkinType(int pct) {
  if (pct < 20) return "건성 (Dry)";
  if (pct < 40) return "복합성 (Combination)";
  if (pct < 60) return "중성 (Normal)";
  if (pct < 80) return "촉촉함 (Moist)";
  return "지성 (Oily)";
}

String getSkinColor(int pct) {
  if (pct < 20) return "#FF6B6B";
  if (pct < 40) return "#FFA94D";
  if (pct < 60) return "#FFD43B";
  if (pct < 80) return "#69DB7C";
  return "#4DABF7";
}

String getSkinAdvice(int pct) {
  if (pct < 20) return "수분이 매우 부족합니다. 즉시 보습이 필요합니다.";
  if (pct < 40) return "수분이 부족합니다. 보습 크림 사용을 권장합니다.";
  if (pct < 60) return "수분 상태가 양호합니다. 현재 루틴을 유지하세요.";
  if (pct < 80) return "수분 상태가 좋습니다. 가벼운 보습제를 사용하세요.";
  return "수분이 충분합니다. 유분 조절에 신경쓰세요.";
}

// ===== 조도 분석 =====

String getLightLevel(float lux) {
  if (lux < 50) return "어두움";
  if (lux < 200) return "실내 조명";
  if (lux < 1000) return "밝은 실내";
  if (lux < 10000) return "흐린 날 실외";
  if (lux < 30000) return "맑은 날 그늘";
  return "직사광선";
}

String getLightColor(float lux) {
  if (lux < 50) return "#6C757D";
  if (lux < 200) return "#FFC107";
  if (lux < 1000) return "#FF9800";
  if (lux < 10000) return "#FF5722";
  if (lux < 30000) return "#F44336";
  return "#D32F2F";
}

String getUVAdvice(float lux) {
  if (lux < 200) return "실내 환경입니다. 자외선 걱정 없어요.";
  if (lux < 1000) return "약한 빛 노출. 장시간 시 가벼운 자외선 차단을 권장합니다.";
  if (lux < 10000) return "야외 활동 시 자외선 차단제(SPF 30+)를 바르세요.";
  if (lux < 30000) return "강한 빛입니다. SPF 50+ 자외선 차단제와 모자 착용을 권장합니다.";
  return "매우 강한 직사광선! 외출 자제 또는 완벽한 자외선 차단이 필요합니다.";
}

int calcUVIndex(float lux) {
  if (lux < 200) return 0;
  if (lux < 1000) return 1;
  if (lux < 5000) return 3;
  if (lux < 10000) return 5;
  if (lux < 30000) return 7;
  if (lux < 50000) return 9;
  return 11;
}

String getUVIndexColor(int uvi) {
  if (uvi <= 2) return "#4CAF50";
  if (uvi <= 5) return "#FFC107";
  if (uvi <= 7) return "#FF9800";
  if (uvi <= 10) return "#F44336";
  return "#9C27B0";
}

String getUVIndexLabel(int uvi) {
  if (uvi <= 2) return "낮음";
  if (uvi <= 5) return "보통";
  if (uvi <= 7) return "높음";
  if (uvi <= 10) return "매우 높음";
  return "위험";
}

#endif
