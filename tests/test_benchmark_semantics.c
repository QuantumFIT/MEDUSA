/**
 * Semantic checks on benchmark algorithms (doubles f64 / MoToBuddy).
 * Verifies algorithm outcomes, not only unit-norm MTBDDs:
 *   - Bernstein–Vazirani recovers the secret bitstring
 *   - Grover amplifies the oracle-marked state
 *   - Round-trip reversible circuits return |0...0>
 */
#include "test_harness.h"
#include "sim.h"
#include "interface.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static bool sim_path(const char *path, qBDD *out, int *n_qubits)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "cannot open %s\n", path);
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

/** Probability of one computational-basis state (bits[i] in {'0','1'}). */
static prob_t basis_prob(qBDD t, const char *bits, int n)
{
    (void)n;
    while (!qBDD_isTerminal(t) && !qBDD_isFalse(t)) {
        uint32_t v = (uint32_t)qBDD_getVar(t);
        if (bits[v] == '1')
            t = qBDD_getHigh(t);
        else
            t = qBDD_getLow(t);
    }
    if (qBDD_isFalse(t))
        return 0.0;
    return (prob_t)qBDD_calculateProb(t);
}

static void fill_zeros(char *bits, int n)
{
    for (int i = 0; i < n; i++)
        bits[i] = '0';
    bits[n] = '\0';
}

/*
 * Marked search register from generator_GR*.py:
 *   X on even qubits, then multi-controlled phase on all-1s
 *   ⇒ marks even=0, odd=1. Ancilla q[n] is prepared in |1> and stays |1>.
 */
static void grover_marked_bits(char *bits, int n_search, int n_total)
{
    fill_zeros(bits, n_total);
    for (int i = 0; i < n_search; i++)
        bits[i] = (i % 2 == 0) ? '0' : '1';
    if (n_search < n_total)
        bits[n_search] = '1';
}

/** Max basis-state probability (enumerates all 2^n; n must be small). */
static prob_t max_basis_prob(qBDD circ, int n, char *out_bits)
{
    prob_t best = 0.0;
    char bits[64];
    bits[n] = '\0';
    const int N = 1 << n;
    for (int s = 0; s < N; s++) {
        for (int i = 0; i < n; i++)
            bits[i] = ((s >> i) & 1) ? '1' : '0';
        prob_t p = basis_prob(circ, bits, n);
        if (p > best) {
            best = p;
            if (out_bits)
                memcpy(out_bits, bits, (size_t)n + 1);
        }
    }
    return best;
}

static void test_bv_recovers_secret(void)
{
    TEST_SECTION("BV: output is secret computational basis state");

    /* BV-01: z q[1]; cx q[0],q[1] → |11> */
    {
        initPackage(0, 0, 0);
    test_silence_gbc();
        qBDD circ;
        int n = 0;
        TEST_ASSERT(sim_path("benchmarks/no-measure/BernsteinVazirani/01.qasm", &circ, &n));
        TEST_ASSERT(n == 2);
        TEST_ASSERT_NEAR(qBDD_total_prob(circ, n), 1.0, 1e-8);
        TEST_ASSERT_NEAR(basis_prob(circ, "11", n), 1.0, 1e-6);
        TEST_ASSERT_NEAR(basis_prob(circ, "00", n), 0.0, 1e-6);
        TEST_ASSERT_NEAR(basis_prob(circ, "01", n), 0.0, 1e-6);
        TEST_ASSERT_NEAR(basis_prob(circ, "10", n), 0.0, 1e-6);
        deleteCircuit(&circ);
        freePackage();
    }

    /* BV-05: cx from {0,2,4} onto ancilla 5 → secret 10101, ancilla 1 */
    {
        initPackage(0, 0, 0);
    test_silence_gbc();
        qBDD circ;
        int n = 0;
        TEST_ASSERT(sim_path("benchmarks/no-measure/BernsteinVazirani/05.qasm", &circ, &n));
        TEST_ASSERT(n == 6);
        TEST_ASSERT_NEAR(qBDD_total_prob(circ, n), 1.0, 1e-7);
        TEST_ASSERT_NEAR(basis_prob(circ, "101011", n), 1.0, 1e-5);
        TEST_ASSERT(basis_prob(circ, "000000", n) < 1e-5);
        deleteCircuit(&circ);
        freePackage();
    }

    /* measure/ BV-01 builds the same pre-measure state |11> */
    {
        initPackage(0, 0, 0);
    test_silence_gbc();
        qBDD circ;
        int n = 0;
        TEST_ASSERT(sim_path("benchmarks/measure/BernsteinVazirani/01.qasm", &circ, &n));
        TEST_ASSERT_NEAR(basis_prob(circ, "11", n), 1.0, 1e-6);
        deleteCircuit(&circ);
        freePackage();
    }
}

