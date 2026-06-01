import os
import re
import json
import shutil
import csv
import time
from pathlib import Path
from datetime import datetime

# ===== 설정 =====
DOWNLOAD_DIR = Path.home() / "Downloads"
OUTPUT_DIR   = Path("C:/damda/data/raw")
MANIFEST_CSV = Path("C:/damda/data/manifest.csv")

MEMBERS = {"M1", "M2", "M3", "M4", "M5"}
PARTS   = {"FOREHEAD", "GLABELLA", "L_EYE", "R_EYE", "L_CHEEK", "R_CHEEK", "CHIN"}

# 파일명 패턴: 20260601_174523_M1_FOREHEAD_white.jpg
PATTERN = re.compile(
    r"^(\d{8}_\d{6})_(M[1-5])_(FOREHEAD|GLABELLA|L_EYE|R_EYE|L_CHEEK|R_CHEEK|CHIN)_(white|uv)\.jpg$"
)

def parse_filename(filename):
    m = PATTERN.match(filename)
    if not m:
        return None
    return {
        "timestamp": m.group(1),
        "member":    m.group(2),
        "part":      m.group(3),
        "light":     m.group(4),
    }

def get_pair(timestamp, member, part):
    """white, uv 두 파일이 모두 있는지 확인"""
    prefix = f"{timestamp}_{member}_{part}"
    white = DOWNLOAD_DIR / f"{prefix}_white.jpg"
    uv    = DOWNLOAD_DIR / f"{prefix}_uv.jpg"
    return (white, uv) if white.exists() and uv.exists() else None

def find_csv(timestamp, member, part):
    """같은 타임스탬프의 labels.csv 찾기"""
    # 다운로드 폴더의 모든 csv 중 가장 최근 것
    csvs = list(DOWNLOAD_DIR.glob("labels.csv"))
    return csvs[0] if csvs else None

def process_pair(timestamp, member, part):
    white_src, uv_src = get_pair(timestamp, member, part)

    # 저장 폴더 생성
    dest_white_dir = OUTPUT_DIR / member / part / "white"
    dest_uv_dir    = OUTPUT_DIR / member / part / "uv"
    dest_white_dir.mkdir(parents=True, exist_ok=True)
    dest_uv_dir.mkdir(parents=True, exist_ok=True)

    # 파일명: 타임스탬프_순번.jpg
    count = len(list(dest_white_dir.glob("*.jpg"))) + 1
    count_str = f"{count:03d}"
    base = f"{timestamp}_{count_str}"

    white_dst = dest_white_dir / f"{base}.jpg"
    uv_dst    = dest_uv_dir    / f"{base}.jpg"

    shutil.move(str(white_src), str(white_dst))
    shutil.move(str(uv_src),    str(uv_dst))
    print(f"✅ 이미지 저장: {member}/{part}/{count_str}")

    # JSON 생성 (센서 데이터)
    csv_file = find_csv(timestamp, member, part)
    sensor_data = {}
    if csv_file:
        with open(csv_file, newline='', encoding='utf-8') as f:
            rows = list(csv.DictReader(f))
            if rows:
                sensor_data = rows[0]
        os.remove(csv_file)

    json_data = {
        "timestamp": timestamp,
        "member":    member,
        "part":      part,
        "white_img": str(white_dst.relative_to(OUTPUT_DIR.parent)),
        "uv_img":    str(uv_dst.relative_to(OUTPUT_DIR.parent)),
        "sensor":    sensor_data
    }

    # white 폴더에 JSON 저장
    json_path = dest_white_dir / f"{base}.json"
    with open(json_path, 'w', encoding='utf-8') as f:
        json.dump(json_data, f, ensure_ascii=False, indent=2)

    # uv 폴더에도 JSON 저장
    json_uv_path = dest_uv_dir / f"{base}.json"
    with open(json_uv_path, 'w', encoding='utf-8') as f:
        json.dump(json_data, f, ensure_ascii=False, indent=2)

    print(f"✅ JSON 저장: {base}.json")

    # manifest.csv 갱신
    update_manifest(json_data, base)

def update_manifest(data, base):
    write_header = not MANIFEST_CSV.exists()
    MANIFEST_CSV.parent.mkdir(parents=True, exist_ok=True)

    with open(MANIFEST_CSV, 'a', newline='', encoding='utf-8') as f:
        fieldnames = [
            'id', 'timestamp', 'member', 'part',
            'white_img', 'uv_img',
            'moistPct', 'oilPct', 'skinType', 'oilLevel',
            'ambientLux', 'reflectedLux'
        ]
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        if write_header:
            writer.writeheader()

        sensor = data.get('sensor', {})
        writer.writerow({
            'id':            base,
            'timestamp':     data['timestamp'],
            'member':        data['member'],
            'part':          data['part'],
            'white_img':     data['white_img'],
            'uv_img':        data['uv_img'],
            'moistPct':      sensor.get('moistPct', ''),
            'oilPct':        sensor.get('oilPct', ''),
            'skinType':      sensor.get('skinType', ''),
            'oilLevel':      sensor.get('oilLevel', ''),
            'ambientLux':    sensor.get('ambientLux', ''),
            'reflectedLux':  sensor.get('reflectedLux', ''),
        })
    print(f"✅ manifest.csv 갱신")

def watch():
    print(f"👁  다운로드 폴더 감시 중: {DOWNLOAD_DIR}")
    print(f"📁 저장 위치: {OUTPUT_DIR}")
    print("Ctrl+C 로 종료\n")

    processed = set()

    while True:
        try:
            # 다운로드 폴더에서 패턴에 맞는 파일 찾기
            for f in DOWNLOAD_DIR.glob("*_white.jpg"):
                info = parse_filename(f.name)
                if not info:
                    continue

                key = f"{info['timestamp']}_{info['member']}_{info['part']}"
                if key in processed:
                    continue

                # white + uv 쌍이 모두 있으면 처리
                pair = get_pair(info['timestamp'], info['member'], info['part'])
                if pair:
                    print(f"\n📥 새 파일 감지: {key}")
                    process_pair(info['timestamp'], info['member'], info['part'])
                    processed.add(key)

            time.sleep(2)

        except KeyboardInterrupt:
            print("\n종료합니다.")
            break
        except Exception as e:
            print(f"❌ 오류: {e}")
            time.sleep(2)

if __name__ == "__main__":
    watch()