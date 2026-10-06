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
import os
import sys
from PIL import Image, ImageDraw, ImageFont

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "data", "port")
os.makedirs(OUT, exist_ok=True)

TEXT = "Settings"
COLOR = (230, 184, 60)   # gold, beside options.png's orange / red / olive / brick
font = ImageFont.truetype(sys.argv[1], 18)
x0, y0, x1, y1 = font.getbbox(TEXT)
img = Image.new("RGBA", (x1 - x0 + 2, 15), (0, 0, 0, 0))
ImageDraw.Draw(img).text((1 - x0, (15 - (y1 - y0)) // 2 - y0), TEXT, font=font, fill=COLOR + (255,))
img.save(os.path.join(OUT, "opsettings.png"))
print("data/port/opsettings.png", img.size)
