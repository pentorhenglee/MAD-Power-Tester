#!/usr/bin/env python3
"""เรนเดอร์ fm_screens.html -> fm_<id>.png ทีละหน้า + fm_sheet.png (ตารางรวม)"""
import subprocess, os
from PIL import Image, ImageDraw

rows = [l.split(None, 2) for l in open("_screens.txt").read().splitlines() if l.strip()]
N = len(rows)
CHROME = "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"
subprocess.run([CHROME, "--headless", "--disable-gpu", "--hide-scrollbars",
                f"--window-size=240,{N*320}", "--screenshot=_strip.png",
                "--virtual-time-budget=4000", "fm_screens.html"], check=True)
strip = Image.open("_strip.png").convert("RGB")
print("strip", strip.size)
assert strip.size == (240, N*320), f"ขนาดแถบไม่ตรง: {strip.size}"

for i, sid, label in rows:
    i = int(i)
    strip.crop((0, i*320, 240, (i+1)*320)).save(f"fm_{sid}.png")

cols, pad, head = 6, 12, 18
rowsn = (N + cols - 1)//cols
sheet = Image.new("RGB", (cols*(240+pad)+pad, rowsn*(320+head+pad)+pad), (26, 26, 30))
d = ImageDraw.Draw(sheet)
for k, (i, sid, label) in enumerate(rows):
    cx = pad + (k % cols)*(240+pad); cy = pad + (k//cols)*(320+head+pad)
    d.text((cx+2, cy+3), label, fill=(190, 195, 205))
    sheet.paste(Image.open(f"fm_{sid}.png"), (cx, cy+head))
sheet.save("fm_sheet.png")
sheet.resize((sheet.width*2, sheet.height*2), Image.NEAREST).save("fm_sheet_big.png")
print("fm_sheet.png", sheet.size)
