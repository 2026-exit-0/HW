#ifndef WEB_H
#define WEB_H

#include <WebServer.h>
#include "esp_camera.h"

WebServer server(80);

// ===== /capture/white — 백색 LED 캡처 이미지 =====
void handleWhiteCapture() {
  if (whiteCaptureLen == 0 || whiteCaptureData == NULL) {
    server.send(404, "text/plain", "No white capture");
    return;
  }
  server.sendHeader("Cache-Control", "no-cache");
  server.send_P(200, "image/jpeg", (const char*)whiteCaptureData, whiteCaptureLen);
}

// ===== /capture/uv — UV LED 캡처 이미지 =====
void handleUVCapture() {
  if (uvCaptureLen == 0 || uvCaptureData == NULL) {
    server.send(404, "text/plain", "No UV capture");
    return;
  }
  server.sendHeader("Cache-Control", "no-cache");
  server.send_P(200, "image/jpeg", (const char*)uvCaptureData, uvCaptureLen);
}

// ===== /scan — 웹에서 스캔 시작 =====
void handleScanStart() {
  if (scanState == IDLE || scanState == DONE) {
    // 이전 캡처 초기화
    whiteCaptureLen = 0;
    uvCaptureLen = 0;
    startScan();
    server.send(200, "application/json", "{\"status\":\"started\"}");
  } else {
    server.send(200, "application/json", "{\"status\":\"busy\"}");
  }
}

// ===== /data — 센서 데이터 JSON =====
void handleData() {
  int pct = calcMoisturePct((uint16_t)avgMoisture);
  int uvi = calcUVIndex(avgLux);

  String state;
  switch (scanState) {
    case IDLE: state = "idle"; break;
    case WHITE_LED: state = "white"; break;
    case UV_LED: state = "uv"; break;
    case MEASURING: state = "measuring"; break;
    case DONE: state = "done"; break;
  }

  String json = "{";
  json += "\"state\":\"" + state + "\"";
  json += ",\"raw\":" + String((int)avgMoisture);
  json += ",\"pct\":" + String(pct);
  json += ",\"skinType\":\"" + getSkinType(pct) + "\"";
  json += ",\"skinColor\":\"" + getSkinColor(pct) + "\"";
  json += ",\"advice\":\"" + getSkinAdvice(pct) + "\"";
  json += ",\"lux\":" + String(avgLux, 1);
  json += ",\"white\":" + String(avgWhite, 0);
  json += ",\"als\":" + String(avgALS, 0);
  json += ",\"lightLevel\":\"" + getLightLevel(avgLux) + "\"";
  json += ",\"lightColor\":\"" + getLightColor(avgLux) + "\"";
  json += ",\"uvi\":" + String(uvi);
  json += ",\"uviLabel\":\"" + getUVIndexLabel(uvi) + "\"";
  json += ",\"uviColor\":\"" + getUVIndexColor(uvi) + "\"";
  json += ",\"uvAdvice\":\"" + getUVAdvice(avgLux) + "\"";
  json += ",\"veml\":" + String(vemlConnected ? "true" : "false");
  json += ",\"cam\":" + String(cameraReady ? "true" : "false");
  json += ",\"hasWhite\":" + String(whiteCaptureLen > 0 ? "true" : "false");
  json += ",\"hasUV\":" + String(uvCaptureLen > 0 ? "true" : "false");
  json += "}";

  server.sendHeader("Cache-Control", "no-cache");
  server.send(200, "application/json", json);
}

