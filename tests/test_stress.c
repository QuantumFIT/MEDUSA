/**
 * @file test_stress.c
 * Extreme stress tests for MoToBuddy GC, terminal ownership, and gate churn.
 * Built twice: LEAF_BACKEND_DOUBLES (default f128) and LEAF_BACKEND_GMP.
 *
 * Intensity via -DSTRESS_LEVEL=1|2|3 (default 2). Level 3 is intentionally brutal.
 */
#include "test_harness.h"

#include "interface.h"
#include "gates.h"
#include "mtbdd.h"
#include "kernel.h"
#include "terminal.h"

#include <stdint.h>
#include <string.h>

#ifndef STRESS_LEVEL
#define STRESS_LEVEL 2
#endif

#if STRESS_LEVEL >= 3
#define TERMINAL_FLOOD   4000
#define GATE_QUBITS      10
#define GATE_ROUNDS      80
#define APPLY_ITERS      1500
#define ORPHAN_CIRCUITS  200
#define GC_EVERY         1
/* Force ≥2 growth steps: INITIAL_TERMINAL_SIZE (10000) → 20000 → 40000 */
#define TERMINAL_REALLOC_COUNT  (INITIAL_TERMINAL_SIZE + INITIAL_TERMINAL_SIZE / 2 + 500)
#elif STRESS_LEVEL >= 2
#define TERMINAL_FLOOD   2000
#define GATE_QUBITS      8
#define GATE_ROUNDS      40
#define APPLY_ITERS      800
#define ORPHAN_CIRCUITS  100
#define GC_EVERY         2
#define TERMINAL_REALLOC_COUNT  (INITIAL_TERMINAL_SIZE + 256)
#else
#define TERMINAL_FLOOD   500
#define GATE_QUBITS      6
#define GATE_ROUNDS      20
#define APPLY_ITERS      200
#define ORPHAN_CIRCUITS  40
#define GC_EVERY         3
#define TERMINAL_REALLOC_COUNT  (INITIAL_TERMINAL_SIZE + 64)
#endif

/* Backend-specific leaf payload layout (must match leaf_*.c). */
#if defined(LEAF_BACKEND_DOUBLES)
struct LEAF_TYPE_IMPL {
    leaf_primitive_t re;
    leaf_primitive_t im;
};
#elif defined(LEAF_BACKEND_GMP)
struct LEAF_TYPE_IMPL {
    leaf_primitive_t a;
    leaf_primitive_t b;
    leaf_primitive_t c;
    leaf_primitive_t d;
};
#else
# error "Build with LEAF_BACKEND_DOUBLES or LEAF_BACKEND_GMP"
#endif

static unsigned refcount(qBDD n) {
    if (n < 0 || n >= bddnodesize) return 0;
    return bddnodes[n].refcou;
}

static void setup_pkg(void) {
    initPackage(0, 0, 0);
    test_silence_gbc();
}

static void assert_terminal_alive(qBDD t, const char *ctx) {
    if (qBDD_isFalse(t)) return;
    TEST_ASSERT_MSG(qBDD_isTerminal(t), ctx);
    LEAF_TYPE v = qBDD_getTerminalValue(t);
    TEST_ASSERT_MSG(v.pImpl != NULL, ctx);
}

static int walk_check_terminals(qBDD root, int max_nodes) {
    qBDD stack[512];
    int sp = 0, seen = 0, terminals = 0;
    if (sp < 512) stack[sp++] = root;
    while (sp > 0 && seen < max_nodes) {
        qBDD n = stack[--sp];
        seen++;
        if (qBDD_isFalse(n)) continue;
        if (qBDD_isTerminal(n)) {
            LEAF_TYPE v = qBDD_getTerminalValue(n);
            TEST_ASSERT_MSG(v.pImpl != NULL, "walk: NULL pImpl on reachable terminal");
            terminals++;
            continue;
        }
        if (qBDD_isInternal(n) && sp + 2 < 512) {
            stack[sp++] = qBDD_getLow(n);
            stack[sp++] = qBDD_getHigh(n);
        }
    }
    return terminals;
}

