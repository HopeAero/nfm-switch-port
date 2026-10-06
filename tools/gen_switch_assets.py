# Switch control art for the Instructions pages, the twin of gen_vita_assets.py:
# the same pixel language as data/images.zip's arrows.gif (light face with a
# faint gradient, a darker line inside the top edge, a solid 3D wall down the
# right and bottom, grey ramp, black glyph), each image drawn to the exact
# footprint of the keyboard art it replaces so the page's labels stay put.
#
# The buttons are the ones native/platform/switch/{platform,input}.c bind. They
# are named and placed the same on the Joy-Cons (handheld or paired) and on the
# Pro Controller -- libnx reads any of them as player 1 -- so one set fits all.
#
#   python3 tools/gen_switch_assets.py      # writes data/switch/*.png
from PIL import Image, ImageDraw
import os

SP = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "data", "switch")
os.makedirs(SP, exist_ok=True)

FACE_TOP, FACE_BOT = (250, 250, 250), (234, 234, 234)
EDGE_TOP = (200, 200, 200)
WALL = (129, 129, 129)
WALL_LT = (168, 168, 168)
OUTLINE = (86, 86, 86)
GLYPH = (12, 12, 12)
CLEAR = (190, 190, 190, 0)
D = 4  # wall depth


def _grad(d, box):
    x0, y0, x1, y1 = box
    for i in range(y1 - y0 + 1):
        t = i / max(1, (y1 - y0))
        c = tuple(int(FACE_TOP[k] + (FACE_BOT[k] - FACE_TOP[k]) * t) for k in range(3))
        d.line([(x0, y0 + i), (x1, y0 + i)], fill=c)


def cap(d, x, y, w, h):
    """Square key, its wall part of the body (gen_vita_assets.cap)."""
    d.rounded_rectangle([x + D, y + D, x + w + D, y + h + D], radius=3, fill=WALL, outline=OUTLINE)
    d.polygon([(x + w, y), (x + w + D, y + D), (x + w + D, y + h), (x + w, y + h - 1)], fill=WALL_LT)
    _grad(d, (x + 1, y + 1, x + w - 1, y + h - 1))
    d.rounded_rectangle([x, y, x + w, y + h], radius=3, outline=OUTLINE)
    d.line([(x + 3, y + 2), (x + w - 3, y + 2)], fill=EDGE_TOP)


def rcap(img, cx, cy, r):
    """Round face button, the face gradient masked to the rim (gen_vita_assets.rcap)."""
    d = ImageDraw.Draw(img)
    d.ellipse([cx - r + D, cy - r + D, cx + r + D, cy + r + D], fill=WALL, outline=OUTLINE)
    face = Image.new('RGBA', (2 * r + 1, 2 * r + 1), (0, 0, 0, 0))
    fd = ImageDraw.Draw(face)
    for i in range(2 * r + 1):
        t = i / max(1, 2 * r)
        c = tuple(int(FACE_TOP[k] + (FACE_BOT[k] - FACE_TOP[k]) * t) for k in range(3))
        fd.line([(0, i), (2 * r, i)], fill=c + (255,))
    mask = Image.new('L', (2 * r + 1, 2 * r + 1), 0)
    ImageDraw.Draw(mask).ellipse([0, 0, 2 * r, 2 * r], fill=255)
    img.paste(face, (cx - r, cy - r), mask)
    d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=OUTLINE)
    d.arc([cx - r + 2, cy - r + 2, cx + r - 2, cy + r - 2], 200, 340, fill=EDGE_TOP)


def arrow(d, cx, cy, dirn, L=7, W=4, T=3):
    def box(x0, y0, x1, y1):
        d.rectangle([min(x0, x1), min(y0, y1), max(x0, x1), max(y0, y1)], fill=GLYPH)
    h = T // 2
    if dirn in 'UD':
        s = -1 if dirn == 'U' else 1
        tip, base = cy + s * L, cy + s * (L - W)
        d.polygon([(cx, tip), (cx - W, base), (cx + W, base)], fill=GLYPH)
        box(cx - h, base, cx + h, cy - s * L)
    else:
        s = -1 if dirn == 'L' else 1
        tip, base = cx + s * L, cx + s * (L - W)
        d.polygon([(tip, cy), (base, cy - W), (base, cy + W)], fill=GLYPH)
        box(base, cy - h, cx - s * L, cy + h)


# The button letters, 3x5 cells drawn as solid blocks (the original's labels
# are blocky too); `px` is the block size.
FONT = {
    'A': ["010", "101", "111", "101", "101"],
    'B': ["110", "101", "110", "101", "110"],
    'X': ["101", "101", "010", "101", "101"],
    'Y': ["101", "101", "010", "010", "010"],
    'Z': ["111", "001", "010", "100", "111"],
    'R': ["110", "101", "110", "101", "101"],
    'L': ["100", "100", "100", "100", "111"],
    '+': ["000", "010", "111", "010", "000"],
    '-': ["000", "000", "111", "000", "000"],
}


