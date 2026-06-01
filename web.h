#ifndef WEB_H
#define WEB_H

#include <WebServer.h>
#include "esp_camera.h"

WebServer server(80);

// ===== /capture/white =====
void handleWhiteCapture() {
  if (whiteCaptureLen == 0 || whiteCaptureData == NULL) {
    server.send(404, "text/plain", "No white capture");
    return;
  }
  server.sendHeader("Cache-Control", "no-cache");
  server.send_P(200, "image/jpeg", (const char*)whiteCaptureData, whiteCaptureLen);
}

// ===== /capture/uv =====
void handleUVCapture() {
  if (uvCaptureLen == 0 || uvCaptureData == NULL) {
    server.send(404, "text/plain", "No UV capture");
    return;
  }
  server.sendHeader("Cache-Control", "no-cache");
  server.send_P(200, "image/jpeg", (const char*)uvCaptureData, uvCaptureLen);
}

// ===== /scan =====
void handleScanStart() {
  if (scanState == IDLE || scanState == DONE) {
    startScan();
    server.send(200, "application/json", "{\"status\":\"started\"}");
  } else {
    server.send(200, "application/json", "{\"status\":\"busy\"}");
  }
}

// ===== /data =====
void handleData() {
  int moistPct = calcMoisturePct((uint16_t)avgMoisture);
  int oilPct = calcOilPct(avgReflectedLux);

  String state;
  switch (scanState) {
    case IDLE: state = "idle"; break;
    case AMBIENT: state = "ambient"; break;
    case WHITE_LED: state = "white"; break;
    case UV_LED: state = "uv"; break;
    case MEASURING: state = "measuring"; break;
    case DONE: state = "done"; break;
  }

  String json = "{";
  json += "\"state\":\"" + state + "\"";
  json += ",\"raw\":" + String((int)avgMoisture);
  json += ",\"moistPct\":" + String(moistPct);
  json += ",\"skinType\":\"" + getSkinType(moistPct) + "\"";
  json += ",\"skinColor\":\"" + getSkinColor(moistPct) + "\"";
  json += ",\"moistAdvice\":\"" + getSkinAdvice(moistPct) + "\"";
  json += ",\"oilPct\":" + String(oilPct);
  json += ",\"oilLevel\":\"" + getOilLevel(oilPct) + "\"";
  json += ",\"oilColor\":\"" + getOilColor(oilPct) + "\"";
  json += ",\"oilAdvice\":\"" + getOilAdvice(oilPct) + "\"";
  json += ",\"ambientLux\":" + String(ambientLux, 1);
  json += ",\"reflectedLux\":" + String(avgReflectedLux, 1);
  json += ",\"veml\":" + String(vemlConnected ? "true" : "false");
  json += ",\"cam\":" + String(cameraReady ? "true" : "false");
  json += ",\"hasWhite\":" + String(whiteCaptureLen > 0 ? "true" : "false");
  json += ",\"hasUV\":" + String(uvCaptureLen > 0 ? "true" : "false");
  json += "}";

  server.sendHeader("Cache-Control", "no-cache");
  server.send(200, "application/json", json);
}

