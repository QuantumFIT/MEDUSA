---
title: Installation
---

[Home](./) · [Installation](installation) · [Usage](usage) · [Reference](reference)

# Installation

## Requirements

- **Linux.** CI builds and tests MEDUSA on Ubuntu (x86-64).
- **A C compiler: GCC 11 or newer.** The MoToBuddy library does not compile with older GCC.
- **GMP**, **CMake**, **git** and a **C++ compiler** (MoToBuddy is partly C++), **Python 3** for the test suites, and **flex** and **bison**, which MoToBuddy's build configuration needs.

On Ubuntu:

```console
$ sudo apt install build-essential cmake git libgmp-dev \
                   python3 flex bison
```

[Graphviz](https://graphviz.org/) (`sudo apt install graphviz`) is optional; it turns the resulting `res.dot` into an image.

## Get and build MEDUSA

```console
$ git clone https://github.com/QuantumFIT/MEDUSA.git
$ cd MEDUSA
$ make init
$ make
```

`make init` clones and builds MoToBuddy, at the commit MEDUSA pins; run it again after pulling a change of that pin.

`make` builds `./MEDUSA_buddy_doubles_f128` and links `./MEDUSA` to it. MEDUSA is not installed system-wide; it runs from the repository.

## Choosing the number representation

The default build stores amplitudes as pairs of 128-bit floating-point numbers. The representation is fixed at build time, and each one gets its own binary, named after the target: `make buddy_doubles_f64` builds `MEDUSA_buddy_doubles_f64`, and so on.

| Target | Amplitudes |
|---|---|
| `buddy_doubles_f128` | 128-bit floating point (the default, also built by plain `make`) |
| `buddy_doubles_f80` | 80-bit (x87 `long double`) |
| `buddy_doubles_f64` | 64-bit (`double`) |
| `buddy_doubles_f32` | 32-bit (`float`) |
| `buddy_doubles_all` | all four floating-point builds |
| `buddy_gmp` | exact algebraic numbers (GMP) |

The algebraic representation computes Clifford+T circuits exactly, at the cost of speed; the floating-point ones round, and are faster the narrower they are.

## Build types

| Command | Flags | Use |
|---|---|---|
| `make` | `-O2 -g`, link-time optimization | normal use, benchmarks |
| `make BUILD=debug` | `-O0 -g` | debugging with gdb or valgrind |

Both produce the same binary names, so a debug build replaces the release binary. The object files are kept apart (`obj/release`, `obj/debug`), so switching back only relinks. `LTO=0` builds a release binary without link-time optimization.

## Optional components

- **C++ gate implementations and MOSF input** (experimental). `make USE_CXX=1` compiles an alternative, traversal-based implementation of some gates and enables the MOSF input format (`--tree-simulation`).

## Check the installation

```console
$ ./MEDUSA --file benchmarks/no-measure/BernsteinVazirani/01.qasm --info
Time=0.000357s
Peak Memory Usage=4608kB
$ make test        # the test suites; a few minutes
```

`make help` lists all build targets.
