/**
 * Grover matrix: several LP-Grover sizes, loop classic vs loop-symbolic vs NL,
 * compiled per backend (f32/f64/f80/f128/gmp).
 *
 * Marked search = even 0 / odd 1; ancilla q[n_search] stays |1>.
 *
 * Each size is simulated three ways and the three are compared against each
 * other, not just against a threshold:
 *
 *   loop classic    NN.qasm, OpenQASM 3, `for int i in [1:k]`, unrolled at
 *                   runtime by sim.c (iters-- with a stream rewind)
 *   loop symbolic   the same file with opt_symb, so the loop is summarised
 *                   rather than unrolled
 *   NL classic      NL_NN.qasm, the same circuit written out by hand in
 *                   OpenQASM 2 with no loop at all
 *
 * The NL leg is what a same-file comparison cannot replace: the iteration
 * count is parsed once (get_iters, sim.c:299) and handed to both modes, so a
 * misparse shifts classic and symbolic identically and they still agree with
 * each other. Only a file that never went through the loop parser disagrees.
 * Verified by mutation - see tests/README.md.
 *
 * The comparison is over basis-state probabilities rather than amplitudes:
 * qBDD_calculateProb is the only value accessor that works on every leaf
 * implementation, and this suite has to keep running on algebraic GMP, whose
 * leaves are nothing like the re/im ones. So a global phase change would pass
 * here; test_metamorphic compares amplitudes and does see phase.
 */
#include "test_harness.h"
#include "sim.h"
#include "interface.h"
#include "symb_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/*
 * Tolerance for the symbolic-vs-classic probability comparison.
 *
 * symb_eval reaches the same state by different arithmetic than the runtime
 * unroll, so the two agree only to the leaf type's precision. Worst observed
 * deviation over all 2^n basis probabilities, across n=5/6/7:
 *
 *   f32   4.4e-05      f64   6.2e-08      f80   1.2e-11      f128, GMP   0
 *
 * These are deterministic - same binary, same input, same result - so the
 * floors below sit roughly a decade above what was measured rather than
 * needing margin for run-to-run spread.
 *
 * The loop-vs-NL leg takes no tolerance at all: it was exactly 0 on every leaf
 * type including f32, which is what you would expect from the same gates in
 * the same order, and asserting it exactly is what makes it sharp.
 */
#ifndef GROVER_CMP_EPS
#if defined(LEAF_BACKEND_DOUBLES) && (LEAF_FLOAT_TYPE == 0)
#define GROVER_CMP_EPS 1e-4
#elif defined(LEAF_BACKEND_DOUBLES) && (LEAF_FLOAT_TYPE == 1)
#define GROVER_CMP_EPS 1e-6
#elif defined(LEAF_BACKEND_DOUBLES) && (LEAF_FLOAT_TYPE == 2)
#define GROVER_CMP_EPS 1e-9
#else
#define GROVER_CMP_EPS 1e-12
#endif
#endif

#ifndef GROVER_MIN_P
#if defined(LEAF_BACKEND_DOUBLES) && (LEAF_FLOAT_TYPE == 0)
#define GROVER_MIN_P 0.80
#else
#define GROVER_MIN_P 0.90
#endif
#endif

static const char *backend_name(void)
{
#if defined(LEAF_BACKEND_GMP)
    return "gmp";
#elif defined(LEAF_BACKEND_DOUBLES)
#if LEAF_FLOAT_TYPE == 0
    return "f32";
#elif LEAF_FLOAT_TYPE == 2
    return "f80";
#elif LEAF_FLOAT_TYPE == 3
    return "f128";
#else
    return "f64";
#endif
#else
    return "unknown";
#endif
}

static double to_p(prob_t x)
{
    return (double)x;
}

static double basis_p(qBDD t, const char *bits)
{
    while (!qBDD_isTerminal(t) && !qBDD_isFalse(t)) {
        uint32_t v = (uint32_t)qBDD_getVar(t);
        t = (bits[v] == '1') ? qBDD_getHigh(t) : qBDD_getLow(t);
    }
    if (qBDD_isFalse(t))
        return 0.0;
    return to_p(qBDD_calculateProb(t));
}

/** P(search=marked, ancilla=1), summed over workspace bits. */
static double marked_search_p(qBDD circ, int n, int n_search)
{
    char bits[64];
    if (n >= 63)
        return -1.0;
    bits[n] = '\0';
    for (int i = 0; i < n; i++)
        bits[i] = '0';
    for (int i = 0; i < n_search; i++)
        bits[i] = (i % 2 == 0) ? '0' : '1';
    if (n_search < n)
        bits[n_search] = '1';

    int n_work = n - n_search - 1;
    if (n_work < 0)
        n_work = 0;
    int W = 1 << n_work;
    double p = 0.0;
    for (int w = 0; w < W; w++) {
        for (int i = 0; i < n_work; i++)
            bits[n_search + 1 + i] = ((w >> i) & 1) ? '1' : '0';
        p += basis_p(circ, bits);
    }
    return p;
}

/**
 * Simulate one file and, on success, hand back the full basis-probability
 * vector (2^n entries, caller frees) so the caller can compare runs.
 *
 * *vec_out is left NULL when the run is skipped or fails; callers must treat a
 * NULL as "nothing to compare" rather than as agreement.
 */