/** Allocate a unique classic terminal value owned by MoToBuddy after maketerminal. */
static LEAF_TYPE *heap_unique_leaf(unsigned idx) {
    LEAF_TYPE *p = malloc(sizeof(LEAF_TYPE));
    if (!p) abort();
    p->pImpl = malloc(sizeof(LEAF_TYPE_IMPL));
    if (!p->pImpl) abort();
#if defined(LEAF_BACKEND_DOUBLES)
    init_generic(p->pImpl->re);
    init_generic(p->pImpl->im);
    set_d_generic(p->pImpl->re, (double)idx + 0.125);
    set_d_generic(p->pImpl->im, -((double)idx) * 0.03125);
#elif defined(LEAF_BACKEND_GMP)
    mpz_init(p->pImpl->a);
    mpz_init(p->pImpl->b);
    mpz_init(p->pImpl->c);
    mpz_init(p->pImpl->d);
    mpz_set_ui(p->pImpl->a, (idx % 97) + 1);
    mpz_set_ui(p->pImpl->b, (idx / 97) % 17);
    mpz_set_ui(p->pImpl->c, (idx * 3) % 13);
    mpz_set_ui(p->pImpl->d, (idx * 5) % 11);
#endif
    return p;
}

/* -------------------------------------------------------------------------- */

static void assert_unique_leaf_value(qBDD t, unsigned idx, const char *ctx)
{
    assert_terminal_alive(t, ctx);
    LEAF_TYPE v = qBDD_getTerminalValue(t);
#if defined(LEAF_BACKEND_DOUBLES)
    TEST_ASSERT_NEAR(to_double_generic(v.pImpl->re), (double)idx + 0.125, 1e-9);
    TEST_ASSERT_NEAR(to_double_generic(v.pImpl->im), -((double)idx) * 0.03125, 1e-9);
#elif defined(LEAF_BACKEND_GMP)
    TEST_ASSERT(mpz_cmp_ui(v.pImpl->a, (idx % 97) + 1) == 0);
    TEST_ASSERT(mpz_cmp_ui(v.pImpl->b, (idx / 97) % 17) == 0);
    TEST_ASSERT(mpz_cmp_ui(v.pImpl->c, (idx * 3) % 13) == 0);
    TEST_ASSERT(mpz_cmp_ui(v.pImpl->d, (idx * 5) % 11) == 0);
#endif
}

/**
 * Force mtbdd_insertvalue CUSTOM realloc past INITIAL_TERMINAL_SIZE and
 * verify early/mid/late terminals stay readable (known failure class when the
 * customPointers table moves or new slots are left uninitialized).
 */
