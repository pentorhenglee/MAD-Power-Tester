#!/usr/bin/env python3
"""
สร้าง fm_screens.html ให้ตรงกับ Figma Make "The Minimal Core" เป๊ะ
ที่มา: figma.com/make/exOFmDFTnklxUETMLPLFGO  (src/App.tsx)

ต่างจาก Figma 2 จุด ตามที่ Pentor สั่ง 2026-10-22
  1. GET READY แยกเป็น 3 หน้า (เลข 3 / 2 / 1) — ภาพนิ่งอันเดียวกัน firmware วาดแค่ตัวเลข
  2. PUNCH ต่อยครั้งเดียว -> ตัดจุด try1/3 ทุกหน้า และตัดหน้า PUNCH TRY RESULT ออก

แปลง React/TSX -> HTML ธรรมดา เพื่อเข้าท่อ Chrome headless -> PNG -> .h
รัน:  python3 build_screens.py
"""

C = dict(black="#000000", white="#f5f5f0", gray="#8b929b",
         cyan="#27c7e8", red="#ef4037", yellow="#f4ce20", green="#30d158")

SCREENS, out = [], []
def w(s): out.append(s)

def attrs(dyn=None, fmt=None):
    a = ""
    if dyn: a += f' data-dyn="{dyn}"'
    if fmt: a += f' data-fmt="{fmt}"'
    return a

def T(x, y, size, text, wd=None, h=None, color=None, bg=None, mono=False,
      italic=False, align="left", spacing=0, dyn=None, fmt=None):
    color = color or C["white"]; hh = h if h is not None else size
    cls = ("f-mono" if mono else "f-display") + (" f-italic" if italic else "")
    st = (f"left:{x}px;top:{y}px;height:{hh}px;font-size:{size}px;line-height:{hh}px;"
          f"color:{color};text-align:{align};letter-spacing:{spacing}px")
    if wd is not None: st += f";width:{wd}px"
    bgv = bg if bg else (C["black"] if dyn else None)
    if bgv: st += f";background:{bgv}"
    w(f'<div class="{cls}" style="{st}"{attrs(dyn,fmt)}>{text}</div>')

def Box(x, y, wd, h, bg=None, border=None, radius=None, dyn=None, fmt=None, inner=""):
    st = f"left:{x}px;top:{y}px;width:{wd}px;height:{h}px"
    if bg: st += f";background:{bg}"
    if border: st += f";border:{border}"
    if radius: st += f";border-radius:{radius}px"
    w(f'<div style="{st}"{attrs(dyn,fmt)}>{inner}</div>')

def Art(x, y, wd, h, inner, dyn=None, fmt=None):
    w(f'<svg style="left:{x}px;top:{y}px" width="{wd}" height="{h}" '
      f'viewBox="0 0 {wd} {h}" aria-hidden="true"{attrs(dyn,fmt)}>{inner}</svg>')

def Frame(tone): Box(6, 6, 228, 308, border=f"3px solid {tone}", radius=8)

def Bolt(x, y, s=1.0, fill="#f4ce20"):
    Art(x, y, round(26*s), round(44*s),
        f'<g transform="scale({s})"><path d="M16 0 L1 25 H11 L6 44 L25 15 H14.5 L20 0 Z" fill="{fill}"/></g>')

def Crown(x, y):
    Art(x, y, 24, 16,
        '<path d="M1 15 L2 3 L8 8 L12 1 L16 8 L22 3 L23 15 Z" fill="#f4ce20"/>'
        '<circle cx="2" cy="3" r="1.8" fill="#f4ce20"/><circle cx="12" cy="1.8" r="1.8" fill="#f4ce20"/>'
        '<circle cx="22" cy="3" r="1.8" fill="#f4ce20"/>')

def screen(sid, label):
    SCREENS.append((sid, label)); w(f'<div class="screen" data-screen="{sid}">')
def end(): w('</div>')

