# MEDUSA: An MTBDD-based quantum circuit simulator

**MEDUSA** (**M**ulti-Terminal Binary Decision Diagram-based **Q**uantum **S**imulator) is an MTBDD-based quantum circuit simulator supporting `OpenQASM` circuits. It is written in C and supports multiple MTBDD backends.

## Build

**Dependencies:**
* `gmp` library (`libgmp-dev`)
* [MoToBuddy](https://github.com/VeriFIT/MoToBuddy)

Download and build MoToBuddy (requires `git`):
```
make init
```
Then build (default is MoToBuddy `__float128` / f128):
```
make
```
`make help` lists all targets.

## Backends

MEDUSA uses the MoToBuddy MTBDD backend, selectable at compile time by leaf type:

| Target | Leaf type |
|---|---|
| `make` / `make buddy_doubles_f128` | Complex floating-point re+im (__float128) |
| `make buddy_gmp` | Algebraic integers (exact, GMP) |
| `make buddy_doubles_f32` | Complex floating-point re+im (float) |
| `make buddy_doubles_f64` | Complex floating-point re+im (double) |
| `make buddy_doubles_f80` | Complex floating-point re+im (long double) |
| `make buddy_doubles_all` | All floating-point variants above |

Sylvan and `buddy_mpfr` targets are not available in this tree.


To enable C++ gate traversal and MOSF simulation support (experimental, use with caution):
```
make buddy_doubles_f128 USE_CXX=1
```

## Usage

The simulator accepts input files in the `OpenQASM` format. Several circuit files can be found in the `benchmarks` directory:
```
./MEDUSA_buddy_doubles_f128 --file benchmarks/no-measure/BernsteinVazirani/01.qasm
```
Run with `--info` to print wall-clock time and peak physical memory usage. MEDUSA also supports symbolic loop simulation via `--symbolic`. For all options:
```
./MEDUSA_buddy_doubles_f128 --help
```

The result of the simulation is written to `res.dot`. Converting large diagrams to a viewable format can take a while - use [Graphviz](https://graphviz.org/):
```
make plot
```
When leaf values are very large, substitute variable names are used in `res.dot`. Their values are stored in `res-vars.txt`.

## License

Simulator sources in this tree are MIT (see `LICENSE`). MoToBuddy/BuDDy and GMP have their own licenses.

## Profiling

To profile with Valgrind's callgrind tool, build with `PROFILE=1`:
```
make buddy_doubles_f128 PROFILE=1
```
This disables optimisation (`-O0`) and keeps debug symbols so callgrind can annotate sources. Then run:
```
valgrind --tool=callgrind ./MEDUSA_buddy_doubles_f128 --file benchmarks/...
callgrind_annotate callgrind.out.<pid>
```