static void stress_terminal_table_realloc(void) {
    TEST_SECTION("STRESS terminal table realloc preserves live values");

    setup_pkg();
    const unsigned N = (unsigned)TERMINAL_REALLOC_COUNT;
    TEST_ASSERT_MSG(N > (unsigned)INITIAL_TERMINAL_SIZE,
        "realloc test must exceed INITIAL_TERMINAL_SIZE");

    /* Sample slots that must remain valid across every growth step. */
    const unsigned sample_idx[] = {
        0, 1, 2,
        (unsigned)INITIAL_TERMINAL_SIZE / 2,
        (unsigned)INITIAL_TERMINAL_SIZE - 1,   /* last slot before first realloc */
        (unsigned)INITIAL_TERMINAL_SIZE,       /* first slot after first realloc */
        (unsigned)INITIAL_TERMINAL_SIZE + 1,
        N / 2,
        N - 2,
        N - 1,
    };
    const int nsample = (int)(sizeof(sample_idx) / sizeof(sample_idx[0]));
    qBDD samples[16];
    int got_sample[16];
    for (int s = 0; s < nsample; s++) {
        samples[s] = 0;
        got_sample[s] = 0;
    }

    int size_before = mtbddmaxTerminalSize;
    int realloc_events = 0;

    for (unsigned i = 0; i < N; i++) {
        LEAF_TYPE *leaf = heap_unique_leaf(i);
        qBDD t = qBDD_maketerminal(qBDD_classicLType(), leaf);
        TEST_ASSERT_MSG(qBDD_isTerminal(t), "maketerminal failed during realloc flood");

        if (mtbddmaxTerminalSize > size_before) {
            realloc_events++;
            size_before = mtbddmaxTerminalSize;
            /* Immediately after growth: every already-captured sample must still decode. */
            for (int s = 0; s < nsample; s++) {
                if (!got_sample[s])
                    continue;
                assert_unique_leaf_value(samples[s], sample_idx[s],
                    "terminal corrupted right after customPointers realloc");
            }
        }

        for (int s = 0; s < nsample; s++) {
            if (i == sample_idx[s]) {
                samples[s] = qBDD_protect(t);
                got_sample[s] = 1;
                assert_unique_leaf_value(t, i, "sample at insert");
            }
        }

        /* Dedup across the realloc boundary must still free the unused copy. */
        if (i == (unsigned)INITIAL_TERMINAL_SIZE || i == N - 1) {
            LEAF_TYPE *dup = heap_unique_leaf(i);
            qBDD t2 = qBDD_maketerminal(qBDD_classicLType(), dup);
            TEST_ASSERT(t2 == t);
        }
    }

    TEST_ASSERT_MSG(realloc_events >= 1,
        "expected at least one terminal-table realloc (N > INITIAL_TERMINAL_SIZE)");
    TEST_ASSERT_MSG(mtbddmaxTerminalSize >= N,
        "terminal table capacity should cover all inserted values");

    forceGC();
    forceGC();

    for (int s = 0; s < nsample; s++) {
        TEST_ASSERT(got_sample[s]);
        assert_unique_leaf_value(samples[s], sample_idx[s],
            "sample corrupted after realloc flood + GC");
    }

    /* Apply equal-result free path on a pre-realloc terminal. */
    {
        qBDD early = samples[0];
        qBDD same = binary_apply(early, qBDD_false(), addLeaf);
        TEST_ASSERT(same == early);
        assert_unique_leaf_value(early, sample_idx[0],
            "pre-realloc terminal after apply equal-result");
    }

    for (int s = 0; s < nsample; s++)
        qBDD_unprotect(samples[s]);
    freePackage();
}

static void stress_terminal_flood_and_dedup(void) {
    TEST_SECTION("STRESS terminal flood + dedup + GC");

    setup_pkg();
    qBDD keepers[64];
    int nkeep = 0;

    for (unsigned i = 0; i < (unsigned)TERMINAL_FLOOD; i++) {
        LEAF_TYPE *leaf = heap_unique_leaf(i);
        qBDD t = qBDD_maketerminal(qBDD_classicLType(), leaf);
        TEST_ASSERT(qBDD_isTerminal(t) || qBDD_isFalse(t));

        /* Re-insert same logical value — must dedup / free unused copy */
        LEAF_TYPE *dup = heap_unique_leaf(i);
        qBDD t2 = qBDD_maketerminal(qBDD_classicLType(), dup);
        TEST_ASSERT(t2 == t);

        if ((i % (TERMINAL_FLOOD / 32 + 1)) == 0 && nkeep < 64) {
            keepers[nkeep++] = qBDD_protect(t);
        }
        if ((i % (unsigned)GC_EVERY) == 0)
            forceGC();
    }

    forceGC();
    forceGC();

    for (int k = 0; k < nkeep; k++) {
        assert_terminal_alive(keepers[k], "keeper terminal corrupted after flood/GC");
        qBDD_unprotect(keepers[k]);
    }

    freePackage();
}