# ─────────────────────────── ไอคอน ───────────────────────────
ICON_SCREAM = (
    '<circle cx="54" cy="50" r="40" fill="#27c7e8"/>'
    '<path d="M30 34 L42 40 L30 44" fill="none" stroke="#000" stroke-width="5" stroke-linecap="round" stroke-linejoin="round"/>'
    '<path d="M78 34 L66 40 L78 44" fill="none" stroke="#000" stroke-width="5" stroke-linecap="round" stroke-linejoin="round"/>'
    '<ellipse cx="54" cy="66" rx="17" ry="18" fill="#000"/><ellipse cx="54" cy="76" rx="10" ry="6" fill="#0d6f86"/>'
    '<path d="M100 30 L116 22" stroke="#27c7e8" stroke-width="5" stroke-linecap="round"/>'
    '<path d="M102 50 L120 50" stroke="#27c7e8" stroke-width="5" stroke-linecap="round"/>'
    '<path d="M100 70 L116 78" stroke="#27c7e8" stroke-width="5" stroke-linecap="round"/>')

_BURST = [(8,22,20,30),(2,52,18,52),(8,82,20,74),(116,22,104,30),(122,52,106,52),(116,82,104,74)]
ICON_SMASH = (
    '<circle cx="62" cy="48" r="40" fill="#2a0e0c"/>'
    '<circle cx="62" cy="48" r="31" fill="#ef4037" stroke="#ff8a82" stroke-width="5"/>'
    '<rect x="44" y="30" width="16" height="8" rx="4" fill="#ffc2bd"/>'
    + "".join(f'<path d="M{a} {b} L{c} {d}" stroke="#ef4037" stroke-width="5" stroke-linecap="round"/>'
              for a, b, c, d in _BURST))

ICON_BAG = (
    '<path d="M80 0 V16" stroke="#8a7410" stroke-width="3"/>'
    '<circle cx="80" cy="19" r="4" fill="none" stroke="#8a7410" stroke-width="3"/>'
    '<g transform="rotate(-14 80 22)">'
    '<rect x="70" y="22" width="20" height="8" rx="2" fill="#8a7410"/>'
    '<rect x="58" y="28" width="44" height="92" rx="20" fill="#f4ce20"/>'
    '<rect x="58" y="28" width="16" height="92" rx="8" fill="#d8b012"/>'
    '<rect x="82" y="38" width="7" height="62" rx="3.5" fill="#ffe680"/>'
    '<rect x="58" y="104" width="44" height="6" fill="#c9a20e"/></g>'
    '<path d="M24 30 L12 52 H21 L16 70 L33 44 H24 L29 30 Z" fill="#f4ce20"/>'
    '<path d="M138 52 L126 74 H135 L130 92 L147 66 H138 L143 52 Z" fill="#f4ce20"/>'
    '<path d="M30 96 L18 104" stroke="#f4ce20" stroke-width="4" stroke-linecap="round"/>'
    '<path d="M126 24 L138 16" stroke="#f4ce20" stroke-width="4" stroke-linecap="round"/>'
    '<path d="M132 112 L146 118" stroke="#f4ce20" stroke-width="4" stroke-linecap="round"/>')

def bag_icon(x=40, y=56, s=1.0):
    Art(x, y, round(160*s), round(136*s), f'<g transform="scale({s})">{ICON_BAG}</g>')

STAGE = dict(
    scream=dict(n=1, name="SCREAM", tone=C["cyan"]),
    smash=dict(n=2, name="SMASH",  tone=C["red"]),
    punch=dict(n=3, name="PUNCH",  tone=C["yellow"]),
)
INTRO = dict(
    scream=dict(lines=["SHOUT AS LOUD", "AS YOU CAN", "FOR 10 SECONDS"]),
    smash=dict(lines=["HIT EVERY PAD", "THE MOMENT IT LIGHTS", "BEFORE TIME RUNS OUT"]),
    punch=dict(lines=["PRESS GREEN TO ARM", "PUNCH ON HIT NOW!", "ONE PUNCH ONLY"]),
)
RESULT = dict(scream=dict(score="668", nxt="NEXT: SMASH"),
              smash=dict(score="735", nxt="NEXT: PUNCH"),
              punch=dict(score="795", nxt="NEXT: TOTAL"))

