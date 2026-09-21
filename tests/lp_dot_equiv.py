#!/usr/bin/env python3
"""Compare two MEDUSA res.dot exports as functions, and check their norm.

    lp_dot_equiv.py A.dot B.dot N_QUBITS [ABS_TOL]
    lp_dot_equiv.py A.dot -     N_QUBITS [ABS_TOL]     (norm check of A only)

Exit 0 when the two MTBDDs denote the same state vector: every basis state gets
amplitudes within ABS_TOL (default 1e-9) of each other, and both vectors have
unit norm within the same tolerance. Exit 1 otherwise, with the first differing
basis state (as a partial assignment) on stdout. Exit 2 on a parse problem.

The comparison walks the two diagrams in lockstep with a memo on node pairs, so
it is linear in the diagram sizes and never enumerates the 2^N basis states.
Reduced diagrams for the same function are isomorphic only when their leaves are
bit-identical; the symbolic and concrete paths round differently, so one side
may keep a node where the other merged two nearly equal leaves. The walk
therefore also descends one side against a terminal on the other, and treats a
variable missing on one side as "does not depend on it".

Only the floating-point backends' terminal labels are understood.
"""
import re
import sys

_FLOAT = r'(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?'
_RE_COMPLEX = re.compile(rf'^\s*(?P<re>[-+]?{_FLOAT})\s*(?P<im>[-+]\s*{_FLOAT})\s*i\s*$')
_RE_IMAG = re.compile(rf'^\s*(?P<im>[-+]?{_FLOAT})\s*i\s*$')
_RE_REAL = re.compile(rf'^\s*(?P<re>[-+]?{_FLOAT})\s*$')


def parse_amplitude(label):
    m = _RE_COMPLEX.match(label)
    if m:
        return complex(float(m.group('re')), float(m.group('im').replace(' ', '')))
    m = _RE_IMAG.match(label)
    if m:
        return complex(0.0, float(m.group('im')))
    m = _RE_REAL.match(label)
    if m:
        return complex(float(m.group('re')), 0.0)
    raise ValueError(f'unsupported terminal label {label!r}')


class Dot:
    def __init__(self, path):
        self.root = None
        self.var = {}    # internal node -> variable index
        self.amp = {}    # terminal node -> complex amplitude
        self.low = {}
        self.high = {}
        for line in open(path):
            for m in re.finditer(r'invisible -> (\d+)', line):
                self.root = int(m.group(1))
            # Terminals: '<id> [label="<amp>", style=filled,shape=box]' for leaves,
            # '0 [shape=box, label="0", ...]' for the zero (false) terminal.
            if 'shape=box' in line:
                for m in re.finditer(r'(\d+) \[[^\]]*label="([^"]+)"[^\]]*shape=box', line):
                    self.amp[int(m.group(1))] = parse_amplitude(m.group(2))
            for m in re.finditer(r'(\d+) \[label="(\d+)"\];', line):
                self.var[int(m.group(1))] = int(m.group(2))
            for m in re.finditer(r'(\d+) -> (\d+) \[style=(dashed|filled)\]', line):
                src, dst = int(m.group(1)), int(m.group(2))
                (self.low if m.group(3) == 'dashed' else self.high)[src] = dst
        if self.root is None:
            raise ValueError(f'{path}: no root')
        for node in self.var:
            if node not in self.low or node not in self.high:
                raise ValueError(f'{path}: internal node {node} lacks a child')
        # Sanity: no internal node also parsed as a terminal, every edge target known.
        for node in list(self.var):
            if node in self.amp:
                del self.var[node]
        for src, dst in list(self.low.items()) + list(self.high.items()):
            if dst not in self.var and dst not in self.amp:
                raise ValueError(f'{path}: edge to unknown node {dst}')

    def level(self, node, n):
        return self.var[node] if node in self.var else n


def norm(d, n):
    """Sum of |amp|^2 over all 2^n basis states, via the diagram."""
    memo = {}

    def mass(node, lvl):
        # total mass of the subfunction rooted at node, over variables lvl..n-1
        v = d.level(node, n)
        skipped = 2.0 ** (v - lvl)
        if node in d.amp:
            return skipped * abs(d.amp[node]) ** 2
        if node not in memo:
            memo[node] = mass(d.low[node], v + 1) + mass(d.high[node], v + 1)
        return skipped * memo[node]

    return mass(d.root, 0)


def first_difference(a, b, n, tol):
    """Return None if a and b agree everywhere within tol, else a witness path."""
    memo = set()
    sys.setrecursionlimit(max(10000, 4 * n + 100))

    def walk(x, y, path):
        key = (x, y)
        if key in memo:
            return None
        memo.add(key)
        xa, ya = x in a.amp, y in b.amp
        if xa and ya:
            if abs(a.amp[x] - b.amp[y]) > tol:
                return path + [f'A={a.amp[x]} B={b.amp[y]}']
            return None
        vx, vy = a.level(x, n), b.level(y, n)
        v = min(vx, vy)
        if vx == v and vy == v:
            return (walk(a.low[x], b.low[y], path + [f'q{v}=0'])
                    or walk(a.high[x], b.high[y], path + [f'q{v}=1']))
        if vx == v:   # only A branches on v
            return (walk(a.low[x], y, path + [f'q{v}=0'])
                    or walk(a.high[x], y, path + [f'q{v}=1']))
        return (walk(x, b.low[y], path + [f'q{v}=0'])
                or walk(x, b.high[y], path + [f'q{v}=1']))

    return walk(a.root, b.root, [])


def main():
    if len(sys.argv) < 4:
        print(__doc__)
        return 2
    n = int(sys.argv[3])
    tol = float(sys.argv[4]) if len(sys.argv) > 4 else 1e-9
    try:
        a = Dot(sys.argv[1])
        b = None if sys.argv[2] == '-' else Dot(sys.argv[2])
    except ValueError as e:
        print(f'parse error: {e}')
        return 2
    for name, d in (('A', a), ('B', b)):
        for v in (d.var.values() if d else ()):
            if v >= n:
                print(f'{name}: variable {v} >= N_QUBITS={n}')
                return 2
    na = norm(a, n)
    bad = 0
    if abs(na - 1.0) > tol:
        print(f'A: norm {na!r} is not 1 within {tol}')
        bad = 1
    if b is None:
        if not bad:
            print(f'unit norm: {len(a.var)}+{len(a.amp)} nodes, norm {na:.12f}')
        return bad
    nb = norm(b, n)
    if abs(nb - 1.0) > tol:
        print(f'B: norm {nb!r} is not 1 within {tol}')
        bad = 1
    diff = first_difference(a, b, n, tol)
    if diff:
        print('differ at ' + ' '.join(diff))
        bad = 1
    if not bad:
        print(f'equivalent: {len(a.var)}+{len(a.amp)} vs {len(b.var)}+{len(b.amp)} nodes, '
              f'norms {na:.12f} {nb:.12f}')
    return bad


if __name__ == '__main__':
    sys.exit(main())