static void stress_orphan_circuit_churn(void) {
    TEST_SECTION("STRESS orphan diagrams (unprotected temps) + GC");

    setup_pkg();
    qBDD survivor;
    circuit_init_interface(&survivor, 4);
    gate_h(&survivor, 0);
    gate_cnot(&survivor, 1, 0);

    for (int i = 0; i < ORPHAN_CIRCUITS; i++) {
        /* Build throwaway DAGs via apply — never protect them */
        qBDD junk = unary_apply(survivor, invertLeaf);
        qBDD junk2 = unary_apply(junk, invertLeaf);
        qBDD junk3 = binary_apply(junk, junk2, addLeaf);
        qBDD junk4 = binary_apply(survivor, qBDD_false(), addLeaf);
        (void)junk3; (void)junk4;

        if ((i & 3) == 0) {
            qBDD brief = unary_apply(survivor, invertLeaf);
            qBDD_protect(brief);
            qBDD_unprotect(brief);
        }
        if ((i % GC_EVERY) == 0)
            forceGC();
    }

    forceGC();
    forceGC();
    TEST_ASSERT_NEAR(qBDD_total_prob(survivor, 4), 1.0, 1e-5);
    walk_check_terminals(survivor, 10000);
    {
        qBDD w = survivor;
        while (qBDD_isInternal(w))
            w = qBDD_getLow(w);
        if (qBDD_isTerminal(w) && !qBDD_isFalse(w))
            assert_terminal_alive(w, "survivor leaf after orphan GC");
    }

    qBDD_unprotect(survivor);
    freePackage();
}

static void stress_gate_storm_with_gc(void) {
    TEST_SECTION("STRESS gate storm + frequent GC");

    setup_pkg();
    const int n = GATE_QUBITS;
    qBDD circ;
    circuit_init_interface(&circ, (uint32_t)n);

    for (int r = 0; r < GATE_ROUNDS; r++) {
        uint32_t a = (uint32_t)(r % n);
        uint32_t b = (uint32_t)((r * 3 + 1) % n);
        uint32_t c = (uint32_t)((r * 5 + 2) % n);
        if (a == b) b = (b + 1) % (uint32_t)n;
        if (c == a || c == b) c = (c + 1) % (uint32_t)n;

        switch (r % 7) {
            case 0: gate_h(&circ, a); break;
            case 1: gate_x(&circ, a); break;
            case 2: gate_y(&circ, a); break;
            case 3: gate_z(&circ, a); break;
            case 4: gate_s(&circ, a); break;
            case 5: gate_cnot(&circ, a, b); break;
            case 6:
                if (n >= 3) gate_toffoli(&circ, a, b, c);
                else gate_cnot(&circ, a, b);
                break;
        }

        if ((r % GC_EVERY) == 0) {
            forceGC();
            walk_check_terminals(circ, 20000);
            prob_t p = qBDD_total_prob(circ, n);
            /* Unitary gates: norm must stay ~1 (float eps grows with depth). */
#if defined(LEAF_BACKEND_DOUBLES)
            TEST_ASSERT_NEAR(p, 1.0, 1e-4);
#else
            TEST_ASSERT_NEAR(p, 1.0, 1e-8);
#endif
            TEST_ASSERT(refcount(circ) >= 1);
        }
    }

    forceGC();
    TEST_ASSERT_NEAR(qBDD_total_prob(circ, n), 1.0,
#if defined(LEAF_BACKEND_DOUBLES)
        1e-3
#else
        1e-6
#endif
    );
    int nt = walk_check_terminals(circ, 50000);
    TEST_ASSERT(nt >= 1);

    qBDD_unprotect(circ);
    freePackage();
}