# ══════════════════════ 01 ATTRACT ══════════════════════
def s_attract():
    screen("attract", "01 ATTRACT")
    Frame("#3a3e45")
    Bolt(18, 18, 1.1); Bolt(192, 18, 1.1, "#b8960c")
    Bolt(20, 268, 0.7, "#b8960c"); Bolt(202, 268, 0.7)
    T(0, 64, 78, "POWER",  wd=240, h=66, italic=True, align="center")
    T(0, 126, 78, "TESTER", wd=240, h=66, italic=True, align="center")
    T(0, 208, 30, "TAP TO START", wd=240, h=30, color=C["yellow"], align="center", spacing=1)
    Box(44, 250, 152, 1, bg="#2a2e35")
    T(0, 260, 15, "HIGH SCORE", wd=240, h=16, color=C["gray"], align="center", spacing=1)
    Crown(72, 287)
    T(102, 284, 24, "2830", wd=64, h=24, mono=True, dyn="highscore", fmt="%4d")
    end()

# ══════════════════════ 02 ALL TIME BEST ════════════════
def s_board():
    screen("board", "02 ALL TIME BEST")
    Frame(C["cyan"])
    T(0, 20, 38, "ALL TIME BEST", wd=240, h=38, color=C["cyan"], align="center", spacing=1)
    Box(24, 66, 192, 2, bg="#0d3a46")
    for i, sc in enumerate([2830, 2710, 2600, 2480, 2320]):
        y = 80 + i*40; first = (i == 0)
        T(30, y+4, 24, str(i+1), wd=20, h=24, color=C["yellow"] if first else C["gray"])
        if first: Crown(54, y+9)
        T(144, y+4, 24, str(sc), wd=72, h=24, mono=True, align="right",
          color=C["yellow"] if first else C["white"], dyn=f"rank{i+1}", fmt="%4d")
        if i < 4: Box(24, y+36, 192, 1, bg="#1c2026")
    Box(24, 278, 192, 2, bg="#0d3a46")
    T(0, 288, 18, "TAP TO START", wd=240, h=18, color=C["cyan"], align="center", spacing=1)
    end()

# ══════════════════════ STAGE INTRO ═════════════════════
def s_intro(key, label):
    st = STAGE[key]
    screen(f"intro-{key}", label)
    Frame(st["tone"])
    T(0, 20, 24, f"STAGE {st['n']} / 3", wd=240, h=24, align="center", spacing=1)
    if key == "scream": Art(58, 50, 124, 96, ICON_SCREAM)
    elif key == "smash": Art(58, 50, 124, 96, ICON_SMASH)
    else: bag_icon(64, 50, 0.7)
    T(0, 152, 56, st["name"], wd=240, h=52, color=st["tone"], align="center", spacing=1)
    for i, line in enumerate(INTRO[key]["lines"]):
        T(0, 212+i*18, 17, line, wd=240, h=18, color=st["tone"], align="center", spacing=1)
    Box(36, 274, 168, 28, bg=st["tone"], radius=4)
    T(36, 274, 19, "TAP TO START", wd=168, h=28, color=C["black"], align="center", spacing=1)
    end()

# ══════════════════════ GET READY 3 / 2 / 1 ═════════════
def s_ready(n, label, sid=None, head="GET READY", sub="STARTS IN",
            tone=None, head_size=44):
    tone = tone or C["yellow"]
    screen(sid or f"ready-{n}", label)
    Frame("#3a3e45")
    T(0, 40, head_size, head, wd=240, h=40, align="center", spacing=1)
    Art(50, 100, 140, 140,
        f'<circle cx="70" cy="70" r="62" fill="none" stroke="{tone}" stroke-width="6"/>'
        '<circle cx="70" cy="70" r="50" fill="none" stroke="#2a2e35" stroke-width="2"/>')
    T(96, 146, 48, str(n), wd=48, h=48, mono=True, color=tone,
      align="center", dyn="count", fmt="%1d")
    T(0, 266, 20, sub, wd=240, h=20, color=C["gray"], align="center", spacing=1)
    end()

