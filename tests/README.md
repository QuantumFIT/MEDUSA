# Tests

```
make init-motobuddy   # once (clones/builds VeriFIT/MoToBuddy) - preferred backend
make init-sylvan      # optional (Sylvan v1.8.1 + Lace; C path only)
make test             # MoToBuddy unit API + small circuit smokes + metamorphic (doubles f128)
make test-sylvan      # same circuit/benchmark smokes on Sylvan + harder Grover/CCX + GMP
make test-all         # test + test-sylvan
make test-stress      # extreme GC/terminal stress for doubles f128 AND gmp
make test-leaks       # valgrind definite+reachable (unit, stress LEVEL=1, symbolic Grover/05)
make test-grover      # LP-Grover n=5,6,7 × {loop, loop-symbolic, NL} × {f32,f64,f80,f128,gmp}
make test-unit-leaf-types # the unit suite against each leaf representation
make test USE_CXX=1   # same suite against the C++ gate implementations
make test-grover USE_CXX=1   # ...and the Grover matrix (covers symbolic x/z)
make coverage         # gcov/gcovr report over test + test-grover
make coverage-cxx     # same, but the C++ gate implementations (USE_CXX=1)
make coverage-all     # as CI measures it: also test-sylvan (needs init-sylvan)
```

Each C/bash suite ends with a **colorful pass/fail summary table** (ANSI when stdout is a TTY;
plain text when piped). Shared helpers: `tests/test_harness.h`, `tests/test_summary.sh`.

- `make test-unit` - protect/unprotect, leaf ownership, apply free-of-unused, gates,
  plus counter checks that MoToBuddy actually calls `freePimpl`
- `make test-circuits` - runs `MEDUSA_buddy_doubles_f128` on small QASM files
  (`MEDUSA_BIN=...` overrides the binary; used by `test-sylvan`), then
  `test_rotation_cache.sh` (op-cache must include rotation angles on both backends)
- `make test-benchmarks` - structural checks (`test_benchmarks.sh`: digraph + unit
  norm) **plus** semantic checks (`test_benchmark_semantics`):
  - **BV**: final MTBDD is the secret basis state (`|11⟩`, `|101011⟩`, …)
  - **MOGrover**: max basis prob ≫ uniform
  - **Reversible** (MCToffoli / Feynman / RevLib): returns `|0…0⟩`
  - **PF / QC**: unit norm + non-trivial / non-spurious support
- `make test-metamorphic` - Stage 4 metamorphic checks via **OpenQASM** (`test_metamorphic`):
  - fixtures in `tests/qasm/metamorphic/`; random circuits written under `/tmp` then `sim_file`
  - random `U`: `U U† |0…0⟩ = |0…0⟩`
  - `(UV)† = V† U†` as `U;V;V†;U†` round-trip to `|0…0⟩`
  - identities `H²`, Paulis², `CX²`, `S⁴`, `T T†`, `Rx(π/2)⁴`
  - **CZ both OpenQASM orders** (`cz c,t` with `c<t` and `c>t`) - relies on `sim.c` swap
  - reverse-without-adj ≠ `(TH)†` on `|1⟩`
  - **symbolic gates vs classic**: fixtures in `tests/qasm/symbolic/` pair a loop
    body with its unrolled equivalent, one pair per gate (`cx`, `s`, `y`,
    `rx(π/2)`, `ry(π/2)`, `mcx`), plus the dense Clifford+T `mixed_th` pair.
    Each asserts the `--symbolic` run matches the classic run on **every basis
    amplitude** - a norm check would miss a dropped phase. This is what covers
    `src/gates_symb.c`, which `LP-Grover/05` alone leaves at ~16%.
  - **heavy GC**: after each success, repeated `forceGC` while the result root stays
    protected; plus deep `U U†`, ~12k distinct `rx(θ)/rx(-θ)`, orphan+retry
  - **mega terminals**: ~15k rx/ry flood (insertvalue churn) plus a 14-qubit product
    state (~16k live amps past `INITIAL_TERMINAL_SIZE`), then GC + fresh `U U†`
- `make test-stress` - brutal GC / terminal / gate churn on both backends
  - includes **terminal table realloc** past `INITIAL_TERMINAL_SIZE` (10000)
  - `make test-stress-f128` / `make test-stress-gmp` individually (`test-stress-f64` still exists)
  - `make test-stress STRESS_LEVEL=3` for maximum intensity (default 2)
