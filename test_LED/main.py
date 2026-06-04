from fastapi import FastAPI, Request
from fastapi.responses import JSONResponse
from supabase import create_client
import base64
from datetime import datetime

app = FastAPI()

SUPABASE_URL = "https://roghtpxrhxkicleukrfz.supabase.co"
SUPABASE_KEY = "sb_publishable_i6hDxJVq5ikMLhcVz2KioA_S1f-tQEn"
supabase = create_client(SUPABASE_URL, SUPABASE_KEY)

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
    b64 = base64.b64encode(data).decode("utf-8")
    pending["white_img"] = b64
    print("✅ white 이미지 수신")
    return JSONResponse({"status": "ok"})

@app.post("/image/uv")
async def receive_uv(request: Request):
    data = await request.body()
    b64 = base64.b64encode(data).decode("utf-8")
    pending["uv_img"] = b64
    print("✅ uv 이미지 수신")
    save_to_supabase()
    return JSONResponse({"status": "ok"})

def save_to_supabase():
    supabase.table("scans").insert({
        "timestamp":  pending.get("timestamp"),
        "member":     pending.get("member"),
        "part":       pending.get("part"),
        "moisture":   pending.get("moisture"),
        "oil":        pending.get("oil"),
        "white_img":  pending.get("white_img"),
        "uv_img":     pending.get("uv_img"),
    }).execute()
    print(f"✅ Supabase 저장 완료: {pending.get('member')}/{pending.get('part')}")
    pending.clear()

@app.get("/scans")
def get_scans():
    res = supabase.table("scans").select("*").order("id", desc=True).execute()
    return res.data

@app.get("/scans/{member}")
def get_scans_by_member(member: str):
    res = supabase.table("scans").select("*").eq("member", member).order("id", desc=True).execute()
    return res.data

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)