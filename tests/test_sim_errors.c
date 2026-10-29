/**
 * sim_file on invalid input: it must report the error and return, not end the
 * process, and leave the package usable for the next circuit.
 *
 * test_cli.sh already pins the diagnostics as the CLI prints them. What it
 * cannot see is the part a library caller depends on: that sim_file returns at
 * all, with the right status, that *circ holds no circuit afterwards, and that
 * nothing from the failed run is left behind - an open symbolic loop, the
 * symbolic expression table, a half-built mcx parameter list. So every case
 * here runs in one package, one after another, and the suite ends by
 * simulating valid circuits (classic and symbolic) in that same package.
 * Leaks are left to valgrind: tests/test_leaks.sh runs this binary.
 */
#include "test_harness.h"
#include "sim.h"
#include "interface.h"
#include "symb_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#define HDR "OPENQASM 3.0;\ninclude \"stdgates.inc\";\n"

typedef struct {
    const char *name;
    bool symbolic;
    const char *qasm;
    sim_status_t want;
    const char *msg; /* expected substring of info.error (NULL: no message) */
} error_case_t;

static const error_case_t CASES[] = {
    { "empty input", false, "", SIM_NO_CIRCUIT, NULL },
    { "comments only", false, "// nothing here\n", SIM_NO_CIRCUIT, NULL },
    { "non-digit register size", false, HDR "qubit[x] q;\n",
      SIM_ERR_SYNTAX, "non-digit character 'x'" },
    { "number too long", false, HDR "qubit[123456789012345678901234567890] q;\n",
      SIM_ERR_SYNTAX, "too many digits" },
    { "gate before the register", false, HDR "h q[0];\nqubit[2] q;\n",
      SIM_ERR_SYNTAX, "Circuit not initialized" },
    { "second qubit register", false, HDR "qubit[2] q;\nh q[0];\nqubit[3] r;\n",
      SIM_ERR_UNSUPPORTED, "Multiple qubit register" },
    { "register size mismatch", false, HDR "qubit[2] q;\nbit[3] c;\n",
      SIM_ERR_UNSUPPORTED, "different than the size" },
    { "qubit index out of range", false, HDR "qubit[2] q;\nh q[0];\ncx q[0], q[5];\n",
      SIM_ERR_SYNTAX, "q[5] is outside the declared register of 2 qubit(s)" },
    { "bit index out of range", false, HDR "qubit[2] q;\nbit[2] c;\nc[7] = measure q[0];\n",
      SIM_ERR_SYNTAX, "c[7] is outside the declared register of 2 bit(s)" },
    { "measure without a bit register", false, HDR "qubit[2] q;\nmeasure q[0] -> c[0];\n",
      SIM_ERR_SYNTAX, "uninitialized bit register" },
    { "unknown gate", false, HDR "qubit[2] q;\nh q[0];\nswap q[0], q[1];\n",
      SIM_ERR_UNSUPPORTED, "Invalid command 'swap'" },
    { "bad rotation angle", false, HDR "qubit[2] q;\nrx(notanangle) q[0];\n",
      SIM_ERR_SYNTAX, "Invalid rx angle" },
    /* Used to be an assert, i.e. SIGABRT. */
    { "cz on one qubit", false, HDR "qubit[2] q;\nh q[0];\ncz q[1], q[1];\n",
      SIM_ERR_SYNTAX, "control and the target qubit must differ" },
    /* The two mcx cases fail with the parameter list half built. */
    { "mcx syntax", false, HDR "qubit[4] q;\nmcx q[0] q[1], q[2];\n",
      SIM_ERR_SYNTAX, "Invalid 'mcx' gate syntax" },
    { "mcx index out of range", false, HDR "qubit[4] q;\nmcx q[0], q[1], q[9];\n",
      SIM_ERR_SYNTAX, "q[9] is outside" },
    { "slash that is not a comment", false, HDR "qubit[2] q;\n/ h q[0];\n",
      SIM_ERR_SYNTAX, "expected a one-line comment" },
    { "slash inside a skipped loop", false,
      HDR "qubit[2] q;\nfor int i in [1:-1:0] {\n/ h q[0];\n}\n",
      SIM_ERR_SYNTAX, "expected a one-line comment" },
    { "loop step zero", false, HDR "qubit[2] q;\nfor int i in [1:0:4] {\nh q[0];\n}\n",
      SIM_ERR_SYNTAX, "step must be non-zero" },
    { "nested loop", false,
      HDR "qubit[2] q;\nfor int i in [1:2] {\nfor int j in [1:2] {\nh q[0];\n}\n}\n",
      SIM_ERR_UNSUPPORTED, "Nested loops" },
    { "unfinished loop", false, HDR "qubit[2] q;\nfor int i in [1:3] {\nh q[0];\n",
      SIM_ERR_SYNTAX, "missing '}'" },
    { "stray closing brace", false, HDR "qubit[2] q;\n}\n",
      SIM_ERR_SYNTAX, "unexpected end of a loop" },

    /* Numbers and indices. */
    { "end of file inside a number", false, HDR "qubit[2",
      SIM_ERR_SYNTAX, "end of file when converting a number" },
    { "empty number", false, HDR "qubit[] q;\n",
      SIM_ERR_SYNTAX, "not a valid number." },
    { "no index at all", false, HDR "qubit q;\n",
      SIM_ERR_SYNTAX, "expected a qubit index" },
    { "negative register size", false, HDR "qubit[-1] q;\n",
      SIM_ERR_SYNTAX, "not a valid qubit identifier" },
    { "register size over 32 bits", false, HDR "qubit[4294967296] q;\n",
      SIM_ERR_SYNTAX, "not a valid qubit identifier" },
    { "non-digit qubit index", false, HDR "qubit[2] q;\nh q[x];\n",
      SIM_ERR_SYNTAX, "non-digit character 'x'" },
    { "non-digit bit index", false, HDR "qubit[2] q;\nbit[2] c;\nc[x] = measure q[0];\n",
      SIM_ERR_SYNTAX, "non-digit character 'x'" },

    /* Commands and registers. */
    { "end of file inside a command", false, HDR "qubit[2] q;\nh",
      SIM_ERR_SYNTAX, "end of file when loading a command" },
    { "command too long", false, HDR "qubit[2] q;\nabcdefghijklmnopqrstuvwxyzabcdef q[0];\n",
      SIM_ERR_UNSUPPORTED, "command too long" },
    { "missing ';'", false, HDR "qubit[2] q;\nh q[0]",
      SIM_ERR_SYNTAX, "expected ';' to end the current line" },
    { "non-digit bit register size", false, HDR "qubit[2] q;\nbit[x] c;\n",
      SIM_ERR_SYNTAX, "non-digit character 'x'" },
    { "end of file before the bit register name", false, HDR "qubit[2] q;\nbit[2]",
      SIM_ERR_SYNTAX, "bit register name expected" },
    { "bit register name too long", false,
      HDR "qubit[2] q;\nbit[2] abcdefghijklmnopqrstuvwxyzabcdef;\n",
      SIM_ERR_UNSUPPORTED, "identifier is too long, max supported length is 30" },
    { "qubit register after a bigger bit register", false, HDR "bit[2] c;\nqubit[3] q;\n",
      SIM_ERR_UNSUPPORTED, "different than the size" },
    { "bad ry angle", false, HDR "qubit[2] q;\nry(pi/) q[0];\n",
      SIM_ERR_SYNTAX, "Invalid ry angle" },
    { "bad rz angle", false, HDR "qubit[2] q;\nrz(2*) q[0];\n",
      SIM_ERR_SYNTAX, "Invalid rz angle" },

    /* Loop headers. */
    { "for without a range", false, HDR "qubit[2] q;\nfor int i in range;\n",
      SIM_ERR_SYNTAX, "expected a number of loop iterations" },
    { "bad loop start", false, HDR "qubit[2] q;\nfor int i in [a:2] {\nh q[0];\n}\n",
      SIM_ERR_SYNTAX, "non-digit character 'a'" },
    { "bad loop end", false, HDR "qubit[2] q;\nfor int i in [1:b] {\nh q[0];\n}\n",
      SIM_ERR_SYNTAX, "non-digit character 'b'" },
    { "bad loop step", false, HDR "qubit[2] q;\nfor int i in [1:1:c] {\nh q[0];\n}\n",
      SIM_ERR_SYNTAX, "non-digit character 'c'" },
    { "loop range overflow", false,
      HDR "qubit[2] q;\nfor int i in [-9223372036854775808:9223372036854775807] {\nh q[0];\n}\n",
      SIM_ERR_SYNTAX, "overflow detected" },
    { "loop range overflow, counting down", false,
      HDR "qubit[2] q;\nfor int i in [9223372036854775807:-1:-9223372036854775808] {\nh q[0];\n}\n",
      SIM_ERR_SYNTAX, "overflow detected" },
    { "loop counting down from below its end", false,
      HDR "qubit[2] q;\nfor int i in [1:-1:4] {\nh q[0];\n}\n",
      SIM_ERR_SYNTAX, "Invalid number of loop iterations." },
    { "end of file before '{'", false, HDR "qubit[2] q;\nfor int i in [1:2]",
      SIM_ERR_SYNTAX, "at the start of a loop" },
    { "skipped loop never closed", false, HDR "qubit[2] q;\nfor int i in [1:-1:0] {\nh q[0];\n",
      SIM_ERR_SYNTAX, "there is an unfinished loop" },

    /* Symbolic: each of these fails with symb_init's state live. */
    { "symbolic: unfinished loop", true, HDR "qubit[2] q;\nh q[0];\nfor int i in [1:3] {\nh q[1];\n",
      SIM_ERR_SYNTAX, "missing '}'" },
    { "symbolic: arbitrary angle in a loop", true,
      HDR "qubit[2] q;\nh q[0];\nfor int i in [1:3] {\nh q[1];\nrx(0.3) q[0];\n}\n",
      SIM_ERR_UNSUPPORTED, "Arbitrary-angle rx" },
    { "symbolic: bad index in a loop", true,
      HDR "qubit[2] q;\nfor int i in [1:3] {\nx q[0];\ncx q[0], q[2];\n}\n",
      SIM_ERR_SYNTAX, "q[2] is outside" },
    /* A first loop that completes, so the circuit and the symbolic expression
     * table have both moved on, then a second one that does not. */
    { "symbolic: error in the second loop", true,
      HDR "qubit[3] q;\nh q[0];\nfor int i in [1:4] {\nh q[1];\ncx q[1], q[2];\n}\n"
          "for int j in [1:2] {\nx q[2];\ny q[7];\n}\n",
      SIM_ERR_SYNTAX, "q[7] is outside" },
};

