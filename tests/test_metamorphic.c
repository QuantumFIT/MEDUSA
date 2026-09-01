/**
 * @file test_metamorphic.c
 * Stage 4 — metamorphic testing via OpenQASM + sim_file (doubles f128).
 *
 * All circuits are loaded as OpenQASM 2.0 files (never direct gate_* API),
 * so CZ goes through sim.c's qc/qt swap (supports either argument order).
 *
 * Properties (validity):
 *   1. U U† |0…0⟩ = |0…0⟩  (random + fixed)
 *   2. (UV)† = V† U†
 *   3. Gate identities (H²=I, …)
 *   4. CZ both OpenQASM argument orders
 *
 * Heavy GC: after each success, hammer forceGC while the result root stays
 * protected; plus a dedicated deep/rx-flood section that induces GC during sim
 * and again afterward.
 */
#include "test_harness.h"
#include "sim.h"
#include "interface.h"
#include "mtbdd.h"   /* INITIAL_TERMINAL_SIZE */
#include "kernel.h"  /* mtbddmaxTerminalSize, mtbddTerminalUsed */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

struct LEAF_TYPE_IMPL {
    leaf_primitive_t re;
    leaf_primitive_t im;
};

#define META_MAX_OPS    128
#define META_MAX_QUBITS 16  /* room for META_WIDE_QUBITS product-state stress */

#ifndef META_GC_ROUNDS
#define META_GC_ROUNDS 8
#endif

#ifndef META_HEAVY_TRIALS
#define META_HEAVY_TRIALS 12
#endif

/* Temporal flood: many distinct insertvalues; GC may recycle so table need not grow. */
#ifndef META_HEAVY_RX_ANGLES
#define META_HEAVY_RX_ANGLES 12000
#endif

#ifndef META_MEGA_FLOOD_PAIRS
#define META_MEGA_FLOOD_PAIRS 15000
#endif

/* Product-state width: 2^14 = 16384 live amps > INITIAL_TERMINAL_SIZE (10000). */
#ifndef META_WIDE_QUBITS
#define META_WIDE_QUBITS 14
#endif

/* -------------------------------------------------------------------------- */
/* Gate IR (QASM emission only)                                               */
/* -------------------------------------------------------------------------- */

typedef enum {
    G_H = 0, G_X, G_Y, G_Z, G_S, G_T, G_TDG,
    G_RX_PIHALF, G_RY_PIHALF,
    G_CX, G_CZ, G_CCX,
    G_KIND_COUNT
} gate_kind_t;

typedef struct {
    gate_kind_t kind;
    uint32_t a, b, c; /* meanings depend on kind; for CX/CZ: a=target, b=control */
} gate_op_t;

typedef struct {
    int n_qubits;
    int n_ops;
    gate_op_t ops[META_MAX_OPS];
} circuit_t;

static uint32_t g_rng;
static char g_tmpdir[256];

static void rng_seed(uint32_t s) { g_rng = s ? s : 1u; }
static uint32_t rng_u32(void) {
    uint32_t x = g_rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return g_rng = x;
}
static unsigned rng_below(unsigned n) { return n ? (rng_u32() % n) : 0; }

static void setup_pkg(void) {
    initPackage(0, 0, 0);
    test_silence_gbc();
}

/* -------------------------------------------------------------------------- */
/* QASM I/O                                                                   */
/* -------------------------------------------------------------------------- */

static void emit_gate_fwd(FILE *f, const gate_op_t *g) {
    switch (g->kind) {
    case G_H:         fprintf(f, "h qubits[%u];\n", g->a); break;
    case G_X:         fprintf(f, "x qubits[%u];\n", g->a); break;
    case G_Y:         fprintf(f, "y qubits[%u];\n", g->a); break;
    case G_Z:         fprintf(f, "z qubits[%u];\n", g->a); break;
    case G_S:         fprintf(f, "s qubits[%u];\n", g->a); break;
    case G_T:         fprintf(f, "t qubits[%u];\n", g->a); break;
    case G_TDG:       fprintf(f, "tdg qubits[%u];\n", g->a); break;
    case G_RX_PIHALF: fprintf(f, "rx(pi/2) qubits[%u];\n", g->a); break;
    case G_RY_PIHALF: fprintf(f, "ry(pi/2) qubits[%u];\n", g->a); break;
    case G_CX:        fprintf(f, "cx qubits[%u], qubits[%u];\n", g->b, g->a); break;
    case G_CZ:        fprintf(f, "cz qubits[%u], qubits[%u];\n", g->b, g->a); break;
    case G_CCX:       fprintf(f, "ccx qubits[%u], qubits[%u], qubits[%u];\n",
                              g->b, g->c, g->a); break;
    default: break;
    }
}

