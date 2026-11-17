/**
 * The simulation core as a library uses it (P0b, issue #73): circuits read
 * from a string, and measurement sampling that returns its results instead of
 * printing them, from a seed of the caller's choosing.
 *
 * - sim_string must agree with sim_file on the same text - loops included,
 *   classic and symbolic, which rewind the stream - and report errors the same
 *   way (status, message, line, no circuit left).
 * - measure_all must draw only outcomes the state allows, count every sample,
 *   give the same samples for the same seed (on any platform: the generator is
 *   MEDUSA's own), and print_samples must print them as the CLI always has.
 *
 * Everything runs in one package, as a library caller would. Built for f128
 * and f32 (make test-sim-api runs both): the sampling's comparisons are where
 * the leaf type's precision shows.
 */
#include "test_harness.h"
#include "sim.h"
#include "interface.h"
#include "symb_utils.h"
#include "gates.h"

#include <dlfcn.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/* sim_string's memory stream, failing on demand: this definition takes the
 * place of the C library's in the link (sim.o resolves fmemopen to it), and
 * otherwise hands over to the real one. That is the only way to reach the
 * SIM_ERR_STREAM path, as a real fmemopen practically never fails. */
static int fail_fmemopen;
FILE *fmemopen(void *buf, size_t size, const char *mode)
{
    static FILE *(*real)(void *, size_t, const char *);
    if (fail_fmemopen) {
        errno = ENOMEM;
        return NULL;
    }
    if (!real)
        *(void **)&real = dlsym(RTLD_NEXT, "fmemopen");
    return real(buf, size, mode);
}

#define HDR "OPENQASM 3.0;\ninclude \"stdgates.inc\";\n"

static double basis_prob(qBDD t, const char *bits)
{
    while (!qBDD_isTerminal(t) && !qBDD_isFalse(t)) {
        uint32_t v = (uint32_t)qBDD_getVar(t);
        t = (bits[v] == '1') ? qBDD_getHigh(t) : qBDD_getLow(t);
    }
    return qBDD_isFalse(t) ? 0.0 : (double)qBDD_calculateProb(t);
}

static FILE *qasm_stream(const char *text)
{
    FILE *f = tmpfile();
    if (!f) {
        perror("tmpfile");
        exit(2);
    }
    fputs(text, f);
    rewind(f);
    return f;
}

static const sim_flags_t CLASSIC = { .opt_symb = false, .opt_info = false };
static const sim_flags_t SYMBOLIC = { .opt_symb = true, .opt_info = false };

/* Valid circuits: straight-line, a classic loop, and a loop with enough
 * passes to need symbolic refinement (the h on q[1] changes the mapping). */
static const char *VALID[] = {
    HDR "qubit[3] q;\nh q[0];\ncx q[0], q[1];\nx q[2];\n",
    HDR "qubit[2] q;\nfor int i in [1:3] {\nx q[0];\n}\nh q[1];\n",
    HDR "qubit[2] q;\nh q[0];\nfor int i in [1:3] {\nh q[1];\ncx q[1], q[0];\n}\n",
};

static void test_string_matches_file(void)
{
    TEST_SECTION("sim_string gives what sim_file gives (loops classic and symbolic)");
    static const char *BASIS3[] = { "000", "100", "010", "110", "001", "101", "011", "111" };
    for (size_t i = 0; i < sizeof(VALID) / sizeof(VALID[0]); i++) {
        for (int symb = 0; symb < 2; symb++) {
            const sim_flags_t *fl = symb ? &SYMBOLIC : &CLASSIC;
            qBDD a, b;
            sim_info_t ia, ib;
            init_sim_info(&ia);
            init_sim_info(&ib);
            FILE *f = qasm_stream(VALID[i]);
            sim_status_t sa = sim_file(f, &a, fl, &ia);
            fclose(f);
            sim_status_t sb = sim_string(VALID[i], &b, fl, &ib);
            char what[96];
            snprintf(what, sizeof(what), "circuit %zu%s", i, symb ? " (symbolic)" : "");
            TEST_ASSERT_MSG(sa == SIM_OK && sb == SIM_OK, what);
            TEST_ASSERT_MSG(ia.n_qubits == ib.n_qubits && ia.n_loops == ib.n_loops, what);
            if (sa == SIM_OK && sb == SIM_OK) {
                for (int k = 0; k < (1 << ia.n_qubits); k++) {
                    TEST_ASSERT_NEAR_MSG(basis_prob(a, BASIS3[k]), basis_prob(b, BASIS3[k]), 1e-12, what);
                }
                deleteCircuit(&a);
                deleteCircuit(&b);
            }
            free_sim_info(&ia);
            free_sim_info(&ib);
        }
    }
}

