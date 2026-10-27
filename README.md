# MEDUSA: An MTBDD-based quantum circuit simulator

<img src="docs/logo/medusa-logo-web.png" alt="MEDUSA logo" width="150" align="right">

[![tests](https://github.com/QuantumFIT/MEDUSA/actions/workflows/tests.yml/badge.svg)](https://github.com/QuantumFIT/MEDUSA/actions/workflows/tests.yml)
[![tag](https://img.shields.io/github/v/tag/QuantumFIT/MEDUSA?sort=semver)](https://github.com/QuantumFIT/MEDUSA/tags)
[![codecov](https://img.shields.io/codecov/c/github/QuantumFIT/MEDUSA/devel?logo=codecov)](https://codecov.io/gh/QuantumFIT/MEDUSA)
[![license: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![DOI](https://img.shields.io/badge/DOI-10.1145%2F3676536.3676711-blue.svg)](https://doi.org/10.1145/3676536.3676711)

**MEDUSA** (**M**ulti-Terminal Binary Decision Diagram-based **Q**uantum **S**imulator) is an MTBDD-based quantum circuit simulator supporting `OpenQASM` circuits. It is written in C on top of the [MoToBuddy](https://github.com/VeriFIT/MoToBuddy) MTBDD library.

**Documentation:** [quantum.fit.vut.cz/MEDUSA](http://quantum.fit.vut.cz/MEDUSA/), covering installation, usage and the command-line reference.

## Build

**Dependencies:**
* `gmp` library (`libgmp-dev`)
* [MoToBuddy](https://github.com/VeriFIT/MoToBuddy) (required)

Download and build MoToBuddy (requires `git`):
```
make init
```
Then build (default is MoToBuddy `__float128` / f128). That also creates `./MEDUSA` as a symlink to `./MEDUSA_buddy_doubles_f128`:
```
make
```
`make help` lists all targets.

### Build types

| | flags | object tree | use |
|---|---|---|---|
| `make` (`BUILD=release`) | `-O2 -g`, LTO | `obj/release` | what CI tests and what benchmarks measure |
| `make BUILD=debug` | `-O0 -g -fno-omit-frame-pointer` | `obj/debug` | gdb, valgrind, gcov: every local visible, every line its own line |

Both produce the same binary names (`./MEDUSA_buddy_doubles_f128`, ...), so a
debug build overwrites the release binary; the object trees are separate, so
switching back is a relink, not a rebuild. `COVERAGE=1` is the debug build plus
`--coverage` (`obj/coverage`); `PROFILE=1` is kept as an alias for `BUILD=debug`.
`LTO=0` gives a release build without link-time optimisation; `USE_CXX=1` and
`MEDUSA_DEBUG=1` combine with either type and get their own trees
(`obj/release-cxx`, `obj/debug-dbglog`, ...).

## Leaf types

MEDUSA runs on the MoToBuddy MTBDD library. The leaf type, i.e. how amplitudes
are stored, is chosen at compile time:

| Target | Leaf type |
|---|---|
| `make` / `make buddy_doubles_f128` | Complex floating-point re+im (__float128), the default |
| `make buddy_gmp` | Algebraic integers (exact, GMP) |
| `make buddy_doubles_f32` | Complex floating-point re+im (float) |
| `make buddy_doubles_f64` | Complex floating-point re+im (double) |
| `make buddy_doubles_f80` | Complex floating-point re+im (long double) |
| `make buddy_doubles_all` | All floating-point variants above |

`buddy_mpfr` is not implemented.

To enable C++ gate traversal and MOSF simulation support (experimental):
```
make buddy_doubles_f128 USE_CXX=1
```

## Tests

```
make test          # unit + circuits + benchmarks + metamorphic
```
See `tests/README.md`.

### Coverage

`COVERAGE=1` selects the debug build (`-O0 -g`) plus `--coverage`, so the suites record
gcov data. Reports are produced with [gcovr](https://gcovr.com/)
(`apt install gcovr`) as a Cobertura `coverage.xml`:
```
make coverage       # make test + make test-grover
```
`make coverage` is what CI measures, so its number is the one behind the
Codecov badge. `make coverage-cxx` measures the C++ gate implementations instead
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

To profile with Valgrind's callgrind tool, use the debug build (`PROFILE=1` is an alias):
```
make BUILD=debug
```
This disables optimisation (`-O0`) and keeps debug symbols so callgrind can annotate sources. Then run:
```
valgrind --tool=callgrind ./MEDUSA --file benchmarks/...
callgrind_annotate callgrind.out.<pid>
```
For wall-clock profiles of the optimised code use `perf record` on the default release build instead; it keeps `-g`, so symbols resolve.

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
BuDDy (permissive, BSD-like).
Their notices, which must be reproduced in redistributions including binaries,
are collected in [`THIRD-PARTY-NOTICES.md`](THIRD-PARTY-NOTICES.md).