- `make test-leaks` - valgrind `--leak-check=full` on unit, short stress, and
  symbolic `LP-Grover/05` (needs valgrind)
- `make test-mutation` - targeted mutants of known past bugs; each must be **killed** by tests
- `make test-unit-leaf-types` - replays `test_unit_api` for every float leaf type
  (`LEAF_FLOAT_TYPE=0,1,2,3`). Representation-dependent bugs hide from a
  single-type run: the x87 80-bit `long double` carries its value in 10 of its 16
  bytes, and hashing the 6 indeterminate padding bytes broke terminal dedup on
  f80 only (issue #6). The existing dedup assertions caught it as soon as they
  were built for that type.
  Comparison tolerances are floored per leaf type via `UNIT_EPS` in
  `test_unit_api.c`: the literals at those sites run from 1e-6 to 1e-12, which
  single precision cannot resolve at all (`FLT_EPSILON` is 1.19e-7), so f32 is
  held to 1e-4 while wider types keep the original values (issue #7).
- `make test-grover` - Grover amplification matrix (classic unroll, `--symbolic`, `NL_*`)
  on f32/f64/f80/f128 and GMP; also `make test-grover-f128` / `test-grover-gmp`
- `make test-sylvan` - optional Sylvan backend (not the default product):
  replays `test_circuits` + `test_benchmarks` on `MEDUSA_sylvan_doubles_f128`,
  then harder Grover (05–07, NL_06, `--symbolic` 05), MCToffoli 12/16,
  MOGrover 04, Barenco tof 3/4, period-finding 07, Buddy vs Sylvan
  `--probability` spot-checks, `test_rotation_cache.sh` on Sylvan, cross-backend
  amplitude checks for `rx`/`ry`/`rz` round-angle repros, and Sylvan GMP Grover/05

MoToBuddy is the preferred backend. `make test` never requires Sylvan.

### C++ gate implementations (`USE_CXX=1`)

`gates.c` and `gates_symb.c` contain `#ifndef __cplusplus` splits: `gate_x`,
`gate_cnot`, `gate_toffoli`, `gate_mcx` and the symbolic `cnot`/`toffoli`/`mcx`
each have a second implementation built on MoToBuddy's C++ traversal
combinators (`mtbdd_with_traverse_to`, `mtbdd_make_swap`) instead of the
apply-algebra composition used by the C path. `USE_CXX=1` compiles those two
files with `g++ -x c++ -std=c++17`, links with `g++`, and additionally defines
`-DUSE_MOSF` and builds `sim_mosf.cpp`.

```
make test USE_CXX=1
```

Because the suite asserts absolute correctness rather than comparing the two
builds, passing it under both `USE_CXX=0` and `USE_CXX=1` is what establishes
that the two gate implementations agree. CI runs both.

Coverage is collected for both paths, but not in one report: the two builds
instrument *different line sets* of the same sources (in the C build the C++
branches are preprocessed away, and vice versa), and they share
`obj/buddy_doubles_f128`, so the profiles would clobber each other. `make
coverage-cxx` therefore starts from a clean tree and writes `coverage-cxx.xml`.
CI uploads the two runs under the Codecov flags `c` and `cxx`, which unions
them per line: a line covered by either build counts as covered, and a line
absent from one report is unmeasured there rather than a miss.

MOSF itself is not exercised: `sim_mosf_file` parses MOSF JSON rather than
OpenQASM, and there is no such fixture in the repository.

### freePimpl / leaks

Classic terminals register `freePimpl` in `initPackage` (`interface_motobuddy.c`).
MoToBuddy invokes it on:

1. equal-result apply (unused op result)
2. maketerminal CUSTOM dedup (upstream MoToBuddy)
3. `mtbdd_delete_terminal` during GC / `bdd_done`

`freePimpl` frees only `LEAF_TYPE.pImpl`; MoToBuddy `free()`s the outer wrapper.

Symbolic terminals register `terminal_symb_val_free` / `terminal_symb_map_free`
(shell only - `symexp` lists live in the shared htab). Symb ops shallow-clone
shells instead of aliasing operands. Teardown: `symexp_htab_delete`, `vmap`
mapping list, `rdata->ref`, and `free_sim_info`.

Terminal-table teardown (`bdd_done` union-aware free + `mtbdd_IndexStackFree`) and
CUSTOM dedup free are in upstream MoToBuddy (`main`).
