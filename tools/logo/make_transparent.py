"""Transparent-background version of the MEDUSA logo.

The original has a white background, and white is part of the drawing too:
the anti-aliased edges and the soft glow where the tentacles fade into the
background are colours mixed with white. Light pixels (all channels >= 215)
are therefore turned into colour plus transparency (colour-to-alpha against
white); the shapes' own colours (any channel <= 175) stay opaque, with a
smooth blend in between. Near-white pixels (all channels >= 250) are
background, including the white gaps between shapes.

Composited on white, the result matches the original (within 5/255, in the
near-white background), except for one deliberate change: the two pale blue
shapes between the outer pairs of tentacles, just under the bell, are removed
(made transparent). On a dark page the edges fade into it, instead of being cut
off at a threshold into opaque near-white patches.

The pale shapes are found from one point inside each (GLOW_SEEDS, in the
original's pixel coordinates), grown through their light pixels: everything
lighter than the tentacles' own colour, short of the background, above row
700 (below that the tentacles themselves turn light). Then a 3-pixel band
along them, where they blend into the tentacles' edges, goes too.

usage: make_transparent.py logo.png out.png
"""
import sys
from PIL import Image
import numpy as np
from scipy import ndimage

GLOW_SEEDS = [(380, 590), (870, 600)]   # (x, y): left and right pale shape

src, dst = sys.argv[1], sys.argv[2]
im = np.asarray(Image.open(src).convert('RGB')).astype(float)
mn = im.min(axis=2)
c2a = np.max((255.0 - im) / 255.0, axis=2)          # colour-to-alpha against white
w = np.clip((mn - 175.0) / (215.0 - 175.0), 0, 1)   # 0: a shape's colour, 1: light
a = w * c2a + (1 - w)
a[mn >= 250] = 0.0

# The pale shapes between the tentacles.
solid = mn < 100                                    # the tentacles' own colour
near_solid = ndimage.distance_transform_edt(~solid) <= 1.5
light = (mn >= 120) & (mn < 250) & ~near_solid
light[700:, :] = False
seeds = np.zeros_like(light)
for x, y in GLOW_SEEDS:
    assert light[y, x], f"GLOW_SEEDS: ({x}, {y}) is not in a pale shape"
    seeds[y, x] = True
glow = ndimage.binary_propagation(seeds, mask=light)
glow |= ndimage.binary_dilation(glow, iterations=3) & (mn >= 150)
a[glow] = 0.0
safe = np.maximum(a, 1e-6)[..., None]
col = np.clip(255.0 - (255.0 - im) / safe, 0, 255)  # col over white == the original
col[a == 0] = 0
Image.fromarray(np.dstack([col, a * 255]).round().astype(np.uint8), 'RGBA').save(dst, optimize=True)