static void stress_apply_temp_churn(void) {
    TEST_SECTION("STRESS apply temp churn (free-of-unused + GC)");

    setup_pkg();
    qBDD circ;
    circuit_init_interface(&circ, 2);
    gate_h(&circ, 0);
    gate_h(&circ, 1);

    qBDD leaf = circ;
    int guard = 0;
    while (qBDD_isInternal(leaf) && guard++ < 32)
        leaf = qBDD_getLow(leaf);
    if (!qBDD_isTerminal(leaf) || qBDD_isFalse(leaf)) {
        /* fall back: any reachable terminal */
        leaf = circ;
        while (qBDD_isInternal(leaf))
            leaf = qBDD_getHigh(leaf);
    }
    qBDD_protect(leaf);

    for (int i = 0; i < APPLY_ITERS; i++) {
        /* Unprotected temps — MoToBuddy may free equal results / GC nodes */
        qBDD a = unary_apply(leaf, invertLeaf);
        qBDD b = unary_apply(a, invertLeaf);
        qBDD s = binary_apply(leaf, qBDD_false(), addLeaf);
        qBDD z = binary_apply(leaf, a, addLeaf); /* cancel → false */

        if ((i % 17) == 0) {
            qBDD_protect(b);
            forceGC();
            assert_terminal_alive(leaf, "live leaf died during apply churn");
            TEST_ASSERT(s == leaf || qBDD_isTerminal(s));
            TEST_ASSERT(qBDD_isFalse(z) || qBDD_isTerminal(z));
            LEAF_TYPE v = qBDD_getTerminalValue(leaf);
            TEST_ASSERT(v.pImpl != NULL);
            qBDD_unprotect(b);
        } else if ((i % GC_EVERY) == 0) {
            forceGC();
        }
        (void)b; (void)s; (void)z;
    }

    forceGC();
    assert_terminal_alive(leaf, "leaf dead after apply storm");
    TEST_ASSERT_NEAR(qBDD_total_prob(circ, 2), 1.0, 1e-5);

    qBDD_unprotect(leaf);
    qBDD_unprotect(circ);
    freePackage();
}

static void stress_protect_thrash(void) {
    TEST_SECTION("STRESS protect/unprotect thrash under mutation");

    setup_pkg();
    qBDD circ;
    circuit_init_interface(&circ, 5);

    for (int i = 0; i < APPLY_ITERS; i++) {
        qBDD snap = circ;
        qBDD_protect(snap);
        gate_h(&circ, (uint32_t)(i % 5));
        if ((i & 1) == 0)
            gate_cnot(&circ, (uint32_t)(i % 5), (uint32_t)((i + 1) % 5));
        /* Drop old snap — may be equal to circ after no-op levels */
        qBDD_unprotect(snap);
        if ((i % GC_EVERY) == 0) {
            forceGC();
            TEST_ASSERT(refcount(circ) >= 1 || circ != 0);
            walk_check_terminals(circ, 20000);
        }
    }

    forceGC();
    TEST_ASSERT_NEAR(qBDD_total_prob(circ, 5), 1.0,
#if defined(LEAF_BACKEND_DOUBLES)
        1e-3
#else
        1e-6
#endif
    );

    qBDD_unprotect(circ);
    freePackage();
}

static void stress_ghz_ladder(void) {
    TEST_SECTION("STRESS GHZ ladder (H + CNOT chain + GC)");

    setup_pkg();
    const int n = GATE_QUBITS > 8 ? 8 : GATE_QUBITS;
    qBDD circ;
    circuit_init_interface(&circ, (uint32_t)n);

    gate_h(&circ, 0);
    forceGC();
    for (int i = 0; i < n - 1; i++) {
        gate_cnot(&circ, (uint32_t)(i + 1), (uint32_t)i);
        forceGC();
        walk_check_terminals(circ, 20000);
        TEST_ASSERT_NEAR(qBDD_total_prob(circ, n), 1.0, 1e-5);
    }

    /* Undo with reverse CNOTs + H */
    for (int i = n - 2; i >= 0; i--) {
        gate_cnot(&circ, (uint32_t)(i + 1), (uint32_t)i);
        forceGC();
    }
    gate_h(&circ, 0);
    forceGC();
    TEST_ASSERT_NEAR(qBDD_total_prob(circ, n), 1.0, 1e-5);

    qBDD_unprotect(circ);
    freePackage();
}