/** Emit U† for one gate (OpenQASM). S† = S³ (no sdg in sim). */
static void emit_gate_adj(FILE *f, const gate_op_t *g) {
    switch (g->kind) {
    case G_H: case G_X: case G_Y: case G_Z:
    case G_CX: case G_CZ: case G_CCX:
        emit_gate_fwd(f, g);
        break;
    case G_S:
        fprintf(f, "s qubits[%u];\n", g->a);
        fprintf(f, "s qubits[%u];\n", g->a);
        fprintf(f, "s qubits[%u];\n", g->a);
        break;
    case G_T:
        fprintf(f, "tdg qubits[%u];\n", g->a);
        break;
    case G_TDG:
        fprintf(f, "t qubits[%u];\n", g->a);
        break;
    case G_RX_PIHALF:
        /* Rx(π/2)³ = Rx(-π/2); keep scaled rx(pi/2) algebra */
        fprintf(f, "rx(pi/2) qubits[%u];\n", g->a);
        fprintf(f, "rx(pi/2) qubits[%u];\n", g->a);
        fprintf(f, "rx(pi/2) qubits[%u];\n", g->a);
        break;
    case G_RY_PIHALF:
        fprintf(f, "ry(pi/2) qubits[%u];\n", g->a);
        fprintf(f, "ry(pi/2) qubits[%u];\n", g->a);
        fprintf(f, "ry(pi/2) qubits[%u];\n", g->a);
        break;
    default: break;
    }
}

static void write_qasm_header(FILE *f, int n) {
    fprintf(f, "OPENQASM 2.0;\n");
    fprintf(f, "include \"qelib1.inc\";\n");
    fprintf(f, "qreg qubits[%d];\n\n", n);
}

/** Write circuit then its adjoint (for U U† tests). */
static int write_circuit_uu_dagger(const char *path, const circuit_t *c) {
    FILE *f = fopen(path, "w");
    if (!f) return 0;
    write_qasm_header(f, c->n_qubits);
    for (int i = 0; i < c->n_ops; i++)
        emit_gate_fwd(f, &c->ops[i]);
    fprintf(f, "\n// U†\n");
    for (int i = c->n_ops - 1; i >= 0; i--)
        emit_gate_adj(f, &c->ops[i]);
    fclose(f);
    return 1;
}

/** Concat U, V, then V†, U†. */
static int write_uv_then_vdag_udag(const char *path,
                                  const circuit_t *u, const circuit_t *v) {
    FILE *f = fopen(path, "w");
    if (!f) return 0;
    int n = u->n_qubits > v->n_qubits ? u->n_qubits : v->n_qubits;
    write_qasm_header(f, n);
    for (int i = 0; i < u->n_ops; i++) emit_gate_fwd(f, &u->ops[i]);
    for (int i = 0; i < v->n_ops; i++) emit_gate_fwd(f, &v->ops[i]);
    fprintf(f, "\n// V† U†\n");
    for (int i = v->n_ops - 1; i >= 0; i--) emit_gate_adj(f, &v->ops[i]);
    for (int i = u->n_ops - 1; i >= 0; i--) emit_gate_adj(f, &u->ops[i]);
    fclose(f);
    return 1;
}

static bool sim_path(const char *path, qBDD *out, int *n_qubits) {
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "cannot open %s: %s\n", path, strerror(errno));
        return false;
    }
    sim_flags_t flags = { .opt_symb = false, .opt_info = false };
    sim_info_t info;
    init_sim_info(&info);
    bool ok = sim_file(f, out, &flags, &info);
    fclose(f);
    if (!ok) return false;
    *n_qubits = info.n_qubits;
    return true;
}

