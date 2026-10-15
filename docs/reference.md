---
title: Command-line reference
---

[Home](./) · [Installation](installation) · [Usage](usage) · [Reference](reference)

# Command-line reference

## `MEDUSA`: simulation

```console
$ ./MEDUSA [options]
```

| Option | Argument | Effect |
|---|---|---|
| `--file`, `-f` | file | OpenQASM circuit to simulate (default: standard input; circuits with loops need a file) |
| `--symbolic`, `-s` | | summarize `for` loops symbolically ([details](usage#loops-and-symbolic-simulation)) |
| `--measure`, `-m` | file, optional | sample the measured qubits at the end of the circuit; print the counts, or write them to the file |
| `--nsamples`, `-n` | number | number of samples for `--measure` (default 1024) |
| `--probability`, `-p` | | write \|amplitude\|² into `res.dot` instead of amplitudes |
| `--info`, `-i` | | print the simulation time and peak memory use |
| `--norm-error`, `-e` | | track the norm of the state after every gate |
| `--norm-csv`, `-c` | file | CSV file for `--norm-error` (default `norm_track.csv`) |
| `--tree-simulation`, `-t` | | read the input as MOSF instead of OpenQASM (experimental; needs a `USE_CXX=1` build) |
| `--help`, `-h` | | list the options |

### Output files

All are written to the current directory.

| File | Content |
|---|---|
| `res.dot` | the final state as a Graphviz decision diagram ([how to read it](usage#the-result)) |
| `res-vars.txt` | values of large algebraic numbers that `res.dot` refers to by name (`MEDUSA_buddy_gmp` only) |
| `norm_track.csv` | per-gate norm deviation, with `--norm-error` |

## `make`: building and testing

| Target | |
|---|---|
| `make init` | clone and build MoToBuddy at the pinned commit (once) |
| `make` | build the default binary, `MEDUSA_buddy_doubles_f128`, and link `./MEDUSA` to it |
| `make buddy_doubles_f32` / `_f64` / `_f80` / `_f128` | floating-point builds ([which one](installation#choosing-the-number-representation)) |
| `make buddy_doubles_all` | all four floating-point builds |
| `make buddy_gmp` | the exact algebraic build |
| `make init-sylvan`, `make sylvan_doubles`, `make sylvan_gmp` | the optional Sylvan backend |
| `make plot` | render `res.dot` as `res.pdf` (needs Graphviz) |
| `make test` | the main test suites |
| `make test-grover` | Grover's search across all number representations, with and without `--symbolic` |
| `make test-sylvan` | the circuit tests on the Sylvan backend |
| `make help` | all targets |

| Variable | Effect |
|---|---|
| `BUILD=debug` | `-O0 -g` instead of `-O2 -g` |
| `LTO=0` | no link-time optimization |
| `USE_CXX=1` | C++ gate implementations and MOSF input (experimental) |
| `COVERAGE=1` | instrument for gcov (`make coverage` produces the report) |

The test suites are described in [`tests/README.md`](https://github.com/QuantumFIT/MEDUSA/blob/devel/tests/README.md).
