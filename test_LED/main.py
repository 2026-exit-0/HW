from fastapi import FastAPI, Request
from fastapi.responses import JSONResponse, HTMLResponse
from supabase import create_client
from datetime import datetime

app = FastAPI()

SUPABASE_URL = "https://roghtpxrhxkicleukrfz.supabase.co"
SUPABASE_KEY = "sb_publishable_i6hDxJVq5ikMLhcVz2KioA_S1f-tQEn"
supabase = create_client(SUPABASE_URL, SUPABASE_KEY)

BUCKET_NAME = "scan-images"

pending = {}

@app.post("/sensor")
async def receive_sensor(request: Request):
    form = await request.form()
    pending["timestamp"] = datetime.now().strftime("%Y%m%d_%H%M%S")
    pending["member"]    = form.get("member", "M1")
    pending["part"]      = form.get("part", "FOREHEAD")
    pending["moisture"]  = float(form.get("moisture", 0))
    pending["oil"]       = float(form.get("oil", 0))
    print(f"✅ 센서 수신: {pending['member']}/{pending['part']}")
    return JSONResponse({"status": "ok"})

@app.post("/image/white")
async def receive_white(request: Request):
    data = await request.body()
    pending["white_img_bytes"] = data
    print("✅ white 이미지 수신")
    return JSONResponse({"status": "ok"})

@app.post("/image/uv")
async def receive_uv(request: Request):
    data = await request.body()
    pending["uv_img_bytes"] = data
    print("✅ uv 이미지 수신")
    save_to_supabase()
    return JSONResponse({"status": "ok"})

def upload_image(image_bytes, filename):
    supabase.storage.from_(BUCKET_NAME).upload(
        filename,
        image_bytes,
        {"content-type": "image/jpeg"}
    )
    return supabase.storage.from_(BUCKET_NAME).get_public_url(filename)

def save_to_supabase():
    timestamp = pending.get("timestamp")
    member = pending.get("member")
    part = pending.get("part")

    white_url = None
    uv_url = None

    if "white_img_bytes" in pending:
        white_filename = f"{timestamp}_{member}_{part}_white.jpg"
        white_url = upload_image(pending["white_img_bytes"], white_filename)
        print("✅ white 이미지 Storage 업로드 완료")

    if "uv_img_bytes" in pending:
        uv_filename = f"{timestamp}_{member}_{part}_uv.jpg"
        uv_url = upload_image(pending["uv_img_bytes"], uv_filename)
        print("✅ uv 이미지 Storage 업로드 완료")

    supabase.table("scans").insert({
        "timestamp":   timestamp,
        "member":      member,
        "part":        part,
        "moisture":    pending.get("moisture"),
        "oil":         pending.get("oil"),
        "white_img":   white_url,
        "uv_img":      uv_url,
    }).execute()
    print(f"✅ Supabase 저장 완료: {member}/{part}")
    pending.clear()

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