static bool sim_path_symb(const char *path, qBDD *out, int *n_qubits) {
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "cannot open %s: %s\n", path, strerror(errno));
        return false;
    }
    sim_flags_t flags = { .opt_symb = true, .opt_info = false };
    sim_info_t info;
    init_sim_info(&info);
    bool ok = sim_file(f, out, &flags, &info);
    fclose(f);
    if (!ok) return false;
    *n_qubits = info.n_qubits;
    return true;
}

static prob_t basis_prob(qBDD t, const char *bits) {
    while (!qBDD_isTerminal(t) && !qBDD_isFalse(t)) {
        uint32_t v = (uint32_t)qBDD_getVar(t);
        t = (bits[v] == '1') ? qBDD_getHigh(t) : qBDD_getLow(t);
    }
    if (qBDD_isFalse(t)) return 0.0;
    return (prob_t)qBDD_calculateProb(t);
}

static int basis_amp(qBDD t, const char *bits, double *re, double *im) {
    *re = *im = 0.0;
    while (!qBDD_isTerminal(t) && !qBDD_isFalse(t)) {
        uint32_t v = (uint32_t)qBDD_getVar(t);
        t = (bits[v] == '1') ? qBDD_getHigh(t) : qBDD_getLow(t);
    }
    if (qBDD_isFalse(t)) return 0;
    LEAF_TYPE leaf = qBDD_getTerminalValue(t);
    if (!leaf.pImpl) return 0;
    *re = to_double_generic(leaf.pImpl->re);
    *im = to_double_generic(leaf.pImpl->im);
    return 1;
}

static void zeros_bits(char *bits, int n) {
    for (int i = 0; i < n; i++) bits[i] = '0';
    bits[n] = '\0';
}

static void assert_is_all_zero(qBDD circ, int n, double eps, const char *ctx) {
    char bits[META_MAX_QUBITS + 1];
    zeros_bits(bits, n);
    TEST_ASSERT_NEAR(qBDD_total_prob(circ, n), 1.0, eps);
    TEST_ASSERT_MSG(fabs((double)basis_prob(circ, bits) - 1.0) <= eps, ctx);
}

static int bdd_var_order_ok(qBDD t, int parent_var) {
    if (qBDD_isFalse(t) || qBDD_isTerminal(t)) return 1;
    int v = (int)qBDD_getVar(t);
    if (parent_var >= 0 && v <= parent_var) return 0;
    return bdd_var_order_ok(qBDD_getLow(t), v)
        && bdd_var_order_ok(qBDD_getHigh(t), v);
}

/** Validity check, then repeated GC while the sim root stays protected. */
static void hammer_gc(void) {
    for (int i = 0; i < META_GC_ROUNDS; i++)
        forceGC();
}

static void assert_zero_survives_gc(qBDD circ, int n, double eps, const char *ctx) {
    assert_is_all_zero(circ, n, eps, ctx);
    TEST_ASSERT_MSG(bdd_var_order_ok(circ, -1), ctx);
    hammer_gc();
    assert_is_all_zero(circ, n, eps, ctx);
    TEST_ASSERT_MSG(bdd_var_order_ok(circ, -1), ctx);
    hammer_gc();
    assert_is_all_zero(circ, n, eps, ctx);
}

static gate_op_t random_gate(int n) {
    gate_op_t g;
    memset(&g, 0, sizeof(g));
    static const gate_kind_t pool[] = {
        G_H, G_X, G_Y, G_Z, G_S, G_T, G_TDG,
        G_RX_PIHALF, G_RY_PIHALF,
        G_CX, G_CZ, G_CCX
    };
    g.kind = pool[rng_below((unsigned)(sizeof pool / sizeof pool[0]))];
    if (g.kind == G_CCX && n < 3) g.kind = G_CX;
    if ((g.kind == G_CX || g.kind == G_CZ) && n < 2) g.kind = G_H;

    g.a = (uint32_t)rng_below((unsigned)n);
    if (g.kind == G_CX || g.kind == G_CZ) {
        do { g.b = (uint32_t)rng_below((unsigned)n); } while (g.b == g.a);
    } else if (g.kind == G_CCX) {
        do { g.b = (uint32_t)rng_below((unsigned)n); } while (g.b == g.a);
        do { g.c = (uint32_t)rng_below((unsigned)n); } while (g.c == g.a || g.c == g.b);
    }
    return g;
}

