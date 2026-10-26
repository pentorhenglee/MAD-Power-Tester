#!/usr/bin/env python3
"""
แปลง fm_*.png (240x320) -> ไฟล์ .c ที่เก็บภาพแบบ RLE สำหรับใส่ลง Flash

รูปแบบข้อมูล 1 ช่วง = 4 ไบต์
    [สี hi] [สี lo] [จำนวน hi] [จำนวน lo]
จบไฟล์ด้วยช่วงที่จำนวน = 0

รัน:  python3 png2rle.py
"""
import os
from PIL import Image

W, H = 240, 320
SRC = os.path.dirname(os.path.abspath(__file__))
PROJ = os.path.abspath(os.path.join(SRC, "..", ".."))
OUT_C = os.path.join(PROJ, "Core", "Src")
OUT_H = os.path.join(PROJ, "Core", "Inc")

rows = [l.split(None, 2) for l in open(os.path.join(SRC, "_screens.txt")).read().splitlines() if l.strip()]

def encode(path):
    im = Image.open(path).convert("RGB")
    assert im.size == (W, H), f"{path} ขนาด {im.size} ไม่ใช่ {W}x{H}"
    px = [((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3) for r, g, b in im.getdata()]
    out = bytearray()
    i = 0
    while i < len(px):
        c, n = px[i], 1
        while i + n < len(px) and px[i + n] == c and n < 65535:
            n += 1
        out += bytes((c >> 8, c & 0xFF, n >> 8, n & 0xFF))
        i += n
    out += bytes(4)                      # ช่วงปิดท้าย จำนวน = 0
    return out

names, total = [], 0
for _, sid, _ in rows:
    var = "img_" + sid.replace("-", "_")
    data = encode(os.path.join(SRC, f"fm_{sid}.png"))
    total += len(data)
    names.append((var, len(data)))
    with open(os.path.join(OUT_C, var + ".c"), "w") as f:
        f.write(f'/* สร้างอัตโนมัติจาก fm_{sid}.png โดย tools/screens/png2rle.py ห้ามแก้มือ */\n')
        f.write('#include <stdint.h>\n\n')
        f.write(f'const uint8_t {var}[] = {{\n')
        for k in range(0, len(data), 16):
            f.write("  " + ",".join(f"0x{b:02X}" for b in data[k:k+16]) + ",\n")
        f.write("};\n")

with open(os.path.join(OUT_H, "screens_img.h"), "w") as f:
    f.write("/* สร้างอัตโนมัติโดย tools/screens/png2rle.py ห้ามแก้มือ */\n")
    f.write("#ifndef SCREENS_IMG_H\n#define SCREENS_IMG_H\n\n#include <stdint.h>\n\n")
    for var, n in names:
        f.write(f"extern const uint8_t {var}[];   /* {n/1024:.1f} KB */\n")
    f.write("\n#endif\n")

print(f"เขียน {len(names)} ไฟล์ รวม {total/1024:.0f} KB")
for var, n in names:
    print(f"  {var:<22}{n/1024:>7.1f} KB")
