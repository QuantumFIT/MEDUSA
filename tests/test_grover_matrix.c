/**
 * Grover matrix: several LP-Grover sizes, loop unrolled vs loop-symbolic vs NL,
 * compiled per backend (f32/f64/f80/f128/gmp).
 *
 * Marked search = even 0 / odd 1; ancilla q[n_search] stays |1>.
 */
#include "test_harness.h"
#include "sim.h"
#include "interface.h"
#include "symb_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

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

static int run_one(const char *label, const char *path, int n_search, int symbolic)
{
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
        freePackage();
        TEST_ASSERT_MSG(0, label);
        return 1;
    }

    int n = info.n_qubits;
    double tot = to_p(qBDD_total_prob(circ, n));
    double pm = marked_search_p(circ, n, n_search);
    int pass = (fabs(tot - 1.0) < 0.05) && (pm >= GROVER_MIN_P);
    printf("  %s %-28s  n=%2d  total=%8.5f  P_marked=%8.5f  %s\n",
           pass ? "OK  " : "FAIL",
           label, n, tot, pm, symbolic ? "symb" : "classic");

    TEST_ASSERT_MSG(pass, label);

    deleteCircuit(&circ);
    freePackage();
    return pass ? 0 : 1;
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
        snprintf(label, sizeof(label), "n=%d loop classic", ns);
        run_one(label, cases[i].loop, ns, 0);
        snprintf(label, sizeof(label), "n=%d loop symbolic", ns);
        run_one(label, cases[i].loop, ns, 1);
        snprintf(label, sizeof(label), "n=%d NL classic", ns);
        run_one(label, cases[i].nl, ns, 0);
    }

    return test_report("test_grover_matrix");
}