static void random_circuit(circuit_t *c, int n_qubits, int n_ops) {
    c->n_qubits = n_qubits;
    c->n_ops = n_ops > META_MAX_OPS ? META_MAX_OPS : n_ops;
    for (int i = 0; i < c->n_ops; i++)
        c->ops[i] = random_gate(n_qubits);
}

static void path_tmp(char *out, size_t n, const char *name) {
    snprintf(out, n, "%s/%s", g_tmpdir, name);
}

/* -------------------------------------------------------------------------- */
/* Tests                                                                      */
/* -------------------------------------------------------------------------- */

static void test_gate_identities_qasm(void) {
    TEST_SECTION("metamorphic: gate identities from OpenQASM");

    static const char *files[] = {
        "tests/qasm/metamorphic/identity_h2.qasm",
        "tests/qasm/metamorphic/identity_pauli2.qasm",
        "tests/qasm/metamorphic/identity_cx2.qasm",
        "tests/qasm/metamorphic/identity_s4.qasm",
        "tests/qasm/metamorphic/identity_t_tdg.qasm",
        "tests/qasm/metamorphic/identity_rx_pihalf.qasm",
    };
    for (size_t i = 0; i < sizeof files / sizeof files[0]; i++) {
        setup_pkg();
        qBDD circ;
        int n = 0;
        TEST_ASSERT_MSG(sim_path(files[i], &circ, &n), files[i]);
        assert_zero_survives_gc(circ, n, 1e-8, files[i]);
        deleteCircuit(&circ);
        freePackage();
    }
}

static void test_cz_both_qasm_orders(void) {
    TEST_SECTION("metamorphic: CZ both OpenQASM argument orders (sim swap)");

    /* OpenQASM: cz control, target — both index orderings must work via sim.c swap */
    static const char *files[] = {
        "tests/qasm/metamorphic/cz_c0_t1.qasm", /* qc < qt */
        "tests/qasm/metamorphic/cz_c1_t0.qasm", /* qc > qt → swapped in sim */
        "tests/qasm/metamorphic/cz_c0_t2.qasm",
        "tests/qasm/metamorphic/cz_c2_t0.qasm",
        "tests/qasm/metamorphic/cz_c1_t2.qasm",
        "tests/qasm/metamorphic/cz_c2_t1.qasm",
    };
    for (size_t i = 0; i < sizeof files / sizeof files[0]; i++) {
        setup_pkg();
        qBDD circ;
        int n = 0;
        TEST_ASSERT_MSG(sim_path(files[i], &circ, &n), files[i]);
        assert_zero_survives_gc(circ, n, 1e-6, files[i]);
        deleteCircuit(&circ);
        freePackage();
    }
}

static void test_uu_dagger_random_qasm(void) {
    TEST_SECTION("metamorphic: U U† |0> = |0> (random OpenQASM)");

    rng_seed(0xC0FFEEu);
    const int trials = 40;
    for (int t = 0; t < trials; t++) {
        int n = 2 + (int)rng_below(3);
        int depth = 4 + (int)rng_below(12);
        circuit_t U;
        random_circuit(&U, n, depth);

        char path[512];
        path_tmp(path, sizeof path, "uu_dagger.qasm");
        TEST_ASSERT(write_circuit_uu_dagger(path, &U));

        setup_pkg();
        qBDD circ;
        int nq = 0;
        TEST_ASSERT(sim_path(path, &circ, &nq));
        TEST_ASSERT(nq == n);
        assert_zero_survives_gc(circ, n, 1e-6, "U U† |0> != |0>");
        deleteCircuit(&circ);
        freePackage();
    }
}