static int run_one(const char *label, const char *path, int n_search, int symbolic,
                   double **vec_out, int *n_out)
{
    if (vec_out)
        *vec_out = NULL;
    if (n_out)
        *n_out = 0;

    FILE *f = fopen(path, "r");
    if (!f) {
        printf("  SKIP %-28s  missing %s\n", label, path);
        return 0;
    }

    initPackage(0, 0, 0);
    test_silence_gbc();
    if (symbolic)
        init_symb_backend();

    sim_flags_t flags = { .opt_symb = symbolic ? true : false, .opt_info = false };
    sim_info_t info;
    init_sim_info(&info);
    qBDD circ;
    bool ok = sim_file(f, &circ, &flags, &info);
    fclose(f);

    TEST_SECTION(label);

    if (!ok) {
        printf("  FAIL %-28s  sim_file failed\n", label);
        free_sim_info(&info);
        freePackage();
        TEST_ASSERT_MSG(0, label);
        return 1;
    }

    int n = info.n_qubits;
    /* sim_file allocates the loop-timing arrays (and bits_to_measure for
     * measured circuits); main.c frees them via free_sim_info and so must we.
     * n is copied out above, so the info is done with here. */
    free_sim_info(&info);
    double tot = to_p(qBDD_total_prob(circ, n));
    double pm = marked_search_p(circ, n, n_search);
    int pass = (fabs(tot - 1.0) < 0.05) && (pm >= GROVER_MIN_P);
    printf("  %s %-28s  n=%2d  total=%8.5f  P_marked=%8.5f  %s\n",
           pass ? "OK  " : "FAIL",
           label, n, tot, pm, symbolic ? "symb" : "classic");

    TEST_ASSERT_MSG(pass, label);

    /* Must happen before freePackage: the vector is read out of the DD. */
    if (vec_out && n > 0 && n < 31) {
        size_t states = (size_t)1 << n;
        double *v = (double *)malloc(states * sizeof *v);
        if (v != NULL) {
            char bits[64];
            bits[n] = '\0';
            for (size_t st = 0; st < states; st++) {
                for (int i = 0; i < n; i++)
                    bits[i] = ((st >> i) & 1) ? '1' : '0';
                v[st] = basis_p(circ, bits);
            }
            *vec_out = v;
            if (n_out)
                *n_out = n;
        }
    }

    deleteCircuit(&circ);
    freePackage();
    return pass ? 0 : 1;
}

/**
 * Compare two basis-probability vectors entry by entry.
 *
 * tol == 0.0 asserts bit equality, which is the right bar for two classic runs
 * of the same gates in the same order.
 */
static void compare_vecs(const char *label, const double *a, int na,
                         const double *b, int nb, double tol)
{
    TEST_SECTION(label);
    if (a == NULL || b == NULL) {
        printf("  SKIP %-28s  a leg did not produce a vector\n", label);
        return;
    }
    TEST_ASSERT_MSG(na == nb, label);
    if (na != nb)
        return;

    size_t states = (size_t)1 << na;
    size_t bad = 0;
    double worst = 0.0;
    size_t worst_at = 0;
    for (size_t st = 0; st < states; st++) {
        double d = fabs(a[st] - b[st]);
        if (d > worst) {
            worst = d;
            worst_at = st;
        }
        if (d > tol)
            bad++;
    }
    printf("  %s %-28s  worst=%.3g at basis %zu%s\n",
           bad ? "FAIL" : "OK  ", label, worst, worst_at,
           tol == 0.0 ? "  (exact)" : "");
    TEST_ASSERT_MSG(bad == 0, label);
}

int main(void)
{
    const char *be = backend_name();
    printf("MEDUSA Grover matrix  backend=%s  min P_marked=%.2f\n",
           be, (double)GROVER_MIN_P);
    printf("  (loop = %s.qasm unrolled; loop-s = same file --symbolic; NL = NL_XX.qasm)\n",
           "LP-Grover/N");

    struct {
        int n_search;
        const char *loop;
        const char *nl;
    } cases[] = {
        { 5, "benchmarks/no-measure/LP-Grover/05.qasm",
             "benchmarks/no-measure/LP-Grover/NL_05.qasm" },
        { 6, "benchmarks/no-measure/LP-Grover/06.qasm",
             "benchmarks/no-measure/LP-Grover/NL_06.qasm" },
        { 7, "benchmarks/no-measure/LP-Grover/07.qasm",
             "benchmarks/no-measure/LP-Grover/NL_07.qasm" },
    };

    char label[64];
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        int ns = cases[i].n_search;
        double *v_lc = NULL, *v_ls = NULL, *v_nl = NULL;
        int n_lc = 0, n_ls = 0, n_nl = 0;

        snprintf(label, sizeof(label), "n=%d loop classic", ns);
        run_one(label, cases[i].loop, ns, 0, &v_lc, &n_lc);
        snprintf(label, sizeof(label), "n=%d loop symbolic", ns);
        run_one(label, cases[i].loop, ns, 1, &v_ls, &n_ls);
        snprintf(label, sizeof(label), "n=%d NL classic", ns);
        run_one(label, cases[i].nl, ns, 0, &v_nl, &n_nl);

        /* Same gates in the same order, both classic, so nothing should move
         * a single bit - only the parser differs (OpenQASM 3 + runtime unroll
         * against a flat OpenQASM 2 list). This is the leg that sees a fault
         * in the shared iteration-count parse. */
        snprintf(label, sizeof(label), "n=%d loop vs NL (classic)", ns);
        compare_vecs(label, v_lc, n_lc, v_nl, n_nl, 0.0);

        /* Summarised against unrolled: the property symbolic mode claims.
         * Not exact - symb_eval reaches the same state by different
         * arithmetic - so this one carries a tolerance. */
        snprintf(label, sizeof(label), "n=%d symbolic vs classic", ns);
        compare_vecs(label, v_ls, n_ls, v_lc, n_lc, GROVER_CMP_EPS);

        free(v_lc);
        free(v_ls);
        free(v_nl);
    }

    return test_report("test_grover_matrix");
}
