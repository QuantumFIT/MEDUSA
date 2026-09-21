# Tests

```
make init-motobuddy   # once (clones/builds VeriFIT/MoToBuddy) - preferred backend
make init-sylvan      # optional (Sylvan v1.8.1 + Lace; C path only)
make test             # MoToBuddy unit API + small circuit smokes + metamorphic (doubles f128)
make test-sylvan      # same circuit/benchmark smokes on Sylvan + harder Grover/CCX + GMP
make test-all         # test + test-sylvan
make test-sylvan-all  # test-sylvan + the slow metamorphic sweep (nightly in CI)
make test-stress      # extreme GC/terminal stress for doubles f128 AND gmp
make test-leaks       # valgrind definite+reachable (unit, stress, Grover/05, semantics, metamorphic)
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
    Two legs per pair, both over **every basis amplitude** - a norm check would
    miss a dropped phase. This is what covers `src/gates_symb.c`, which
    `LP-Grover/05` alone leaves at ~16%.
    - **leg A**, the primary one: the loop file `--symbolic` against the *same
      file* classic, within `eps`. That is the property symbolic mode claims,
      and a failure names one file and means one thing.
    - **leg B**: the hand-unrolled partner classic against the loop file
      classic, asserted **exactly**. See below for why a second file is needed
      at all.
  - **heavy GC**: after each success, repeated `forceGC` while the result root stays
    protected; plus deep `U U†`, ~12k distinct `rx(θ)/rx(-θ)`, orphan+retry
  - **mega terminals**: ~15k rx/ry flood (insertvalue churn) plus a 14-qubit product
    state (~16k live amps past `INITIAL_TERMINAL_SIZE`), then GC + fresh `U U†`
- `make test-stress` - brutal GC / terminal / gate churn on both backends
  - includes **terminal table realloc** past `INITIAL_TERMINAL_SIZE` (10000)
  - `make test-stress-f128` / `make test-stress-gmp` individually (`test-stress-f64` still exists)
  - `make test-stress STRESS_LEVEL=3` for maximum intensity (default 2)
- `make test-leaks` - valgrind `--leak-check=full` (reachable blocks count as
  errors, not just definitely-lost) over five runs: unit, short stress, and
  symbolic `LP-Grover/05`, benchmark semantics and metamorphic (needs
  valgrind). The last two are what make it worth running: the first three
  reach 54% of the lines in this build and miss most of the allocation-dense
  code (`leaf_reim_double.c`, the file with the most malloc/free in the tree,
  sat at 52%; `qparam.c` at 0%), while adding them takes it to 73% and was
  what caught the missing `free_sim_info` in the test helpers. ~45s, of which
  metamorphic under valgrind is ~38s.
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
  `test_benchmark_semantics.c` has the same floor under the name `SEM_EPS`. It
  needed one for the same reason and had gone unnoticed only because nothing
  ever built that suite at f32: five of its norm checks sit at 1e-6/1e-7 while
  the measured error over a whole benchmark circuit reaches 4e-5. Both backends
  produce identical values there, so it is precision, not divergence.
- `make test-unit-gmp` - unit tests for the GMP leaf primitive
  (`tests/test_unit_leaf_gmp.c`). `test_unit_api` is hard-wired to
  `LEAF_BACKEND_DOUBLES` and reads `leaf.pImpl->re`/`->im` throughout, so
  `leaf_primitive_mpz.c` had no unit coverage at all - 48.9% line, seven
  functions never executed. That is the blind spot the componentwise
  `mulLeaf`/`divLeaf` bug lived in (issue #12). Covers the mpz wrappers, the
  hash/equality contract the terminal table depends on (issue #6's failure
  mode, asserted directly), multi-limb arithmetic, and the fact that
  `inv_sqrt2_pow_generic` stores the exponent rather than computing a power.
  The rotation entry points are `abort()` stubs on this backend and are
  deliberately not exercised
- `make test-cli` - argument handling, parser diagnostics and the long-number
  output file (`tests/test_cli.sh`). Before it, `main.c` sat at 55.7% branch
  coverage and `sim.c` at 66.4%, because every other suite feeds the simulator
  a well-formed circuit and correct arguments. Also the only cover for
  `res-vars.txt`: algebraic coefficients longer than `MAX_NUM_LEN` (50 digits)
  are written there as `large-number[N]` references, and the test asserts every
  reference in `res.dot` resolves to a definition that really is longer than
  the inline limit. `LP-QuantumCounting/08_04_05_0.qasm` on the GMP binary is
  the circuit that triggers it - 358 such numbers.

  Where the simulator is lenient the suite asserts the **current** behaviour
  and marks it, rather than asserting what it arguably should do; see issue
  #13 for the out-of-range qubit index, which is a real out-of-bounds read,
  and the two lenience cases found alongside it
- `make test-mosf` - MOSF, the JSON input path (`--tree-simulation`,
  `sim_mosf_file`, `src/sim_mosf.cpp`). Compiled only under `USE_CXX=1` and,
  before this suite, executed by nothing: **0 of 104 lines**, the largest
  wholly-uncovered file in the tree, and invisible in the default coverage
  report because the C build does not compile the translation unit at all.
  The target does not force `USE_CXX=1` - that build shares
  `obj/buddy_doubles_f128` with the C one - so `make test` skips it and
  `make test USE_CXX=1` runs it. Fixtures and the format notes are in
  `tests/mosf/`
- `make test-grover` - Grover amplification matrix (classic unroll, `--symbolic`, `NL_*`)
  on f32/f64/f80/f128 and GMP; also `make test-grover-f128` / `test-grover-gmp`
- `make test-lp-symbolic` - `--symbolic` sweep over every `LP-Grover`, `LP-PeriodFinding`
  and `LP-QuantumCounting` circuit (`test_lp_symbolic.sh`), each under a wall-clock
  cap (`MEDUSA_TEST_TIMEOUT`, 10 s) and an address-space cap (`MEDUSA_TEST_MEM_KB`,
  2 GB). Every circuit that finishes symbolically is also run concretely under the
  same caps; `lp_dot_equiv.py` then walks the two `res.dot` diagrams in lockstep
  and requires the same amplitude for every basis state (within `MEDUSA_EQUIV_TOL`,
  1e-9) and unit norm - a real check on `symb_utils.c`/`gates_symb.c`, since the
  two paths share no gate code. When only the symbolic run finishes, its norm alone
  is checked. `tests/lp_symbolic_expected.txt` lists the circuits that must finish;
  a crash, an empty digraph, a disagreement, or a timeout on one of those fails the
  suite, a circuit that newly finishes is reported as `NEW` so it can be added. Nightly.
- `make test-sylvan` - optional Sylvan backend (not the default product):
  replays `test_circuits` + `test_benchmarks` on `MEDUSA_sylvan_doubles_f128`,
  then harder Grover (05–07, NL_06, `--symbolic` 05), MCToffoli 12/16,
  MOGrover 04, Barenco tof 3/4, period-finding 07, Buddy vs Sylvan
  `--probability` spot-checks, `test_rotation_cache.sh` on Sylvan, cross-backend
  amplitude checks for `rx`/`ry`/`rz` round-angle repros, Sylvan GMP Grover/05
  classic and `--symbolic`, Sylvan vs MoToBuddy algebraic GMP terminal-label
  comparisons (see below), then `test-sylvan-leaf-types`
- `make test-sylvan-leaf-types` - `test_benchmark_semantics` relinked against
  Sylvan, across every float leaf type (under a second each)
- `make test-grover-sylvan` - the Grover matrix on Sylvan across float leaf
  types and algebraic GMP. This is Sylvan's counterpart to `make test-grover`:
  before it, the Sylvan binary was only ever exercised at f128 and GMP, so
  f32/f64/f80 had no Grover coverage on that backend. Its GMP case is what
  uncovered issue #11 - symbolic simulation on Sylvan + algebraic GMP
  segfaulted, a combination nothing had ever run
- `make test-sylvan-metamorphic` - `test_metamorphic` relinked against Sylvan,
  across `SYLVAN_META_LEAF_TYPES`. ~170s per type, so it does not gate everyday
  PRs into devel: `.github/workflows/nightly.yml` runs it (and `test-lp-symbolic`)
  nightly, on demand, and on any PR into `main`, so it still gates the
  devel -> main merge.
  `make test-sylvan-all` is `test-sylvan` plus this

MoToBuddy is the preferred backend. `make test` never requires Sylvan.

#### Why the `_unrolled` fixtures exist

A reasonable question is why the symbolic comparison needs a second file at
all, rather than just running the one loop file both ways. The answer is
narrow but real: the iteration count is parsed **once**, in `get_iters`
(`sim.c:299`), and handed to both modes - classic unrolls it with `iters--`
and a stream rewind, symbolic passes the same value to `symb_eval`. A misparse
therefore shifts both modes *identically*, and any same-file comparison still
agrees with itself.

Mutation-checked in both directions, counting failing assertions inside
`assert_symb_matches_classic`:

| mutation | leg A (same file, both modes) | leg B (loop vs unrolled) |
|---|---|---|
| `get_iters` off by one - shared by both modes | **0** | 558 |
| classic runtime unroller off by one - classic only | 570 | 570 |

So leg B earns its fixtures on exactly one class of fault: the shared loop
*parse*. Faults in the unrolling itself are caught by leg A alone, and so are
faults in `gates_symb.c` (a symbolic `S` rotating like `T` fails leg A twice
and leg B not at all).

Leg B also stops a subtler failure: without it a hand-edited `_unrolled` file
can drift from its loop partner, and the tempting response to the resulting
red test - editing the unrolled file until it passes - would mask a real
`gates_symb.c` bug.

The same shape at benchmark scale is in `test_grover_matrix`, where
`NL_NN.qasm` is the hand-written no-loop counterpart of `NN.qasm`. Its
loop-vs-NL comparison is exact on every leaf type including f32; the
symbolic-vs-classic one carries a per-leaf-type tolerance taken from
measurement (worst observed: f32 4.4e-05, f64 6.2e-08, f80 1.2e-11, f128 and
GMP exactly 0). That comparison is over basis *probabilities* rather than
amplitudes, since `qBDD_calculateProb` is the only value accessor that works
on the algebraic GMP leaf too - so a global phase change would pass there, and
is caught by the metamorphic suite instead.

#### Symbolic simulation on Sylvan + algebraic GMP

The C suites cannot reach the algebraic leaf: `test_metamorphic` and
`test_benchmark_semantics` read `leaf.pImpl->re` / `->im` and call the double
primitive's `to_double_generic`, so they are tied to the re/im leaf and would
have to be ported, not relinked. `test_grover_matrix` goes through the
backend-agnostic API and does run on GMP - but Grover is H/X/CCX/Z, whose
amplitudes stay entirely in the `a` component of `a + b.w + c.w2 + d.w3`, so it
cannot see a fault in the other three.

So the cover for that combination is in `test_sylvan.sh`, which compares
`res.dot` terminal labels between `MEDUSA_sylvan_gmp` and `MEDUSA_buddy_gmp`.
Algebraic labels are exact and print every component, and
`tests/qasm/metamorphic/mixed_th_loop.qasm` fills all four across its 256
terminals, so a leaf payload that loses `c` or `d` shows up as a differing
label rather than as a wrong number in the last digits. That is the regression
guard for issue #11, whose silent half - dropped `w2`/`w3` components - no
existing suite could have detected; only its crash was visible.

#### Which C suites run on Sylvan, and at which leaf types

The shell suites reach Sylvan for free: they exec whatever `MEDUSA_BIN` names.
The C suites cannot - they fix the backend at compile time through `-include`,
so running them on Sylvan means linking the same sources a second time against
`interface_sylvan.h`.

`test_unit_api` is deliberately *not* ported. It includes `mtbdd.h`, `kernel.h`
and `terminal.h` and asserts on the MoToBuddy node and terminal tables
themselves - dedup, `bddnodes[].refcou` under protect, terminal-table realloc.
Sylvan's node table and GC model leave nothing equivalent to assert against.

`test_metamorphic` runs all ten of its sections on Sylvan. Two assertions in
"mega distinct terminals" - those reading `mtbddmaxTerminalSize` against
`INITIAL_TERMINAL_SIZE` - are `#ifndef SYLVAN_BACKEND`, since `customPointers`
and its realloc threshold are MoToBuddy-specific. Hence 2317 assertions on
Sylvan against 2319 on MoToBuddy.

