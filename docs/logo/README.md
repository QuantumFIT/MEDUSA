# MEDUSA logo

| File | What |
|---|---|
| `medusa-logo.png` | the original, 1254 × 1254, white background |
| `medusa-logo-transparent.png` | the same with a transparent background |
| `medusa-logo-web.png` | transparent, 292 × 400, cropped to the logo (used by the website) |
| `medusa-logo.svg` | vector version |
| `medusa-logo.tex` | TikZ version, a standalone document |
| `medusa-logo.pdf` | the TikZ version compiled, for `\includegraphics` |

**The vector versions are traced from the PNG.**
- **How they're made:** 24 stacked brightness layers, each filled with a gradient fitted to its own pixels.
- **Fidelity:** they reproduce the original closely. The mean colour difference is about 3.5/255, and the white gaps between shapes, including where the orbit passes in front of the bell, come out as real, transparent gaps.
- **Not hand-drawn:** each layer is a traced outline, not a designed shape, so expect large files of path data.

**Using the TikZ version.** It needs only the `svg.path` library. To use it inside another document:
1. copy its preamble lines (the `\definecolor` and `\pgfdeclarehorizontalshading` lines);
2. copy the `tikzpicture`;
3. scale it with `[scale=…]`. Its natural size is 806 × 1097 pt.

## Regenerating

The scripts are in `tools/logo/`. They need Pillow, NumPy and SciPy; `stack_trace.py` also needs scikit-image.

```console
$ python3 tools/logo/make_transparent.py docs/logo/medusa-logo.png docs/logo/medusa-logo-transparent.png
$ python3 tools/logo/stack_trace.py docs/logo/medusa-logo.png docs/logo/medusa-logo.svg
$ python3 tools/logo/svg2tikz.py docs/logo/medusa-logo.svg docs/logo/medusa-logo.tex
$ (cd docs/logo && pdflatex medusa-logo.tex)
```
