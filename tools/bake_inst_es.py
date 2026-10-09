"""Bakes the Spanish lettering of two Instructions-page images, for the
Spanish mode (the native port loads data/port/es/<name>.png in place of
images.zip's <name> when the language is Spanish):

  stunts.png  "STUNTS:"  -> "ACROBACIAS:"  (the four-car stunt diagram's heading)
  ory.gif     "OR"       -> "O"            (between the racing and wasting pictures)

The original letters are black, in the Adventure face the menus use; the
replacement is drawn in it at the original's cap height, on the same
transparent ground. Run from the repository root:

    python3 tools/bake_inst_es.py <path to Adventure.ttf>
"""
import io
import sys
import zipfile

from PIL import Image, ImageDraw, ImageFont

FONT = sys.argv[1]
z = zipfile.ZipFile('data/images.zip')


def lettering(text, cap_h, fill=(0, 0, 0, 255)):
    """`text` in Adventure, cropped to its ink, its capitals cap_h pixels tall."""
    size = cap_h
    while True:
        font = ImageFont.truetype(FONT, size)
        x0, y0, x1, y1 = font.getbbox('H')
        if y1 - y0 >= cap_h:
            break
        size += 1
    x0, y0, x1, y1 = font.getbbox(text)
    im = Image.new('RGBA', (x1 - x0 + 2, y1 - y0 + 2), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.text((1 - x0, 1 - y0), text, font=font, fill=fill)
    return im


# stunts.png: the heading sits alone in x 5..89, y 22..39.
st = Image.open(io.BytesIO(z.read('stunts.png'))).convert('RGBA')
px = st.load()
for y in range(20, 41):
    for x in range(0, 100):
        px[x, y] = (0, 0, 0, 0)
head = lettering('ACROBACIAS:', 16)
st.alpha_composite(head, (5, 39 - head.height + 1))
st.save('data/port/es/stunts.png')
print('data/port/es/stunts.png', st.size, head.size)

# ory.gif: "OR", 32x18 -> "O", centred where the pair was.
ory = Image.open(io.BytesIO(z.read('ory.gif'))).convert('RGBA')
o = lettering('O', 16)
out = Image.new('RGBA', ory.size, (0, 0, 0, 0))
out.alpha_composite(o, ((ory.width - o.width) // 2, (ory.height - o.height) // 2))
out.save('data/port/es/ory.png')
print('data/port/es/ory.png', out.size, o.size)
