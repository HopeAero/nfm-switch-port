"""Bakes data/port/special.png: Extended's Special bar for the race HUD, in the
shape of NFM 2's damage.gif / power.gif (176x16, label left, frame from x 58).

The frame is Extended's own special.GIF (ext/data/images.radq); the label is
lettered "SPECIAL" in Adventure, the face Extended writes its HUD labels in,
emboldened by one pixel like the original lettering. Run:

    python3 tools/bake_special.py <path to Adventure.ttf>

(nfm-master/ext/fonts/Adventure.ttf). Only the baked PNG is shipped; game.c
tints it with the stage's snap like the zip's GIFs.
"""
import io
import sys
import zipfile

from PIL import Image, ImageDraw, ImageFont

font = ImageFont.truetype(sys.argv[1], 13)
frame = Image.open(io.BytesIO(zipfile.ZipFile('ext/data/images.radq').read('special.GIF'))).convert('RGBA')

im = Image.new('RGBA', (176, 16), (0, 0, 0, 0))
im.paste(frame, (58, 0), frame)
d = ImageDraw.Draw(im)
w = d.textlength('SPECIAL', font=font)
for dx in (0, 1):
    d.text((54 - w + dx, 8), 'SPECIAL', font=font, fill=(0, 0, 0, 255), anchor='lm')

im.save('data/port/special.png')
print('data/port/special.png', im.size)
