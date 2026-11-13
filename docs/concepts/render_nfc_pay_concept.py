#!/usr/bin/env python3
"""Render a concept UI for the band's NFC-pay screen (336x480).

This is a design mockup, not a working payment UI. It uses the same
336x480 canvas as the existing watchface and a dark AMOLED visual language.
Run with a Python environment that has Pillow:

    python3 docs/concepts/render_nfc_pay_concept.py

Output: docs/concepts/nfc-pay-screen-concept.png
"""
from pathlib import Path
import math

from PIL import Image, ImageChops, ImageDraw, ImageFont, ImageFilter

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "docs/concepts/nfc-pay-screen-concept.png"

# Logical screen size, matching the p67glt watchface canvas.
SW, SH = 336, 480
S = 2  # render at 2x, downsample for crisp text and icons
W, H = SW * S, SH * S

# Poster geometry (logical px).
PW, PH = 780, 1220
MARGIN = 40
GAP_X = 28
GAP_Y = 16
TITLE_H = 92
ROW1_Y = 116
ROW2_Y = 652

# Palette.
INK = (244, 248, 252)
MUTED = (146, 166, 186)
DIM = (92, 111, 130)
LINE = (30, 43, 55)
SURFACE = (14, 26, 36)
GREEN = (0, 201, 138)
GREEN_DARK = (0, 163, 110)
MIR_GREEN = (0, 168, 107)
BLUE = (77, 163, 255)
AMBER = (255, 176, 32)
RED = (255, 92, 92)
CARD_TOP = (11, 42, 74)
CARD_BOTTOM = (22, 76, 119)

SFNS = "/System/Library/Fonts/SFNS.ttf"
PTSANS = "/System/Library/Fonts/Supplemental/PTSans.ttc"


def px(value):
    return int(round(value * S))


def rgb(color, alpha=255):
    return (color[0], color[1], color[2], alpha)


def font(size, weight="Regular"):
    f = ImageFont.truetype(SFNS, px(size))
    try:
        f.set_variation_by_name(weight)
    except Exception:
        pass
    return f


def pfont(size):
    return ImageFont.truetype(PTSANS, px(size), index=0)


def rounded_mask(w, h, radius):
    mask = Image.new("L", (w, h), 0)
    ImageDraw.Draw(mask).rounded_rectangle(
        [0, 0, w - 1, h - 1], radius=radius, fill=255
    )
    return mask


def vertical_gradient(w, h, top, bottom):
    img = Image.new("RGB", (w, h))
    d = ImageDraw.Draw(img)
    for y in range(h):
        t = y / max(1, h - 1)
        color = tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3))
        d.line([(0, y), (w, y)], fill=color)
    return img


def glow(w, h, cx, cy, radius, color, alpha):
    layer = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    d.ellipse(
        [px(cx - radius), px(cy - radius), px(cx + radius), px(cy + radius)],
        fill=rgb(color, alpha),
    )
    return layer.filter(ImageFilter.GaussianBlur(px(26)))


def contactless(draw, cx, cy, size, color, width=2.0, arcs=4):
    """Draw a contactless symbol with the dot at the left and arcs to the right."""
    r0 = size * 0.11
    draw.ellipse(
        [px(cx - r0), px(cy - r0), px(cx + r0), px(cy + r0)],
        fill=rgb(color),
    )
    for i in range(arcs):
        r = size * (0.30 + i * 0.20)
        draw.arc(
            [px(cx - r), px(cy - r), px(cx + r), px(cy + r)],
            start=-54,
            end=54,
            fill=rgb(color),
            width=px(width),
        )


def phone_icon(draw, cx, cy, size, color, width=1.6, slash=False):
    w = size * 0.55
    h = size
    r = size * 0.12
    box = [px(cx - w / 2), px(cy - h / 2), px(cx + w / 2), px(cy + h / 2)]
    draw.rounded_rectangle(box, radius=px(r), outline=rgb(color), width=px(width))
    draw.ellipse(
        [px(cx - 1), px(cy + h / 2 - 5), px(cx + 1), px(cy + h / 2 - 3)],
        fill=rgb(color),
    )
    if slash:
        draw.line(
            [px(cx - size * 0.55), px(cy + size * 0.55), px(cx + size * 0.55), px(cy - size * 0.55)],
            fill=rgb(color),
            width=px(width + 0.4),
        )


