from fastapi import FastAPI, Request
from fastapi.responses import JSONResponse, HTMLResponse
from fastapi.middleware.cors import CORSMiddleware
from supabase import create_client
from datetime import datetime

app = FastAPI()

# CORS 허용 (프론트엔드에서 EC2로 직접 호출 가능하도록)
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)

SUPABASE_URL = "https://roghtpxrhxkicleukrfz.supabase.co"
SUPABASE_KEY = "sb_publishable_i6hDxJVq5ikMLhcVz2KioA_S1f-tQEn"
supabase = create_client(SUPABASE_URL, SUPABASE_KEY)

BUCKET_NAME = "scan-images"

# 기기별 pending 데이터 (ESP32_1, ESP32_2 등)
pending = {}

# 기기별 스캔 명령 큐
scan_commands = {}


# ── 스캔 명령 (프론트 → EC2 → ESP32 폴링) ─────────────────

@app.post("/scan-command")
async def set_scan_command(request: Request):
    """프론트엔드에서 스캔 명령 전송"""
    body = await request.json()
    device_id = body.get("device_id", "ESP32_1")
    scan_commands[device_id] = {
        "pending": True,
        "member": body.get("member", "M1"),
        "part":   body.get("part", "FOREHEAD"),
    }
    print(f"📡 스캔 명령 수신: {device_id} / {body.get('member')} / {body.get('part')}")
    return JSONResponse({"status": "ok"})

@app.get("/scan-command/{device_id}")
def get_scan_command(device_id: str):
    """ESP32가 주기적으로 폴링 — 명령 있으면 반환 후 초기화"""
    cmd = scan_commands.get(device_id, {})
    if cmd.get("pending"):
        scan_commands[device_id]["pending"] = False
        return JSONResponse({
            "pending": True,
            "member": cmd["member"],
            "part":   cmd["part"],
        })
    return JSONResponse({"pending": False})


# ── ESP32 데이터 수신 ──────────────────────────────────────

@app.post("/sensor")
async def receive_sensor(request: Request):
    form = await request.form()
    device_id = form.get("device_id", "ESP32_1")

    if device_id not in pending:
        pending[device_id] = {}

    pending[device_id]["timestamp"] = datetime.now().strftime("%Y%m%d_%H%M%S")
    pending[device_id]["member"]    = form.get("member", "M1")
    pending[device_id]["part"]      = form.get("part", "FOREHEAD")
    pending[device_id]["moisture"]  = float(form.get("moisture", 0))
    pending[device_id]["oil"]       = float(form.get("oil", 0))
    print(f"✅ 센서 수신 [{device_id}]: {pending[device_id]['member']}/{pending[device_id]['part']}")
    return JSONResponse({"status": "ok"})

@app.post("/image/white")
async def receive_white(request: Request):
    device_id = request.headers.get("X-Device-ID", "ESP32_1")
    data = await request.body()
    if device_id not in pending:
        pending[device_id] = {}
    pending[device_id]["white_img_bytes"] = data
    print(f"✅ white 이미지 수신 [{device_id}]")
    return JSONResponse({"status": "ok"})

@app.post("/image/uv")
async def receive_uv(request: Request):
    device_id = request.headers.get("X-Device-ID", "ESP32_1")
    data = await request.body()
    if device_id not in pending:
        pending[device_id] = {}
    pending[device_id]["uv_img_bytes"] = data
    print(f"✅ uv 이미지 수신 [{device_id}]")
    save_to_supabase(device_id)
    return JSONResponse({"status": "ok"})


# ── Supabase 저장 ──────────────────────────────────────────

def upload_image(image_bytes, filename):
    supabase.storage.from_(BUCKET_NAME).upload(
        filename,
        image_bytes,
        {"content-type": "image/jpeg"}
    )
    return supabase.storage.from_(BUCKET_NAME).get_public_url(filename)

def save_to_supabase(device_id: str):
    p = pending.get(device_id, {})
    timestamp = p.get("timestamp")
    member    = p.get("member")
    part      = p.get("part")

    white_url = None
    uv_url    = None

    if "white_img_bytes" in p:
        white_filename = f"{timestamp}_{member}_{part}_white.jpg"
        white_url = upload_image(p["white_img_bytes"], white_filename)
        print(f"✅ white 업로드 완료 [{device_id}]")

    if "uv_img_bytes" in p:
        uv_filename = f"{timestamp}_{member}_{part}_uv.jpg"
        uv_url = upload_image(p["uv_img_bytes"], uv_filename)
        print(f"✅ uv 업로드 완료 [{device_id}]")

    supabase.table("scans").insert({
        "timestamp": timestamp,
        "member":    member,
        "part":      part,
        "moisture":  p.get("moisture"),
        "oil":       p.get("oil"),
        "white_img": white_url,
        "uv_img":    uv_url,
    }).execute()
    print(f"✅ Supabase 저장 완료 [{device_id}]: {member}/{part}")
    pending.pop(device_id, None)


# ── 조회 ──────────────────────────────────────────────────

@app.get("/scans")
def get_scans():
    res = supabase.table("scans").select("*").order("id", desc=True).execute()
    return res.data

@app.get("/scans/{member}")
def get_scans_by_member(member: str):
    res = supabase.table("scans").select("*").eq("member", member).order("id", desc=True).execute()
    return res.data

@app.get("/view/{scan_id}", response_class=HTMLResponse)
def view_scan(scan_id: int):
    res = supabase.table("scans").select("*").eq("id", scan_id).execute()
    if not res.data:
        return "Not found"
    d = res.data[0]
    return f"""
    <html><body>
    <h2>{d['member']} / {d['part']}</h2>
    <p>수분: {d['moisture']} / 유분: {d['oil']}</p>
    <img src="{d['white_img']}" width="400">
    <img src="{d['uv_img']}" width="400">
    </body></html>
    """

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)