static void test_dagger_antihomomorphism_qasm(void) {
    TEST_SECTION("metamorphic: (UV)† = V† U† (OpenQASM round-trip)");

    rng_seed(0xA11CE5u);
    for (int t = 0; t < 24; t++) {
        int n = 2 + (int)rng_below(2);
        circuit_t U, V;
        random_circuit(&U, n, 3 + (int)rng_below(5));
        random_circuit(&V, n, 3 + (int)rng_below(5));

        char path_rt[512];
        path_tmp(path_rt, sizeof path_rt, "uv_roundtrip.qasm");
        TEST_ASSERT(write_uv_then_vdag_udag(path_rt, &U, &V));

        setup_pkg();
        qBDD circ;
        int nq = 0;
        TEST_ASSERT(sim_path(path_rt, &circ, &nq));
        assert_zero_survives_gc(circ, nq, 1e-6, "V† U† U V |0> != |0>");
        deleteCircuit(&circ);
        freePackage();
    }
}

static void test_reverse_without_adj_qasm(void) {
    TEST_SECTION("metamorphic: reverse-without-adj ≠ (TH)† (OpenQASM)");

    setup_pkg();
    qBDD correct, wrong;
    int n0 = 0, n1 = 0;
    TEST_ASSERT(sim_path("tests/qasm/metamorphic/adj_th_on_one.qasm", &correct, &n0));
    {
        const int N = 1 << n0;
        double *Cre = calloc((size_t)N, sizeof(double));
        double *Cim = calloc((size_t)N, sizeof(double));
        char bits[8];
        bits[n0] = '\0';
        for (int s = 0; s < N; s++) {
            for (int i = 0; i < n0; i++)
                bits[i] = ((s >> i) & 1) ? '1' : '0';
            basis_amp(correct, bits, &Cre[s], &Cim[s]);
        }
        deleteCircuit(&correct);
        freePackage();

        setup_pkg();
        TEST_ASSERT(sim_path("tests/qasm/metamorphic/rev_th_on_one.qasm", &wrong, &n1));
        TEST_ASSERT(n1 == n0);
        double worst = 0.0;
        for (int s = 0; s < N; s++) {
            for (int i = 0; i < n0; i++)
                bits[i] = ((s >> i) & 1) ? '1' : '0';
            double wr, wi;
            basis_amp(wrong, bits, &wr, &wi);
            double d = fabs(Cre[s] - wr);
            if (fabs(Cim[s] - wi) > d) d = fabs(Cim[s] - wi);
            if (d > worst) worst = d;
        }
        TEST_ASSERT_MSG(worst > 1e-4, "reverse-without-adj should differ from (TH)†");
        free(Cre);
        free(Cim);
        deleteCircuit(&wrong);
        freePackage();
    }

    setup_pkg();
    qBDD rt;
    int n = 0;
    TEST_ASSERT(sim_path("tests/qasm/metamorphic/th_roundtrip.qasm", &rt, &n));
    assert_zero_survives_gc(rt, n, 1e-8, "H Tdg T H |0> != |0>");
    deleteCircuit(&rt);
    freePackage();
}

static void test_symb_float_t_tdg(void) {
    TEST_SECTION("metamorphic: float-symbolic T/Tdg (loop)");

    setup_pkg();
    qBDD circ;
    int n = 0;
    TEST_ASSERT(sim_path_symb("tests/qasm/metamorphic/symb_t_tdg_loop.qasm", &circ, &n));
    assert_zero_survives_gc(circ, n, 1e-8, "symbolic H (T Tdg)^2 H |0> != |0>");
    deleteCircuit(&circ);
    freePackage();

    /* H T H mixes unscaled |0> with T-scaled |1>; symbolic must match classic. */
    setup_pkg();
    qBDD classic;
    int nc = 0;
    TEST_ASSERT(sim_path("tests/qasm/metamorphic/hth_unrolled.qasm", &classic, &nc));
    {
        char bits[8];
        bits[nc] = '\0';
        int N = 1 << nc;
        double *Cre = calloc((size_t)N, sizeof(double));
        double *Cim = calloc((size_t)N, sizeof(double));
        for (int s = 0; s < N; s++) {
            for (int i = 0; i < nc; i++)
                bits[i] = ((s >> i) & 1) ? '1' : '0';
            basis_amp(classic, bits, &Cre[s], &Cim[s]);
        }
        deleteCircuit(&classic);
        freePackage();

        setup_pkg();
        qBDD symb;
        int ns = 0;
        TEST_ASSERT(sim_path_symb("tests/qasm/metamorphic/symb_hth_loop.qasm", &symb, &ns));
        TEST_ASSERT(nc == ns);
        for (int s = 0; s < N; s++) {
            for (int i = 0; i < nc; i++)
                bits[i] = ((s >> i) & 1) ? '1' : '0';
            double sr, si;
            basis_amp(symb, bits, &sr, &si);
            TEST_ASSERT_NEAR(Cre[s], sr, 1e-8);
            TEST_ASSERT_NEAR(Cim[s], si, 1e-8);
        }
        free(Cre);
        free(Cim);
        deleteCircuit(&symb);
        freePackage();
    }
}

