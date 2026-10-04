# Vita control art, in the pixel language of data/images.zip's arrows.gif:
# a light face with a faint vertical gradient, a thin darker line inside the
# top edge, and a SOLID 3D side wall down the right and bottom (the body
# extends into it -- it is not a detached drop shadow), all in the original's
# grey ramp with a solid black glyph.
from PIL import Image, ImageDraw
import os, sys
# Writes into data/vita/ relative to the repo root, wherever this is run from.
SP = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "data", "vita")
os.makedirs(SP, exist_ok=True)

FACE_TOP, FACE_BOT = (250,250,250), (234,234,234)
EDGE_TOP = (200,200,200)
WALL     = (129,129,129)     # the 132 family the original uses for key sides
WALL_LT  = (168,168,168)
OUTLINE  = (86,86,86)
GLYPH    = (12,12,12)
CLEAR    = (190,190,190,0)
D        = 4                 # wall depth

def _grad(d, box, r=0):
    x0,y0,x1,y1 = box
    for i in range(y1-y0+1):
        t = i/max(1,(y1-y0))
        c = tuple(int(FACE_TOP[k]+(FACE_BOT[k]-FACE_TOP[k])*t) for k in range(3))
        d.line([(x0,y0+i),(x1,y0+i)], fill=c)

def cap(d, x, y, w, h):
    """Square key. Body occupies x..x+w+D / y..y+h+D so the wall is part of
    the key, matching how the original's sides meet the face."""
    d.rounded_rectangle([x+D, y+D, x+w+D, y+h+D], radius=3, fill=WALL, outline=OUTLINE)
    d.polygon([(x+w,y),(x+w+D,y+D),(x+w+D,y+h),(x+w,y+h-1)], fill=WALL_LT)
    _grad(d, (x+1,y+1,x+w-1,y+h-1))
    d.rounded_rectangle([x,y,x+w,y+h], radius=3, outline=OUTLINE)
    d.line([(x+3,y+2),(x+w-3,y+2)], fill=EDGE_TOP)

def rcap(img, cx, cy, r):
    """Round face button. Built on its own layer and composited through a
    circular mask, so the face gradient stops at the rim instead of leaving
    the square block the first pass left behind."""
    d = ImageDraw.Draw(img)
    d.ellipse([cx-r+D, cy-r+D, cx+r+D, cy+r+D], fill=WALL, outline=OUTLINE)
    face = Image.new('RGBA', (2*r+1, 2*r+1), (0,0,0,0))
    fd = ImageDraw.Draw(face)
    for i in range(2*r+1):
        t = i/max(1,2*r)
        c = tuple(int(FACE_TOP[k]+(FACE_BOT[k]-FACE_TOP[k])*t) for k in range(3))
        fd.line([(0,i),(2*r,i)], fill=c+(255,))
    mask = Image.new('L', (2*r+1, 2*r+1), 0)
    ImageDraw.Draw(mask).ellipse([0,0,2*r,2*r], fill=255)
    img.paste(face, (cx-r, cy-r), mask)
    d.ellipse([cx-r, cy-r, cx+r, cy+r], outline=OUTLINE)
    d.arc([cx-r+2, cy-r+2, cx+r-2, cy+r-2], 200, 340, fill=EDGE_TOP)

def arrow(d, cx, cy, dirn, L=7, W=4, T=3):
    def box(x0,y0,x1,y1):
        d.rectangle([min(x0,x1),min(y0,y1),max(x0,x1),max(y0,y1)], fill=GLYPH)
    h=T//2
    if dirn in 'UD':
        s = -1 if dirn=='U' else 1
        tip, base = cy+s*L, cy+s*(L-W)
        d.polygon([(cx,tip),(cx-W,base),(cx+W,base)], fill=GLYPH)
        box(cx-h, base, cx+h, cy-s*L)
    else:
        s = -1 if dirn=='L' else 1
        tip, base = cx+s*L, cx+s*(L-W)
        d.polygon([(tip,cy),(base,cy-W),(base,cy+W)], fill=GLYPH)
        box(base, cy-h, cx-s*L, cy+h)

