#!/usr/bin/env python3
"""Render the Mi Home quick-actions design mockup (not band firmware)."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

OUT = Path(__file__).with_name("mihome-6-actions.png")
W, H = 672, 960
BG = (11, 15, 20)
TILE = (21, 28, 36)
BORDER = (38, 50, 65)
ACCENT = (255, 105, 0)
ACCENT_DIM = (42, 26, 14)
TEXT = (242, 246, 250)
MUTED = (138, 151, 166)
GREEN = (52, 199, 89)
RED = (255, 69, 58)
BLUE = (10, 132, 255)

BOLD = "/System/Library/Fonts/Supplemental/Arial Bold.ttf"
REG = "/System/Library/Fonts/Supplemental/Arial.ttf"
def font(path, size):
    return ImageFont.truetype(path, size)

def text(draw, xy, s, f, fill):
    draw.text(xy, s, font=f, fill=fill)

def center_text(draw, cx, y, s, f, fill):
    box = draw.textbbox((0, 0), s, font=f)
    draw.text((cx - (box[2] - box[0]) / 2, y), s, font=f, fill=fill)

def icon_robot(d, cx, cy):
    d.rounded_rectangle((cx-24, cy-14, cx+24, cy+18), 8, outline=ACCENT, width=4)
    d.ellipse((cx-10, cy-22, cx+10, cy-6), outline=ACCENT, width=4)
    d.ellipse((cx-5, cy-18, cx-1, cy-14), fill=ACCENT)
    d.ellipse((cx+3, cy-18, cx+7, cy-14), fill=ACCENT)
    d.line((cx-18, cy+8, cx+18, cy+8), fill=ACCENT, width=3)
    d.line((cx-12, cy+26, cx-12, cy+18), fill=ACCENT, width=4)
    d.line((cx+12, cy+26, cx+12, cy+18), fill=ACCENT, width=4)
    d.arc((cx-18, cy+20, cx-2, cy+34), 200, 340, fill=ACCENT, width=3)
    d.arc((cx+2, cy+20, cx+18, cy+34), 200, 340, fill=ACCENT, width=3)

def icon_plug(d, cx, cy):
    d.rounded_rectangle((cx-20, cy-12, cx+20, cy+20), 8, outline=ACCENT, width=4)
    d.line((cx-8, cy-20, cx-8, cy-10), fill=ACCENT, width=5)
    d.line((cx+8, cy-20, cx+8, cy-10), fill=ACCENT, width=5)
    d.line((cx, cy+20, cx, cy+30), fill=ACCENT, width=4)
    d.line((cx, cy+30, cx+14, cy+30), fill=ACCENT, width=4)
    d.line((cx+14, cy+30, cx+14, cy+22), fill=ACCENT, width=4)

def icon_fish(d, cx, cy):
    d.ellipse((cx-22, cy-14, cx+14, cy+14), outline=ACCENT, width=4)
    d.polygon([(cx+12, cy), (cx+28, cy-14), (cx+28, cy+14)], outline=ACCENT)
    d.ellipse((cx-12, cy-6, cx-8, cy-2), fill=ACCENT)
    d.arc((cx-6, cy+2, cx+8, cy+12), 20, 160, fill=ACCENT, width=3)
    d.ellipse((cx-30, cy-24, cx-24, cy-18), outline=ACCENT, width=2)
    d.ellipse((cx-20, cy-30, cx-16, cy-26), outline=ACCENT, width=2)

def icon_bulb(d, cx, cy):
    d.ellipse((cx-18, cy-22, cx+18, cy+14), outline=ACCENT, width=4)
    d.rounded_rectangle((cx-10, cy+12, cx+10, cy+26), 4, outline=ACCENT, width=4)
    d.line((cx-10, cy+18, cx+10, cy+18), fill=ACCENT, width=3)
    for dx, dy in [(-28, -10), (28, -10), (-24, 18), (24, 18), (0, -30)]:
        d.line((cx+dx*0.6, cy+dy*0.6, cx+dx, cy+dy), fill=ACCENT, width=3)

def icon_ac(d, cx, cy):
    d.rounded_rectangle((cx-26, cy-18, cx+26, cy+14), 8, outline=ACCENT, width=4)
    for y in (cy-6, cy+2):
        d.line((cx-18, y, cx+18, y), fill=ACCENT, width=3)
    d.arc((cx-10, cy+14, cx+10, cy+30), 200, 340, fill=ACCENT, width=3)
    d.line((cx-18, cy+10, cx+18, cy+10), fill=ACCENT, width=3)

def icon_curtain(d, cx, cy):
    d.line((cx-28, cy-18, cx+28, cy-18), fill=ACCENT, width=4)
    d.line((cx-28, cy-18, cx-28, cy+24), fill=ACCENT, width=3)
    d.line((cx+28, cy-18, cx+28, cy+24), fill=ACCENT, width=3)
    for x in (cx-18, cx-6, cx+6, cx+18):
        d.line((x, cy-12, x, cy+24), fill=ACCENT, width=3)
    d.line((cx-28, cy+24, cx-18, cy+18), fill=ACCENT, width=3)
    d.line((cx+28, cy+24, cx+18, cy+18), fill=ACCENT, width=3)

def main():
    img = Image.new("RGB", (W, H), (4, 6, 9))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle((0, 0, W-1, H-1), 26, fill=BG, outline=(28, 38, 50), width=2)

    # header
    d.ellipse((26, 30, 34, 38), fill=GREEN)
    text(d, (46, 20), "Mi Home", font(BOLD, 30), ACCENT)
    text(d, (48, 56), "6 быстрых действий", font(REG, 15), MUTED)
    text(d, (W-120, 22), "10:09", font(BOLD, 20), TEXT)
    d.rounded_rectangle((W-64, 28, W-28, 46), 4, outline=MUTED, width=2)
    d.rectangle((W-60, 32, W-36, 42), fill=GREEN)
    d.line((30, 82, W-30, 82), fill=(26, 35, 46), width=1)

    tiles = [
        ("Пылесос", "Быстрая уборка", "Готов", "Запустить", icon_robot, None),
        ("Розетка", "Кофеварка", "Выключена", "Включить", icon_plug, ("ВЫКЛ", RED)),
        ("Кормушка", "Аквариум", "12 г", "Покормить", icon_fish, None),
        ("Свет", "Лампа", "Включена", "Выключить", icon_bulb, ("ВКЛ", GREEN)),
        ("Кондиционер", "Охлаждение", "24°", "Выключить", icon_ac, None),
        ("Шторы", "Гостиная", "50%", "Открыть", icon_curtain, None),
    ]
    mv, gap = 24, 16
    tw = (W - 2*mv - gap) // 2
    th = 252
    top = 104
    for i, (name, sub, state, action, icon, pill) in enumerate(tiles):
        col, row = i % 2, i // 2
        x = mv + col * (tw + gap)
        y = top + row * (th + gap)
        d.rounded_rectangle((x, y, x+tw, y+th), 20, fill=TILE, outline=BORDER, width=2)
        d.ellipse((x+18, y+18, x+78, y+78), fill=ACCENT_DIM, outline=(73, 42, 18), width=1)
        icon(d, x+48, y+48)
        text(d, (x+94, y+18), name, font(BOLD, 21), TEXT)
        text(d, (x+94, y+48), sub, font(REG, 15), MUTED)
        text(d, (x+94, y+72), state, font(REG, 16), MUTED)
        if pill:
            label, color = pill
            d.rounded_rectangle((x+tw-92, y+20, x+tw-16, y+46), 12, outline=color, width=2)
            center_text(d, x+tw-54, y+25, label, font(BOLD, 12), color)
        d.line((x+18, y+th-44, x+tw-18, y+th-44), fill=(29, 38, 50), width=1)
        text(d, (x+18, y+th-32), action, font(BOLD, 15), ACCENT)
        d.line((x+18, y+th-12, x+tw-18, y+th-12), fill=(29, 38, 50), width=1)

    # footer
    center_text(d, W//2, 892, "долгое нажатие — изменить действие", font(REG, 15), MUTED)
    text(d, (W-84, 918), "6 / 6", font(BOLD, 15), ACCENT)
    text(d, (24, 918), "синхронизировано с Pixel", font(REG, 16), GREEN)
    img.save(OUT)
    print(OUT)

if __name__ == "__main__":
    main()