static void test_bell_qasm(void) {
    TEST_SECTION("metamorphic: Bell prep+uncompute OpenQASM");

    setup_pkg();
    qBDD circ;
    int n = 0;
    TEST_ASSERT(sim_path("tests/qasm/metamorphic/bell_roundtrip.qasm", &circ, &n));
    assert_zero_survives_gc(circ, n, 1e-8, "Bell uncompute failed");
    deleteCircuit(&circ);
    freePackage();
}

/**
 * Heavy GC + metamorphic validity:
 *  - deep random U U† (stresses node table / natural GC during sim)
 *  - many distinct rx(θ) then rx(-θ) (stresses CUSTOM terminal table + GC after)
 *  - mega terminal flood past INITIAL_TERMINAL_SIZE realloc(s)
 *  - orphan junk circuits collected, then a fresh metamorphic round-trip
 */
static int write_heavy_rx_roundtrip(const char *path, int n, int n_angles) {
    FILE *f = fopen(path, "w");
    if (!f) return 0;
    write_qasm_header(f, n);
    for (int q = 0; q < n; q++)
        fprintf(f, "h qubits[%d];\n", q);
    for (int i = 0; i < n_angles; i++) {
        double th = 0.017 + 0.013 * (double)i;
        fprintf(f, "rx(%.12f) qubits[%d];\n", th, i % n);
    }
    fprintf(f, "\n// adjoint rx(-θ) in reverse\n");
    for (int i = n_angles - 1; i >= 0; i--) {
        double th = 0.017 + 0.013 * (double)i;
        fprintf(f, "rx(%.12f) qubits[%d];\n", -th, i % n);
    }
    for (int q = 0; q < n; q++)
        fprintf(f, "h qubits[%d];\n", q);
    fclose(f);
    return 1;
}

/**
 * Alternate rx and ry with unique angles (no H fan-out): diagram stays tiny while
 * insertvalue sees many distinct values over time; GC may recycle slots.
 * Round-trips with rx(-θ) / ry(-θ) in reverse → |0…0⟩.
 */
static int write_mega_terminal_flood(const char *path, int n, int n_pairs) {
    FILE *f = fopen(path, "w");
    if (!f) return 0;
    write_qasm_header(f, n);

    for (int i = 0; i < n_pairs; i++) {
        double thr = 0.011 + 0.007 * (double)i;
        double thy = 0.019 + 0.009 * (double)i;
        int q = i % n;
        fprintf(f, "rx(%.14f) qubits[%d];\n", thr, q);
        fprintf(f, "ry(%.14f) qubits[%d];\n", thy, q);
        if ((i % 17) == 0)
            fprintf(f, "t qubits[%d];\n", q);
        if ((i % 23) == 0)
            fprintf(f, "s qubits[%d];\n", (q + 1) % n);
    }

    fprintf(f, "\n// adjoints in reverse\n");
    for (int i = n_pairs - 1; i >= 0; i--) {
        double thr = 0.011 + 0.007 * (double)i;
        double thy = 0.019 + 0.009 * (double)i;
        int q = i % n;
        if ((i % 23) == 0) {
            /* S† = S³ */
            fprintf(f, "s qubits[%d];\n", (q + 1) % n);
            fprintf(f, "s qubits[%d];\n", (q + 1) % n);
            fprintf(f, "s qubits[%d];\n", (q + 1) % n);
        }
        if ((i % 17) == 0)
            fprintf(f, "tdg qubits[%d];\n", q);
        fprintf(f, "ry(%.14f) qubits[%d];\n", -thy, q);
        fprintf(f, "rx(%.14f) qubits[%d];\n", -thr, q);
    }

    fclose(f);
    return 1;
}