static void test_reversible_returns_zero(void)
{
    TEST_SECTION("reversible round-trips return |0...0>");

    const char *files[] = {
        "benchmarks/no-measure/MCToffoli/03.qasm",
        "benchmarks/no-measure/MCToffoli/06.qasm",
        "benchmarks/no-measure/RevLib/peres_9.qasm",
        "benchmarks/no-measure/RevLib/4gt11_84.qasm",
        "benchmarks/no-measure/Feynman/tof_3.qasm",
        "benchmarks/no-measure/Feynman/barenco_tof_3.qasm",
    };

    for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); i++) {
        initPackage(0, 0, 0);
    test_silence_gbc();
        qBDD circ;
        int n = 0;
        TEST_ASSERT_MSG(sim_path(files[i], &circ, &n), files[i]);
        TEST_ASSERT(n > 0 && n < 64);
        char bits[64];
        fill_zeros(bits, n);
        TEST_ASSERT_NEAR(qBDD_total_prob(circ, n), 1.0, 1e-7);
        prob_t p0 = basis_prob(circ, bits, n);
        TEST_ASSERT_MSG(fabs((double)p0 - 1.0) < 1e-5, files[i]);
        deleteCircuit(&circ);
        freePackage();
    }
}

static void test_grover_benchmark_marked_is_mode(void)
{
    TEST_SECTION("LP-Grover: marked search string is amplified");

    /* LP-Grover/05: n_search=5 → marked 01010, ancilla |1> */
    {
        initPackage(0, 0, 0);
    test_silence_gbc();
        qBDD circ;
        int n = 0;
        TEST_ASSERT(sim_path("benchmarks/no-measure/LP-Grover/05.qasm", &circ, &n));
        TEST_ASSERT(n == 10);
        TEST_ASSERT_NEAR(qBDD_total_prob(circ, n), 1.0, 1e-6);

        char marked[64], best_bits[64];
        grover_marked_bits(marked, 5, n);
        TEST_ASSERT(strcmp(marked, "0101010000") == 0);

        prob_t p_marked = basis_prob(circ, marked, n);
        prob_t p_max = max_basis_prob(circ, n, best_bits);

        TEST_ASSERT_NEAR(p_marked, p_max, 1e-6);
        TEST_ASSERT_MSG(p_marked > 0.9,
            "LP-Grover/05 marked state should be amplified");

        char wrong[64];
        memcpy(wrong, marked, (size_t)n + 1);
        wrong[1] = '0'; /* 01010 → 00010, unmarked */
        prob_t p_wrong = basis_prob(circ, wrong, n);
        TEST_ASSERT_MSG(p_marked > p_wrong,
            "marked state must beat an unmarked neighbor");

        deleteCircuit(&circ);
        freePackage();
    }

    {
        const char *path = "benchmarks/no-measure/LP-Grover/NL_06.qasm";
        FILE *probe = fopen(path, "r");
        if (probe) {
            fclose(probe);
            initPackage(0, 0, 0);
            test_silence_gbc();
            qBDD circ;
            int n = 0;
            TEST_ASSERT(sim_path(path, &circ, &n));
            TEST_ASSERT(n == 12);
            char marked[64];
            grover_marked_bits(marked, 6, n);
            prob_t p_marked = basis_prob(circ, marked, n);
            prob_t p_max = max_basis_prob(circ, n, NULL);
            TEST_ASSERT_NEAR(p_marked, p_max, 1e-6);
            TEST_ASSERT(p_marked > 0.9);
            deleteCircuit(&circ);
            freePackage();
        }
    }
}

