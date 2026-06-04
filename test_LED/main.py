import os
import sqlite3
import shutil
from fastapi import FastAPI, UploadFile, File, Form
from fastapi.middleware.cors import CORSMiddleware

app = FastAPI()

# 통신 허용 설정
app.add_middleware(CORSMiddleware, allow_origins=["*"], allow_methods=["*"], allow_headers=["*"])

BASE_DIR = "C:/damda/data/raw"

def init_db():
    conn = sqlite3.connect("damda.db")
    cursor = conn.cursor()
    cursor.execute("""
        CREATE TABLE IF NOT EXISTS skin_records (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            moist_pct REAL,
            oil_pct REAL,
            image_path TEXT
        )
    """)
    conn.commit()
    conn.close()

init_db()

@app.post("/upload")
async def receive_data(
    member: str = Form(...),
    part: str = Form(...),
    white_img: UploadFile = File(...),
    uv_img: UploadFile = File(...),
    moisture: float = Form(...),
    oil: float = Form(...)
):
    save_path = os.path.join(BASE_DIR, member, part)
    os.makedirs(save_path, exist_ok=True)
    
    w_path = os.path.join(save_path, f"white_{white_img.filename}")
    u_path = os.path.join(save_path, f"uv_{uv_img.filename}")
    
    with open(w_path, "wb") as buffer: shutil.copyfileobj(white_img.file, buffer)
    with open(u_path, "wb") as buffer: shutil.copyfileobj(uv_img.file, buffer)
    
    conn = sqlite3.connect("damda.db")
    cursor = conn.cursor()
    cursor.execute("INSERT INTO skin_records (moist_pct, oil_pct, image_path) VALUES (?, ?, ?)", 
                   (moisture, oil, w_path))
    conn.commit()
    conn.close()
    return {"status": "success"}