static void test_string_errors(void)
{
    TEST_SECTION("sim_string: errors as from sim_file, no circuit left");
    static const struct {
        const char *qasm;
        sim_status_t want;
        size_t line;
        const char *msg;
    } CASES[] = {
        { "", SIM_NO_CIRCUIT, 1, NULL },
        { HDR, SIM_NO_CIRCUIT, 3, NULL },
        { HDR "qubit[2] q;\nh q[0];\nfoo q[1];\n", SIM_ERR_UNSUPPORTED, 5, "Invalid command 'foo'" },
        { HDR "qubit[2] q;\nfor int i in [1:2] {\nh q[0];\n", SIM_ERR_SYNTAX, 6, "missing '}'" },
    };
    for (size_t i = 0; i < sizeof(CASES) / sizeof(CASES[0]); i++) {
        qBDD circ;
        memset(&circ, 0x5a, sizeof(circ));   /* the caller's handle need not be initialised */
        sim_info_t info;
        init_sim_info(&info);
        sim_status_t got = sim_string(CASES[i].qasm, &circ, &CLASSIC, &info);
        char what[512];
        snprintf(what, sizeof(what), "case %zu: status %d, line %zu, error \"%s\"",
                 i, (int)got, info.line, info.error);
        TEST_ASSERT_MSG(got == CASES[i].want && info.status == got, what);
        TEST_ASSERT_MSG(qBDD_isFalse(circ), what);
        if (CASES[i].msg) {
            TEST_ASSERT_MSG(strstr(info.error, CASES[i].msg) != NULL, what);
            TEST_ASSERT_MSG(info.line == CASES[i].line, what);
        }
        free_sim_info(&info);
    }
}

/* Simulates qasm (which must measure) and samples it. */
static bool sample(const char *qasm, unsigned long n, uint64_t seed, sim_samples_t *out, int *nq)
{
    qBDD circ;
    sim_info_t info;
    init_sim_info(&info);
    bool ok = sim_string(qasm, &circ, &CLASSIC, &info) == SIM_OK && info.is_measure;
    if (ok) {
        measure_all(n, seed, circ, info.n_qubits, info.bits_to_measure, out);
        *nq = info.n_qubits;
        deleteCircuit(&circ);
    }
    free_sim_info(&info);
    return ok;
}

static unsigned long count_of(const sim_samples_t *s, const char *bits)
{
    for (size_t k = 0; k < s->n; k++) {
        if (strcmp(s->v[k].bits, bits) == 0)
            return s->v[k].count;
    }
    return 0;
}

static const char *BELL = HDR "bit[2] c;\nqubit[2] q;\nh q[0];\ncx q[0], q[1];\n"
                          "c[0] = measure q[0];\nc[1] = measure q[1];\n";

static void test_measure_outcomes(void)
{
    TEST_SECTION("measure_all: only allowed outcomes, every sample counted");
    sim_samples_t s;
    int nq = 0;
    TEST_ASSERT(sample(BELL, 4000, 42, &s, &nq));
    unsigned long total = 0;
    for (size_t k = 0; k < s.n; k++) {
        total += s.v[k].count;
        TEST_ASSERT(strlen(s.v[k].bits) == (size_t)nq);
    }
    TEST_ASSERT(total == 4000);
    TEST_ASSERT(s.n == 2);
    unsigned long c00 = count_of(&s, "00"), c11 = count_of(&s, "11");
    TEST_ASSERT(c00 + c11 == 4000);
    /* 2000 +- 126 is 4 standard deviations of a fair coin over 4000 draws */
    TEST_ASSERT(c00 > 1874 && c00 < 2126);
    free_samples(&s);
    TEST_ASSERT(s.n == 0 && s.v == NULL);

    /* A basis state: one outcome, highest classical bit first; a bit that
     * nothing is measured into reads 'x'. */
    static const char *ONE = HDR "bit[3] c;\nqubit[3] q;\nx q[0];\nc[0] = measure q[0];\nc[2] = measure q[2];\n";
    TEST_ASSERT(sample(ONE, 50, 1, &s, &nq));
    TEST_ASSERT(s.n == 1 && strcmp(s.v[0].bits, "0x1") == 0 && s.v[0].count == 50);
    free_samples(&s);
}