// ===== / =====
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
.cam-img{width:100%;border-radius:8px;margin-top:8px;background:#eee;min-height:150px}
.btn{display:block;width:100%;padding:16px;font-size:16px;font-weight:bold;color:white;background:#4CAF50;border:none;border-radius:12px;cursor:pointer;margin-bottom:16px}
.btn:disabled{background:#ccc;cursor:not-allowed}
.btn:active{background:#388E3C}
.scanning{text-align:center;padding:40px 20px}
.scanning-text{font-size:18px;font-weight:bold;color:#333;margin-bottom:8px}
.scanning-sub{font-size:14px;color:#888}
.dot-anim::after{content:'';animation:dots 1.5s infinite}
@keyframes dots{0%{content:''}25%{content:'.'}50%{content:'..'}75%{content:'...'}}

/* 선택 UI */
.select-group{display:flex;flex-wrap:wrap;gap:8px;margin-top:8px}
.select-btn{padding:8px 14px;border-radius:20px;border:2px solid #ddd;background:white;font-size:13px;cursor:pointer;transition:all 0.2s}
.select-btn.active{border-color:#4CAF50;background:#4CAF50;color:white;font-weight:bold}
.modal-overlay{display:none;position:fixed;top:0;left:0;width:100%;height:100%;background:rgba(0,0,0,0.5);z-index:100;justify-content:center;align-items:center}
.modal-overlay.show{display:flex}
.modal{background:white;border-radius:16px;padding:24px;width:90%;max-width:400px}
.modal h2{font-size:18px;margin-bottom:16px;color:#333}
.modal-btn{display:block;width:100%;padding:14px;font-size:15px;font-weight:bold;color:white;background:#4CAF50;border:none;border-radius:12px;cursor:pointer;margin-top:16px}
.modal-btn:disabled{background:#ccc;cursor:not-allowed}
</style></head><body>

<h1>🌿 담다 피부 분석</h1>

<button id='scanBtn' class='btn' onclick='startScan()'>🔍 피부 스캔 시작</button>
<a id='csvBtn' class='btn' download='labels.csv'
   style='background:#2196F3;text-align:center;text-decoration:none;display:none'>
  📥 labels.csv 저장
</a>

<!-- 스캔 완료 후 팀원/부위 선택 모달 -->
<div class='modal-overlay' id='selectModal'>
  <div class='modal'>
    <h2>📋 측정 정보 입력</h2>

    <div class='label'>팀원 선택</div>
    <div class='select-group' id='memberGroup'>
      <button class='select-btn' onclick='selectMember("M1")'>M1</button>
      <button class='select-btn' onclick='selectMember("M2")'>M2</button>
      <button class='select-btn' onclick='selectMember("M3")'>M3</button>
      <button class='select-btn' onclick='selectMember("M4")'>M4</button>
      <button class='select-btn' onclick='selectMember("M5")'>M5</button>
    </div>

    <div class='label' style='margin-top:16px'>부위 선택</div>
    <div class='select-group' id='partGroup'>
      <button class='select-btn' onclick='selectPart("FOREHEAD")'>이마</button>
      <button class='select-btn' onclick='selectPart("GLABELLA")'>미간</button>
      <button class='select-btn' onclick='selectPart("L_EYE")'>눈가(좌)</button>
      <button class='select-btn' onclick='selectPart("R_EYE")'>눈가(우)</button>
      <button class='select-btn' onclick='selectPart("L_CHEEK")'>볼(좌)</button>
      <button class='select-btn' onclick='selectPart("R_CHEEK")'>볼(우)</button>
      <button class='select-btn' onclick='selectPart("CHIN")'>턱</button>
    </div>

    <button class='modal-btn' id='confirmBtn' onclick='confirmAndDownload()' disabled>
      💾 저장 및 다운로드
    </button>
  </div>
</div>

<div id='scanningMsg' class='card' style='display:none'>
  <div class='scanning'>
    <div class='scanning-text dot-anim' id='scanText'>측정 중입니다</div>
    <div class='scanning-sub' id='scanSub'>준비 중...</div>
  </div>
</div>

<div id='resultSection' style='display:none'>

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
  <div class='label'>수분도</div>
  <div class='value'><span id='moistPct'>-</span> %</div>
  <div class='bar-bg'><div id='mBar' class='bar-fill' style='width:0%'></div></div>
</div>
<div class='card'>
  <div class='label'>피부 타입 (수분)</div>
  <div><span id='skinBadge' class='badge'>-</span></div>
  <div id='moistAdvice' class='advice'>-</div>
</div>

<div class='section'>✨ 피부 유분</div>
<div class='card'>
  <div class='label'>유분도</div>
  <div class='value'><span id='oilPct'>-</span> %</div>
  <div class='bar-bg'><div id='oBar' class='bar-fill' style='width:0%'></div></div>
</div>
<div class='card'>
  <div class='label'>유분 상태</div>
  <div><span id='oilBadge' class='badge'>-</span></div>
  <div id='oilAdvice' class='advice'>-</div>
</div>

<div class='section'>🔧 RAW 데이터</div>
<div class='card'>
  <div id='rawData' style='font-family:monospace;font-size:13px;color:#aaa;line-height:2'></div>
</div>

</div>

<script>
var polling=null;
var lastState='idle';
var currentData=null;
var selectedMember='';
var selectedPart='';

function selectMember(m){
  selectedMember=m;
  document.querySelectorAll('#memberGroup .select-btn').forEach(function(b){b.classList.remove('active')});
  event.target.classList.add('active');
  checkConfirmBtn();
}

function selectPart(p){
  selectedPart=p;
  document.querySelectorAll('#partGroup .select-btn').forEach(function(b){b.classList.remove('active')});
  event.target.classList.add('active');
  checkConfirmBtn();
}

function checkConfirmBtn(){
  document.getElementById('confirmBtn').disabled=!(selectedMember && selectedPart);
}

function confirmAndDownload(){
  if(!selectedMember || !selectedPart) return;
  var d=currentData;
  var ts=new Date();
  var timestamp=ts.getFullYear().toString()
    +(('0'+(ts.getMonth()+1)).slice(-2))
    +(('0'+ts.getDate()).slice(-2))+'_'
    +(('0'+ts.getHours()).slice(-2))
    +(('0'+ts.getMinutes()).slice(-2))
    +(('0'+ts.getSeconds()).slice(-2));

  var prefix=timestamp+'_'+selectedMember+'_'+selectedPart;

  // white.jpg 다운로드
  if(d.hasWhite){
    var a=document.createElement('a');
    a.href='/capture/white?t='+Date.now();
    a.download=prefix+'_white.jpg';
    a.click();
  }

  // uv.jpg 다운로드 (500ms 딜레이)
  setTimeout(function(){
    if(d.hasUV){
      var a=document.createElement('a');
      a.href='/capture/uv?t='+Date.now();
      a.download=prefix+'_uv.jpg';
      a.click();
    }
  }, 500);

  // labels.csv 생성
  var csvData='timestamp,member,part,moistPct,oilPct,skinType,oilLevel,ambientLux,reflectedLux\n';
  csvData+=timestamp+','+selectedMember+','+selectedPart+','+d.moistPct+','+d.oilPct+','+d.skinType+','+d.oilLevel+','+d.ambientLux+','+d.reflectedLux+'\n';
  var blob=new Blob([csvData],{type:'text/csv'});
  var url=URL.createObjectURL(blob);
  var btn=document.getElementById('csvBtn');
  btn.href=url;
  btn.style.display='block';

  // 모달 닫기
  document.getElementById('selectModal').classList.remove('show');

  // 선택 초기화
  selectedMember='';
  selectedPart='';
  document.querySelectorAll('.select-btn').forEach(function(b){b.classList.remove('active')});
  document.getElementById('confirmBtn').disabled=true;
}

function startScan(){
  document.getElementById('scanBtn').disabled=true;
  document.getElementById('scanBtn').textContent='스캔 진행 중...';
  document.getElementById('scanningMsg').style.display='block';
  document.getElementById('resultSection').style.display='none';
  document.getElementById('csvBtn').style.display='none';
  fetch('/scan').then(function(r){return r.json();}).then(function(){});
}

function pollStatus(){
  fetch('/data').then(function(r){return r.json();}).then(function(d){
    var txt=document.getElementById('scanText');
    var sub=document.getElementById('scanSub');
    var btn=document.getElementById('scanBtn');

    if(d.state=='ambient'){
      txt.textContent='측정 중입니다';
      sub.textContent='주변광 측정 중...';
      btn.disabled=true;
    } else if(d.state=='white'){
      txt.textContent='측정 중입니다';
      sub.textContent='백색 LED 촬영 + 센서 측정 중...';
      btn.disabled=true;
    } else if(d.state=='uv'){
      txt.textContent='측정 중입니다';
      sub.textContent='UV LED 촬영 중...';
      btn.disabled=true;
    } else if(d.state=='done'){
      if(lastState!='done'){
        showResult(d);
      }
    } else {
      btn.disabled=false;
      btn.textContent='🔍 피부 스캔 시작';
    }
    lastState=d.state;
  }).catch(function(){});
}

function showResult(d){
  currentData=d;
  document.getElementById('scanningMsg').style.display='none';
  document.getElementById('resultSection').style.display='block';
  document.getElementById('scanBtn').disabled=false;
  document.getElementById('scanBtn').textContent='🔍 다시 스캔';

  // 이미지 로딩
  setTimeout(function(){
    if(d.hasWhite) document.getElementById('whiteImg').src='/capture/white?t='+Date.now();
    if(d.hasUV) document.getElementById('uvImg').src='/capture/uv?t='+Date.now();
  }, 500);

  // 수분
  document.getElementById('moistPct').textContent=d.moistPct;
  var mbar=document.getElementById('mBar');
  mbar.style.width=d.moistPct+'%';
  mbar.style.background=d.skinColor;
  var sbadge=document.getElementById('skinBadge');
  sbadge.textContent=d.skinType;
  sbadge.style.background=d.skinColor;
  document.getElementById('moistAdvice').textContent=d.moistAdvice;

  // 유분
  document.getElementById('oilPct').textContent=d.oilPct;
  var obar=document.getElementById('oBar');
  obar.style.width=d.oilPct+'%';
  obar.style.background=d.oilColor;
  var obadge=document.getElementById('oilBadge');
  obadge.textContent=d.oilLevel;
  obadge.style.background=d.oilColor;
  document.getElementById('oilAdvice').textContent=d.oilAdvice;

  // RAW
  var raw=document.getElementById('rawData');
  var t='수분 RAW (평균): '+d.raw+'<br>';
  t+='주변광: '+d.ambientLux+' lux<br>';
  t+='반사광 (평균): '+d.reflectedLux+' lux<br>';
  t+='카메라: '+(d.cam?'✅':'❌')+'<br>';
  t+='VEML7700: '+(d.veml?'✅':'❌');
  raw.innerHTML=t;

  // 모달 표시
  document.getElementById('selectModal').classList.add('show');
}

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