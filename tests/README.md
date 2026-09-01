# Tests

```
make init-motobuddy   # once (clones/builds VeriFIT/MoToBuddy) — preferred backend
make init-sylvan      # optional (Sylvan v1.8.1 + Lace; C path only)
make test             # MoToBuddy unit API + small circuit smokes + metamorphic (doubles f128)
make test-sylvan      # same circuit/benchmark smokes on Sylvan + harder Grover/CCX + GMP
make test-all         # test + test-sylvan
make test-stress      # extreme GC/terminal stress for doubles f128 AND gmp
make test-leaks       # valgrind definite+reachable (unit, stress LEVEL=1, symbolic Grover/05)
make test-grover      # LP-Grover n=5,6,7 × {loop, loop-symbolic, NL} × {f32,f64,f80,f128,gmp}
```

Each C/bash suite ends with a **colorful pass/fail summary table** (ANSI when stdout is a TTY;
plain text when piped). Shared helpers: `tests/test_harness.h`, `tests/test_summary.sh`.

- `make test-unit` — protect/unprotect, leaf ownership, apply free-of-unused, gates,
  plus counter checks that MoToBuddy actually calls `freePimpl`
- `make test-circuits` — runs `MEDUSA_buddy_doubles_f128` on small QASM files
  (`MEDUSA_BIN=...` overrides the binary; used by `test-sylvan`)
- `make test-benchmarks` — structural checks (`test_benchmarks.sh`: digraph + unit
  norm) **plus** semantic checks (`test_benchmark_semantics`):
  - **BV**: final MTBDD is the secret basis state (`|11⟩`, `|101011⟩`, …)
  - **MOGrover**: max basis prob ≫ uniform
  - **Reversible** (MCToffoli / Feynman / RevLib): returns `|0…0⟩`
  - **PF / QC**: unit norm + non-trivial / non-spurious support
- `make test-metamorphic` — Stage 4 metamorphic checks via **OpenQASM** (`test_metamorphic`):
  - fixtures in `tests/qasm/metamorphic/`; random circuits written under `/tmp` then `sim_file`
  - random `U`: `U U† |0…0⟩ = |0…0⟩`
  - `(UV)† = V† U†` as `U;V;V†;U†` round-trip to `|0…0⟩`
  - identities `H²`, Paulis², `CX²`, `S⁴`, `T T†`, `Rx(π/2)⁴`
  - **CZ both OpenQASM orders** (`cz c,t` with `c<t` and `c>t`) — relies on `sim.c` swap
  - reverse-without-adj ≠ `(TH)†` on `|1⟩`
  - **heavy GC**: after each success, repeated `forceGC` while the result root stays
    protected; plus deep `U U†`, ~12k distinct `rx(θ)/rx(-θ)`, orphan+retry
  - **mega terminals**: ~15k rx/ry flood (insertvalue churn) plus a 14-qubit product
    state (~16k live amps past `INITIAL_TERMINAL_SIZE`), then GC + fresh `U U†`
- `make test-stress` — brutal GC / terminal / gate churn on both backends
  - includes **terminal table realloc** past `INITIAL_TERMINAL_SIZE` (10000)
  - `make test-stress-f128` / `make test-stress-gmp` individually (`test-stress-f64` still exists)
  - `make test-stress STRESS_LEVEL=3` for maximum intensity (default 2)
- `make test-leaks` — valgrind `--leak-check=full` on unit, short stress, and
  symbolic `LP-Grover/05` (needs valgrind)
- `make test-mutation` — targeted mutants of known past bugs; each must be **killed** by tests
- `make test-grover` — Grover amplification matrix (classic unroll, `--symbolic`, `NL_*`)
  on f32/f64/f80/f128 and GMP; also `make test-grover-f128` / `test-grover-gmp`
- `make test-sylvan` — optional Sylvan backend (not the default product):
  replays `test_circuits` + `test_benchmarks` on `MEDUSA_sylvan_doubles_f128`,
  then harder Grover (05–07, NL_06, `--symbolic` 05), MCToffoli 12/16,
  MOGrover 04, Barenco tof 3/4, period-finding 07, Buddy vs Sylvan
  `--probability` spot-checks, and Sylvan GMP Grover/05

MoToBuddy is the preferred backend. `make test` never requires Sylvan.

### freePimpl / leaks

Classic terminals register `freePimpl` in `initPackage` (`interface_motobuddy.c`).
MoToBuddy invokes it on:

1. equal-result apply (unused op result)
2. maketerminal CUSTOM dedup (upstream MoToBuddy)
3. `mtbdd_delete_terminal` during GC / `bdd_done`

`freePimpl` frees only `LEAF_TYPE.pImpl`; MoToBuddy `free()`s the outer wrapper.

Symbolic terminals register `terminal_symb_val_free` / `terminal_symb_map_free`
(shell only — `symexp` lists live in the shared htab). Symb ops shallow-clone
shells instead of aliasing operands. Teardown: `symexp_htab_delete`, `vmap`
mapping list, `rdata->ref`, and `free_sim_info`.

Terminal-table teardown (`bdd_done` union-aware free + `mtbdd_IndexStackFree`) and
CUSTOM dedup free are in upstream MoToBuddy (`main`).

If classic freefun causes crashes:

```
patch -p1 < patches/revert-classic-freepimpl.patch
```