static void test_measure_seed(void)
{
    TEST_SECTION("measure_all: same seed, same samples; another seed, others");
    sim_samples_t a, b, c;
    int nq;
    TEST_ASSERT(sample(BELL, 1024, 7, &a, &nq));
    TEST_ASSERT(sample(BELL, 1024, 7, &b, &nq));
    TEST_ASSERT(sample(BELL, 1024, 8, &c, &nq));
    TEST_ASSERT(count_of(&a, "00") == count_of(&b, "00"));
    TEST_ASSERT(count_of(&a, "00") != count_of(&c, "00"));
    /* Pinned: the generator is MEDUSA's own (SplitMix64), so these are the
     * counts on every platform. They change only if the sampling does. */
    TEST_ASSERT(count_of(&a, "00") == 495 && count_of(&a, "11") == 529);
    free_samples(&a);
    free_samples(&b);
    free_samples(&c);
}

static void test_string_stream_failure(void)
{
    TEST_SECTION("sim_string: a memory stream that cannot be opened is SIM_ERR_STREAM");
    qBDD circ;
    memset(&circ, 0x5a, sizeof(circ));
    sim_info_t info;
    init_sim_info(&info);
    fail_fmemopen = 1;
    sim_status_t got = sim_string(HDR "qubit[1] q;\nh q[0];\n", &circ, &CLASSIC, &info);
    fail_fmemopen = 0;
    TEST_ASSERT_MSG(got == SIM_ERR_STREAM && info.status == SIM_ERR_STREAM, info.error);
    TEST_ASSERT_MSG(strstr(info.error, "Could not open the circuit string") != NULL, info.error);
    TEST_ASSERT(qBDD_isFalse(circ));
    free_sim_info(&info);
}

/* One measured qubit: the outcome table used to get n^3 - 1 = 0 buckets, and
 * the first sample a division by zero (SIGFPE), whatever the leaf type. */
static void test_measure_one_qubit(void)
{
    TEST_SECTION("measure_all: a single qubit");
    static const char *Q1 = HDR "bit[1] c;\nqubit[1] q;\nh q[0];\nc[0] = measure q[0];\n";
    sim_samples_t s;
    int nq;
    TEST_ASSERT(sample(Q1, 1000, 3, &s, &nq));
    TEST_ASSERT(nq == 1 && count_of(&s, "0") + count_of(&s, "1") == 1000);
    TEST_ASSERT(count_of(&s, "0") > 400 && count_of(&s, "1") > 400);
    free_samples(&s);
}

/* A wide register: the outcome table's n^3 - 1 buckets are capped at 2^20
 * from 102 qubits on (n^3 overflowed an int past 1290). Every qubit is
 * measured; q[101] is 1, the rest 0. */
static void test_measure_wide_register(void)
{
    TEST_SECTION("measure_all: 102 qubits (capped outcome table)");
    enum { W = 102 };
    char qasm[64 + W * 32];
    int len = snprintf(qasm, sizeof(qasm), HDR "bit[%d] c;\nqubit[%d] q;\nx q[%d];\n", W, W, W - 1);
    for (int i = 0; i < W; i++)
        len += snprintf(qasm + len, sizeof(qasm) - (size_t)len, "c[%d] = measure q[%d];\n", i, i);
    char want[W + 1];
    want[0] = '1';
    memset(want + 1, '0', W - 1);
    want[W] = '\0';
    sim_samples_t s;
    int nq;
    TEST_ASSERT(sample(qasm, 20, 5, &s, &nq));
    TEST_ASSERT(nq == W && s.n == 1 && count_of(&s, want) == 20);
    free_samples(&s);
}

/* Seed 63433462's first draw is 0.99999998926..., which f32 rounds to 1.0f.
 * Compared as a prob_t, it was not below the probability 1 of q[0], so the
 * qubit was sampled as 0 (and the assert that this cannot happen fired). */
static void test_measure_draw_near_one(void)
{
    TEST_SECTION("measure_all: a draw just below 1 on a certain qubit");
    static const char *X = HDR "bit[1] c;\nqubit[1] q;\nx q[0];\nc[0] = measure q[0];\n";
    sim_samples_t s;
    int nq;
    TEST_ASSERT(sample(X, 1, 63433462, &s, &nq));
    TEST_ASSERT(s.n == 1 && count_of(&s, "1") == 1);
    free_samples(&s);
}

