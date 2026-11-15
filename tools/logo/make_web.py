"""The website's logo: the transparent version cropped to the drawing and
scaled to 292 x 400 (the drawing's own aspect, within 0.3%), sharp on 2x
displays at the size docs/assets/css/style.scss shows it.

Scaling uses premultiplied alpha, so the colour of transparent pixels cannot
bleed into the edges.

usage: make_web.py logo-transparent.png out.png
"""
import sys
from PIL import Image

src, dst = sys.argv[1], sys.argv[2]
im = Image.open(src).convert('RGBA')
im = im.crop(im.getchannel('A').getbbox())
im.convert('RGBa').resize((292, 400), Image.LANCZOS).convert('RGBA').save(dst, optimize=True)