# ══════════════════════ SCREAM RUN ══════════════════════
def s_scream(label):
    screen("scream", label)
    Frame(C["cyan"])
    T(0, 18, 44, "SCREAM", wd=240, h=40, color=C["cyan"], align="center", spacing=1)
    track = 293.2; prog = track*0.624
    Art(50, 62, 140, 140,
        f'<circle cx="70" cy="70" r="56" fill="none" stroke="#0b3540" stroke-width="12" '
        f'stroke-linecap="round" stroke-dasharray="{track} 400" transform="rotate(120 70 70)"/>'
        f'<circle cx="70" cy="70" r="56" fill="none" stroke="#27c7e8" stroke-width="12" '
        f'stroke-linecap="round" stroke-dasharray="{prog} 400" transform="rotate(120 70 70)"/>',
        dyn="energyarc", fmt="%4d")
    T(72, 108, 40, "624", wd=96, h=40, mono=True, align="center", dyn="score", fmt="%3d")
    T(70, 152, 16, "/ 1000", wd=100, h=16, color=C["gray"], align="center")
    wave = [4,6,9,7,12,10,16,13,20,17,24,28,22,30,26,32,28,30,24,27,20,22,16,18,12,14,9,11,7,8,5,6,4]
    bars = "".join(f'<div style="left:{i*6}px;top:{17-round(h/2)}px;width:3px;height:{h}px;background:{C["cyan"]}"></div>'
                   for i, h in enumerate(wave))
    Box(21, 214, 198, 34, bg=C["black"], dyn="wave", fmt="%2d", inner=bars)
    T(76, 264, 32, "06.2", wd=72, h=32, mono=True, align="right", dyn="time", fmt="%04.1f")
    T(150, 268, 28, "s", wd=20, h=28)
    end()