def text(d, cx, cy, s, px=2):
    """Centre `s` at (cx, cy), one blank cell between letters."""
    w = len(s) * 3 * px + (len(s) - 1) * px
    x0, y0 = cx - w // 2, cy - (5 * px) // 2
    for i, ch in enumerate(s):
        for r, row in enumerate(FONT[ch]):
            for c, bit in enumerate(row):
                if bit == '1':
                    x, y = x0 + i * 4 * px + c * px, y0 + r * px
                    d.rectangle([x, y, x + px - 1, y + px - 1], fill=GLYPH)


def round_btn(size, cx, cy, r, label, px=2):
    img = Image.new('RGBA', size, CLEAR)
    rcap(img, cx, cy, r)
    text(ImageDraw.Draw(img), cx, cy, label, px)
    return img


def stick(d, cx, cy, r, dirs, nub=0):
    """An analog stick: a dished top, a nub pushed `nub` (-1 left, +1 right),
    and an arrow for each direction in `dirs`."""
    d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=GLYPH, width=2)
    d.ellipse([cx - 3 + nub * 3, cy - 3, cx + 3 + nub * 3, cy + 3], fill=GLYPH)
    for k in dirs:
        off = r + 8
        arrow(d, cx + {'L': -off, 'R': off}.get(k, 0), cy + {'U': -off, 'D': off}.get(k, 0), k, L=5, W=4, T=3)


# ---- sw_arrows.png (81x62, arrows.gif's T): ZR on top accelerates, ZL below
# brakes and reverses, the side keys steer (the left stick) -- input.c's split.
img = Image.new('RGBA', (81, 62), CLEAR); d = ImageDraw.Draw(img)
cap(d, 28, 0, 20, 20); text(d, 38, 10, "ZR")
cap(d, 2, 32, 20, 20); arrow(d, 12, 42, 'L')
cap(d, 28, 32, 20, 20); text(d, 38, 42, "ZL")
cap(d, 54, 32, 20, 20); arrow(d, 64, 42, 'R')
img.save(f'{SP}/sw_arrows.png')

# ---- sw_space.png (212x30, space.gif's slot): the handbrake is B.
round_btn((212, 30), 106, 14, 13, "B").save(f'{SP}/sw_space.png')

# ---- sw_stick.png (81x62): the stunt diagrams, B + the left stick four ways.
img = Image.new('RGBA', (81, 62), CLEAR)
rcap(img, 40, 31, 15)
d = ImageDraw.Draw(img)
d.ellipse([40 - 9, 31 - 9, 40 + 9, 31 + 9], outline=GLYPH, width=2)
d.ellipse([40 - 3, 31 - 3, 40 + 3, 31 + 3], fill=GLYPH)
arrow(d, 40, 7, 'U', L=6, W=4, T=3)
arrow(d, 40, 55, 'D', L=6, W=4, T=3)
arrow(d, 12, 31, 'L', L=6, W=4, T=3)
arrow(d, 68, 31, 'R', L=6, W=4, T=3)
img.save(f'{SP}/sw_stick.png')

# ---- the "OTHER CONTROLS" page's key caps (29x33; ENTER 97x33) ----
# Z / X look one way and the other: the right stick, pushed right (Z, lookback
# 1) and left (X, lookback -1), as input.c reads it.
for name, nub in (("sw_kz", 1), ("sw_kx", -1)):
    img = Image.new('RGBA', (29, 33), CLEAR)
    rcap(img, 13, 15, 11)
    d = ImageDraw.Draw(img)
    d.ellipse([13 - 7, 15 - 7, 13 + 7, 15 + 7], outline=GLYPH, width=2)
    d.ellipse([13 - 2 + nub * 3, 15 - 2, 13 + 2 + nub * 3, 15 + 2], fill=GLYPH)
    img.save(f'{SP}/{name}.png')
round_btn((29, 33), 13, 15, 11, "X").save(f'{SP}/sw_kv.png')   # V: camera
round_btn((29, 33), 13, 15, 11, "Y").save(f'{SP}/sw_km.png')   # M: music
# N (sound effects) is Minus: a small round button with its bar.
round_btn((29, 33), 13, 15, 9, "-").save(f'{SP}/sw_kn.png')
# S (radar) is D-pad down.
img = Image.new('RGBA', (29, 33), CLEAR); d = ImageDraw.Draw(img)
cap(d, 1, 2, 22, 24); arrow(d, 12, 14, 'D')
img.save(f'{SP}/sw_ks.png')
# ENTER ("navigate and pause"): A confirms, Plus pauses.
img = Image.new('RGBA', (97, 33), CLEAR)
rcap(img, 16, 15, 11); text(ImageDraw.Draw(img), 16, 15, "A")
rcap(img, 64, 15, 9); text(ImageDraw.Draw(img), 64, 15, "+")
img.save(f'{SP}/sw_kenter.png')

print("data/switch: sw_arrows sw_space sw_stick sw_kz sw_kx sw_kv sw_km sw_kn sw_ks sw_kenter")
