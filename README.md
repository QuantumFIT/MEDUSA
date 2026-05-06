# MEDUSA: An MTBDD-based quantum circuit simulator

**MEDUSA** (**M**ulti-Terminal Binary Decision Diagram-based **Q**uantum **S**imulator) is an MTBDD-based quantum circuit simulator supporting `OpenQASM` circuits. It is written in C and supports multiple MTBDD backends.

## Build

**Dependencies:**
* `gmp` library (`libgmp-dev`)
* Backend library - either [Sylvan](https://trolando.github.io/sylvan/) or [MoToBuddy](https://github.com/VeriFIT/MoToBuddy) depending on the target (see Backends below)

For Sylvan-based targets, download Sylvan and Lace (requires `git`):
```
make init
```
For MoToBuddy-based targets instead:
```
make init-motobuddy
```
Then build the desired target, e.g.:
```
make buddy_doubles_f64
```

## Backends

MEDUSA supports multiple MTBDD backends selectable at compile time:

| Target | Backend | Leaf type |
|---|---|---|
| `make` | Sylvan | Algebraic integers (exact, GMP) |
| `make buddy_gmp` | MoToBuddy | Algebraic integers (exact, GMP) |
| `make buddy_doubles_f32` | MoToBuddy | Complex floating-point re+im (float) |
| `make buddy_doubles_f64` | MoToBuddy | Complex floating-point re+im (double) |
| `make buddy_doubles_f80` | MoToBuddy | Complex floating-point re+im (long double) |
| `make buddy_doubles_f128` | MoToBuddy | Complex floating-point re+im (__float128) |
| `make buddy_doubles_all` | MoToBuddy | All floating-point variants above |


To enable C++ gate traversal and MOSF simulation support (experimental, use with caution):
```
make buddy_doubles_f64 USE_CXX=1
```

## Usage

The simulator accepts input files in the `OpenQASM` format. Several circuit files can be found in the `benchmarks` directory:
```
./MEDUSA_buddy_doubles_f64 --file benchmarks/no-measure/BernsteinVazirani/01.qasm
```
Run with `--info` to print wall-clock time and peak physical memory usage. MEDUSA also supports symbolic loop simulation via `--symbolic`. For all options:
```
./MEDUSA_buddy_doubles_f64 --help
```

The result of the simulation is written to `res.dot`. Converting large diagrams to a viewable format can take a while - use [Graphviz](https://graphviz.org/):
```
make plot
```
When leaf values are very large, substitute variable names are used in `res.dot`. Their values are stored in `res-vars.txt`.

## Profiling

To profile with Valgrind's callgrind tool, build with `PROFILE=1`:
```
make buddy_doubles_f64 PROFILE=1
```
This disables optimisation (`-O0`) and keeps debug symbols so callgrind can annotate sources. Then run:
```
valgrind --tool=callgrind ./MEDUSA_buddy_doubles_f64 --file benchmarks/...
callgrind_annotate callgrind.out.<pid>
```