def battery(draw, x, y, w, h, pct, color, fill_color):
    draw.rounded_rectangle(
        [px(x), px(y), px(x + w), px(y + h)],
        radius=px(2.4),
        outline=rgb(color),
        width=px(1.2),
    )
    draw.rounded_rectangle(
        [px(x + 2), px(y + 2), px(x + 2 + (w - 4) * pct), px(y + h - 2)],
        radius=px(1.2),
        fill=rgb(fill_color),
    )
    draw.rounded_rectangle(
        [px(x + w + 1.5), px(y + h * 0.32), px(x + w + 3.5), px(y + h * 0.68)],
        radius=px(0.8),
        fill=rgb(color),
    )


def check(draw, cx, cy, size, color, width):
    draw.line(
        [px(cx - size * 0.38), px(cy + size * 0.02), px(cx - size * 0.10), px(cy + size * 0.30)],
        fill=rgb(color),
        width=px(width),
        joint="curve",
    )
    draw.line(
        [px(cx - size * 0.10), px(cy + size * 0.30), px(cx + size * 0.42), px(cy - size * 0.28)],
        fill=rgb(color),
        width=px(width),
        joint="curve",
    )


def spaced(draw, xy, text, f, fill, tracking=1.2):
    x, y = xy
    for ch in text:
        draw.text((px(x), px(y)), ch, font=f, fill=rgb(fill), anchor="lt")
        x += draw.textlength(ch, font=f) / S + tracking
    return x


def pill(draw, x, y, w, h, fill, outline, radius=None):
    r = radius if radius is not None else h / 2
    draw.rounded_rectangle(
        [px(x), px(y), px(x + w), px(y + h)],
        radius=px(r),
        fill=rgb(fill) if fill else None,
        outline=rgb(outline) if outline else None,
        width=px(1),
    )


def gradient_button(size, top, bottom, radius):
    w, h = size
    img = vertical_gradient(w, h, top, bottom).convert("RGBA")
    img.putalpha(rounded_mask(w, h, radius))
    return img


