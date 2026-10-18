"""Vectorise the MEDUSA logo as stacked brightness layers.

Layer k covers every logo pixel darker than threshold t_k. Layers are drawn
from the lightest (largest) to the darkest (smallest), each filled with the
median colour of the pixels in its own brightness band. The white gaps
between the shapes are lighter than every threshold, so they stay out of all
layers and come out as real gaps (transparent), overlaps included.

usage: stack_trace.py logo.png out.svg [levels [inner_sigma [inner_tol]]]
       (defaults 24 4 1.4, which is what docs/logo/medusa-logo.svg uses)
"""
import sys
import numpy as np
from PIL import Image
from scipy import ndimage
from skimage import measure

src, dst = sys.argv[1], sys.argv[2]
levels = int(sys.argv[3]) if len(sys.argv) > 3 else 24
INNER_SIGMA = float(sys.argv[4]) if len(sys.argv) > 4 else 4.0
INNER_TOL = float(sys.argv[5]) if len(sys.argv) > 5 else 1.4
rgb = np.asarray(Image.open(src).convert('RGB')).astype(float)
L = rgb @ np.array([0.299, 0.587, 0.114])
logo = rgb.min(axis=2) < 246
H, W = logo.shape
ys, xs = np.nonzero(logo)
pad = 12
x0, y0 = max(xs.min() - pad, 0), max(ys.min() - pad, 0)
x1, y1 = min(xs.max() + pad, W), min(ys.max() + pad, H)

# thresholds: quantiles of the logo's brightness, so bands hold similar areas
lv = L[logo]
qs = np.quantile(lv, np.linspace(1.0, 0.0, levels + 1))
qs[0] = 256.0


def catmull_rom(pts):
    k = len(pts)
    out = [f"M{pts[0][0]:.1f} {pts[0][1]:.1f}"]
    for i in range(k):
        p0, p1, p2, p3 = pts[i - 1], pts[i], pts[(i + 1) % k], pts[(i + 2) % k]
        c1 = p1 + (p2 - p0) / 6.0
        c2 = p2 - (p3 - p1) / 6.0
        out.append(f"C{c1[0]:.1f} {c1[1]:.1f} {c2[0]:.1f} {c2[1]:.1f} {p2[0]:.1f} {p2[1]:.1f}")
    return "".join(out) + "Z"


def outline(region, sigma, tol=0.9, minlen=24):
    soft = ndimage.gaussian_filter(region.astype(float), sigma)
    d = ""
    for c in measure.find_contours(np.pad(soft, 1), 0.5):
        if len(c) < minlen:
            continue
        poly = measure.approximate_polygon(c, tolerance=tol)[:-1]
        if len(poly) >= 4:
            d += catmull_rom(poly[:, ::-1] - 1.0)
    return d


defs = []


def gradient_fill(band, gid):
    """Fill for one layer: a linear gradient along the direction in which the
    colour of the layer's own band changes most (hue as well as brightness),
    with colour stops taken from the band; flat if the band is uniform."""
    cy, cx = np.nonzero(band)
    col = rgb[cy, cx]
    A = np.c_[cx - cx.mean(), cy - cy.mean()]
    J = np.linalg.lstsq(A, col - col.mean(0), rcond=None)[0]      # 2x3 colour Jacobian
    u, sv, _ = np.linalg.svd(J)
    if len(cx) < 400 or sv[0] * 200 < 6:                           # < 6 levels across 200 px
        return hexc(np.median(col, axis=0))
    g = u[:, 0]
    t = A @ g
    t0, t1 = np.percentile(t, 2), np.percentile(t, 98)
    stops = []
    for q in np.linspace(0, 1, 4):
        tq = t0 + q * (t1 - t0)
        sel = np.abs(t - tq) <= max((t1 - t0) / 8.0, 2.0)
        if sel.any():
            stops.append((q, np.median(col[sel], axis=0)))
    m = np.array([cx.mean(), cy.mean()])
    pa, pb = m + t0 * g, m + t1 * g
    defs.append(f'<linearGradient id="{gid}" gradientUnits="userSpaceOnUse" x1="{pa[0]:.1f}" y1="{pa[1]:.1f}" '
                f'x2="{pb[0]:.1f}" y2="{pb[1]:.1f}">'
                + "".join(f'<stop offset="{q:.2f}" stop-color="{hexc(c)}"/>' for q, c in stops)
                + "</linearGradient>")
    return f"url(#{gid})"


hexc = lambda c: "#%02x%02x%02x" % tuple(int(round(v)) for v in np.clip(c, 0, 255))
layers = []
for k in range(levels):
    hi, lo = qs[k], qs[k + 1]
    region = logo & (L < hi)
    if k > 0:
        # inner layers are smoothed more: their edges are colour transitions, not shape edges
        region = ndimage.binary_opening(region, iterations=1)
    band = logo & (L < hi) & (L >= lo)
    if region.sum() < 60 or band.sum() == 0:
        continue
    colour = gradient_fill(band, f"g{len(layers)}")
    d = outline(region, sigma=0.9 if k == 0 else INNER_SIGMA, tol=0.9 if k == 0 else INNER_TOL)
    if d:
        layers.append((colour, d))

svg = (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="{x0} {y0} {x1 - x0} {y1 - y0}" '
       f'width="{x1 - x0}" height="{y1 - y0}">\n<title>MEDUSA</title>\n'
       + "<defs>\n" + "\n".join(defs) + "\n</defs>\n"
       + "\n".join(f'<path fill="{c}" fill-rule="evenodd" d="{d}"/>' for c, d in layers)
       + "\n</svg>\n")
open(dst, "w").write(svg)
print(f"{len(layers)} layers, viewBox {x0} {y0} {x1 - x0} {y1 - y0}, {len(svg) // 1024} KiB")
