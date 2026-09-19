#!/usr/bin/env python3
"""Enumerate basis |amp|^2 from a MEDUSA res.dot MTBDD export."""
import re
import sys
import math

# Terminal labels from the floating-point backends have the form
# "<re><sign><im>i", where the real part may itself be signed:
#
#   -0.0012...-0.0373...i        0.5+0.5i        -1e-20+2e-21i        0.5
#
# Splitting on '+' alone silently mis-parsed every amplitude with a negative
# imaginary part: "0.0606...-0.0295..." was read as a single float, which threw
# ValueError and aborted the whole run. The separating sign is the one directly
# preceding the imaginary part, which is not necessarily the first sign in the
# label and must not be confused with the sign inside an exponent.
_FLOAT = r'(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?'
_RE_COMPLEX = re.compile(rf'^\s*(?P<re>[-+]?{_FLOAT})\s*(?P<im>[-+]\s*{_FLOAT})\s*i\s*$')
_RE_IMAG = re.compile(rf'^\s*(?P<im>[-+]?{_FLOAT})\s*i\s*$')
_RE_REAL = re.compile(rf'^\s*(?P<re>[-+]?{_FLOAT})\s*$')


def parse_amplitude(label):
    """Parse a floating-point terminal label into a (re, im) pair.

    Raises ValueError naming the label for anything else - in particular the
    algebraic/GMP backend's "(1/sqrt2)^(k) * (a+b.w+c.w2+d.w3)" form, which
    this tool does not implement. Failing loudly beats returning a wrong
    amplitude.
    """
    m = _RE_COMPLEX.match(label)
    if m:
        return float(m.group('re')), float(m.group('im').replace(' ', ''))
    m = _RE_IMAG.match(label)
    if m:
        return 0.0, float(m.group('im'))
    m = _RE_REAL.match(label)
    if m:
        return float(m.group('re')), 0.0
    raise ValueError(
        f"unrecognised terminal label {label!r}; this tool reads the "
        f"floating-point backends only (not algebraic/GMP labels)")


def parse_dot(path):
    root = None
    var_of = {}
    term_of = {}
    low = {}
    high = {}

    for line in open(path):
        for m in re.finditer(r'invisible -> (\d+)', line):
            root = int(m.group(1))
        for m in re.finditer(r'(\d+) \[label="(\d+)"\]', line):
            if 'shape=box' not in line[m.start():m.end()+20]:
                var_of[int(m.group(1))] = int(m.group(2))
        for m in re.finditer(r'(\d+) \[label="([^"]+)", style=filled,shape=box\]', line):
            term_of[int(m.group(1))] = parse_amplitude(m.group(2))
        for m in re.finditer(r'(\d+) -> (\d+) \[style=(dashed|filled)\]', line):
            src, dst, sty = int(m.group(1)), int(m.group(2)), m.group(3)
            if sty == 'dashed':
                low[src] = dst
            else:
                high[src] = dst

    return root, var_of, term_of, low, high

def amp_prob(node, var_of, term_of, low, high):
    if node in term_of:
        re_, im_ = term_of[node]
        return re_ * re_ + im_ * im_
    v = var_of[node]
    pl = amp_prob(low[node], var_of, term_of, low, high)
    ph = amp_prob(high[node], var_of, term_of, low, high)
    return pl + ph  # WRONG for shared structure - need path enumeration

def walk(node, var_of, term_of, low, high, assign, n, out):
    if node in term_of:
        re_, im_ = term_of[node]
        p = re_ * re_ + im_ * im_
        out.append((assign.copy(), p))
        return
    v = var_of[node]
    assign[v] = '0'
    walk(low[node], var_of, term_of, low, high, assign, n, out)
    assign[v] = '1'
    walk(high[node], var_of, term_of, low, high, assign, n, out)

def basis_prob(root, var_of, term_of, low, high, bits, n):
    node = root
    while node not in term_of:
        v = var_of[node]
        node = high[node] if bits[v] == '1' else low[node]
    re_, im_ = term_of[node]
    return re_ * re_ + im_ * im_

def main():
    path = sys.argv[1] if len(sys.argv) > 1 else 'res.dot'
    n = int(sys.argv[2]) if len(sys.argv) > 2 else 17
    root, var_of, term_of, low, high = parse_dot(path)

    print(f"file={path} var_levels={len(set(var_of.values()))} terminals={len(term_of)}")

    bits = ['0'] * n
    sum_p = 0.0
    nz = 0
    min_p = 1.0
    max_p = 0.0
    uniq = set()
    for s in range(1 << n):
        for i in range(n):
            bits[i] = '1' if (s >> i) & 1 else '0'
        p = basis_prob(root, var_of, term_of, low, high, bits, n)
        sum_p += p
        if p > 1e-30:
            nz += 1
            min_p = min(min_p, p)
            max_p = max(max_p, p)
            uniq.add(round(p, 18))

    print(f"n={n} sum_basis={sum_p:.12g} nz={nz} min_p={min_p:.12g} max_p={max_p:.12g} unique_p={len(uniq)}")

if __name__ == '__main__':
    main()
