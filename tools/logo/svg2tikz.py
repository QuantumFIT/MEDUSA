"""Convert the stacked-layer MEDUSA logo SVG (as written by stack_trace.py) to
a standalone TikZ picture. Paths are reused verbatim through the svg.path
library; a gradient fill becomes a clip plus a horizontal shading laid out
between the gradient's own end points, so the colour stops land where they
do in the SVG.

usage: svg2tikz.py logo.svg out.tex
"""
import math
import re
import sys

svg = open(sys.argv[1]).read()
vb = [float(v) for v in re.search(r'viewBox="([^"]+)"', svg).group(1).split()]
grads = {}
for m in re.finditer(r'<linearGradient id="([^"]+)"[^>]*x1="([^"]+)" y1="([^"]+)" x2="([^"]+)" y2="([^"]+)">(.*?)</linearGradient>', svg):
    stops = [(float(o), c) for o, c in re.findall(r'offset="([^"]+)" stop-color="#([0-9a-f]{6})"', m.group(6))]
    grads[m.group(1)] = (tuple(float(m.group(k)) for k in range(2, 6)), stops)

colours = {}


def colour(hexstr):
    if hexstr not in colours:
        colours[hexstr] = f"mc{len(colours)}"
    return colours[hexstr]


body, shadings = [], []
for fill, d in re.findall(r'<path fill="([^"]+)" fill-rule="evenodd" d="([^"]+)"/>', svg):
    d = d.replace("Z", " Z ").strip()
    if fill.startswith("#"):
        body.append(f"\\fill[{colour(fill[1:])}, even odd rule] svg {{{d}}};")
        continue
    gid = fill[5:-1]
    (x1, y1, x2, y2), stops = grads[gid]
    length = math.hypot(x2 - x1, y2 - y1)
    angle = math.degrees(math.atan2(y2 - y1, x2 - x1))
    # horizontal shading: 0bp .. 100bp spans the gradient's own length
    spec = "; ".join(f"color({100 * o:.1f}bp)=({colour(c)})" for o, c in stops)
    shadings.append(f"\\pgfdeclarehorizontalshading{{{gid}}}{{100bp}}{{{spec}}}")
    first, last = colour(stops[0][1]), colour(stops[-1][1])
    big = 2000
    body.append(
        f"\\begin{{scope}}[even odd rule]\n  \\clip svg {{{d}}};\n"
        f"  \\begin{{scope}}[shift={{({x1:.1f},{y1:.1f})}}, rotate={angle:.2f}]\n"
        f"    \\fill[{first}] (-{big},-{big}) rectangle (0,{big});\n"
        f"    \\fill[{last}] ({length:.1f},-{big}) rectangle ({length + big:.1f},{big});\n"
        f"    \\shade[shading={gid}] (0,-{big}) rectangle ({length:.1f},{big});\n"
        f"  \\end{{scope}}\n\\end{{scope}}")

defs = "\n".join(f"\\definecolor{{{name}}}{{HTML}}{{{h.upper()}}}" for h, name in colours.items())
tex = f"""% MEDUSA logo as a TikZ picture (generated from medusa-logo.svg).
% Standalone: `pdflatex medusa-logo.tex`. To include it elsewhere, copy the
% preamble lines (svg.path, the colours and shadings) and the tikzpicture.
% The picture is {vb[2]:.0f} x {vb[3]:.0f} pt; scale it with [scale=...].
\\documentclass[tikz]{{standalone}}
\\usetikzlibrary{{svg.path}}
{defs}
{chr(10).join(shadings)}
\\begin{{document}}
\\begin{{tikzpicture}}[x=1pt, y=1pt, yscale=-1, line join=round]
\\useasboundingbox ({vb[0]:.0f},{vb[1]:.0f}) rectangle ({vb[0] + vb[2]:.0f},{vb[1] + vb[3]:.0f});
{chr(10).join(body)}
\\end{{tikzpicture}}
\\end{{document}}
"""
open(sys.argv[2], "w").write(tex)
print(f"{len(body)} layers, {len(shadings)} shadings, {len(colours)} colours, {len(tex) // 1024} KiB")
