"""Transparent-background version of the MEDUSA logo.

Near-white pixels (min channel >= 246) are background, including the white
gaps between shapes; shapes stay fully opaque. Only a 2.5 px band along the
shape edges gets fractional alpha (colour-to-alpha against white), which keeps
the anti-aliasing: composited on white, the result matches the original.

usage: make_transparent.py logo.png out.png
"""
import sys
from PIL import Image
import numpy as np
from scipy import ndimage

src, dst = sys.argv[1], sys.argv[2]
im = np.asarray(Image.open(src).convert('RGB')).astype(float)
bg = im.min(axis=2) >= 246
dist = ndimage.distance_transform_edt(~bg)
band = (dist > 0) & (dist <= 2.5)
c2a = np.clip((np.max((255.0 - im) / 255.0, axis=2) - 0.012) / (1 - 0.012), 0, 1)
a = np.where(bg, 0.0, 1.0)
a[band] = c2a[band]
safe = np.maximum(a, 1e-6)[..., None]
col = np.where(band[..., None], np.clip((im - 255.0 * (1 - safe)) / safe, 0, 255), im)
Image.fromarray(np.dstack([col, a * 255]).round().astype(np.uint8), 'RGBA').save(dst, optimize=True)
