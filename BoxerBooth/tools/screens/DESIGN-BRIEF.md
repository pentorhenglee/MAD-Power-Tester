# Design brief - ส่งให้ Figma Make / GPT / Gemini

> **วิธีใช้:** ก๊อปทุกอย่างตั้งแต่เส้นคั่นข้างล่างลงไป แล้วแนบไฟล์ 3 ไฟล์ตามรายการท้ายเอกสาร

---

You are redesigning the UI of a physical arcade machine. The hardware is extremely constrained. A beautiful design that cannot be drawn on this hardware is worthless - read section 2 before designing anything.

I am attaching three files:
- `v_sheet.png` - the current design, all 8 screens. This is the baseline to beat.
- `v_screens.html` - the current source. It already contains a spacing token system in the `<style>` block. Keep that system or replace it with a better one, but keep it explicit.
- this brief.

## 1. The machine

A coin-op style arcade booth built by engineering students. A player walks up, plays three physical challenges back to back, and gets a combined score out of 3000.

| Stage | What the player physically does | Score |
|---|---|---|
| 1. SCREAM | Shouts into a microphone as loud and as long as possible, 10 seconds | 0–1000 |
| 2. SMASH | Nine illuminated arcade buttons in a 3×3 grid. One lights at random; hit it; a new one lights. 30 seconds | 0–1000 |
| 3. PUNCH | A bag hangs on a swinging arm. Player presses a green button to arm, then punches. Three punches, best one counts | 0–1000 |

Scoring details that the UI should express:
- SCREAM accumulates energy = (loudness above threshold) × time. **The score only ever rises. Pausing to breathe stops accumulation but never loses progress.** The UI should make this obvious without words.
- SMASH score = number of correct hits.
- PUNCH score = peak impact of the best of three punches.

### Screen flow

```
IDLE ─(PIR senses a person)-> ATTRACT ⇄ ALL TIME BEST   (alternate every 5s)
                                │ screen tapped
                                ▼
            STAGE_INTRO -> STAGE_RUN -> STAGE_RESULT ──(3 stages done)-> FINAL_RESULT
                  ▲                         │                              │
                  └┘                     (15s)┘-> IDLE
```

### Who looks at it

- Viewing distance **1–2 metres**, standing, in a bright room with ~15 other noisy student projects around.
- While playing the player is **screaming, slapping buttons, or punching**. They can glance at the screen for about half a second at a time.
- Spectators queue behind and watch. The screen is as much for them as for the player.
- Nobody reads a paragraph. Nobody reads small text.

## 2. Hardware limits - absolute, not negotiable

**Display:** ILI9341, **240 × 320 pixels, portrait**, 16-bit RGB565. Single-point touch only.

**Only these drawing operations exist. Nothing else.**

```
fill_screen(colour)
rect(x, y, w, h, colour)            // filled
rect_outline(x, y, w, h, colour)    // 1px outline
hline(x, y, w, colour)
vline(x, y, h, colour)
circle(x, y, r, colour)             // filled
circle_outline(x, y, r, colour)
text(string, x, y, colour, size, background_colour)
full_screen_bitmap(array)           // ENTIRE screen only, cannot be positioned
```

**There is no:** gradient, transparency, alpha, anti-aliasing, rounded corner, variable line thickness, diagonal line, triangle, polygon, rotated text, proportional font, or positioned sprite.

Anything you draw that is not in that list must either be baked into a full-screen bitmap, or approximated with the primitives above (for example a ring built from many small squares, which is what the current SCREAM screen does).

**Font:** one fixed bitmap font, cell **6 × 8 px** at size 1, integer scaling only. **ASCII only - it cannot render Thai or any non-Latin script.** All on-screen text must be English. Uppercase reads best at distance.

| Size | Cell | Chars across 240 px |
|---|---|---|
| 1 | 6 × 8 | 40 |
| 2 | 12 × 16 | 20 |
| 3 | 18 × 24 | 13 |
| 4 | 24 × 32 | 10 |
| 5 | 30 × 40 | 8 |
| 6 | 36 × 48 | 6 |

Check every string against this table. A 14-character word at size 3 does not fit.

**Colour:** the library predefines eleven. Custom RGB565 is possible but keep the palette tight and deliberate.

```
BLACK  WHITE  RED  GREEN  BLUE  CYAN  MAGENTA  YELLOW
DARKGREEN  DARKCYAN  GREENYELLOW
```

**Redraw speed - this shapes everything.** Pixels go out over SPI one byte at a time. Repainting the whole screen is visibly slow. The main loop runs about every 50 ms.

So every screen splits into:
- **Static layer** - drawn once when the screen is entered (background, frames, labels, headings, icons)
- **Dynamic regions** - small rectangles repainted each frame (numbers, bars, indicators)

**Budget: no more than about 15 % of the screen area may be repainted per frame.** Design so changing values live inside small fixed boxes.

Erasing works by redrawing text over itself with a background colour - which only works if the new string is the **same character length**, so numeric fields are space-padded to a fixed width, and **must sit on a flat single-colour background**.

## 3. What is wrong with the current design

Be honest and fix these; do not just restyle.