# glyphs sized to sit INSIDE the face with margin (r<=6 on a 20px cap)
def g_cross(d,cx,cy,r=6,t=3):
    for k in range(-(t//2),t//2+1):
        d.line([(cx-r+k,cy-r),(cx+r+k,cy+r)], fill=GLYPH)
        d.line([(cx-r+k,cy+r),(cx+r+k,cy-r)], fill=GLYPH)
def g_circle(d,cx,cy,r=6,t=3): d.ellipse([cx-r,cy-r,cx+r,cy+r], outline=GLYPH, width=t)
def g_tri(d,cx,cy,r=6,t=3):    d.polygon([(cx,cy-r-1),(cx+r+1,cy+r),(cx-r-1,cy+r)], outline=GLYPH, width=t)
def g_sq(d,cx,cy,r=5,t=3):     d.rectangle([cx-r,cy-r,cx+r,cy+r], outline=GLYPH, width=t)

def letter_L(d, x, y, h=11, w=7, t=3):
    d.rectangle([x, y, x+t-1, y+h], fill=GLYPH)              # spine
    d.rectangle([x, y+h-t+1, x+w, y+h], fill=GLYPH)          # foot

def letter_R(d, x, y, h=11, w=8, t=3):
    d.rectangle([x, y, x+t-1, y+h], fill=GLYPH)              # spine
    d.rectangle([x, y, x+w, y+t-1], fill=GLYPH)              # top bar
    d.rectangle([x+w-t+1, y, x+w, y+h//2], fill=GLYPH)       # bowl's right side
    d.rectangle([x, y+h//2-t+2, x+w, y+h//2+1], fill=GLYPH)  # waist
    for k in range(t):                                        # diagonal leg
        d.line([(x+t+k, y+h//2+1), (x+w+k, y+h)], fill=GLYPH)

# ---- vita_arrows.png: drop-in for arrows.gif (81x62, same T layout) ----
dpad=Image.new('RGBA',(81,62),CLEAR); d=ImageDraw.Draw(dpad)
cap(d,28,0,20,20);  arrow(d,38,10,'U')
cap(d, 2,32,20,20); arrow(d,12,42,'L')
cap(d,28,32,20,20); arrow(d,38,42,'D')
cap(d,54,32,20,20); arrow(d,64,42,'R')
dpad.save(f'{SP}/vita_arrows.png')

# ---- vita_space.png: drop-in for space.gif (212x30). The original is a
# wide SPACEBAR key; the Vita equivalent is one round button, drawn centred
# in the same footprint so every label placed around the bar stays put.
sp=Image.new('RGBA',(212,30),CLEAR)
rcap(sp,106,14,13); g_cross(ImageDraw.Draw(sp),106,14,r=7)
sp.save(f'{SP}/vita_space.png')

print("vita_arrows.png 81x62 | vita_space.png 212x30")

# ---------------------------------------------------------------------------
# Per-key replacements for the "OTHER CONTROLS" page. Each is drawn to the
# exact footprint of the keyboard cap it stands in for (29x33 for the single
# keys, 97x33 for the wide ENTER), so the page's labels keep their positions.
# The mapping follows what platform/vita/platform.c actually binds.
# ---------------------------------------------------------------------------
def single(draw_glyph, round_btn=False):
    """One 29x33 key slot: either a square cap or a round face button."""
    img = Image.new('RGBA', (29, 33), CLEAR)
    if round_btn:
        rcap(img, 13, 15, 11)
        draw_glyph(ImageDraw.Draw(img), 13, 15)
    else:
        d = ImageDraw.Draw(img)
        cap(d, 1, 2, 22, 24)
        draw_glyph(d, 12, 14)
    return img

def g_letter_L(d, cx, cy): letter_L(d, cx-3, cy-6)
def g_letter_R(d, cx, cy): letter_R(d, cx-4, cy-6)

def g_stick(d, cx, cy, r=7):
    """Right analog stick: a dished top with a nub pushed right."""
    d.ellipse([cx-r, cy-r, cx+r, cy+r], outline=GLYPH, width=2)
    d.ellipse([cx-2, cy-2, cx+2, cy+2], fill=GLYPH)
    d.polygon([(cx+r+3, cy), (cx+r-1, cy-3), (cx+r-1, cy+3)], fill=GLYPH)

single(g_letter_L).save(f'{SP}/vita_kz.png')
single(g_letter_R).save(f'{SP}/vita_kx.png')
single(g_tri,    round_btn=True).save(f'{SP}/vita_kv.png')
single(g_sq,     round_btn=True).save(f'{SP}/vita_km.png')
# S (radar) is D-pad down on the Vita: a key cap with a down arrow.
def g_dpad_down(d, cx, cy): arrow(d, cx, cy, 'D')
single(g_dpad_down).save(f'{SP}/vita_ks.png')

# The stunt diagrams: CROSS + the LEFT STICK, pushed in one of four
# directions. Same 81x62 footprint as arrows.gif, so the page's
# FORWARD LOOP / BACKWARD LOOP / LEFT ROLL / RIGHT ROLL labels around it
# keep their places.
stick = Image.new('RGBA', (81, 62), CLEAR)
rcap(stick, 40, 31, 15)
sd = ImageDraw.Draw(stick)
sd.ellipse([40-9, 31-9, 40+9, 31+9], outline=GLYPH, width=2)
sd.ellipse([40-3, 31-3, 40+3, 31+3], fill=GLYPH)
arrow(sd, 40, 7, 'U', L=6, W=4, T=3)
arrow(sd, 40, 55, 'D', L=6, W=4, T=3)
arrow(sd, 12, 31, 'L', L=6, W=4, T=3)
arrow(sd, 68, 31, 'R', L=6, W=4, T=3)
stick.save(f'{SP}/vita_stick.png')

# SELECT: a small pill, the shape the device uses for it
sel = Image.new('RGBA', (29, 33), CLEAR); d = ImageDraw.Draw(sel)
cap(d, 1, 8, 22, 12)
d.rectangle([8, 12, 17, 15], fill=GLYPH)
sel.save(f'{SP}/vita_kn.png')

# ENTER (97x33) carries two jobs on this page, "navigate and pause", so it
# gets both buttons: Cross to confirm, START to pause.
en = Image.new('RGBA', (97, 33), CLEAR)
rcap(en, 16, 15, 11); g_cross(ImageDraw.Draw(en), 16, 15, r=6)
d = ImageDraw.Draw(en)
cap(d, 48, 8, 34, 12)
d.polygon([(70, 10), (78, 14), (70, 18)], fill=GLYPH)   # play-mark, as the pad marks START
d.rectangle([54, 12, 64, 15], fill=GLYPH)
en.save(f'{SP}/vita_kenter.png')

print("teclas individuais: kz kx kv km kn ks kenter | vita_stick.png 81x62")