/** A stream holding text, seekable as loops need. */
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

static sim_status_t run(FILE *f, bool symbolic, qBDD *circ, sim_info_t *info)
{
    sim_flags_t flags = { .opt_symb = symbolic, .opt_info = false };
    init_sim_info(info);
    *circ = qBDD_false();
    return sim_file(f, circ, &flags, info);
}

static void test_error_cases(void)
{
    TEST_SECTION("invalid input: status, message, no circuit left");
    for (size_t i = 0; i < sizeof(CASES) / sizeof(CASES[0]); i++) {
        const error_case_t *t = &CASES[i];
        FILE *f = qasm_stream(t->qasm);
        qBDD circ;
        sim_info_t info;
        sim_status_t got = run(f, t->symbolic, &circ, &info);
        fclose(f);

        char what[512];
        snprintf(what, sizeof(what), "%s: status %d (want %d), error \"%s\"",
                 t->name, (int)got, (int)t->want, info.error);
        TEST_ASSERT_MSG(got == t->want, what);
        TEST_ASSERT_MSG(info.status == got, what);
        TEST_ASSERT_MSG(qBDD_isFalse(circ), what);
        if (t->msg) {
            TEST_ASSERT_MSG(strstr(info.error, t->msg) != NULL, what);
            /* One line, as CLI and API callers print it. */
            TEST_ASSERT_MSG(strchr(info.error, '\n') == NULL, what);
        }
        else {
            TEST_ASSERT_MSG(info.error[0] == '\0', what);
        }
        free_sim_info(&info);
    }
}

