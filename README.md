# MEDUSA: An MTBDD-based quantum circuit simulator

[![tests](https://github.com/QuantumFIT/MEDUSA/actions/workflows/tests.yml/badge.svg)](https://github.com/QuantumFIT/MEDUSA/actions/workflows/tests.yml)
[![tag](https://img.shields.io/github/v/tag/QuantumFIT/MEDUSA?sort=semver)](https://github.com/QuantumFIT/MEDUSA/tags)
[![codecov](https://img.shields.io/codecov/c/github/QuantumFIT/MEDUSA/devel?logo=codecov)](https://codecov.io/gh/QuantumFIT/MEDUSA)
[![license: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![DOI](https://img.shields.io/badge/DOI-10.1145%2F3676536.3676711-blue.svg)](https://doi.org/10.1145/3676536.3676711)

**MEDUSA** (**M**ulti-Terminal Binary Decision Diagram-based **Q**uantum **S**imulator) is an MTBDD-based quantum circuit simulator supporting `OpenQASM` circuits. It is written in C and supports multiple MTBDD backends.

**MoToBuddy is the preferred backend** (`make` / `./MEDUSA`). Sylvan is an optional C-only package for comparison with original MEDUSA; it does not support MOSF / `USE_CXX=1`.

## Build

**Dependencies:**
* `gmp` library (`libgmp-dev`)
* [MoToBuddy](https://github.com/VeriFIT/MoToBuddy) (required)
* [Sylvan](https://github.com/trolando/sylvan) v1.8.1 + Lace v1.4.1 (optional, `make init-sylvan`)

Download and build MoToBuddy (requires `git`):
```
make init
```
Then build (default is MoToBuddy `__float128` / f128). That also creates `./MEDUSA` as a symlink to `./MEDUSA_buddy_doubles_f128`:
```
make
```
`make help` lists all targets.

Optional Sylvan backend (C gates only):
```
make init-sylvan
make sylvan_doubles          # ./MEDUSA_sylvan_doubles_f128
make sylvan_gmp              # ./MEDUSA_sylvan_gmp
```

## Backends

The default product is MoToBuddy, selectable at compile time by leaf type:

| Target | Backend | Leaf type |
|---|---|---|
| `make` / `make buddy_doubles_f128` | **MoToBuddy (preferred)** | Complex floating-point re+im (__float128) |
| `make buddy_gmp` | MoToBuddy | Algebraic integers (exact, GMP) |
| `make buddy_doubles_f32` | MoToBuddy | Complex floating-point re+im (float) |
| `make buddy_doubles_f64` | MoToBuddy | Complex floating-point re+im (double) |
| `make buddy_doubles_f80` | MoToBuddy | Complex floating-point re+im (long double) |
| `make buddy_doubles_all` | MoToBuddy | All floating-point variants above |
| `make sylvan_doubles` | Sylvan (optional) | Same float leaves as MoToBuddy (`LEAF_FLOAT_TYPE`) |
| `make sylvan_gmp` | Sylvan (optional) | Algebraic integers (exact, GMP) |

`buddy_mpfr` is not implemented. Sylvan has no C++ / MOSF path.

To enable C++ gate traversal and MOSF simulation support (experimental, MoToBuddy only):
```
make buddy_doubles_f128 USE_CXX=1
```

## Tests

```
make test          # MoToBuddy unit + circuits + benchmarks + metamorphic
make test-sylvan   # same circuit/benchmark smokes on Sylvan, plus harder Grover/CCX
make test-all      # test + test-sylvan
```
See `tests/README.md`.

### Coverage

`COVERAGE=1` switches the build to `-O0 -g --coverage`, so the suites record
gcov data. Reports are produced with [gcovr](https://gcovr.com/)
(`apt install gcovr`) as a Cobertura `coverage.xml`:
```
make coverage       # default product: make test + make test-grover
make coverage-all   # adds the optional Sylvan backend (needs init-sylvan)
```
`make coverage-all` is what CI measures, so its number is the one behind the
Codecov badge; `make coverage` skips Sylvan and therefore reports slightly
lower. `make coverage-cxx` measures the C++ gate implementations instead
(`USE_CXX=1`); CI uploads it separately under the Codecov flag `cxx`, because
the two builds instrument different line sets of the same sources.

## Usage

The simulator accepts input files in the `OpenQASM` format. Several circuit files can be found in the `benchmarks` directory:
```
./MEDUSA --file benchmarks/no-measure/BernsteinVazirani/01.qasm
```
Run with `--info` to print wall-clock time and peak physical memory usage. MEDUSA also supports symbolic loop simulation via `--symbolic`. For all options:
```
./MEDUSA --help
```

The result of the simulation is written to `res.dot`. Converting large diagrams to a viewable format can take a while - use [Graphviz](https://graphviz.org/):
```
make plot
```
When leaf values are very large, substitute variable names are used in `res.dot`. Their values are stored in `res-vars.txt`.

## Profiling

To profile with Valgrind's callgrind tool, build with `PROFILE=1`:
```
make PROFILE=1
```
This disables optimisation (`-O0`) and keeps debug symbols so callgrind can annotate sources. Then run:
```
valgrind --tool=callgrind ./MEDUSA --file benchmarks/...
callgrind_annotate callgrind.out.<pid>
```

## Citing MEDUSA

MEDUSA is MIT-licensed, so you are free to use it without conditions beyond
keeping the copyright notice. If MEDUSA contributes to academic work, we would
be grateful if you cited the paper it is based on:

> Tian-Fu Chen, Yu-Fang Chen, Jie-Hong Roland Jiang, Sára Jobranová, and
> Ondřej Lengál. *Accelerating Quantum Circuit Simulation with Symbolic
> Execution and Loop Summarization*. In Proceedings of the 43rd IEEE/ACM
> International Conference on Computer-Aided Design (ICCAD '24), pages 1-9.
> ACM, 2024. [doi:10.1145/3676536.3676711](https://doi.org/10.1145/3676536.3676711)

```bibtex
@inproceedings{medusa-iccad24,
  author    = {Chen, Tian-Fu and Chen, Yu-Fang and Jiang, Jie-Hong Roland and
               Jobranov{\'a}, S{\'a}ra and Leng{\'a}l, Ond{\v r}ej},
  title     = {Accelerating Quantum Circuit Simulation with Symbolic Execution
               and Loop Summarization},
  booktitle = {Proceedings of the 43rd IEEE/ACM International Conference on
               Computer-Aided Design (ICCAD '24)},
  publisher = {Association for Computing Machinery},
  year      = {2024},
  pages     = {1--9},
  doi       = {10.1145/3676536.3676711}
}
```

This request is a courtesy, not a licence condition. Contributors are listed in
[`AUTHORS`](AUTHORS).

## License

MEDUSA is released under the [MIT License](LICENSE).

It builds against third-party components under their own licenses - notably
GMP (LGPL-3.0-or-later or GPL-2.0-or-later, linked dynamically), MoToBuddy /
BuDDy (permissive, BSD-like), and optionally Sylvan and Lace (Apache-2.0).
Their notices, which must be reproduced in redistributions including binaries,
are collected in [`THIRD-PARTY-NOTICES.md`](THIRD-PARTY-NOTICES.md).