Leaf types swept (`SYLVAN_SEM_LEAF_TYPES`, `SYLVAN_META_LEAF_TYPES`):

| suite | f32 | f64 | f80 | f128 | GMP | cost per type | runs |
|---|---|---|---|---|---|---|---|
| `test_benchmark_semantics` | yes | yes | yes | yes | no | <1s | every PR |
| `test_grover_matrix` | yes | yes | yes | yes | yes | ~1s | every PR |
| `test_metamorphic` | no | no | yes | yes | no | ~170s | nightly |

The shell suites (`test_circuits.sh`, `test_rotation_cache.sh`,
`test_benchmarks.sh`, `test_sylvan.sh`) still drive the Sylvan binary at f128
only, plus `sylvan_gmp`. `test_sylvan.sh` in particular compares against
`MEDUSA_buddy_doubles_f128` and `MEDUSA_buddy_gmp`, so looping it over leaf
types would mean building the matching MoToBuddy binary for each as well.

f32 is out of the metamorphic sweep because it cannot hold the tolerances: the
deep random circuits and the 12000-angle rx flood put the worst basis amplitude
5e-3 from where it belongs (median 1e-6), and a floor that loose would stop the
assertions meaning anything. f64 is left out as redundant with f128. f80 is kept
because it is the representation that produced issue #6.