def card_image(w, h, radius, dim=False):
    pw, ph = px(w), px(h)
    img = vertical_gradient(pw, ph, CARD_TOP, CARD_BOTTOM).convert("RGBA")
    mask = rounded_mask(pw, ph, radius)
    img.putalpha(mask)
    ov = Image.new("RGBA", (pw, ph), (0, 0, 0, 0))
    d = ImageDraw.Draw(ov)
    d.ellipse([px(w - 34), px(-42), px(w + 82), px(74)], fill=(255, 255, 255, 10))
    d.ellipse([px(-50), px(h - 42), px(70), px(h + 78)], fill=(0, 201, 138, 14))
    d.polygon(
        [
            (px(w * 0.52), 0),
            (px(w), 0),
            (px(w), px(h * 0.62)),
            (px(w * 0.76), px(h * 0.54)),
        ],
        fill=(255, 255, 255, 7),
    )
    img = Image.alpha_composite(img, ov)
    if dim:
        shade = Image.new("RGBA", (pw, ph), (2, 6, 10, 0))
        shade.putalpha(mask.point(lambda v: v * 118 // 255))
        img = Image.alpha_composite(img, shade)
    return img


def draw_card(screen, x, y, w, h, dim=False):
    radius = px(20)
    card = card_image(w, h, radius, dim=dim)
    screen.alpha_composite(card, (px(x), px(y)))
    d = ImageDraw.Draw(screen)
    # Border
    d.rounded_rectangle(
        [px(x), px(y), px(x + w), px(y + h)],
        radius=px(20),
        outline=rgb((42, 90, 134), 120),
        width=px(1),
    )
    # Mir badge
    badge_w, badge_h = 42, 21
    d.rounded_rectangle(
        [px(x + 15), px(y + 13), px(x + 15 + badge_w), px(y + 13 + badge_h)],
        radius=px(6),
        fill=rgb(MIR_GREEN),
    )
    d.text(
        (px(x + 15 + badge_w / 2), px(y + 13 + badge_h / 2 - 0.5)),
        "МИР",
        font=font(10.5, "Bold"),
        fill=rgb((255, 255, 255)),
        anchor="mm",
    )
    # Contactless
    contactless(d, x + w - 27, y + 27, 22, (255, 255, 255), width=1.5)
    # Number
    d.text(
        (px(x + 16), px(y + 74)),
        "•••• 4417",
        font=font(24, "Semibold"),
        fill=rgb(INK),
        anchor="lt",
    )
    # Type
    d.text(
        (px(x + 16), px(y + 108)),
        "Дебетовая карта",
        font=font(10.5, "Regular"),
        fill=rgb((169, 196, 220)),
        anchor="lt",
    )
    # Faint MIR wordmark, bottom right.
    d.text(
        (px(x + w - 16), px(y + h - 14)),
        "MIR",
        font=font(8.5, "Medium"),
        fill=rgb((255, 255, 255), 70),
        anchor="rb",
    )


def status_bar(d, pct=82):
    d.text((px(24), px(15)), "21:42", font=font(12, "Medium"), fill=rgb(INK), anchor="lt")
    battery(d, 264, 18, 21, 9, pct / 100.0, MUTED, GREEN)
    d.text((px(312), px(16)), f"{pct}%", font=font(9.5, "Medium"), fill=rgb(MUTED), anchor="rt")
    # Tiny Bluetooth rune.
    cx, cy, sz = 251, 22, 5.5
    d.line([px(cx), px(cy - sz), px(cx), px(cy + sz)], fill=rgb(MUTED), width=px(1))
    d.line([px(cx), px(cy - sz), px(cx + sz * 0.8), px(cy - sz * 0.35)], fill=rgb(MUTED), width=px(1))
    d.line([px(cx + sz * 0.8), px(cy - sz * 0.35), px(cx - sz * 0.5), px(cy + sz * 0.2)], fill=rgb(MUTED), width=px(1))
    d.line([px(cx), px(cy + sz), px(cx + sz * 0.8), px(cy + sz * 0.35)], fill=rgb(MUTED), width=px(1))
    d.line([px(cx + sz * 0.8), px(cy + sz * 0.35), px(cx - sz * 0.5), px(cy - sz * 0.2)], fill=rgb(MUTED), width=px(1))


def header(d):
    spaced(d, (24, 40), "NFC PAY", font(9.5, "Semibold"), GREEN, tracking=1.5)
    d.text((px(312), px(40)), "MIR", font=font(8.5, "Semibold"), fill=rgb(MUTED), anchor="rt")
    d.line([px(24), px(60), px(312), px(60)], fill=rgb(LINE), width=px(0.8))


def connection_chips(d, state="ready"):
    y = 386
    total = 96 + 8 + 82
    x = (SW - total) / 2
    for label, w in (("Pixel", 96), ("NFC", 82)):
        ok = state != "offline" if label == "Pixel" else state in ("ready", "waiting", "success")
        dot = GREEN if ok else AMBER
        pill(d, x, y, w, 24, (14, 26, 36), LINE)
        d.ellipse([px(x + 10), px(y + 9), px(x + 16), px(y + 15)], fill=rgb(dot))
        d.text((px(x + 21), px(y + 12)), label, font=font(9.5, "Medium"), fill=rgb(MUTED), anchor="lm")
        x += w + 8


def footer(d, text):
    d.text((px(SW / 2), px(463)), text, font=font(8.5, "Regular"), fill=rgb(DIM), anchor="mt")


def center_lines(draw, cx, y, lines, f, fill, spacing=4):
    probe = draw.textbbox((0, 0), "Ag", font=f)
    line_h = (probe[3] - probe[1]) / S
    yy = y
    for line in lines:
        draw.text((px(cx), px(yy)), line, font=f, fill=rgb(fill), anchor="mt")
        yy += line_h + spacing


def title_block(d, title, subtitle, y, size=23):
    d.text((px(SW / 2), px(y)), title, font=font(size, "Semibold"), fill=rgb(INK), anchor="mt")
    d.text((px(SW / 2), px(y + 30)), subtitle, font=font(11.5, "Regular"), fill=rgb(MUTED), anchor="mt")


def main_button(screen, label, y=414, w=240, h=44, x=None, kind="primary"):
    if x is None:
        x = (SW - w) / 2
    d = ImageDraw.Draw(screen)
    if kind == "primary":
        btn = gradient_button((px(w), px(h)), (0, 210, 146), (0, 158, 108), px(h / 2))
        screen.alpha_composite(btn, (px(x), px(y)))
        text_color = (3, 20, 14)
        outline = None
    elif kind == "white":
        d.rounded_rectangle(
            [px(x), px(y), px(x + w), px(y + h)],
            radius=px(h / 2),
            fill=rgb((244, 248, 252)),
        )
        text_color = (8, 16, 22)
        outline = None
    elif kind == "amber":
        d.rounded_rectangle(
            [px(x), px(y), px(x + w), px(y + h)],
            radius=px(h / 2),
            fill=rgb(AMBER),
        )
        text_color = (30, 20, 2)
        outline = None
    else:
        d.rounded_rectangle(
            [px(x), px(y), px(x + w), px(y + h)],
            radius=px(h / 2),
            outline=rgb((42, 58, 74)),
            width=px(1),
        )
        text_color = INK
        outline = True

    f = font(15.5, "Semibold")
    label_w = d.textlength(label, font=f) / S
    icon_size = 17
    icon_w = icon_size + 8 if kind in ("primary", "white") else 0
    start_x = x + (w - (label_w + icon_w)) / 2
    if icon_w:
        contactless(d, start_x + icon_size * 0.35, y + h / 2, icon_size, text_color if kind != "primary" else text_color, width=1.4)
    d.text(
        (px(start_x + icon_w), px(y + h / 2)),
        label,
        font=f,
        fill=rgb(text_color),
        anchor="lm",
    )


def screen_ready():
    screen = Image.new("RGBA", (W, H), (0, 0, 0, 255))
    bg = vertical_gradient(W, H, (7, 16, 25), (0, 0, 0)).convert("RGBA")
    screen.alpha_composite(bg)
    screen.alpha_composite(glow(W, H, 168, 120, 170, BLUE, 18))
    screen.alpha_composite(glow(W, H, 168, 300, 150, GREEN, 16))
    d = ImageDraw.Draw(screen)
    status_bar(d)
    header(d)
    draw_card(screen, 24, 70, 288, 148)
    d = ImageDraw.Draw(screen)
    # Card dots
    for i in range(3):
        cx = 168 + (i - 1) * 10
        color = GREEN if i == 0 else (42, 58, 74)
        d.ellipse([px(cx - 2.4), px(229), px(cx + 2.4), px(233.8)], fill=rgb(color))
    # Large contactless
    contactless(d, 168, 274, 54, GREEN, width=2.4, arcs=4)
    title_block(d, "Готов к оплате", "Поднесите браслет к терминалу", 318, size=23)
    connection_chips(d, "ready")
    main_button(screen, "Оплатить", y=414, w=240, h=44, kind="primary")
    footer(d, "Сеанс 120 с · только этот браслет")
    screen.putalpha(ImageChops.multiply(screen.getchannel("A"), rounded_mask(W, H, px(40))))
    return screen


def screen_waiting():
    screen = Image.new("RGBA", (W, H), (0, 0, 0, 255))
    bg = vertical_gradient(W, H, (7, 16, 25), (0, 0, 0)).convert("RGBA")
    screen.alpha_composite(bg)
    screen.alpha_composite(glow(W, H, 168, 120, 170, BLUE, 16))
    screen.alpha_composite(glow(W, H, 168, 290, 150, BLUE, 20))
    d = ImageDraw.Draw(screen)
    status_bar(d, pct=81)
    header(d)
    draw_card(screen, 24, 70, 288, 148, dim=True)
    d = ImageDraw.Draw(screen)
    for i in range(3):
        cx = 168 + (i - 1) * 10
        color = BLUE if i == 0 else (42, 58, 74)
        d.ellipse([px(cx - 2.4), px(229), px(cx + 2.4), px(233.8)], fill=rgb(color))
    # Progress ring around the contactless icon.
    cx, cy, r = 168, 274, 31
    d.arc([px(cx - r), px(cy - r), px(cx + r), px(cy + r)], start=0, end=360, fill=rgb((30, 48, 64)), width=px(3))
    d.arc([px(cx - r), px(cy - r), px(cx + r), px(cy + r)], start=-90, end=120, fill=rgb(BLUE), width=px(3))
    contactless(d, cx, cy, 46, BLUE, width=2.2, arcs=4)
    title_block(d, "Не убирайте браслет", "Обмен с терминалом…", 318, size=21)
    connection_chips(d, "waiting")
    main_button(screen, "Отмена", y=414, w=208, h=42, kind="outline")
    footer(d, "Ожидание ответа терминала")
    screen.putalpha(ImageChops.multiply(screen.getchannel("A"), rounded_mask(W, H, px(40))))
    return screen


def screen_success():
    screen = Image.new("RGBA", (W, H), (0, 0, 0, 255))
    bg = vertical_gradient(W, H, (5, 18, 15), (0, 0, 0)).convert("RGBA")
    screen.alpha_composite(bg)
    screen.alpha_composite(glow(W, H, 168, 190, 150, GREEN, 24))
    d = ImageDraw.Draw(screen)
    status_bar(d, pct=81)
    header(d)
    # Check badge
    cx, cy, r = 168, 180, 34
    d.ellipse([px(cx - r), px(cy - r), px(cx + r), px(cy + r)], fill=rgb((0, 201, 138), 18), outline=rgb(GREEN), width=px(1.6))
    check(d, cx, cy, 34, GREEN, 4.2)
    # Text
    d.text((px(168), px(236)), "Оплачено", font=font(28, "Semibold"), fill=rgb(INK), anchor="mt")
    # Amount with ruble sign from PT Sans (SFNS has no U+20BD).
    fn = font(30, "Semibold")
    fr = pfont(33)
    num, rub = "1 420,50", "₽"
    wn = d.textlength(num, font=fn) / S
    wr = d.textlength(rub, font=fr) / S
    total = wn + wr + 4
    ax = 168 - total / 2
    d.text((px(ax), px(286)), num, font=fn, fill=rgb(INK), anchor="lm")
    d.text((px(ax + wn + 4), px(286)), rub, font=fr, fill=rgb(MUTED), anchor="lm")
    d.text((px(168), px(316)), "Терминал · 21:42", font=font(10.5, "Regular"), fill=rgb(MUTED), anchor="mt")
    # Small card row
    row_w, row_h = 168, 28
    rx = (SW - row_w) / 2
    pill(d, rx, 346, row_w, row_h, (14, 26, 36), LINE)
    d.rounded_rectangle([px(rx + 9), px(353), px(rx + 37), px(367)], radius=px(4), fill=rgb(MIR_GREEN))
    d.text((px(rx + 23), px(360)), "МИР", font=font(6.8, "Bold"), fill=rgb((255, 255, 255)), anchor="mm")
    d.text((px(rx + 45), px(360)), "•••• 4417", font=font(9.5, "Medium"), fill=rgb(INK), anchor="lm")
    d.text((px(rx + row_w - 10), px(360)), "Дебетовая", font=font(7.5, "Regular"), fill=rgb(MUTED), anchor="rm")
    main_button(screen, "Готово", y=398, w=208, h=42, kind="white")
    footer(d, "Чек появится в приложении")
    screen.putalpha(ImageChops.multiply(screen.getchannel("A"), rounded_mask(W, H, px(40))))
    return screen


def screen_offline():
    screen = Image.new("RGBA", (W, H), (0, 0, 0, 255))
    bg = vertical_gradient(W, H, (20, 14, 5), (0, 0, 0)).convert("RGBA")
    screen.alpha_composite(bg)
    screen.alpha_composite(glow(W, H, 168, 190, 150, AMBER, 18))
    d = ImageDraw.Draw(screen)
    status_bar(d, pct=80)
    header(d)
    cx, cy, r = 168, 178, 34
    d.ellipse([px(cx - r), px(cy - r), px(cx + r), px(cy + r)], fill=rgb(AMBER, 16), outline=rgb(AMBER), width=px(1.6))
    phone_icon(d, cx, cy, 34, AMBER, width=1.8, slash=True)
    center_lines(d, 168, 230, ["Телефон", "не подключён"], font(23, "Semibold"), INK, spacing=2)
    center_lines(
        d,
        168,
        296,
        ["Откройте приложение на Pixel", "и подключите браслет"],
        font(11, "Regular"),
        MUTED,
        spacing=3,
    )
    # Connection chips with Pixel offline.
    y = 338
    total = 96 + 8 + 82
    x = (SW - total) / 2
    for label, w, dot, text_color in (("Pixel", 96, AMBER, MUTED), ("NFC", 82, (58, 72, 86), DIM)):
        pill(d, x, y, w, 24, (14, 26, 36), LINE)
        d.ellipse([px(x + 10), px(y + 9), px(x + 16), px(y + 15)], fill=rgb(dot))
        d.text((px(x + 21), px(y + 12)), label, font=font(9.5, "Medium"), fill=rgb(text_color), anchor="lm")
        x += w + 8
    main_button(screen, "Повторить", y=380, w=240, h=44, kind="amber")
    d.text((px(168), px(436)), "Открыть Pixel", font=font(10, "Medium"), fill=rgb(BLUE), anchor="mt")
    footer(d, "Без Pixel оплата невозможна")
    screen.putalpha(ImageChops.multiply(screen.getchannel("A"), rounded_mask(W, H, px(40))))
    return screen


def render_poster():
    poster = vertical_gradient(px(PW), px(PH), (7, 10, 14), (11, 15, 21)).convert("RGBA")
    # Soft atmosphere.
    poster.alpha_composite(glow(px(PW), px(PH), 190, 170, 300, BLUE, 12))
    poster.alpha_composite(glow(px(PW), px(PH), 610, 980, 320, GREEN, 8))
    d = ImageDraw.Draw(poster)
    # Title.
    d.text((px(40), px(26)), "NFC", font=font(30, "Semibold"), fill=rgb(INK), anchor="lt")
    nfc_w = d.textlength("NFC", font=font(30, "Semibold")) / S
    d.text((px(40 + nfc_w + 3), px(26)), "Pay", font=font(30, "Semibold"), fill=rgb(GREEN), anchor="lt")
    d.text(
        (px(40), px(66)),
        "Концепт экрана браслета · 336 × 480 · AMOLED",
        font=font(11.5, "Regular"),
        fill=rgb(MUTED),
        anchor="lt",
    )
    pill(d, PW - 40 - 112, 31, 112, 24, (14, 22, 30), LINE)
    d.text((px(PW - 40 - 56), px(43)), "4 состояния", font=font(9.5, "Medium"), fill=rgb(MUTED), anchor="mm")
    d.text(
        (px(PW - 40), px(65)),
        "NFC · HCE",
        font=font(8.5, "Medium"),
        fill=rgb(DIM),
        anchor="rt",
    )
    d.line([px(40), px(92), px(PW - 40), px(92)], fill=rgb(LINE), width=px(0.8))

    specs = [
        ("ready", 40, ROW1_Y, "01", "Готов", "Карта выбрана, связь есть"),
        ("waiting", 404, ROW1_Y, "02", "Обмен", "Не убирайте браслет"),
        ("success", 40, ROW2_Y, "03", "Успех", "Подтверждение оплаты"),
        ("offline", 404, ROW2_Y, "04", "Нет связи", "Проверьте Pixel"),
    ]
    builders = {
        "ready": screen_ready,
        "waiting": screen_waiting,
        "success": screen_success,
        "offline": screen_offline,
    }
    for state, x, y, num, title, desc in specs:
        # Bezel
        d.rounded_rectangle(
            [px(x - 5), px(y - 5), px(x + SW + 5), px(y + SH + 5)],
            radius=px(45),
            fill=rgb((5, 7, 10)),
            outline=rgb((28, 39, 50)),
            width=px(1),
        )
        screen = builders[state]()
        poster.alpha_composite(screen, (px(x), px(y)))
        # Labels
        d = ImageDraw.Draw(poster)
        d.text((px(x), px(y + SH + 12)), num, font=font(10.5, "Semibold"), fill=rgb(GREEN), anchor="lt")
        d.text((px(x + 22), px(y + SH + 11)), title, font=font(12, "Semibold"), fill=rgb(INK), anchor="lt")
        d.text((px(x), px(y + SH + 28)), desc, font=font(9, "Regular"), fill=rgb(MUTED), anchor="lt")

    d = ImageDraw.Draw(poster)
    d.line([px(40), px(PH - 34), px(PW - 40), px(PH - 34)], fill=rgb(LINE), width=px(0.8))
    d.text(
        (px(PW / 2), px(PH - 24)),
        "Концепт-визуализация. Реальный платёж требует Mir Pay / Mir HCE SDK и сертификации НСПК.",
        font=font(8.5, "Regular"),
        fill=rgb(DIM),
        anchor="mt",
    )
    final = poster.convert("RGB").resize((PW, PH), Image.LANCZOS)
    final.save(OUT)
    print(f"wrote {OUT} ({final.width}x{final.height})")


if __name__ == "__main__":
    render_poster()
