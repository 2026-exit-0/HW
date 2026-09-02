"""
scan_images 다운로드 스크립트
================================
사용법:
  1. 이 파일과 scans_rows.csv를 같은 폴더에 놓거나,
     아래 CSV_PATH를 CSV 파일 경로로 수정하세요.
  2. 터미널에서 실행:
       python download_scan_images.py
  3. 이미지가 아래 구조로 저장됩니다:
       scan_images/
         WHITE_LED/
           M1/FOREHEAD/파일명.jpg
           M1/L_CHEEK/파일명.jpg
           ...
         UV/
           M1/FOREHEAD/파일명.jpg
           ...

필요 패키지:  pip install requests
"""

import csv
import os
import requests
from pathlib import Path
from urllib.parse import urlparse

# ── 설정 ─────────────────────────────────────────────
CSV_PATH   = "scans_rows_2.csv"          # CSV 파일 경로 (필요시 수정)
OUTPUT_DIR = Path("scan_images")       # 저장 폴더 이름 (필요시 수정)
TIMEOUT    = 20                        # 이미지 요청 타임아웃(초)
# ─────────────────────────────────────────────────────

success = 0
failed  = []

with open(CSV_PATH, newline='', encoding='utf-8') as f:
    reader = csv.DictReader(f)
    rows   = list(reader)

total = len(rows) * 2
print(f"총 {len(rows)}개 행 → {total}개 이미지 다운로드 시작\n")

for i, row in enumerate(rows, 1):
    member = row['member'].strip()   # M1, M2, M3 …
    part   = row['part'].strip()     # FOREHEAD, NOSE, L_CHEEK …

    for col, led_type in [('white_img', 'WHITE_LED'), ('uv_img', 'UV')]:
        url = row[col].strip()
        if not url:
            continue

        # 저장 경로: scan_images/WHITE_LED/M1/FOREHEAD/파일명.jpg
        save_dir  = OUTPUT_DIR / led_type / member / part
        save_dir.mkdir(parents=True, exist_ok=True)

        filename  = urlparse(url).path.split('/')[-1]
        save_path = save_dir / filename

        # 이미 있으면 스킵
        if save_path.exists():
            print(f"  ↩  (스킵) {led_type}/{member}/{part}/{filename}")
            success += 1
            continue

        try:
            resp = requests.get(url, timeout=TIMEOUT)
            if resp.status_code == 200:
                save_path.write_bytes(resp.content)
                success += 1
                print(f"  ✓  {led_type}/{member}/{part}/{filename}")
            else:
                failed.append((url, f"HTTP {resp.status_code}"))
                print(f"  ✗  [{resp.status_code}] {filename}")
        except Exception as e:
            failed.append((url, str(e)))
            print(f"  ✗  {filename}  →  {e}")

print(f"\n{'='*50}")
print(f"완료:  성공 {success}개  /  실패 {len(failed)}개")

if failed:
    print("\n[실패 목록]")
    for url, reason in failed:
        print(f"  • {url.split('/')[-1]}")
        print(f"    → {reason}")