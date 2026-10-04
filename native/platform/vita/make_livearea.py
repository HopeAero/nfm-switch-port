#!/usr/bin/env python3
"""Builds the Vita bubble and LiveArea images from the game's own assets.

The art is the original main menu (xtGraphics.maini(), :4316-4344): the
logomadbg.jpg backdrop, logocars.png, the Dude (d1.png) and logomad.png,
at the same offsets inside the 670x400 menu window. The bubble is the
Dude's face. LiveArea wants 8-bit paletted PNGs, so every output is
quantised to 256 colours.

    python3 native/platform/vita/make_livearea.py     # from the repo root
Needs Pillow. Writes into native/platform/vita/sce_sys/.
"""
import io
import os
import zipfile

from PIL import Image

ROOT = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(ROOT, "..", "..", ".."))
OUT = os.path.join(ROOT, "sce_sys")

zf = zipfile.ZipFile(os.path.join(REPO, "data", "images.zip"))


def img(name):
    return Image.open(io.BytesIO(zf.read(name))).convert("RGBA")


def menu_art():
    """The main menu without its option panel, 670x400."""
    art = img("logomadbg.jpg")
    # maini() offsets, minus the window's own (65, 25).
    art.alpha_composite(img("d1.png"), (351 - 65, 28 - 25))
    art.alpha_composite(img("logocars.png"), (66 - 65, 33 - 25))
    art.alpha_composite(img("logomad.png"), (233 - 65, 186 - 25))
    return art


def cover(src, w, h, focus_y=0.5):
    """Scale to fill w x h, cropping the overflow."""
    s = max(w / src.width, h / src.height)
    r = src.resize((round(src.width * s), round(src.height * s)), Image.LANCZOS)
    x = (r.width - w) // 2
    y = round((r.height - h) * focus_y)
    return r.crop((x, y, x + w, y + h))


def save8(im, name):
    path = os.path.join(OUT, name)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    im.convert("RGB").quantize(256, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.FLOYDSTEINBERG).save(
        path, optimize=True)
    print("wrote", os.path.relpath(path, REPO), im.size)


def bubble():
    """128x128: the Dude's face over the menu's fiery sky."""
    sky = cover(img("logomadbg.jpg").crop((180, 0, 520, 340)), 128, 128)
    dude = img("d1.png").crop((0, 0, 120, 162))  # the whole face, chin to hair
    h = 124
    dude = dude.resize((round(120 * h / 162), h), Image.LANCZOS)
    sky.alpha_composite(dude, ((128 - dude.width) // 2, 8))
    return sky


def startup():
    """280x158 gate: the title over the backdrop."""
    gate = cover(img("logomadbg.jpg"), 280, 158, 0.35)
    cars = img("logocars.png")
    cars = cars.resize((272, round(cars.height * 272 / cars.width)), Image.LANCZOS)
    gate.alpha_composite(cars, (4, 2))
    logo = img("logomad.png")
    logo = logo.resize((250, round(logo.height * 250 / logo.width)), Image.LANCZOS)
    gate.alpha_composite(logo, (15, 158 - logo.height - 10))
    return gate


art = menu_art()
save8(bubble(), "icon0.png")
save8(cover(art, 960, 544), "pic0.png")
save8(cover(art, 840, 500), "livearea/contents/bg.png")
save8(startup(), "livearea/contents/startup.png")
