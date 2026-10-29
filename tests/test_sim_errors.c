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

/** Probability of one basis state; bits[v] is qubit v. */
static double basis_prob(qBDD t, const char *bits)
{
    while (!qBDD_isTerminal(t) && !qBDD_isFalse(t)) {
        uint32_t v = (uint32_t)qBDD_getVar(t);
        t = (bits[v] == '1') ? qBDD_getHigh(t) : qBDD_getLow(t);
    }
    return qBDD_isFalse(t) ? 0.0 : (double)qBDD_calculateProb(t);
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
    test_valid_after_errors();

    freePackage();
    return test_report("test_sim_errors");
}