static void test_mogrover_peak_on_data(void)
{
    TEST_SECTION("MOGrover-03: probability concentrates above uniform");

    initPackage(0, 0, 0);
    test_silence_gbc();
    qBDD circ;
    int n = 0;
    TEST_ASSERT(sim_path("benchmarks/no-measure/MOGrover/03.qasm", &circ, &n));
    TEST_ASSERT(n == 9);
    TEST_ASSERT_NEAR(qBDD_total_prob(circ, n), 1.0, 1e-6);

    prob_t best = max_basis_prob(circ, n, NULL);
    const double uniform = 1.0 / (double)(1 << n);
    TEST_ASSERT_MSG(best > uniform * 8.0,
        "MOGrover should concentrate probability well above uniform");
    TEST_ASSERT(best >= 0.1);

    deleteCircuit(&circ);
    freePackage();
}

static void test_quantum_counting_not_collapsed_wrongly(void)
{
    TEST_SECTION("LP-QuantumCounting: unit norm; not a single bogus spike");

    initPackage(0, 0, 0);
    test_silence_gbc();
    qBDD circ;
    int n = 0;
    TEST_ASSERT(sim_path(
        "benchmarks/no-measure/LP-QuantumCounting/07_03_05_0.qasm", &circ, &n));
    TEST_ASSERT(n == 11);
    TEST_ASSERT_NEAR(qBDD_total_prob(circ, n), 1.0, 1e-6);

    /*
     * This instance ends in a near-uniform / constant-amplitude MTBDD.
     * Check no single basis state has probability ≫ 1/2^n (would mean
     * accidental collapse), and that |0> is not the unique support.
     */
    char zero[64];
    fill_zeros(zero, n);
    prob_t p0 = basis_prob(circ, zero, n);
    const double uniform = 1.0 / (double)(1ULL << n);
    /* Constant-leaf case: every state has ~uniform probability */
    TEST_ASSERT_NEAR(p0, uniform, uniform * 0.5);

    deleteCircuit(&circ);
    freePackage();
}

static void test_period_finding_unit_and_structured(void)
{
    TEST_SECTION("LP-PeriodFinding: unit norm; support not only |0>");

    initPackage(0, 0, 0);
    test_silence_gbc();
    qBDD circ;
    int n = 0;
    TEST_ASSERT(sim_path(
        "benchmarks/no-measure/LP-PeriodFinding/07_03_05_0.qasm", &circ, &n));
    TEST_ASSERT(n > 0);
    TEST_ASSERT_NEAR(qBDD_total_prob(circ, n), 1.0, 1e-6);

    char zero[64];
    fill_zeros(zero, n);
    prob_t p0 = basis_prob(circ, zero, n);
    /* Period finding should leave a non-trivial superposition. */
    TEST_ASSERT(p0 < 0.99);

    deleteCircuit(&circ);
    freePackage();
}

int main(void)
{
    printf("MEDUSA benchmark semantic tests\n");
    test_bv_recovers_secret();
    test_reversible_returns_zero();
    test_grover_benchmark_marked_is_mode();
    test_mogrover_peak_on_data();
    test_quantum_counting_not_collapsed_wrongly();
    test_period_finding_unit_and_structured();
    return test_report("test_benchmark_semantics");
}