Expect Sylvan to be slow: ~170s against ~1.4s for the same suite on MoToBuddy.
That is package setup, not simulation. Sylvan's `initPackage` runs
`lace_start()` plus `sylvan_set_limits(2 GB, ...)` and `sylvan_init_package()`,
and the metamorphic suite calls `setup_pkg()` once per trial (40 and 24 trials
in the two random-circuit sections), so each trial rebuilds a 2 GB table.

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

MOSF is exercised by `make test-mosf` (see above). The fixtures had to be
written from the spec in `lib/MoToBuddy/doc/mosf.mosf`: the `.mosf` files
shipped under `lib/MoToBuddy/examples/` are gate *definitions*, not circuits -
they carry no `x_levels` and use `plus_s`/`minus_s`, which MEDUSA's extension
registry does not define.

Each `tests/mosf/NAME.mosf` is paired with a `NAME.qasm` for the same circuit,
and the two must agree on every basis amplitude. Comparing the `res.dot` files
byte for byte does **not** work: node ids are allocation order, and the two
front ends number the terminals differently while building the same state.
`tests/dot_amps_equal.py` does the comparison properly.

Fixture choice matters more than it looks. H applied to |0> leaves the high
child as the zero BDD, so `(low + high)` and `(low - high)` coincide and the
comparison cannot tell `plus_mulsqrt2` from `minus_mulsqrt2` - a build with
the two swapped passes `h1`, `hh` and `hxz`. The `xh` and `hxh` fixtures apply
X first so that H acts on |1>, which is what makes the arithmetic observable.
Mutation-checked: swapping either binary op fails exactly those two, and
making `neg` a no-op fails `hxz`.

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
