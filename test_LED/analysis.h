#ifndef ANALYSIS_H
#define ANALYSIS_H

#include <Arduino.h>

// ===== 수분 분석 =====

int calcMoisturePct(uint16_t raw) {
  if (raw <= BASELINE) return 0;
  if (raw >= MIN_VAL) return 100;
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

// ===== 유분 분석 (반사광 기반) =====

int calcOilPct(float reflectedLux) {
  // 반사광이 높을수록 유분이 많음 (번들거림 = 반사 많음)
  // 측정값 기준: 이마(유분) ~54, 팔뚝(건조) ~85 였지만 주변광 제거 후 재캘리브 필요
  // 우선 0~50 lux 범위로 설정 (테스트 후 조정)
  float minRef = 100.0;   // 유분 많은 피부 반사광 (낮음)
  float maxRef = 165.0;  // 건조한 피부 반사광 (높음)

  if (reflectedLux >= maxRef) return 0;
  if (reflectedLux <= minRef) return 100;
  return (int)((maxRef - reflectedLux) / (maxRef - minRef) * 100.0);
}

String getOilLevel(int pct) {
  if (pct < 20) return "건조 (Dry)";
  if (pct < 40) return "약간 건조 (Slightly Dry)";
  if (pct < 60) return "보통 (Normal)";
  if (pct < 80) return "약간 유분 (Slightly Oily)";
  return "유분 많음 (Oily)";
}

String getOilColor(int pct) {
  if (pct < 20) return "#FF6B6B";
  if (pct < 40) return "#FFA94D";
  if (pct < 60) return "#69DB7C";
  if (pct < 80) return "#FFC107";
  return "#FF5722";
}

String getOilAdvice(int pct) {
  if (pct < 20) return "유분이 부족합니다. 유분감 있는 크림을 사용하세요.";
  if (pct < 40) return "유분이 약간 부족합니다. 가벼운 오일 세럼을 권장합니다.";
  if (pct < 60) return "유수분 밸런스가 좋습니다. 현재 루틴을 유지하세요.";
  if (pct < 80) return "유분이 약간 많습니다. 가벼운 보습제를 사용하세요.";
  return "유분이 많습니다. 유분 조절 제품을 사용하세요.";
}

#endif