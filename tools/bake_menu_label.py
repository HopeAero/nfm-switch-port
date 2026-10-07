"""Bakes data/port/extended_label.png: the game-mode menu's "Extended" row
label, lettered like options2.png's NFM 1 / NFM 2 / Free Play (Adventure,
the same orange, 16 px tall). Run:

    python3 tools/bake_menu_label.py <path to Adventure.ttf>
"""
import io
import sys
import zipfile
from collections import Counter

from PIL import Image, ImageDraw, ImageFont

ref = Image.open(io.BytesIO(zipfile.ZipFile('data/images.zip').read('options2.png'))).convert('RGBA')
orange = Counter(p[:3] for p in ref.getdata() if p[3] > 200).most_common(1)[0][0]
font = ImageFont.truetype(sys.argv[1], 17)
d0 = ImageDraw.Draw(Image.new('RGBA', (1, 1)))
w = int(d0.textlength('Extended', font=font)) + 4
im = Image.new('RGBA', (w, 16), (0, 0, 0, 0))
d = ImageDraw.Draw(im)
for dx in (0, 1):
    d.text((1 + dx, 8), 'Extended', font=font, fill=orange + (255,), anchor='lm')
im.save('data/port/extended_label.png')
print('data/port/extended_label.png', im.size, orange)