/** Probability of one basis state; bits[v] is qubit v. */
static double basis_prob(qBDD t, const char *bits)
{
    while (!qBDD_isTerminal(t) && !qBDD_isFalse(t)) {
        uint32_t v = (uint32_t)qBDD_getVar(t);
        t = (bits[v] == '1') ? qBDD_getHigh(t) : qBDD_getLow(t);
    }
    return qBDD_isFalse(t) ? 0.0 : (double)qBDD_calculateProb(t);
}

/* Every gate checks each of its qubit operands. One case per operand
 * position, since each is parsed (and checked) by its own call. */
static void test_gate_operands(void)
{
    static const char *GATES[] = {
        "x q[9];", "y q[9];", "z q[9];", "h q[9];", "s q[9];", "t q[9];",
        "tdg q[9];", "rx(pi/2) q[9];", "ry(pi/2) q[9];",
        "rx(0.3) q[9];", "ry(0.3) q[9];", "rz(0.3) q[9];",
        "cx q[9], q[0];", "cx q[0], q[9];",
        "cz q[9], q[0];", "cz q[0], q[9];",
        "ccx q[9], q[0], q[1];", "ccx q[0], q[9], q[1];", "ccx q[0], q[1], q[9];",
        "mcx q[9], q[0], q[1];", "mcx q[0], q[1], q[9];",
    };
    TEST_SECTION("every gate operand is range-checked");
    for (size_t i = 0; i < sizeof(GATES) / sizeof(GATES[0]); i++) {
        char qasm[256];
        snprintf(qasm, sizeof(qasm), HDR "qubit[3] q;\nh q[0];\n%s\n", GATES[i]);
        FILE *f = qasm_stream(qasm);
        qBDD circ;
        sim_info_t info;
        sim_status_t got = run(f, false, &circ, &info);
        fclose(f);
        char what[512];
        snprintf(what, sizeof(what), "%s: status %d, error \"%s\"", GATES[i], (int)got, info.error);
        TEST_ASSERT_MSG(got == SIM_ERR_SYNTAX, what);
        TEST_ASSERT_MSG(strstr(info.error, "q[9] is outside the declared register of 3") != NULL, what);
        TEST_ASSERT_MSG(qBDD_isFalse(circ), what);
        free_sim_info(&info);
    }
}

