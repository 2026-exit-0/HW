from fastapi import FastAPI, Request
from fastapi.responses import JSONResponse
import sqlite3
import os
from datetime import datetime
from pathlib import Path

app = FastAPI()

# ===== 설정 =====
DB_PATH      = Path("C:/damda/data/damda.db")
IMAGE_DIR    = Path("C:/damda/data/raw")
DB_PATH.parent.mkdir(parents=True, exist_ok=True)

# ===== DB 초기화 =====
def init_db():
    conn = sqlite3.connect(DB_PATH)
    c = conn.cursor()
    c.execute('''
        CREATE TABLE IF NOT EXISTS scans (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp TEXT,
            member TEXT,
            part TEXT,
            moisture REAL,
            oil REAL,
            white_img TEXT,
            uv_img TEXT
        )
    ''')
    conn.commit()
    conn.close()

init_db()

# 임시 저장용
pending = {}

# ===== 센서 데이터 수신 =====
@app.post("/sensor")
async def receive_sensor(request: Request):
    form = await request.form()
    moisture = float(form.get("moisture", 0))
    oil      = float(form.get("oil", 0))
    member   = form.get("member", "M1")
    part     = form.get("part", "FOREHEAD")
    ts       = datetime.now().strftime("%Y%m%d_%H%M%S")

    pending["timestamp"] = ts
    pending["member"]    = member
    pending["part"]      = part
    pending["moisture"]  = moisture
    pending["oil"]       = oil

    print(f"✅ 센서 수신: {member}/{part} moisture={moisture} oil={oil}")
    return JSONResponse({"status": "ok"})

# ===== white 이미지 수신 =====
@app.post("/image/white")
async def receive_white(request: Request):
    data = await request.body()
    ts     = pending.get("timestamp", datetime.now().strftime("%Y%m%d_%H%M%S"))
    member = pending.get("member", "M1")
    part   = pending.get("part", "FOREHEAD")

    save_dir = IMAGE_DIR / member / part / "white"
    save_dir.mkdir(parents=True, exist_ok=True)

    count     = len(list(save_dir.glob("*.jpg"))) + 1
    filename  = f"{ts}_{count:03d}.jpg"
    filepath  = save_dir / filename

    with open(filepath, "wb") as f:
        f.write(data)

    pending["white_img"] = str(filepath)
    print(f"✅ white 이미지 저장: {filepath}")
    return JSONResponse({"status": "ok"})

# ===== uv 이미지 수신 =====
@app.post("/image/uv")
async def receive_uv(request: Request):
    data = await request.body()
    ts     = pending.get("timestamp", datetime.now().strftime("%Y%m%d_%H%M%S"))
    member = pending.get("member", "M1")
    part   = pending.get("part", "FOREHEAD")

    save_dir = IMAGE_DIR / member / part / "uv"
    save_dir.mkdir(parents=True, exist_ok=True)

    count     = len(list(save_dir.glob("*.jpg"))) + 1
    filename  = f"{ts}_{count:03d}.jpg"
    filepath  = save_dir / filename

    with open(filepath, "wb") as f:
        f.write(data)

    pending["uv_img"] = str(filepath)
    print(f"✅ uv 이미지 저장: {filepath}")

    # uv까지 받으면 DB에 저장
    save_to_db()
    return JSONResponse({"status": "ok"})

# ===== DB 저장 =====
def save_to_db():
    conn = sqlite3.connect(DB_PATH)
    c = conn.cursor()
    c.execute('''
        INSERT INTO scans (timestamp, member, part, moisture, oil, white_img, uv_img)
        VALUES (?, ?, ?, ?, ?, ?, ?)
    ''', (
        pending.get("timestamp"),
        pending.get("member"),
        pending.get("part"),
        pending.get("moisture"),
        pending.get("oil"),
        pending.get("white_img"),
        pending.get("uv_img"),
    ))
    conn.commit()
    conn.close()
    print(f"✅ DB 저장 완료: {pending.get('member')}/{pending.get('part')}")
    pending.clear()

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)