#!/usr/bin/env python3
"""
อบตัวเลข 0-9 . / เป็นภาพ เพื่อให้ค่าที่เฟิร์มแวร์วาดทับใช้ฟอนต์เดียวกับภาพพื้นหลัง

เก็บเป็น "ความทึบ" 1 ไบต์ต่อจุด (0 = พื้นหลังล้วน, 255 = สีตัวอักษรเต็ม)
เวลาวาดจึงเลือกสีตัวอักษรกับสีพื้นได้อิสระ ไม่ต้องเก็บภาพแยกทุกสี
"""
import os, subprocess
from PIL import Image

SRC = os.path.dirname(os.path.abspath(__file__))
PROJ = os.path.abspath(os.path.join(SRC, "..", ".."))
CHROME = "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"

GLYPHS = "0123456789./"
SIZES = [16, 24, 32, 40, 48]          # ขนาดที่ดีไซน์ใช้จริง

cells = [(s, round(s * 0.6), s) for s in SIZES]   # (ขนาดฟอนต์, กว้าง, สูง)

html = ['<!doctype html><html><head><meta charset="utf-8"><style>',
        '@import url("https://fonts.googleapis.com/css2?family=IBM+Plex+Mono:wght@700&display=swap");',
        '*{margin:0;padding:0;box-sizing:border-box}',
        'body{background:#000}',
        '.row{position:relative;white-space:nowrap;font-size:0}',
        '.g{display:inline-block;vertical-align:top;color:#fff;background:#000;',
        'font-family:"IBM Plex Mono",monospace;font-weight:700;text-align:center}',
        '</style></head><body>']
for size, w, h in cells:
    html.append(f'<div class="row" style="height:{h}px">')
    for ch in GLYPHS:
        html.append(f'<span class="g" style="width:{w}px;height:{h}px;'
                    f'font-size:{size}px;line-height:{h}px">{ch}</span>')
    html.append('</div>')
html.append('</body></html>')
open(os.path.join(SRC, "digits.html"), "w").write("\n".join(html))

total_h = sum(h for _, _, h in cells)
total_w = max(w * len(GLYPHS) for _, w, _ in cells)
subprocess.run([CHROME, "--headless", "--disable-gpu", "--hide-scrollbars",
                f"--window-size={total_w},{total_h}", "--screenshot=_digits.png",
                "--virtual-time-budget=4000", "digits.html"], cwd=SRC, check=True,
               capture_output=True)

sheet = Image.open(os.path.join(SRC, "_digits.png")).convert("L")
lines_c = ['/* สร้างอัตโนมัติจาก digits.html โดย tools/screens/gen_digits.py ห้ามแก้มือ */',
           '#include "digits.h"', '']
lines_h = ['/* สร้างอัตโนมัติโดย tools/screens/gen_digits.py ห้ามแก้มือ */',
           '#ifndef DIGITS_H', '#define DIGITS_H', '', '#include <stdint.h>', '',
           '/* ตัวอักษรที่มี: 0-9 แล้วตามด้วย . และ /  (เว้นวรรค = วาดพื้นอย่างเดียว) */',
           'typedef struct {', '    uint8_t w;', '    uint8_t h;',
           '    const uint8_t *alpha;   /* 12 ตัว เรียงต่อกัน ตัวละ w*h ไบต์ */',
           '} digit_font_t;', '']
y = 0
total = 0
for size, w, h in cells:
    data = bytearray()
    for i in range(len(GLYPHS)):
        cell = sheet.crop((i * w, y, (i + 1) * w, y + h))
        data += bytes(cell.getdata())
    y += h
    total += len(data)
    lines_c.append(f'static const uint8_t alpha{size}[] = {{')
    for k in range(0, len(data), 24):
        lines_c.append("  " + ",".join(str(b) for b in data[k:k+24]) + ",")
    lines_c.append('};')
    lines_c.append(f'const digit_font_t digits{size} = {{ {w}, {h}, alpha{size} }};')
    lines_c.append('')
    lines_h.append(f'extern const digit_font_t digits{size};   /* เซลล์ {w}x{h} */')

lines_h += ['', '#endif']
open(os.path.join(PROJ, "Core", "Src", "digits.c"), "w").write("\n".join(lines_c) + "\n")
open(os.path.join(PROJ, "Core", "Inc", "digits.h"), "w").write("\n".join(lines_h) + "\n")
print(f"อบตัวเลข {len(SIZES)} ขนาด รวม {total/1024:.0f} KB")
for size, w, h in cells:
    print(f"  digits{size:<3} เซลล์ {w}x{h}  ({len(GLYPHS)*w*h/1024:.1f} KB)")