static void stress_mixed_toffoli_gc(void) {
    TEST_SECTION("STRESS mixed Toffoli/CNOT with relentless GC");

    setup_pkg();
    qBDD circ;
    circuit_init_interface(&circ, 6);

    gate_h(&circ, 0);
    gate_h(&circ, 1);
    forceGC();

    for (int r = 0; r < GATE_ROUNDS; r++) {
        gate_toffoli(&circ, 2, 0, 1);
        forceGC();
        gate_cnot(&circ, 3, 2);
        forceGC();
        gate_toffoli(&circ, 4, 1, 3);
        forceGC();
        gate_x(&circ, 5);
        gate_toffoli(&circ, 5, 4, 0);
        forceGC();

        walk_check_terminals(circ, 30000);
        TEST_ASSERT_NEAR(qBDD_total_prob(circ, 6), 1.0,
#if defined(LEAF_BACKEND_DOUBLES)
            5e-3
#else
            1e-6
#endif
        );
    }

    qBDD_unprotect(circ);
    freePackage();
}

static void stress_replace_root_gc(void) {
    TEST_SECTION("STRESS replace root repeatedly + GC (protect balance)");

    setup_pkg();
    qBDD circ;
    circuit_init_interface(&circ, 4);

    for (int i = 0; i < APPLY_ITERS / 2; i++) {
        qBDD old = circ;
        gate_h(&circ, (uint32_t)(i % 4));
        /* circ already re-protected by gate; force collection of discarded DAGs */
        forceGC();
        if (old != circ) {
            /* old should not be required; touching circ terminals must work */
            walk_check_terminals(circ, 10000);
        }
        TEST_ASSERT_NEAR(qBDD_total_prob(circ, 4), 1.0, 1e-4);
    }

    qBDD_unprotect(circ);
    freePackage();
}

/* -------------------------------------------------------------------------- */

int main(void) {
#if defined(LEAF_BACKEND_DOUBLES)
#if LEAF_FLOAT_TYPE == 0
    const char *backend = "doubles_f32";
#elif LEAF_FLOAT_TYPE == 2
    const char *backend = "doubles_f80";
#elif LEAF_FLOAT_TYPE == 3
    const char *backend = "doubles_f128";
#else
    const char *backend = "doubles_f64";
#endif
#elif defined(LEAF_BACKEND_GMP)
    const char *backend = "gmp";
#else
    const char *backend = "unknown";
#endif
    printf("MEDUSA STRESS tests [%s] STRESS_LEVEL=%d "
           "(flood=%d realloc=%d qubits=%d rounds=%d apply=%d)\n",
           backend, STRESS_LEVEL, TERMINAL_FLOOD, TERMINAL_REALLOC_COUNT,
           GATE_QUBITS, GATE_ROUNDS, APPLY_ITERS);

    setup_pkg();
    TEST_ASSERT_MSG(
        mtbdd_terminal_functions_list[qBDD_classicLType()].freefun == freePimpl,
        "classic freefun not registered — apply will not free pImpl; "
        "revert with patches/revert-classic-freepimpl.patch if enabling broke you");
    freePackage();

    stress_terminal_table_realloc();
    stress_terminal_flood_and_dedup();
    stress_orphan_circuit_churn();
    stress_apply_temp_churn();
    stress_protect_thrash();
    stress_ghz_ladder();
    stress_replace_root_gc();
    stress_gate_storm_with_gc();
    stress_mixed_toffoli_gc();

    return test_report("test_stress");
}