/**
 * Product state: distinct ry on each qubit → generally 2^n distinct live amplitudes
 * (forces customPointers past INITIAL_TERMINAL_SIZE), then ry(-θ) → |0…0⟩.
 */
static int write_wide_product_roundtrip(const char *path, int n) {
    FILE *f = fopen(path, "w");
    if (!f) return 0;
    write_qasm_header(f, n);
    for (int q = 0; q < n; q++) {
        double th = 0.13 + 0.17 * (double)q + 0.001 * (double)(q * q);
        fprintf(f, "ry(%.14f) qubits[%d];\n", th, q);
        fprintf(f, "rx(%.14f) qubits[%d];\n", 0.07 + 0.11 * (double)q, q);
    }
    fprintf(f, "\n// adjoints\n");
    for (int q = n - 1; q >= 0; q--) {
        fprintf(f, "rx(%.14f) qubits[%d];\n", -(0.07 + 0.11 * (double)q), q);
        double th = 0.13 + 0.17 * (double)q + 0.001 * (double)(q * q);
        fprintf(f, "ry(%.14f) qubits[%d];\n", -th, q);
    }
    fclose(f);
    return 1;
}

static void test_heavy_gc_metamorphic(void) {
    TEST_SECTION("metamorphic+GC: deep U U† and rx-flood round-trip");

    rng_seed(0x6C6C6Cu);

    /* Deep random circuits — freelist pressure during sim + GC hammer after */
    for (int t = 0; t < META_HEAVY_TRIALS; t++) {
        int n = 4 + (int)rng_below(3);           /* 4..6 */
        int depth = 24 + (int)rng_below(24);     /* 24..47 */
        if (depth > META_MAX_OPS / 2)
            depth = META_MAX_OPS / 2;
        circuit_t U;
        random_circuit(&U, n, depth);

        char path[512];
        path_tmp(path, sizeof path, "heavy_uu.qasm");
        TEST_ASSERT(write_circuit_uu_dagger(path, &U));

        setup_pkg();
        qBDD circ;
        int nq = 0;
        TEST_ASSERT(sim_path(path, &circ, &nq));
        assert_zero_survives_gc(circ, nq, 1e-5, "heavy U U† after GC");
        deleteCircuit(&circ);
        freePackage();
    }

    /* Many distinct rx(θ)/rx(-θ): insertvalue churn; GC may recycle (no size claim). */
    {
        char path[512];
        path_tmp(path, sizeof path, "heavy_rx.qasm");
        TEST_ASSERT(write_heavy_rx_roundtrip(path, 4, META_HEAVY_RX_ANGLES));

        setup_pkg();
        qBDD circ;
        int nq = 0;
        TEST_ASSERT(sim_path(path, &circ, &nq));
        TEST_ASSERT(nq == 4);
        assert_zero_survives_gc(circ, nq, 1e-5, "heavy rx flood U U† after GC");
        qBDD_protect(circ);
        hammer_gc();
        assert_is_all_zero(circ, nq, 1e-5, "heavy rx still |0> after extra protect+GC");
        qBDD_unprotect(circ);
        deleteCircuit(&circ);
        freePackage();
    }

    /* Orphan: simulate junk UU†, drop root, GC, then validity circuit in new package */
    {
        circuit_t junk;
        random_circuit(&junk, 5, 30);
        char path_junk[512], path_ok[512];
        path_tmp(path_junk, sizeof path_junk, "orphan_junk.qasm");
        path_tmp(path_ok, sizeof path_ok, "orphan_ok.qasm");
        TEST_ASSERT(write_circuit_uu_dagger(path_junk, &junk));
        TEST_ASSERT(write_circuit_uu_dagger(path_ok, &junk));

        setup_pkg();
        qBDD waste;
        int nw = 0;
        TEST_ASSERT(sim_path(path_junk, &waste, &nw));
        deleteCircuit(&waste);
        hammer_gc();
        freePackage();

        setup_pkg();
        qBDD circ;
        int nq = 0;
        TEST_ASSERT(sim_path(path_ok, &circ, &nq));
        assert_zero_survives_gc(circ, nq, 1e-5, "metamorphic after orphan GC");
        deleteCircuit(&circ);
        freePackage();
    }
}

