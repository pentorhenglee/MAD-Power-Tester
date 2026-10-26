#!/usr/bin/env python3
"""
แปลงรูปถ่าย -> C array RGB565 สำหรับ ILI9341_Draw_Image()

ทำงานเดียวกับ LCD Image Converter (สไลด์หน้า 42) แค่ไม่ต้องเปิด GUI
ประกอบผืนผ้าใบขาว 320x240 แล้ววางรูปลงในกรอบ PIC_* ให้เอง (ครอปตรงกลาง ไม่บีบรูป)

วิธีใช้:
    python3 tools/img2h.py รูปของฉัน.jpg

ผลลัพธ์: Core/Inc/photo.h  ->  const char screen2_img[]  (153,600 ไบต์)
"""
import sys, os
from PIL import Image

SCREEN_W, SCREEN_H = 320, 240
PIC_X0, PIC_Y0, PIC_X1, PIC_Y1 = 20, 60, 120, 180   # ต้องตรงกับ #define ใน main.c
ARRAY_NAME = "screen2_img"
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Core", "Inc", "photo.h")


def fit_center_crop(img, w, h):
    """ครอปตรงกลางให้ได้สัดส่วน w:h แล้วย่อเป็น w x h"""
    dst_ratio = w / h
    if img.width / img.height > dst_ratio:        # รูปกว้างเกิน -> ตัดข้าง
        new_w = int(img.height * dst_ratio)
        left = (img.width - new_w) // 2
        img = img.crop((left, 0, left + new_w, img.height))
    else:                                          # รูปสูงเกิน -> ตัดบนล่าง
        new_h = int(img.width / dst_ratio)
        top = (img.height - new_h) // 2
        img = img.crop((0, top, img.width, top + new_h))
    return img.resize((w, h), Image.LANCZOS)


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)

    box_w, box_h = PIC_X1 - PIC_X0, PIC_Y1 - PIC_Y0

    photo = Image.open(sys.argv[1]).convert("RGB")
    photo = fit_center_crop(photo, box_w, box_h)

    canvas = Image.new("RGB", (SCREEN_W, SCREEN_H), (255, 255, 255))
    canvas.paste(photo, (PIC_X0, PIC_Y0))

    # RGB888 -> RGB565 ไบต์สูงก่อน (big-endian) ตามที่ ILI9341 กิน
    data = bytearray()
    for r, g, b in canvas.getdata():
        v = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        data.append(v >> 8)
        data.append(v & 0xFF)

    out = os.path.normpath(OUT)
    with open(out, "w") as f:
        f.write("#ifndef PHOTO_H\n#define PHOTO_H\n\n")
        f.write(f"/* {SCREEN_W}x{SCREEN_H} RGB565 big-endian, {len(data)} bytes */\n")
        f.write(f"const char {ARRAY_NAME}[] = {{\n")
        for i in range(0, len(data), 16):
            f.write("  " + ", ".join(f"0x{b:02X}" for b in data[i:i+16]) + ",\n")
        f.write("};\n\n#endif\n")

    print(f"เขียน  {out}")
    print(f"จำนวน  {len(data)} ไบต์  ({len(data)/1024:.0f} KB ลงแฟลช)")
    print(f"รูปอยู่ในกรอบ x {PIC_X0}-{PIC_X1}, y {PIC_Y0}-{PIC_Y1}  (ตรงกับ PIC_* ใน main.c)")


if __name__ == "__main__":
    main()
