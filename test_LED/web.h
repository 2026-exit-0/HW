#ifndef WEB_H
#define WEB_H

#include <WebServer.h>

WebServer server(80);

String selectedMember = "M1";
String selectedPart = "FOREHEAD";

void handleRoot() {
  String html = R"rawliteral(<!DOCTYPE html><html><head>
<meta charset='UTF-8'>
<meta name='viewport' content='width=device-width,initial-scale=1'>
<title>담다 피부 분석</title>
<style>
*{box-sizing:border-box}
body{font-family:sans-serif;max-width:500px;margin:0 auto;padding:20px;background:#f8f9fa}
h1{font-size:22px;color:#333;margin-bottom:24px}
.card{background:white;border-radius:12px;padding:20px;margin-bottom:16px;box-shadow:0 2px 8px rgba(0,0,0,0.08)}
.label{font-size:12px;color:#888;margin-bottom:8px}
.select-group{display:flex;flex-wrap:wrap;gap:8px}
.select-btn{padding:8px 14px;border-radius:20px;border:2px solid #ddd;background:white;font-size:13px;cursor:pointer}
.select-btn.active{border-color:#4CAF50;background:#4CAF50;color:white;font-weight:bold}
.btn{display:block;width:100%;padding:16px;font-size:16px;font-weight:bold;color:white;background:#4CAF50;border:none;border-radius:12px;cursor:pointer;margin-top:16px}
.btn:disabled{background:#ccc}
.status{text-align:center;padding:20px;font-size:15px;color:#555}
</style></head><body>

<h1>🌿 담다 피부 분석</h1>

<div class='card'>
  <div class='label'>팀원 선택</div>
  <div class='select-group' id='memberGroup'>
    <button class='select-btn active' onclick='selectMember("M1")'>M1</button>
    <button class='select-btn' onclick='selectMember("M2")'>M2</button>
    <button class='select-btn' onclick='selectMember("M3")'>M3</button>
    <button class='select-btn' onclick='selectMember("M4")'>M4</button>
    <button class='select-btn' onclick='selectMember("M5")'>M5</button>
  </div>
</div>

<div class='card'>
  <div class='label'>부위 선택</div>
  <div class='select-group' id='partGroup'>
    <button class='select-btn active' onclick='selectPart("FOREHEAD")'>이마</button>
    <button class='select-btn' onclick='selectPart("GLABELLA")'>미간</button>
    <button class='select-btn' onclick='selectPart("L_EYE")'>눈가(좌)</button>
    <button class='select-btn' onclick='selectPart("R_EYE")'>눈가(우)</button>
    <button class='select-btn' onclick='selectPart("L_CHEEK")'>볼(좌)</button>
    <button class='select-btn' onclick='selectPart("R_CHEEK")'>볼(우)</button>
    <button class='select-btn' onclick='selectPart("CHIN")'>턱</button>
  </div>
</div>

<button class='btn' id='scanBtn' onclick='startScan()'>🔍 스캔 시작</button>
<div class='status' id='status'></div>

<script>
var member='M1';
var part='FOREHEAD';

function selectMember(m){
  member=m;
  document.querySelectorAll('#memberGroup .select-btn').forEach(function(b){b.classList.remove('active')});
  event.target.classList.add('active');
}

function selectPart(p){
  part=p;
  document.querySelectorAll('#partGroup .select-btn').forEach(function(b){b.classList.remove('active')});
  event.target.classList.add('active');
}

function startScan(){
  document.getElementById('scanBtn').disabled=true;
  document.getElementById('status').textContent='스캔 중...';
  fetch('/scan?member='+member+'&part='+part)
    .then(function(r){return r.json();})
    .then(function(d){
      if(d.status=='started'){
        pollStatus();
      } else {
        document.getElementById('status').textContent='이미 스캔 중입니다.';
        document.getElementById('scanBtn').disabled=false;
      }
    });
}

function pollStatus(){
  fetch('/status')
    .then(function(r){return r.json();})
    .then(function(d){
      if(d.state=='done'){
        document.getElementById('status').textContent='✅ 완료! 데이터가 서버에 저장됐습니다.';
        document.getElementById('scanBtn').disabled=false;
        document.getElementById('scanBtn').textContent='🔍 다시 스캔';
      } else if(d.state=='idle'){
        document.getElementById('status').textContent='';
        document.getElementById('scanBtn').disabled=false;
      } else {
        document.getElementById('status').textContent='측정 중... ('+d.state+')';
        setTimeout(pollStatus, 1000);
      }
    });
}
</script>
</body></html>)rawliteral";
  server.send(200, "text/html", html);
}

void handleScan() {
  String m = server.arg("member");
  String p = server.arg("part");
  if(m.length() > 0) selectedMember = m;
  if(p.length() > 0) selectedPart = p;

  if(scanState == IDLE || scanState == DONE){
    startScan();
    server.send(200, "application/json", "{\"status\":\"started\"}");
  } else {
    server.send(200, "application/json", "{\"status\":\"busy\"}");
  }
}

void handleStatus() {
  String state;
  switch(scanState){
    case IDLE: state="idle"; break;
    case AMBIENT: state="ambient"; break;
    case WHITE_LED: state="white"; break;
    case UV_LED: state="uv"; break;
    case MEASURING: state="measuring"; break;
    case DONE: state="done"; break;
    case SENT: state="done"; break;
  }
  server.send(200, "application/json", "{\"state\":\""+state+"\"}");
}

void initWebServer(){
  server.on("/", handleRoot);
  server.on("/scan", handleScan);
  server.on("/status", handleStatus);

  server.on("/stream", HTTP_GET, []() { // 카메라 초점 조절 테스트용
    WiFiClient client = server.client();
    
    String response = "HTTP/1.1 200 OK\r\n";
    response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
    client.print(response);

    while (client.connected()) {
      camera_fb_t *fb = esp_camera_fb_get();
      if (!fb) continue;

      client.printf("--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", fb->len);
      client.write(fb->buf, fb->len);
      client.print("\r\n");
      esp_camera_fb_return(fb);
      
      delay(100);
    }
  });

  server.begin();
}

#endif