#!/usr/bin/env python3
"""
แปลงรูป PNG -> ไฟล์ .h สำหรับ ILI9341_Draw_Image()

ใช้:  python3 png2header.py attract.png attract_img
ได้:  attract_img.h  ที่มี  const char attract_img[] = {...};

รูปต้องเป็น 320x240 พอดี ถ้าไม่ใช่สคริปต์จะย่อ/ครอปให้แล้วเตือน
"""
import sys, os
from PIL import Image

W, H = 240, 320   # แนวตั้ง (SCREEN_VERTICAL_1)

def main():
    if len(sys.argv) < 3:
        print(__doc__); sys.exit(1)
    src, name = sys.argv[1], sys.argv[2]
    im = Image.open(src).convert("RGB")
    if im.size != (W, H):
        print(f"เตือน: รูปขนาด {im.size} ไม่ใช่ {W}x{H} -> ย่อให้อัตโนมัติ")
        im = im.resize((W, H), Image.LANCZOS)

    # RGB888 -> RGB565 ไบต์สูงก่อน (big-endian) ตรงกับที่ไดรเวอร์ส่งออก SPI
    out = bytearray()
    for r, g, b in im.getdata():
        v = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        out.append(v >> 8); out.append(v & 0xFF)

    guard = name.upper() + "_H"
    path = name + ".h"
    with open(path, "w") as f:
        f.write(f"// สร้างจาก {os.path.basename(src)} โดย png2header.py\n")
        f.write(f"// {W}x{H} RGB565 big-endian = {len(out)} ไบต์\n")
        f.write(f"#ifndef {guard}\n#define {guard}\n\n")
        f.write(f"const char {name}[] = {{\n")
        for i in range(0, len(out), 16):
            f.write("  " + ",".join(f"0x{b:02X}" for b in out[i:i+16]) + ",\n")
        f.write("};\n\n#endif\n")

    print(f"เสร็จ: {path}")
    print(f"  ข้อมูล {len(out):,} ไบต์  ({len(out)/1024:.0f} KB ใน Flash)")
    print(f"  ขนาดไฟล์ {os.path.getsize(path)/1024/1024:.2f} MB")
    print(f"\nวิธีใช้ใน main.c:")
    print(f'  #include "{path}"')
    print(f"  ILI9341_Draw_Image({name}, SCREEN_HORIZONTAL_1);")

main()