static void test_mega_distinct_terminals(void) {
    TEST_SECTION("metamorphic+GC: mega distinct terminals (flood + wide product)");

    /* Temporal flood: many distinct values over time (GC may recycle). */
    {
        char path[512];
        path_tmp(path, sizeof path, "mega_flood.qasm");
        TEST_ASSERT(write_mega_terminal_flood(path, 5, META_MEGA_FLOOD_PAIRS));

        setup_pkg();
        qBDD circ;
        int nq = 0;
        TEST_ASSERT(sim_path(path, &circ, &nq));
        TEST_ASSERT(nq == 5);
        assert_zero_survives_gc(circ, nq, 5e-5, "mega flood round-trip after GC");
        deleteCircuit(&circ);
        hammer_gc();
        freePackage();
    }

    /*
     * Wide product: META_WIDE_QUBITS distinct (ry,rx) → ~2^n live leaf values,
     * past INITIAL_TERMINAL_SIZE so customPointers must realloc; then U† → |0>.
     */
    {
        char path[512];
        path_tmp(path, sizeof path, "mega_wide.qasm");
        TEST_ASSERT(write_wide_product_roundtrip(path, META_WIDE_QUBITS));

        setup_pkg();
        int size0 = mtbddmaxTerminalSize;
        qBDD circ;
        int nq = 0;
        TEST_ASSERT(sim_path(path, &circ, &nq));
        TEST_ASSERT(nq == META_WIDE_QUBITS);
        TEST_ASSERT_MSG(mtbddmaxTerminalSize > INITIAL_TERMINAL_SIZE,
            "wide product must grow past INITIAL_TERMINAL_SIZE (10000)");
        TEST_ASSERT_MSG(mtbddmaxTerminalSize > size0,
            "wide product should trigger customPointers realloc");
        assert_zero_survives_gc(circ, nq, 5e-5, "wide product round-trip after GC");
        deleteCircuit(&circ);
        hammer_gc();
        freePackage();
    }

    /* Recycled indices must still work for a fresh UU† */
    {
        circuit_t U;
        rng_seed(0x71E711ALu);
        random_circuit(&U, 4, 20);
        char path2[512];
        path_tmp(path2, sizeof path2, "mega_after_gc.qasm");
        TEST_ASSERT(write_circuit_uu_dagger(path2, &U));
        setup_pkg();
        qBDD circ;
        int nq = 0;
        TEST_ASSERT(sim_path(path2, &circ, &nq));
        assert_zero_survives_gc(circ, nq, 1e-5, "UU† after mega flood + full GC");
        deleteCircuit(&circ);
        freePackage();
    }
}

int main(void) {
    printf("MEDUSA Stage 4 metamorphic tests (OpenQASM + GC)\n");

    snprintf(g_tmpdir, sizeof g_tmpdir, "/tmp/medusa_meta_%d", (int)getpid());
    if (mkdir(g_tmpdir, 0700) != 0 && errno != EEXIST) {
        fprintf(stderr, "mkdir %s failed: %s\n", g_tmpdir, strerror(errno));
        return 1;
    }

    test_gate_identities_qasm();
    test_cz_both_qasm_orders();
    test_uu_dagger_random_qasm();
    test_dagger_antihomomorphism_qasm();
    test_reverse_without_adj_qasm();
    test_symb_float_t_tdg();
    test_bell_qasm();
    test_heavy_gc_metamorphic();
    test_mega_distinct_terminals();

    char cmd[512];
    snprintf(cmd, sizeof cmd, "rm -rf '%s'", g_tmpdir);
    int rm_st = system(cmd);
    (void)rm_st;

    return test_report("test_metamorphic");
}