/* Valid inputs at the edges of the paths above:
 *   - a comment as the very last thing in the file, with no newline after it;
 *   - a loop with zero iterations, whose body (comments included) is skipped
 *     without being simulated - x q[0] in it must not happen;
 *   - the bit register declared first, and with a space before its ';'. */
static void test_valid_edges(void)
{
    static const char *QASM[] = {
        HDR "qubit[2] q;\nx q[1];\n// done",
        HDR "qubit[2] q;\nx q[1];\nfor int i in [1:-1:0] {\n// skipped\nx q[0];\n}\n",
        HDR "bit[2] c ;\nqubit[2] q;\nx q[1];\n",
    };
    TEST_SECTION("valid edge inputs: trailing comment, empty loop, bit register first");
    for (size_t i = 0; i < sizeof(QASM) / sizeof(QASM[0]); i++) {
        FILE *f = qasm_stream(QASM[i]);
        qBDD circ;
        sim_info_t info;
        sim_status_t got = run(f, false, &circ, &info);
        fclose(f);
        TEST_ASSERT_MSG(got == SIM_OK, info.error);
        if (got == SIM_OK) {
            TEST_ASSERT_NEAR(basis_prob(circ, "01"), 1.0, 1e-6);
            deleteCircuit(&circ);
        }
        free_sim_info(&info);
    }
}

#ifdef __GLIBC__
/*
 * A stream whose n-th seek fails, for the repositioning errors a pipe cannot
 * reach: on a pipe the very first one (the fseek in get_iters) fails already.
 * Each of fseek, fgetpos and fsetpos calls the seek function exactly once, so
 * for a single loop the 1st seek is get_iters' fseek, the 2nd the fgetpos that
 * marks the start of the body, and the 3rd the fsetpos that rewinds to it at
 * the first '}'.
 */
typedef struct {
    const char *buf;
    size_t len, pos;
    int seeks, fail_at;
} flaky_t;

static ssize_t flaky_read(void *cookie, char *out, size_t n)
{
    flaky_t *m = cookie;
    size_t k = m->len - m->pos;
    if (k > n)
        k = n;
    memcpy(out, m->buf + m->pos, k);
    m->pos += k;
    return (ssize_t)k;
}

static int flaky_seek(void *cookie, __off64_t *off, int whence)
{
    flaky_t *m = cookie;
    if (++m->seeks == m->fail_at)
        return -1;
    __off64_t base = whence == SEEK_SET ? 0 : whence == SEEK_CUR ? (__off64_t)m->pos : (__off64_t)m->len;
    if (base + *off < 0 || base + *off > (__off64_t)m->len)
        return -1;
    m->pos = (size_t)(base + *off);
    *off = (__off64_t)m->pos;
    return 0;
}