static void test_measure_no_samples(void)
{
    TEST_SECTION("measure_all: zero samples, no outcomes");
    sim_samples_t s = { (sim_sample_t *)&s, 99 };   /* overwritten, not freed */
    int nq;
    TEST_ASSERT(sample(BELL, 0, 1, &s, &nq));
    TEST_ASSERT(s.n == 0 && s.v == NULL);
    free_samples(&s);
}

/* The false node holds no value: asking for one is answered, with a warning
 * on stderr (not stdout, which belongs to the caller), not dereferenced. */
static void test_false_terminal_value(void)
{
    TEST_SECTION("qBDD_getTerminalValue of the false node: no value");
    LEAF_TYPE v = qBDD_getTerminalValue(qBDD_false());
    TEST_ASSERT(v.pImpl == NULL);
}

/* A NULL argument (or no qubits) is a bug in the caller: an assert, as for
 * sim_file. Each case runs in a child, which must die of SIGABRT. The child
 * flushes the coverage counters before dying, which abort itself skips. */
#ifdef MEDUSA_COVERAGE
extern void __gcov_dump(void);
#endif
static void dump_coverage_and_abort(int sig)
{
#ifdef MEDUSA_COVERAGE
    __gcov_dump();
#endif
    signal(sig, SIG_DFL);
    raise(sig);
}

static void test_bad_arguments(void)
{
    TEST_SECTION("NULL arguments, no qubits: asserts");
    for (int which = 0; which < 6; which++) {
        fflush(NULL);
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            exit(2);
        }
        if (pid == 0) {
            struct rlimit no_core = { 0, 0 };
            setrlimit(RLIMIT_CORE, &no_core);
            if (!freopen("/dev/null", "w", stderr))
                _exit(3);
            signal(SIGABRT, dump_coverage_and_abort);
            qBDD circ = qBDD_false();
            sim_info_t info;
            init_sim_info(&info);
            sim_samples_t s;
            int bits[1] = { 0 };
            const char *q = HDR "qubit[1] q;\n";
            switch (which) {
            case 0: sim_string(NULL, &circ, &CLASSIC, &info); break;
            case 1: sim_string(q, NULL, &CLASSIC, &info); break;
            case 2: sim_string(q, &circ, NULL, &info); break;
            case 3: sim_string(q, &circ, &CLASSIC, NULL); break;
            case 4: measure_all(1, 1, circ, 1, bits, NULL); break;
            default: measure_all(1, 1, circ, 0, bits, &s); break;
            }
            _exit(0); /* reached only if the assert did not fire */
        }
        int status = 0;
        waitpid(pid, &status, 0);
        char what[64];
        snprintf(what, sizeof(what), "case %d: wait status %d", which, status);
        TEST_ASSERT_MSG(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT, what);
    }
}

/* measure() against the exact probability, for every target qubit and every
 * combination of the other qubits being unmeasured, 0 or 1: the sum of the
 * basis-state probabilities that agree with it and have the target at 1. A
 * path in the diagram skips variables; each skipped one that is unmeasured
 * counts twice. That used to count the node's own variable too, and skipped
 * variables beyond the path's end, so any unmeasured qubit before the target
 * doubled the result (issue #75). */
