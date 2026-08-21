# Tests

```
make init-motobuddy   # once (applies patches/motobuddy-*.patch)
make test             # unit API + small circuit smokes (doubles f64)
make test-stress      # extreme GC/terminal stress for doubles f64 AND gmp
make test-leaks       # valgrind definite-leak check (unit + stress LEVEL=1)
make test-grover      # LP-Grover n=5,6,7 × {loop, loop-symbolic, NL} × {f32,f64,f80,f128,gmp}
```

Each C/bash suite ends with a **colorful pass/fail summary table** (ANSI when stdout is a TTY;
plain text when piped). Shared helpers: `tests/test_harness.h`, `tests/test_summary.sh`.

- `make test-unit` — protect/unprotect, leaf ownership, apply free-of-unused, gates,
  plus counter checks that MoToBuddy actually calls `freePimpl`
- `make test-circuits` — runs `MEDUSA_buddy_doubles_f64` on small QASM files
- `make test-benchmarks` — structural checks (`test_benchmarks.sh`: digraph + unit
  norm) **plus** semantic checks (`test_benchmark_semantics`):
  - **BV**: final MTBDD is the secret basis state (`|11⟩`, `|101011⟩`, …)
  - **MOGrover**: max basis prob ≫ uniform
  - **Reversible** (MCToffoli / Feynman / RevLib): returns `|0…0⟩`
  - **PF / QC**: unit norm + non-trivial / non-spurious support
- `make test-stress` — brutal GC / terminal / gate churn on both backends
  - includes **terminal table realloc** past `INITIAL_TERMINAL_SIZE` (10000)
  - `make test-stress-f64` / `make test-stress-gmp` individually
  - `make test-stress STRESS_LEVEL=3` for maximum intensity (default 2)
- `make test-leaks` — valgrind `--leak-check=full` on unit + short stress (needs valgrind)
- `make test-grover` — Grover amplification matrix (classic unroll, `--symbolic`, `NL_*`)
  on f32/f64/f80/f128 and GMP; also `make test-grover-f64` / `test-grover-gmp`

### freePimpl / leaks

Classic terminals register `freePimpl` in `initPackage` (`interface_motobuddy.c`).
MoToBuddy invokes it on:

1. equal-result apply (unused op result)
2. maketerminal CUSTOM dedup (patch `motobuddy-maketerminal-free-unused.patch`)
3. `mtbdd_delete_terminal` during GC / `bdd_done`

`freePimpl` frees only `LEAF_TYPE.pImpl`; MoToBuddy `free()`s the outer wrapper.

Remaining known non-classic issues (not covered by these tests):

- Symbolic terminal types still use `freefun = NULL`
- Symbolic leaf ops that alias operand pointers

Classic `bdd_done` used to leak the terminal index freelist (`mtbdd_IndexStackFree`)
and the CUSTOM `customPointers` table; fixed by
`patches/motobuddy-bdd-done-terminal-teardown.patch` (union-aware free).

If classic freefun causes crashes:

```
patch -p1 < patches/revert-classic-freepimpl.patch
```
