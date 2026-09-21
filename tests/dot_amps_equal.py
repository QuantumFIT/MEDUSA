#!/usr/bin/env python3
"""Compare two MEDUSA res.dot files by basis-state amplitude.

    dot_amps_equal.py A.dot B.dot N

Exits 0 when the two decision diagrams assign the same terminal label to every
one of the 2^N basis states, 1 otherwise, printing each disagreement.

Why not just diff the files: node ids in res.dot are allocation order, so two
runs that agree on every amplitude can still differ byte for byte. The MOSF
and OpenQASM front ends build the same state while numbering its terminals
differently, and a byte comparison calls that a failure.

Labels are compared as strings rather than parsed as numbers. Both sides are
the same binary and the same leaf type, so agreement should be exact; parsing
would only introduce a tolerance where none is wanted.
"""
import re
import sys


def parse(path):
    """Return (root, var_of, term_of, low, high) from a res.dot."""
    var_of, term_of, low, high, root = {}, {}, {}, {}, None
    for line in open(path, encoding="utf-8", errors="replace"):
        m = re.search(r"invisible -> (\d+)", line)
        if m:
            root = int(m.group(1))
        # Terminals: filled boxes carrying the amplitude.
        for m in re.finditer(r'(\d+) \[label="([^"]*)", style=filled,shape=box\]', line):
            term_of[int(m.group(1))] = m.group(2).strip()
        # Internal nodes: label is the variable level.
        for m in re.finditer(r'(\d+) \[label="(\d+)"\]', line):
            node = int(m.group(1))
            if node not in term_of:
                var_of[node] = int(m.group(2))
        for m in re.finditer(r"(\d+) -> (\d+) \[style=(dashed|filled)\]", line):
            src, dst, style = int(m.group(1)), int(m.group(2)), m.group(3)
            (low if style == "dashed" else high)[src] = dst
    return root, var_of, term_of, low, high


def amplitudes(path, n):
    """Terminal label for each of the 2^n basis states, index i = bit pattern."""
    root, var_of, term_of, low, high = parse(path)
    out = []
    for state in range(1 << n):
        node = root
        # Walk to a terminal. A missing edge or an unlabelled node means the
        # path leads to the zero sink, which res.dot does not always draw.
        while node is not None and node not in term_of:
            if node not in var_of:
                node = None
                break
            edges = high if (state >> var_of[node]) & 1 else low
            node = edges.get(node)
        out.append(term_of[node] if node in term_of else "0")
    return out


def main():
    if len(sys.argv) != 4:
        print(__doc__, file=sys.stderr)
        return 2
    a_path, b_path, n = sys.argv[1], sys.argv[2], int(sys.argv[3])
    a, b = amplitudes(a_path, n), amplitudes(b_path, n)

    bad = [(i, x, y) for i, (x, y) in enumerate(zip(a, b)) if x != y]
    if not bad:
        return 0
    for i, x, y in bad[:8]:
        print(f"  basis {i:0{n}b}:  {a_path.split('/')[-1]}={x!r}  "
              f"{b_path.split('/')[-1]}={y!r}", file=sys.stderr)
    if len(bad) > 8:
        print(f"  ... and {len(bad) - 8} more", file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
