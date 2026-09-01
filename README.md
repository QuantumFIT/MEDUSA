# MEDUSA: An MTBDD-based quantum circuit simulator

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

## License

Simulator sources in this tree are MIT (see `LICENSE`). MoToBuddy/BuDDy, Sylvan/Lace, and GMP have their own licenses.

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