# ══════════════════════ SMASH RUN ═══════════════════════
def s_smash(label):
    screen("smash", label)
    Frame(C["red"])
    T(0, 18, 44, "SMASH", wd=240, h=40, color=C["red"], align="center", spacing=1)
    Box(47, 66, 146, 146, bg="#101215", border="2px solid #262a30", radius=10)
    for i in range(9):
        x = 54 + (i % 3)*47; y = 73 + (i//3)*47; on = (i == 5)
        Box(x, y, 38, 38, radius=19, bg="#ef4037" if on else "#3a3e46",
            border="3px solid #ff8a82" if on else "3px solid #23262b", dyn=f"pad{i}", fmt="%1d")
        Box(x+10, y+8, 10, 5, radius=3, bg="#ffc2bd" if on else "#50555e")
    T(22, 226, 20, "TIME", wd=60, h=20, color=C["red"], spacing=1)
    T(140, 228, 16, "12.8", wd=64, h=16, mono=True, align="right", dyn="time", fmt="%4.1f")
    T(205, 226, 20, "s", wd=14, h=20)
    Box(22, 252, 196, 6, bg="#3d1412")
    Box(22, 252, 84, 6, bg=C["red"], dyn="timebar", fmt="%3d")
    T(22, 272, 20, "HITS", wd=60, h=20, color=C["red"], spacing=1)
    Box(22, 295, 22, 3, bg=C["red"])
    T(120, 270, 24, "12/30", wd=96, h=24, mono=True, align="right", dyn="hits", fmt="%2d/30")
    end()

# ══════════════════════ PUNCH RUN (ต่อยครั้งเดียว) ═══════
def s_punch(label):
    screen("punch", label)
    Frame(C["yellow"])
    T(0, 18, 44, "PUNCH", wd=240, h=40, color=C["yellow"], align="center", spacing=1)
    bag_icon(40, 56)
    Box(68, 198, 104, 28, bg=C["yellow"], radius=6)
    T(80, 204, 16, "READY", wd=80, h=16, mono=True, color=C["black"], bg=C["yellow"],
      align="center", dyn="state", fmt="%-5s")
    T(0, 242, 17, "PRESS GREEN TO ARM", wd=240, h=18, align="center", spacing=1)
    T(0, 266, 17, "THEN PUNCH ONCE", wd=240, h=18, align="center", spacing=1)
    end()

# ═════════ PUNCH ARMED — พื้นเหลืองเต็มจอ อ่านจากไกลได้ทันที ═════════
def s_punch_hit(label):
    screen("punch-hit", label)
    Box(0, 0, 240, 320, bg=C["yellow"])
    Frame(C["black"])
    T(0, 18, 44, "PUNCH", wd=240, h=40, color=C["black"], align="center", spacing=1)
    Box(60, 66, 120, 3, bg=C["black"])
    Bolt(20, 110, 1.4, "#000"); Bolt(184, 110, 1.4, "#000")
    T(0, 84, 78, "HIT",  wd=240, h=66, color=C["black"], italic=True, align="center")
    T(0, 146, 78, "NOW!", wd=240, h=66, color=C["black"], italic=True, align="center")
    Box(36, 244, 168, 28, bg=C["black"], radius=4)
    T(36, 244, 19, "PUNCH THE BAG", wd=168, h=28, color=C["yellow"], align="center", spacing=1)
    end()

# ══════════════════════ STAGE RESULT ════════════════════
def s_result(key, label):
    st, rs = STAGE[key], RESULT[key]
    screen(f"result-{key}", label)
    Frame(st["tone"])
    T(0, 18, 44, st["name"], wd=240, h=40, color=st["tone"], align="center", spacing=1)
    Box(28, 74, 184, 150, border=f'3px solid {st["tone"]}', radius=8)
    T(0, 94, 22, "SCORE", wd=240, h=22, align="center", spacing=1)
    T(48, 128, 48, rs["score"], wd=144, h=48, mono=True, align="center", dyn="score", fmt="%4d")
    T(0, 186, 20, "/ 1000", wd=240, h=20, color=C["gray"], align="center")
    T(0, 258, 28, rs["nxt"], wd=240, h=28, color=st["tone"], align="center", spacing=1)
    end()

# ══════════════════════ FINAL ═══════════════════════════
def s_final(high, label):
    screen("final-high" if high else "final", label)
    Frame(C["yellow"] if high else C["white"])
    T(0, 20, 32, "TOTAL SCORE", wd=240, h=30, align="center", spacing=1)
    T(48, 60, 48, "2198", wd=144, h=48, mono=True, align="center",
      color=C["yellow"] if high else C["white"], dyn="total", fmt="%4d")
    T(0, 114, 20, "/ 3000", wd=240, h=20, color=C["gray"], align="center")
    Box(24, 144, 192, 1, bg="#2a2e35")
    for i, (name, sc, tone) in enumerate([("SCREAM","668",C["cyan"]),("SMASH","735",C["red"]),("PUNCH","795",C["yellow"])]):
        y = 156 + i*30
        Box(26, y+3, 4, 16, bg=tone)
        T(38, y, 21, name, wd=100, h=22, spacing=1)
        T(160, y+3, 16, sc, wd=54, h=16, mono=True, align="right", dyn=f"stage{i+1}", fmt="%3d")
    Box(24, 250, 192, 1, bg="#2a2e35")
    if high:
        Crown(108, 260)
        T(0, 280, 22, "NEW HIGH SCORE!", wd=240, h=22, color=C["yellow"], align="center", spacing=1)
    else:
        T(0, 260, 15, "ALL TIME BEST", wd=240, h=16, color=C["gray"], align="center", spacing=1)
        Crown(72, 285)
        T(102, 282, 24, "2830", wd=64, h=24, mono=True, dyn="best", fmt="%4d")
    end()

# ══════════════════════ OUT OF ORDER ════════════════════
def s_error(label):
    screen("error", label)
    Frame(C["red"])
    Art(70, 48, 100, 88,
        '<path d="M50 6 L95 82 H5 Z" fill="none" stroke="#ef4037" stroke-width="7" stroke-linejoin="round"/>'
        '<rect x="46" y="32" width="8" height="28" rx="3" fill="#ef4037"/>'
        '<circle cx="50" cy="70" r="5" fill="#ef4037"/>')
    T(0, 150, 40, "OUT OF ORDER", wd=240, h=40, color=C["red"], align="center", spacing=1)
    T(0, 206, 22, "PLEASE CALL STAFF", wd=240, h=22, align="center", spacing=1)
    Box(24, 246, 192, 1, bg="#2a2e35")
    T(70, 264, 20, "CODE", wd=50, h=20, color=C["gray"], spacing=1)
    T(122, 266, 16, "E03", wd=48, h=16, mono=True, dyn="code", fmt="E%02d")
    end()

# ───── ลำดับ = ลำดับการเล่นจริง ─────
s_attract()
s_board()
s_intro("scream", "03 INTRO SCREAM")
s_ready(3, "04 GET READY 3")
s_ready(2, "05 GET READY 2")
s_ready(1, "06 GET READY 1")
s_ready(2, "07 STAY QUIET 2", sid="quiet-2", head="STAY QUIET",
        sub="MEASURING ROOM", tone=C["cyan"], head_size=38)
s_ready(1, "08 STAY QUIET 1", sid="quiet-1", head="STAY QUIET",
        sub="MEASURING ROOM", tone=C["cyan"], head_size=38)
s_scream("09 SCREAM")
s_result("scream", "10 RESULT SCREAM")
s_intro("smash", "11 INTRO SMASH")
s_smash("12 SMASH")
s_result("smash", "13 RESULT SMASH")
s_intro("punch", "14 INTRO PUNCH")
s_punch("15 PUNCH")
s_punch_hit("16 PUNCH HIT NOW")
s_result("punch", "17 RESULT PUNCH")
s_final(False, "18 FINAL SCORE")
s_final(True,  "19 FINAL NEW HIGH")
s_error("20 OUT OF ORDER")

CSS = """
@import url("https://fonts.googleapis.com/css2?family=Barlow+Condensed:ital,wght@0,700;0,800;1,800;1,900&family=IBM+Plex+Mono:wght@700&display=swap");
*{margin:0;padding:0;box-sizing:border-box}
html,body{width:240px;background:#000;overflow:hidden}
.screen{position:relative;width:240px;height:320px;overflow:hidden;background:#000}
.screen div,.screen svg{position:absolute;display:block}
.f-display{font-family:"Barlow Condensed",sans-serif;font-weight:800;text-transform:uppercase;white-space:nowrap}
.f-italic{font-style:italic;font-weight:900;letter-spacing:-1px}
.f-mono{font-family:"IBM Plex Mono",monospace;font-weight:700;white-space:nowrap;font-variant-numeric:tabular-nums}
"""
n = len(SCREENS)
CSS += f"html,body{{height:{n*320}px}}\n"
open("fm_screens.html", "w").write(
    '<!doctype html>\n<html lang="en"><head><meta charset="utf-8">\n<style>'
    + CSS + '</style></head><body>\n' + "\n".join(out) + '\n</body></html>\n')
open("_screens.txt", "w").write("".join(f"{i} {sid} {label}\n" for i, (sid, label) in enumerate(SCREENS)))
print(f"เขียน fm_screens.html — {n} หน้า (240 x {n*320})")
for i, (sid, label) in enumerate(SCREENS): print(f"  {i:2d}  {sid:<15} {label}")