static void test_measure_exact(void)
{
    TEST_SECTION("measure: exact for every target and every partial assignment");
    static const char *CIRCS[] = {
        HDR "qubit[3] q;\nh q[0];\ncx q[0], q[2];\nx q[1];\n",
        HDR "qubit[3] q;\nh q[0];\nh q[1];\ncx q[1], q[2];\nrx(0.3) q[2];\nry(1.1) q[0];\n",
        HDR "qubit[4] q;\nh q[3];\ncx q[3], q[0];\nrx(0.9) q[1];\nry(0.4) q[2];\ncx q[2], q[1];\n",
    };
    /* within the leaf type's precision; a wrong count is off by a factor 2 */
    const double tol = sizeof(prob_t) == sizeof(float) ? 1e-5 : 1e-12;
    int bad = 0, checked = 0;
    for (size_t c = 0; c < sizeof(CIRCS) / sizeof(CIRCS[0]); c++) {
        qBDD circ;
        sim_info_t info;
        init_sim_info(&info);
        if (sim_string(CIRCS[c], &circ, &CLASSIC, &info) != SIM_OK) {
            TEST_ASSERT_MSG(0, info.error);
            free_sim_info(&info);
            continue;
        }
        int n = info.n_qubits, total = 1;
        for (int i = 0; i < n; i++)
            total *= 3;
        for (int xt = 0; xt < n; xt++) {
            for (int code = 0; code < total; code++) {
                char state[8];
                int k = code;
                for (int i = 0; i < n; i++, k /= 3)
                    state[i] = "x01"[k % 3];
                state[n] = '\0';
                if (state[xt] != 'x')
                    continue;
                double exact = 0.0;
                for (int b = 0; b < (1 << n); b++) {
                    char bits[8];
                    bool ok = true;
                    for (int i = 0; i < n; i++) {
                        bits[i] = (b >> i) & 1 ? '1' : '0';
                        if ((i == xt && bits[i] != '1') || (state[i] != 'x' && state[i] != bits[i]))
                            ok = false;
                    }
                    bits[n] = '\0';
                    if (ok)
                        exact += basis_prob(circ, bits);
                }
                double got = (double)measure(&circ, (uint32_t)xt, state, n);
                checked++;
                if (got > exact + tol || got < exact - tol) {
                    if (bad++ < 5)
                        fprintf(stderr, "  circuit %zu, target q[%d], state %s: measure %g, exact %g\n",
                                c, xt, state, got, exact);
                }
            }
        }
        deleteCircuit(&circ);
        free_sim_info(&info);
    }
    TEST_ASSERT(checked == 2 * 3 * 9 + 4 * 27);   /* targets x assignments of the others */
    TEST_ASSERT(bad == 0);
}

/* The cases from issue #75 through measure_all: a subset without q[0], and a
 * Bell pair on q[0], q[2] read into c[0], c[1] with q[1] unmeasured. */
static void test_measure_subsets(void)
{
    TEST_SECTION("measure_all: subsets without q[0], other classical bits");
    sim_samples_t s;
    int nq;
    static const char *LAST = HDR "bit[3] c;\nqubit[3] q;\nx q[2];\nc[2] = measure q[2];\n";
    TEST_ASSERT(sample(LAST, 100, 1, &s, &nq));
    TEST_ASSERT(s.n == 1 && count_of(&s, "1xx") == 100);
    free_samples(&s);
    static const char *PAIR = HDR "bit[3] c;\nqubit[3] q;\nh q[0];\ncx q[0], q[2];\n"
                              "c[0] = measure q[0];\nc[1] = measure q[2];\n";
    TEST_ASSERT(sample(PAIR, 4000, 2, &s, &nq));
    unsigned long c00 = count_of(&s, "x00"), c11 = count_of(&s, "x11");
    TEST_ASSERT(s.n == 2 && c00 + c11 == 4000 && c00 > 1874 && c00 < 2126);
    free_samples(&s);
    /* q[0] into c[2], q[2] into c[0], q[1] unmeasured */
    static const char *SWAP = HDR "bit[3] c;\nqubit[3] q;\nx q[0];\n"
                              "c[2] = measure q[0];\nc[0] = measure q[2];\n";
    TEST_ASSERT(sample(SWAP, 100, 3, &s, &nq));
    TEST_ASSERT(s.n == 1 && count_of(&s, "1x0") == 100);
    free_samples(&s);
}

static void test_print_samples(void)
{
    TEST_SECTION("print_samples: the CLI's format");
    char b0[] = "10", b1[] = "01";
    sim_sample_t v[] = { { b0, 3 }, { b1, 12 } };
    sim_samples_t s = { v, 2 };
    char buf[256] = { 0 };
    FILE *f = tmpfile();
    TEST_ASSERT(f != NULL);
    if (!f)
        return;
    print_samples(f, &s);
    rewind(f);
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = '\0';
    TEST_ASSERT(strcmp(buf, "Sampled results:\n    '10'    3\n    '01'    12\n") == 0);
}

int main(void)
{
    printf("MEDUSA library API tests (sim_string, measure_all)\n");
    initPackage(0, 0, 0);
    test_silence_gbc();
    init_symb_backend();

    test_string_matches_file();
    test_string_errors();
    test_measure_outcomes();
    test_measure_seed();
    test_measure_one_qubit();
    test_measure_wide_register();
    test_measure_draw_near_one();
    test_measure_exact();
    test_measure_subsets();
    test_measure_no_samples();
    test_string_stream_failure();
    test_false_terminal_value();
    test_bad_arguments();
    test_print_samples();

    freePackage();
    return test_report("test_sim_api");
}