// ===== / — 메인 페이지 =====
void handleRoot() {
  String html = R"rawliteral(<!DOCTYPE html><html><head>
<meta charset='UTF-8'>
<meta name='viewport' content='width=device-width,initial-scale=1'>
<title>담다 피부 분석</title>
<style>
*{box-sizing:border-box}
body{font-family:sans-serif;max-width:500px;margin:0 auto;padding:20px;background:#f8f9fa}
h1{color:#333;font-size:22px;margin-bottom:24px}
.card{background:white;border-radius:12px;padding:20px;margin-bottom:16px;box-shadow:0 2px 8px rgba(0,0,0,0.08)}
.label{font-size:12px;color:#888;margin-bottom:4px}
.value{font-size:28px;font-weight:bold;color:#333}
.bar-bg{background:#eee;border-radius:8px;height:16px;margin-top:8px}
.bar-fill{height:16px;border-radius:8px;transition:width 0.5s}
.badge{display:inline-block;padding:4px 12px;border-radius:20px;font-size:13px;font-weight:bold;color:white;margin-top:8px}
.advice{font-size:13px;color:#555;margin-top:8px;line-height:1.6}
.section{font-size:14px;font-weight:bold;color:#666;margin:24px 0 8px;padding-left:4px}
.row{display:flex;gap:12px}
.half{flex:1}
.mini-val{font-size:20px;font-weight:bold;color:#333}
.sub{font-size:11px;color:#aaa;margin-top:2px}
.cam-img{width:100%;border-radius:8px;margin-top:8px;background:#eee;min-height:150px}
.btn{display:block;width:100%;padding:16px;font-size:16px;font-weight:bold;color:white;background:#4CAF50;border:none;border-radius:12px;cursor:pointer;margin-bottom:16px}
.btn:disabled{background:#ccc;cursor:not-allowed}
.btn:active{background:#388E3C}
.scanning{text-align:center;padding:40px 20px}
.scanning-text{font-size:18px;font-weight:bold;color:#333;margin-bottom:8px}
.scanning-sub{font-size:14px;color:#888}
.dot-anim::after{content:'';animation:dots 1.5s infinite}
@keyframes dots{0%{content:''}25%{content:'.'}50%{content:'..'}75%{content:'...'}}
.hidden{display:none}
</style></head><body>

<h1>🌿 담다 피부 분석</h1>

<button id='scanBtn' class='btn' onclick='startScan()'>🔍 피부 스캔 시작</button>

<!-- 측정 중 안내 -->
<div id='scanningMsg' class='card hidden'>
<div class='scanning'>
<div class='scanning-text dot-anim' id='scanText'>측정 중입니다</div>
<div class='scanning-sub' id='scanSub'>백색 LED 촬영 중...</div>
</div>
</div>

<!-- 결과 섹션 -->
<div id='resultSection' class='hidden'>

<div class='section'>📷 피부 촬영</div>
<div class='row'>
<div class='card half'>
<div class='label'>백색 LED 촬영</div>
<img id='whiteImg' class='cam-img' src=''>
</div>
<div class='card half'>
<div class='label'>UV LED 촬영</div>
<img id='uvImg' class='cam-img' src=''>
</div>
</div>

<div class='section'>💧 피부 수분</div>
<div class='card'>
<div class='label'>수분도 (5회 평균)</div>
<div class='value'><span id='pct'>-</span> %</div>
<div class='bar-bg'><div id='mBar' class='bar-fill' style='width:0%'></div></div>
</div>
<div class='card'>
<div class='label'>피부 타입</div>
<div><span id='skinBadge' class='badge'>-</span></div>
<div id='skinAdvice' class='advice'>-</div>
</div>

<div class='section'>☀️ 조도 / 자외선</div>
<div id='vemlSection'></div>

<div class='section'>🔧 RAW 데이터</div>
<div class='card'>
<div id='rawData' style='font-family:monospace;font-size:13px;color:#aaa;line-height:2'></div>
</div>

</div>

<script>
var polling=null;

function startScan(){
  document.getElementById('scanBtn').disabled=true;
  document.getElementById('scanBtn').textContent='스캔 진행 중...';
  document.getElementById('scanningMsg').classList.remove('hidden');
  document.getElementById('resultSection').classList.add('hidden');

  fetch('/scan').then(function(r){return r.json();}).then(function(){
    if(!polling) polling=setInterval(pollStatus,500);
  });
}

function pollStatus(){
  fetch('/data').then(function(r){return r.json();}).then(function(d){
    var txt=document.getElementById('scanText');
    var sub=document.getElementById('scanSub');
    var btn=document.getElementById('scanBtn');

    if(d.state=='white'){
      document.getElementById('scanningMsg').classList.remove('hidden');
      document.getElementById('resultSection').classList.add('hidden');
      btn.disabled=true;
      btn.textContent='스캔 진행 중...';
      txt.textContent='측정 중입니다';
      sub.textContent='백색 LED 촬영 중...';
    } else if(d.state=='uv'){
      document.getElementById('scanningMsg').classList.remove('hidden');
      document.getElementById('resultSection').classList.add('hidden');
      btn.disabled=true;
      btn.textContent='스캔 진행 중...';
      txt.textContent='측정 중입니다';
      sub.textContent='UV LED 촬영 중...';
    } else if(d.state=='measuring'){
      document.getElementById('scanningMsg').classList.remove('hidden');
      document.getElementById('resultSection').classList.add('hidden');
      btn.disabled=true;
      btn.textContent='스캔 진행 중...';
      txt.textContent='측정 중입니다';
      sub.textContent='센서 데이터 수집 중... (5초)';
    } else if(d.state=='done'){
      showResult(d);
    }
  }).catch(function(){});
}

function showResult(d){
  document.getElementById('scanningMsg').classList.add('hidden');
  document.getElementById('resultSection').classList.remove('hidden');
  document.getElementById('scanBtn').disabled=false;
  document.getElementById('scanBtn').textContent='🔍 피부 스캔 시작';

  if(d.hasWhite) document.getElementById('whiteImg').src='/capture/white?t='+Date.now();
  if(d.hasUV) document.getElementById('uvImg').src='/capture/uv?t='+Date.now();

  document.getElementById('pct').textContent=d.pct;
  var bar=document.getElementById('mBar');
  bar.style.width=d.pct+'%';
  bar.style.background=d.skinColor;

  var badge=document.getElementById('skinBadge');
  badge.textContent=d.skinType;
  badge.style.background=d.skinColor;
  document.getElementById('skinAdvice').textContent=d.advice;

  var vs=document.getElementById('vemlSection');
  if(d.veml){
    var luxBar=Math.min(Math.round(d.lux/500*100),100);
    vs.innerHTML='<div class="card"><div class="label">현재 조도 (5회 평균)</div>'
      +'<div class="value">'+d.lux+' lux</div>'
      +'<div class="bar-bg"><div class="bar-fill" style="width:'+luxBar+'%;background:'+d.lightColor+'"></div></div>'
      +'<div class="badge" style="background:'+d.lightColor+'">'+d.lightLevel+'</div></div>'
      +'<div class="row"><div class="card half"><div class="label">UV 지수 (추정)</div>'
      +'<div class="mini-val" style="color:'+d.uviColor+'">'+d.uvi+'</div>'
      +'<div class="badge" style="background:'+d.uviColor+'">'+d.uviLabel+'</div></div>'
      +'<div class="card half"><div class="label">White 채널</div>'
      +'<div class="mini-val">'+d.white+'</div>'
      +'<div class="sub">ALS: '+d.als+'</div></div></div>'
      +'<div class="card"><div class="label">자외선 차단 가이드</div>'
      +'<div class="advice">'+d.uvAdvice+'</div></div>';
  } else {
    vs.innerHTML='<div class="card"><div style="color:#FF6B6B;font-weight:bold">⚠️ VEML7700 미연결</div></div>';
  }

  var raw=document.getElementById('rawData');
  var t='수분 RAW (평균): '+d.raw+'<br>';
  if(d.veml) t+='조도 (평균): '+d.lux+' lux<br>White: '+d.white+'<br>ALS: '+d.als+'<br>';
  t+='카메라: '+(d.cam?'✅':'❌')+'<br>';
  t+='I2C: 0x2A(수분) '+(d.veml?'0x10(조도) ✅':'0x10(조도) ❌');
  raw.innerHTML=t;
}

// 항상 상태 감시 (스위치로 시작해도 감지)
polling=setInterval(pollStatus,1000);
</script>
</body></html>)rawliteral";

  server.send(200, "text/html", html);
}

// ===== 웹 서버 시작 =====
void initWebServer() {
  server.on("/", handleRoot);
  server.on("/capture/white", handleWhiteCapture);
  server.on("/capture/uv", handleUVCapture);
  server.on("/scan", handleScanStart);
  server.on("/data", handleData);
  server.begin();
}

#endif