static void test_failing_seeks(void)
{
    static const struct {
        int fail_at;
        bool symbolic;
        const char *msg;
    } STEPS[] = {
        { 2, false, "mark the start of a loop" },
        { 3, false, "start the next iteration" },
        { 3, true,  "during symbolic simulation" },
    };
    /* The body needs more than one symbolic pass (the h on q[1] makes the
     * first refinement change the variable mapping), so the symbolic run
     * rewinds too. */
    const char *text = HDR "qubit[2] q;\nh q[0];\nfor int i in [1:3] {\nh q[1];\ncx q[1], q[0];\n}\n";
    TEST_SECTION("stream repositioning fails: SIM_ERR_STREAM");
    for (size_t i = 0; i < sizeof(STEPS) / sizeof(STEPS[0]); i++) {
        flaky_t m = { text, strlen(text), 0, 0, STEPS[i].fail_at };
        FILE *f = fopencookie(&m, "r", (cookie_io_functions_t){ .read = flaky_read, .seek = flaky_seek });
        qBDD circ;
        sim_info_t info;
        sim_status_t got = run(f, STEPS[i].symbolic, &circ, &info);
        fclose(f);
        char what[512];
        snprintf(what, sizeof(what), "seek %d fails%s: status %d, error \"%s\"", STEPS[i].fail_at,
                 STEPS[i].symbolic ? " (symbolic)" : "", (int)got, info.error);
        TEST_ASSERT_MSG(got == SIM_ERR_STREAM, what);
        TEST_ASSERT_MSG(strstr(info.error, STEPS[i].msg) != NULL, what);
        TEST_ASSERT_MSG(qBDD_isFalse(circ), what);
        free_sim_info(&info);
    }
}
#endif

/* Loops rewind the stream, which a pipe cannot do. That is an error of its
 * own, distinct from a malformed circuit. */
static void test_unseekable_stream(void)
{
    TEST_SECTION("loop on an unseekable stream: SIM_ERR_STREAM");
    int fds[2];
    if (pipe(fds) != 0) {
        perror("pipe");
        exit(2);
    }
    const char *text = HDR "qubit[2] q;\nfor int i in [1:2] {\nh q[0];\n}\n";
    /* Fits in the pipe buffer, so the write cannot block. */
    if (write(fds[1], text, strlen(text)) != (ssize_t)strlen(text)) {
        perror("write");
        exit(2);
    }
    close(fds[1]);
    FILE *f = fdopen(fds[0], "r");
    qBDD circ;
    sim_info_t info;
    sim_status_t got = run(f, false, &circ, &info);
    fclose(f);
    TEST_ASSERT_MSG(got == SIM_ERR_STREAM, info.error);
    TEST_ASSERT(strstr(info.error, "cannot be simulated from stdin") != NULL);
    TEST_ASSERT(qBDD_isFalse(circ));
    free_sim_info(&info);
}

/* After all of the above, in the same package, valid circuits still simulate
 * correctly - classic and symbolic, loops included. */
static void test_valid_after_errors(void)
{
    static const char *QASM =
        HDR "qubit[3] q;\nh q[0];\n"
            "for int i in [1:5] {\nx q[1];\ncx q[1], q[2];\n}\n"
            "for int j in [1:2] {\nh q[0];\n}\n";
    /* x on q[1] five times leaves it at 1, and so is cx onto q[2] (odd
     * count). Two h on q[0] undo each other, after the first one: q[0] is in
     * (|0> + |1>)/sqrt(2). */
    for (int symbolic = 0; symbolic <= 1; symbolic++) {
        TEST_SECTION(symbolic ? "valid circuit after the errors: symbolic"
                              : "valid circuit after the errors: classic");
        FILE *f = qasm_stream(QASM);
        qBDD circ;
        sim_info_t info;
        sim_status_t got = run(f, symbolic, &circ, &info);
        fclose(f);
        TEST_ASSERT_MSG(got == SIM_OK, info.error);
        TEST_ASSERT(info.error[0] == '\0');
        TEST_ASSERT(info.n_qubits == 3);
        TEST_ASSERT(info.n_loops == 2);
        if (got == SIM_OK) {
            TEST_ASSERT_NEAR(qBDD_total_prob(circ, 3), 1.0, 1e-6);
            TEST_ASSERT_NEAR(basis_prob(circ, "011"), 0.5, 1e-6);
            TEST_ASSERT_NEAR(basis_prob(circ, "111"), 0.5, 1e-6);
            TEST_ASSERT_NEAR(basis_prob(circ, "000"), 0.0, 1e-6);
            deleteCircuit(&circ);
        }
        free_sim_info(&info);
    }
}

int main(void)
{
    printf("MEDUSA sim_file error tests\n");
    initPackage(0, 0, 0);
    test_silence_gbc();
    init_symb_backend();

    test_error_cases();
    test_unseekable_stream();
    test_gate_operands();
    test_valid_edges();
#ifdef __GLIBC__
    test_failing_seeks();
#endif
    test_valid_after_errors();

    freePackage();
    return test_report("test_sim_errors");
}
