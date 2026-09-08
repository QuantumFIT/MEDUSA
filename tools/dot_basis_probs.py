#!/usr/bin/env python3
"""Enumerate basis |amp|^2 from a MEDUSA res.dot MTBDD export."""
import re
import sys
import math

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
            lab = m.group(2).replace('i', '').strip()
            if '+' in lab:
                re_s, im_s = lab.split('+', 1)
            else:
                re_s, im_s = lab, '0'
            term_of[int(m.group(1))] = (float(re_s), float(im_s))
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
