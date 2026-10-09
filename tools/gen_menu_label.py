# The main menu's "Settings" label, in the style of data/images.zip's
# options.png (Play Game / Game Instructions / Credits): the Adventure face at
# 18 px -- the size that reproduces those labels' own widths ("Credits" 66 px,
# "Game Instructions" 169 px) -- one flat colour, anti-aliased edges, on a
# transparent strip as tall as each options.png row (15 px of text).
#
# The font is not in this repository. The original game's own copy ships in
# DS-addons' appcore.jar (com/radicalplay/nfmm/adventure.ttf):
#
#   python3 tools/gen_menu_label.py path/to/adventure.ttf   # -> data/port/opsettings.png
#
# Another label in the same style: [text] [out.png] [r,g,b], e.g. the Spanish
# rows (data/port/es/), coloured as the options.png rows they replace:
#   ... adventure.ttf "Jugar" data/port/es/op_play.png 255,152,51
#   ... adventure.ttf "Instrucciones" data/port/es/op_inst.png 168,148,34
#   ... adventure.ttf "Créditos" data/port/es/op_credits.png 168,66,34
#   ... adventure.ttf "Ajustes" data/port/es/opsettings.png
import os
import sys
from PIL import Image, ImageDraw, ImageFont

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "data", "port")
os.makedirs(OUT, exist_ok=True)

TEXT = sys.argv[2] if len(sys.argv) > 2 else "Settings"
PATH = sys.argv[3] if len(sys.argv) > 3 else os.path.join(OUT, "opsettings.png")
# gold, beside options.png's orange / red / olive / brick
COLOR = tuple(int(c) for c in sys.argv[4].split(",")) if len(sys.argv) > 4 else (230, 184, 60)
font = ImageFont.truetype(sys.argv[1], 18)
x0, y0, x1, y1 = font.getbbox(TEXT)
_, hy0, _, hy1 = font.getbbox("H")
# The capitals sit centred in the 15 px row; an accent above them (Créditos)
# grows the image upwards by `pad`, so the caller bottom-aligns the row.
pad = max(0, hy0 - y0)
img = Image.new("RGBA", (x1 - x0 + 2, 15 + pad), (0, 0, 0, 0))
top = pad + (15 - (hy1 - hy0)) // 2 if pad else (15 - (y1 - y0)) // 2
ImageDraw.Draw(img).text((1 - x0, top - (hy0 if pad else y0)), TEXT, font=font, fill=COLOR + (255,))
img.save(PATH)
print(PATH, img.size)