1. It reads as a wireframe, not as a product. Everything is a rectangle or a circle.
2. The three stage screens do not feel like one family beyond sharing a border colour.
3. The SCREAM screen shows a ring, a waveform and a timer - unclear which one the player should look at.
4. The PUNCH screen has long dead moments while the player resets the arm, and the screen does nothing during them.
5. ALL TIME BEST is a plain list with no sense of occasion.
6. The lightning bolts on ATTRACT are decoration with no meaning.

## 4. What to produce

**One self-contained HTML file**, in exactly this shape, because it feeds an automated pipeline:

- Eight `<div class="screen">` elements stacked vertically, in this order:
  `attract, board, intro, scream, smash, punch, stageresult, final`
- Each **exactly 240 px wide × 320 px tall**, `position: relative`, no margin, no gap, no page padding. The document is therefore 240 × 2560. They will be screenshotted as one strip and sliced.
- All children absolutely positioned with **integer pixel** `left` / `top` / `width` / `height`. No percentages, no flexbox, no transforms.
- A token block of CSS custom properties at the top for the spacing grid, the colour palette and the type scale. State the grid in a comment.
- Icons and illustrations as **inline `<svg>`**. No external images, no `<img>`, no icon fonts, no emoji.
- Monospace font for anything that will be drawn by firmware; a display font (Google Fonts link is fine) only for things baked into a bitmap.

**Mark every dynamic region.** On each element whose value changes at runtime, add:

```html
data-dyn="score" data-fmt="%4d"
```

`data-dyn` is a short lowercase name; `data-fmt` is the printf format the firmware will use (so I know the fixed character width). Every `data-dyn` element must sit on a flat single-colour background. This is how the layout gets turned into firmware constants automatically - an element without it will be treated as static artwork.

Also return, as a short table: for each screen, which regions are dynamic and roughly how often each updates.

**Give me three distinct directions**, not three recolours of one. Let them genuinely disagree about section 5. For each, one line on what it optimises for and what it gives up, plus the weakest point of your own direction and what you would test first with real players.

## 5. Take a position on these

- During SCREAM, which single thing earns the largest element: live loudness, accumulated score, or remaining time? The player's eyes are half shut.
- The nine buttons physically light up in front of the player. Should the screen mirror the grid at all, or is that redundant and the space better spent?
- What should PUNCH show during the dead time while the arm is being reset?
- Should a stage score be visible while that stage is being played? It motivates, but it also pulls the player's attention off the physical task.
- How much should the design serve the queue of spectators rather than the player?

## 6. Hard rules

1. Never use a drawing operation that is not in section 2.
2. No Thai or non-ASCII characters on screen.
3. Every coordinate inside 240 × 320; every value an integer.
4. Readable at 2 metres - justify any text below size 2.
5. Respect the 15 % redraw budget and mark dynamic regions with `data-dyn`.
6. Check every string length against the font table.

---
---

# ไฟล์ที่ต้องแนบ - 3 ไฟล์

| ไฟล์ | ทำไมต้องให้ |
|---|---|
| `DESIGN-BRIEF.md` (ไฟล์นี้) | ข้อจำกัดฮาร์ดแวร์ + รูปแบบผลลัพธ์ที่ต้องการ |
| `v_sheet.png` | ของเดิมทั้ง 8 หน้า ให้มันเห็นว่าต้องทำให้ดีกว่านี้ ไม่ใช่เริ่มใหม่จากศูนย์ |
| `v_screens.html` | ซอร์สเดิม มีระบบโทเคนระยะอยู่แล้ว มันจะได้ต่อยอดแทนที่จะคิดใหม่ |

ทั้ง 3 ไฟล์อยู่ในโฟลเดอร์เดียวกันนี้

## ถ้าเครื่องมือนั้นคืน HTML ไม่ได้

เรียงตามที่ผมเอาไปต่อได้ง่ายที่สุด

1. **HTML ตามสเปกข้อ 4** - ดีที่สุด ต่อได้ทันที
2. **SVG ต่อหน้า** ขนาด 240×320 พร้อมพิกัดเป็นจำนวนเต็ม - ผมแปลงเองได้
3. **ไฟล์ Figma** - บอกมา ผมดึงพิกัดผ่าน MCP ได้ แต่ต้องตั้งเฟรมเป็น 240×320 เป๊ะ
4. **รูป PNG อย่างเดียว** - แย่ที่สุด ผมต้องไล่วัดพิกัดเองทุกชิ้น ช้าและคลาดเคลื่อน

## สิ่งที่คุ้มที่สุดที่จะขอจากเครื่องมือพวกนี้

ถ้าเวลาน้อย ขอแค่ 2 อย่างนี้ก็พอ ผมทำส่วนที่เหลือได้

1. **ไอคอน SVG** - ไมค์ กระสอบทราย มงกุฎ สายฟ้า ถ้วยรางวัล ให้สวยกว่าที่ผมวาด ส่งมาเป็น SVG ล้วน พื้นโปร่ง
2. **ชุดสี** - บอกมาเป็นค่า hex ว่าแต่ละด่านควรใช้สีอะไร พื้นหลังควรเป็นอะไร

สองอย่างนี้คือจุดที่เครื่องมือออกแบบเก่งกว่าผมจริง ส่วนการวางพิกัดให้ตรงกับโค้ดผมทำเองเร็วกว่าและแม่นกว่า
