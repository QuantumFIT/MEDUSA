#include <ctype.h>  // For isspace(), isdigit()
#include <string.h>
#include <math.h>
#include <errno.h>
#include <stdarg.h>
#include "sim.h"
#include "gates.h"
#include "gates_symb.h"
#include "mtbdd_symb_val.h"
#include "symb_utils.h"
#include "qparam.h"
#include "htab.h"
#include "error.h"
#include "interface.h"
#include <assert.h>
#include "norm_track.h"
#include "medusa_debug.h"

/// Max. supported length of the string with classical bit register identifier (includes '\0')
#define BIT_REG_ID_MAX_LEN (30+1)
/// Max. supported length of the string with the qasm command (set to identifier length because of OpenQASM 3 measurement syntax)
#define CMD_MAX_LEN BIT_REG_ID_MAX_LEN
/// Max. number of characters in a parsed number
#define NUM_MAX_LEN 25
/// Constant for no other possible end character for 'parse_num()'
#define NO_ALT_END -2
/// Constant for index into classical bit register used for measurement results if the qubit shouldn't be measured
#define Q_NOT_MEASURED -1
/// Eps for checking if probability is > 0 & < 1
#define EPSILON 0.001
/// Size increment for time arrays resize in sim_info
#define TIMES_RESIZE_COEF 10

void init_sim_info(sim_info_t *i)
{
    i->n_qubits = 0;
    i->is_measure = false;
    i->bits_to_measure = NULL;
    i->n_loops = 0;
    i->t_len = 0;
    i->t_el_loop = NULL;
    i->t_el_eval = NULL;
    i->status = SIM_OK;
    i->error[0] = '\0';
}

void free_sim_info(sim_info_t *i)
{
    free(i->bits_to_measure);
    i->bits_to_measure = NULL;
    free(i->t_el_loop);
    i->t_el_loop = NULL;
    free(i->t_el_eval);
    i->t_el_eval = NULL;
    i->t_len = 0;
    i->n_loops = 0;
}

/**
 * Resizes both the arrays used for time tracking to the new size (or allocates memory if empty)
 */
static void sim_info_times_addsize(sim_info_t *i, int inc)
{
    size_t size;
    if (i->t_len == 0) {
        size = (size_t)inc;
        i->t_el_loop = my_malloc(sizeof(double) * size);
        i->t_el_eval = my_malloc(sizeof(double) * size);
    }
    else {
        size = i->t_len + (size_t)inc;
        i->t_el_loop = my_realloc(i->t_el_loop, sizeof(double) * size);
        i->t_el_eval = my_realloc(i->t_el_eval, sizeof(double) * size);
    }
    i->t_len = size;
}

/**
 * Records why the simulation failed and returns false, so that a parse helper
 * can report an error with `return sim_error(...)`.
 *
 * Called once per run: every error stops the parse.
 */
__attribute__((format(printf, 3, 4)))
static bool sim_error(sim_info_t *info, sim_status_t status, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(info->error, sizeof(info->error), fmt, args);
    va_end(args);
    info->status = status;
    return false;
}

/**
 * Function for number parsing from the input file (reads the number from the input until the end character is encountered)
 * Checks for two possible end characters. If only one character should be checked agains, set alt_end to NO_ALT_END.
 */
static bool parse_num(FILE *in, char end, char alt_end, sim_info_t *info, long long *out)
{
    int c = fgetc(in);
    char num[NUM_MAX_LEN] = {0};

    // Skip leading whitespace
    while (isspace(c)) {
        c = fgetc(in);
    }

    // Load number to string
    while ( c != end && c != alt_end) {
        if (c == EOF) {
            return sim_error(info, SIM_ERR_SYNTAX, "Invalid format - reached an unexpected end of file when converting a number.");
        }
        else if (!isdigit(c) && c != '-') {
            // Check if isn't just trailing whitespace
            while (isspace(c)) {
                c = fgetc(in);
            }
            if (c == end || c == alt_end) {
                break;
            }
            else {
                return sim_error(info, SIM_ERR_SYNTAX, "Invalid format - not a valid number (a non-digit character '%c' encountered while parsing a number).", c);
            }
        }
        else if (strlen(num) + 1 < NUM_MAX_LEN) {
            int *temp = &c;
            strncat(num, (char*)temp, 1);
        }
        else {
            return sim_error(info, SIM_ERR_SYNTAX, "Invalid format - not a valid number (too many digits).");
        }
        c = fgetc(in);
    }

    // Convert to integer value
    char *ptr;
    errno = 0;
    *out = strtoll(num, &ptr, 10);
    if (num == ptr || errno != 0) {
        return sim_error(info, SIM_ERR_SYNTAX, "Invalid format - not a valid number.");
    }
    return true;
}

/**
 * Parse an OpenQASM-style angle expression (inner text of rx(...)/ry(...)/rz(...)).
 * Grammar (left-associative * and / only):
 *   expr := term (('*'|'/') term)*
 *   term := ['+'|'-'] (number | "pi")
 * Rejects leftovers after a successful parse (aside from whitespace).
 */
static bool parse_qasm_angle(const char *inner, double *out)
{
    const char *p = inner;
    double value;

    while (isspace((unsigned char)*p))
        p++;

    /* ---- first term ---- */
    {
        int sign = 1;
        double term;
        while (*p == '+' || *p == '-') {
            if (*p == '-')
                sign = -sign;
            p++;
            while (isspace((unsigned char)*p))
                p++;
        }
        if (strncasecmp(p, "pi", 2) == 0 &&
            !isalnum((unsigned char)p[2]) && p[2] != '_') {
            term = M_PI;
            p += 2;
        } else {
            char *end = NULL;
            errno = 0;
            term = strtod(p, &end);
            if (end == p || errno != 0)
                return false;
            p = end;
        }
        value = sign * term;
    }

    while (isspace((unsigned char)*p))
        p++;

    /* ---- (*|/) term)* ---- */
    while (*p == '*' || *p == '/') {
        char op = *p++;
        int sign = 1;
        double term;

        while (isspace((unsigned char)*p))
            p++;
        while (*p == '+' || *p == '-') {
            if (*p == '-')
                sign = -sign;
            p++;
            while (isspace((unsigned char)*p))
                p++;
        }
        if (strncasecmp(p, "pi", 2) == 0 &&
            !isalnum((unsigned char)p[2]) && p[2] != '_') {
            term = M_PI;
            p += 2;
        } else {
            char *end = NULL;
            errno = 0;
            term = strtod(p, &end);
            if (end == p || errno != 0)
                return false;
            p = end;
        }
        term *= sign;
        if (op == '*')
            value *= term;
        else {
            if (term == 0.0)
                return false;
            value /= term;
        }
        while (isspace((unsigned char)*p))
            p++;
    }

    if (*p != '\0')
        return false;
    *out = value;
    return true;
}

/**
 * Extract "..." from "rx(...)" / "ry(...)" / "rz(...)" into angle.
 */
static bool parse_rotation_cmd_angle(const char *cmd, double *angle)
{
    const char *open = strchr(cmd, '(');
    const char *close = strrchr(cmd, ')');
    char buf[CMD_MAX_LEN];
    size_t n;

    if (!open || !close || close <= open + 1)
        return false;
    n = (size_t)(close - open - 1);
    if (n >= sizeof(buf))
        return false;
    memcpy(buf, open + 1, n);
    buf[n] = '\0';
    return parse_qasm_angle(buf, angle);
}

/** 
 * Function for getting the next qubit index for the command on the given line.
 */
static bool get_q_num(FILE *in, sim_info_t *info, uint32_t *out)
{
    int c;
    long long n;

    while ((c = fgetc(in)) != '[') {
        if (c == EOF) {
            return sim_error(info, SIM_ERR_SYNTAX, "Invalid format - reached an unexpected end of file (expected a qubit index).");
        }
    }

    if (!parse_num(in, ']', NO_ALT_END, info, &n)) {
        return false;
    }
    if (n > UINT32_MAX || n < 0) {
        return sim_error(info, SIM_ERR_SYNTAX, "Invalid format - not a valid qubit identifier.");
    }

    *out = (uint32_t)n;
    return true;
}

/**
 * Qubit index for a gate operand, checked against the declared register.
 *
 * get_q_num alone only rejects indices that do not fit in a uint32_t, which
 * says nothing about whether the qubit exists. An index past the end of the
 * register used to be applied at a BDD variable level that was never
 * allocated: the walk in prob_sum_recurse and the dot output both read off
 * the end of a heap block, res.dot still looked well-formed, and the exit
 * status was 0. See issue #13.
 *
 * Declaration sites (creg/bit, qreg/qubit) read a register *size* rather than
 * an index and so keep calling get_q_num directly.
 */
static bool get_q_idx(FILE *in, sim_info_t *info, uint32_t *out)
{
    uint32_t q;
    if (!get_q_num(in, info, &q)) {
        return false;
    }

    /* No "register not declared yet" check here: gate parsing sits inside
     * `if (init)`, and the else branch already reports "Circuit not
     * initialized". A guard here would be unreachable. */
    if (q >= (uint32_t)info->n_qubits) {
        return sim_error(info, SIM_ERR_SYNTAX,
                         "Invalid format - qubit index q[%u] is outside the declared "
                         "register of %d qubit(s).", q, info->n_qubits);
    }

    *out = q;
    return true;
}

/**
 * Classical bit index for a measurement target, checked against the declared
 * bit register. Separate from get_q_idx so the diagnostic names the right
 * register: the two are required to be the same size, but a user who wrote
 * c[9] should not be told about qubits.
 */
static bool get_c_idx(FILE *in, sim_info_t *info, int n_bits, uint32_t *out)
{
    uint32_t c;
    if (!get_q_num(in, info, &c)) {
        return false;
    }

    /* Likewise no "bit register missing" check: the measure branch rejects a
     * NULL bits_to_measure before it reads either operand. */
    if (c >= (uint32_t)n_bits) {
        return sim_error(info, SIM_ERR_SYNTAX,
                         "Invalid format - bit index c[%u] is outside the declared "
                         "register of %d bit(s).", c, n_bits);
    }

    *out = c;
    return true;
}

/**
 * Reads the number of iterations into *iters, should be called when a for loop is encountered
 */
static bool get_iters(FILE *in, sim_info_t *info, uint64_t *iters)
{
    int c;
    long long start, end;
    long long step = 1;

    while ((c = fgetc(in)) != '[') {
        if (c == EOF) {
            return sim_error(info, SIM_ERR_SYNTAX, "Invalid format - reached an unexpected end of file (expected a number of loop iterations).");
        }
    }

    if (!parse_num(in, ':', NO_ALT_END, info, &start) ||
        !parse_num(in, ']', ':', info, &end)) {
        return false;
    }
    if (fseek(in, -1, SEEK_CUR) != 0) {
        return sim_error(info, SIM_ERR_STREAM, "Error has occured when parsing loop parameters (fseek error). Circuits with loops cannot be simulated from stdin.");
    }
    if ((c = fgetc(in)) == ':') {
        step = end;
        if (!parse_num(in, ']', NO_ALT_END, info, &end)) {
            return false;
        }
    }
    
    // Note: expects 64bit long long
    if (step == 0) {
        return sim_error(info, SIM_ERR_SYNTAX, "Invalid number of loop iterations - step must be non-zero.");
    }
    else if ((end == LLONG_MAX && start == LLONG_MIN) || (end == LLONG_MIN && start == LLONG_MAX)) {
        return sim_error(info, SIM_ERR_SYNTAX, "Invalid number of loop iterations - overflow detected.");
    }
    else if ((end + 1 - start) % step != 0) {
        return sim_error(info, SIM_ERR_SYNTAX, "Invalid number of loop iterations - not an integer.");
    }
    else if ((end < start && step > 0) || (end > start && step < 0)) {
        return sim_error(info, SIM_ERR_SYNTAX, "Invalid number of loop iterations.");
    }

    *iters = (uint64_t)((end + 1 - start) / step);
    return true;
}

/// skip_one_line_comments: there was a comment and it has been skipped
#define COMMENT_SKIPPED 1
/// skip_one_line_comments: there is no comment here
#define COMMENT_NONE 0
/// skip_one_line_comments: the input ended inside the comment
#define COMMENT_EOF (-1)
/// skip_one_line_comments: a '/' that does not start a comment (the error is in info)
#define COMMENT_INVALID (-2)

/**
 * Skips a one line comment (checks if the given char doesn't mark a start of a comment)
 * 
 * @return One of the COMMENT_* values above
 */
static int skip_one_line_comments(char c, FILE *in, sim_info_t *info)
{
    if (c == '/') {
        if ((c = fgetc(in)) == '/') {
            while ((c = fgetc(in)) != '\n') {
                if (c == EOF) {
                    return COMMENT_EOF;
                }
            }
            return COMMENT_SKIPPED;
        }
        else {
            sim_error(info, SIM_ERR_SYNTAX, "Invalid command, expected a one-line comment.");
            return COMMENT_INVALID;
        }
    }
    return COMMENT_NONE;
}

/// In sim_file: record the error, then leave through the clean-up at its end
#define FAIL(status, ...) do { sim_error(info, (status), __VA_ARGS__); goto fail; } while (0)
/// In sim_file: leave through the clean-up if a parse helper reported an error
#define TRY(call) do { if (!(call)) goto fail; } while (0)

sim_status_t sim_file(FILE *in, qBDD *circ, const sim_flags_t *flags, sim_info_t *info)
{
    int c;
    char cmd[CMD_MAX_LEN]; // initialized to 0s in the loop
    char bit_reg[BIT_REG_ID_MAX_LEN] = {0};
    bool init = false;
    int n_bits = 0;

    bool is_loop = false;
    fpos_t loop_start;
    mtbdd_symb_t symbc;
    uint64_t iters;
    struct timespec t_loop_start, t_loop_finish, t_eval_start;
    bool symexp_live = false; // symexp_htab_init has run and needs the clean-up

    /* A NULL here is a bug in the caller, not bad input, so it is an assert
     * rather than an error status. */
    assert(in != NULL);
    assert(circ != NULL);
    assert(flags != NULL);
    assert(info != NULL);

    info->status = SIM_OK;
    info->error[0] = '\0';
    while ((c = fgetc(in)) != EOF) {
        for (int i=0; i < CMD_MAX_LEN; i++) {
            cmd[i] = '\0';
        }

        while (isspace(c)) {
            c = fgetc(in);
        }

        if (c == EOF) {
            break;
        }

        // Skip one-line comments
        int comment_check = skip_one_line_comments(c, in, info);
        if (comment_check == COMMENT_EOF) {
            break;
        }
        else if (comment_check == COMMENT_SKIPPED) {
            continue;
        }
        else if (comment_check == COMMENT_INVALID) {
            goto fail;
        }

        // Load the command
        do {
            if (c == EOF) {
                FAIL(SIM_ERR_SYNTAX, "Invalid format - reached an unexpected end of file when loading a command.");
            }
            int len = strlen(cmd);
            if (len + 1 < CMD_MAX_LEN) {
                cmd[len] = (char) c;
            }
            else {
                FAIL(SIM_ERR_UNSUPPORTED, "Invalid command (command too long).");
            }
        } while (!isspace(c = fgetc(in)) && (c != '['));
        /* The loop above ends only on whitespace or '[': an EOF is reported
         * inside it, as an end of file while loading the command. */
        if (c == '[') {
            // get_q_num() needs to see '[' (needed because of OpenQASM 3 new syntax)
            ungetc(c, in);
        }

        // Identify the command
        if (strcmp(cmd, "OPENQASM") == 0) {}
        else if (strcmp(cmd, "include") == 0) {}
        else if ((strcmp(cmd, "creg") == 0) || (strcmp(cmd, "bit") == 0)) {
            if (n_bits != 0) {
                FAIL(SIM_ERR_UNSUPPORTED, "Multiple bit register definitions encountered - currently not supported.");
            }

            uint32_t n;
            TRY(get_q_num(in, info, &n));
            n_bits = (int)n;
            if (info->n_qubits != 0 && n != info->n_qubits) { // != 0 check because maybe it's not initialized yet
                FAIL(SIM_ERR_UNSUPPORTED, "Bit register size is different than the size of the qubit register - currently not supported.");
            }

            info->bits_to_measure = my_malloc(n * sizeof(int));
            for (int i=0; i < n; i++) {
                (info->bits_to_measure)[i] =  Q_NOT_MEASURED;
            }

            while (isspace(c = fgetc(in))) { }
            // Save the register name to detect measurements
            do {
                if (c == ';') {
                    break; // continue needs to be outside of the loop
                }
                else if (c == EOF) {
                    FAIL(SIM_ERR_SYNTAX, "Invalid format - reached an unexpected end of file (bit register name expected).");
                }
                int len = strlen(bit_reg);
                if (len + 1 < BIT_REG_ID_MAX_LEN) {
                    bit_reg[len] = (char) c;
                }
                else {
                    FAIL(SIM_ERR_UNSUPPORTED, "Invalid bit register identifier (identifier is too long, max supported length is %d).", BIT_REG_ID_MAX_LEN-1);
                }
            } while (!isspace(c = fgetc(in)));
            if (c == ';') {
                continue; // end of command
            }
        }
        else if ((strcmp(cmd, "qreg") == 0) || (strcmp(cmd, "qubit") == 0)) {
            if (init) {
                FAIL(SIM_ERR_UNSUPPORTED, "Multiple qubit register definitions encountered - currently not supported.");
            }

            uint32_t n;
            TRY(get_q_num(in, info, &n));
            info->n_qubits = (int)n;
            g_num_qubits = (int)n; // for norm tracking
            if (n_bits != 0 && n != n_bits) { // != 0 check because maybe it's not initialized yet
                FAIL(SIM_ERR_UNSUPPORTED, "Bit register size is different than the size of the qubit register - currently not supported.");
            }

            circuit_init_interface(circ, n); /* returns already protected root */
            init = true;
        }
        else if (init) {
            if (strcmp(cmd, "for") == 0) {
                if (is_loop) { // currently doesn't allow nested loops
                    FAIL(SIM_ERR_UNSUPPORTED, "Nested loops are not allowed, aborting.");
                }
                TRY(get_iters(in, info, &iters));
                if (iters == 0) {
                    // skip symbolic
                    while ((c = fgetc(in)) != '}') {
                        int skipped = (c == EOF)? COMMENT_EOF : skip_one_line_comments(c, in, info);
                        if (skipped == COMMENT_INVALID) {
                            goto fail;
                        }
                        if (skipped == COMMENT_EOF) {
                            FAIL(SIM_ERR_SYNTAX, "Invalid format - reached an unexpected end of file (there is an unfinished loop).");
                        }
                    }
                    continue;
                }
                while ((c = fgetc(in)) != '{') {
                    if (c == EOF) {
                        FAIL(SIM_ERR_SYNTAX, "Invalid format - reached an unexpected end of file at the start of a loop.");
                    }
                }
                is_loop = true;
                if (flags->opt_symb) {
                    if (!symexp_live) {
                        symexp_htab_init(1LL<<17);
                        symexp_live = true;
                    }
                    symb_init(circ, &symbc);
                    MEDUSA_DBG(.cat = MEDUSA_DBG_SYMB, .evt = "loop_start", .where = "sim_file",
                               .use_bdd = 1, .bdd = (int)*circ, .ref = medusa_dbg_bdd_ref((int)*circ),
                               .is_false = qBDD_isFalse(*circ), .leaves = qBDD_leafcount(*circ),
                               .use_n = 1, .n_qubits = info->n_qubits,
                               .use_iters = 1, .iters = iters,
                               .use_loop = 1, .loop_idx = (int)info->n_loops);
                }
                if (fgetpos(in, &loop_start) != 0) {
                    FAIL(SIM_ERR_STREAM, "Could not get the current position of the stream to mark the start of a loop.");
                }
                if (info->n_loops == info->t_len) {
                    sim_info_times_addsize(info, TIMES_RESIZE_COEF);
                }
                clock_gettime(CLOCK_MONOTONIC, &t_loop_start);
                continue; // ';' not expected
            }
            else if (strcmp(cmd, "}") == 0) {
                if (!is_loop) {
                    FAIL(SIM_ERR_SYNTAX, "Invalid loop syntax - reached an unexpected end of a loop.");
                }
                if (!flags->opt_symb) {
                    iters--;
                    if (!iters) {
                        is_loop = false;
                        clock_gettime(CLOCK_MONOTONIC, &t_loop_finish); 
                        // must be here because after if-else also the not finished loops appear
                        info->t_el_loop[info->n_loops] = get_time_el(t_loop_start, t_loop_finish);
                        info->n_loops++;
                    }
                    else { // next iteration
                        if (fsetpos(in, &loop_start) != 0) {
                            FAIL(SIM_ERR_STREAM, "Could not set a new position of the stream to start the next iteration.");
                        }
                    }
                }
                else {
                    rdata_t *rdata = rdata_create(symbc.vm); // vm will be directly updated during refine
                    if (symb_refine(&symbc, rdata)) {
                        // is final result
                        is_loop = false;
                        clock_gettime(CLOCK_MONOTONIC, &t_eval_start);
                        symb_eval(circ, &symbc, iters, rdata);
                        clock_gettime(CLOCK_MONOTONIC, &t_loop_finish);
                        info->t_el_loop[info->n_loops] = get_time_el(t_loop_start, t_loop_finish);
                        info->t_el_eval[info->n_loops] = get_time_el(t_eval_start, t_loop_finish);

                        {
                            double tp = (double)qBDD_total_prob(*circ, info->n_qubits);
                            MEDUSA_DBG(.cat = MEDUSA_DBG_NORM, .evt = "loop_done", .where = "sim_file",
                                       .use_bdd = 1, .bdd = (int)*circ,
                                       .ref = medusa_dbg_bdd_ref((int)*circ),
                                       .is_false = qBDD_isFalse(*circ),
                                       .leaves = qBDD_leafcount(*circ),
                                       .use_total = 1, .total = tp,
                                       .use_n = 1, .n_qubits = info->n_qubits,
                                       .use_iters = 1, .iters = iters,
                                       .use_loop = 1, .loop_idx = (int)info->n_loops);
                        }

                        info->n_loops++;

                        if (g_norm_track_enabled) {
                            char label[32];
                            snprintf(label, sizeof(label), "LOOP (%lu iters)", (unsigned long)iters);
                            norm_track_record(label, *circ, g_num_qubits);
                        }
                    }
                    else {
                        if (fsetpos(in, &loop_start) != 0) {
                            rdata_delete(rdata);
                            FAIL(SIM_ERR_STREAM, "Could not set a new position of the stream during symbolic simulation.");
                        }
                    }
                    rdata_delete(rdata);
                }
                continue; // ';' not expected
            }
            else if ((strcmp(cmd, "measure") == 0) || (strcmp(cmd, bit_reg) == 0)) {
                if (info->bits_to_measure == NULL) {
                    FAIL(SIM_ERR_SYNTAX, "Measuring into an uninitialized bit register.");
                }
                /* qt indexes bits_to_measure, so it is a qubit index and gets
                 * the qubit bound. ct is a *classical bit* index and gets the
                 * bit-register bound - the two registers are required to be
                 * the same size just above, so today these coincide, but
                 * checking ct against n_qubits would report "qubit index" for
                 * what the user wrote as a bit index. */
                uint32_t qt, ct;
                if ((strcmp(cmd, "measure") == 0)) {
                    TRY(get_q_idx(in, info, &qt));
                    TRY(get_c_idx(in, info, n_bits, &ct));
                }
                else {
                    TRY(get_c_idx(in, info, n_bits, &ct));
                    TRY(get_q_idx(in, info, &qt));
                }
                
                info->is_measure = true;
                (info->bits_to_measure)[qt] = ct;
            }
            else if (strcasecmp(cmd, "x") == 0) {
                uint32_t qt;
                TRY(get_q_idx(in, info, &qt));
                (flags->opt_symb && is_loop)? gate_symb_x(&symbc.val, qt) : gate_x(circ, qt);
            }
            else if (strcasecmp(cmd, "y") == 0) {
                uint32_t qt;
                TRY(get_q_idx(in, info, &qt));
                (flags->opt_symb && is_loop)? gate_symb_y(&symbc.val, qt) : gate_y(circ, qt);
            }
            else if (strcasecmp(cmd, "z") == 0) {
                uint32_t qt;
                TRY(get_q_idx(in, info, &qt));
                (flags->opt_symb && is_loop)? gate_symb_z(&symbc.val, qt) : gate_z(circ, qt);
            }
            else if (strcasecmp(cmd, "h") == 0) {
                uint32_t qt;
                TRY(get_q_idx(in, info, &qt));
                (flags->opt_symb && is_loop)? gate_symb_h(&symbc.val, qt) : gate_h(circ, qt);
            }
            else if (strcasecmp(cmd, "s") == 0) {
                uint32_t qt;
                TRY(get_q_idx(in, info, &qt));
                (flags->opt_symb && is_loop)? gate_symb_s(&symbc.val, qt) : gate_s(circ, qt);
            }
            else if (strcasecmp(cmd, "t") == 0) {
                uint32_t qt;
                TRY(get_q_idx(in, info, &qt));
                (flags->opt_symb && is_loop)? gate_symb_t(&symbc.val, qt) : gate_t(circ, qt);
            }
            else if (strcasecmp(cmd, "tdg") == 0) {
                uint32_t qt;
                TRY(get_q_idx(in, info, &qt));
                (flags->opt_symb && is_loop)? gate_symb_tdg(&symbc.val, qt) : gate_tdg(circ, qt);
            }
            else if (strcasecmp(cmd, "rx(pi/2)") == 0) {
                uint32_t qt;
                TRY(get_q_idx(in, info, &qt));
                (flags->opt_symb && is_loop)? gate_symb_rx_pihalf(&symbc.val, qt) : gate_rx_pihalf(circ, qt);
            }
            else if (strcasecmp(cmd, "ry(pi/2)") == 0) {
                uint32_t qt;
                TRY(get_q_idx(in, info, &qt));
                (flags->opt_symb && is_loop)? gate_symb_ry_pihalf(&symbc.val, qt) : gate_ry_pihalf(circ, qt);
            }
            else if (strncasecmp(cmd, "rx(", 3) == 0 && strcasecmp(cmd, "rx(pi/2)") != 0) {
                if (flags->opt_symb && is_loop) {
                    FAIL(SIM_ERR_UNSUPPORTED, "Arbitrary-angle rx is not supported inside symbolic loops "
                                              "(only rx(pi/2) has a symbolic form); command '%s'.", cmd);
                }
                double angle;
                if (!parse_rotation_cmd_angle(cmd, &angle)) {
                    FAIL(SIM_ERR_SYNTAX, "Invalid rx angle in command '%s'.", cmd);
                }
                uint32_t qt;
                TRY(get_q_idx(in, info, &qt));
                gate_rx(circ, qt, angle);
            }
            else if (strncasecmp(cmd, "ry(", 3) == 0 && strcasecmp(cmd, "ry(pi/2)") != 0) {
                if (flags->opt_symb && is_loop) {
                    FAIL(SIM_ERR_UNSUPPORTED, "Arbitrary-angle ry is not supported inside symbolic loops "
                                              "(only ry(pi/2) has a symbolic form); command '%s'.", cmd);
                }
                double angle;
                if (!parse_rotation_cmd_angle(cmd, &angle)) {
                    FAIL(SIM_ERR_SYNTAX, "Invalid ry angle in command '%s'.", cmd);
                }
                uint32_t qt;
                TRY(get_q_idx(in, info, &qt));
                gate_ry(circ, qt, angle);
            }
            else if (strncasecmp(cmd, "rz(", 3) == 0) {
                if (flags->opt_symb && is_loop) {
                    FAIL(SIM_ERR_UNSUPPORTED, "Arbitrary-angle rz is not supported inside symbolic loops; "
                                              "command '%s'.", cmd);
                }
                double angle;
                if (!parse_rotation_cmd_angle(cmd, &angle)) {
                    FAIL(SIM_ERR_SYNTAX, "Invalid rz angle in command '%s'.", cmd);
                }
                uint32_t qt;
                TRY(get_q_idx(in, info, &qt));
                gate_rz(circ, qt, angle);
            }
            else if (strcasecmp(cmd, "cx") == 0) {
                uint32_t qc;
                TRY(get_q_idx(in, info, &qc));
                uint32_t qt;
                TRY(get_q_idx(in, info, &qt));
                (flags->opt_symb && is_loop)? gate_symb_cnot(&symbc.val, qt, qc) : gate_cnot(circ, qt, qc);
            }
            else if (strcasecmp(cmd, "cz") == 0) {
                uint32_t qc;
                TRY(get_q_idx(in, info, &qc));
                uint32_t qt;
                TRY(get_q_idx(in, info, &qt));
                if (qc == qt) {
                    FAIL(SIM_ERR_SYNTAX, "Invalid 'cz' gate - the control and the target qubit must differ.");
                }
                if (qc > qt) { // can swap as it is only a controlled rotation
                    uint32_t temp = qt;
                    qt = qc;
                    qc = temp;
                }
                (flags->opt_symb && is_loop)? gate_symb_cz(&symbc.val, qt, qc) : gate_cz(circ, qt, qc);
            }
            else if (strcasecmp(cmd, "ccx") == 0) {
                uint32_t qc1;
                TRY(get_q_idx(in, info, &qc1));
                uint32_t qc2;
                TRY(get_q_idx(in, info, &qc2));
                uint32_t qt;
                TRY(get_q_idx(in, info, &qt));
                (flags->opt_symb && is_loop)? gate_symb_toffoli(&symbc.val, qt, qc1, qc2) : gate_toffoli(circ, qt, qc1, qc2);
            }
            else if (strcasecmp(cmd, "mcx") == 0) { // supports 2 and 3 control qubits
                qparam_list_t* qparams = qparam_list_create();
                
                // Read all control qubits and the target qubit and save it in qparams
                while(true) {
                    uint32_t q;
                    if (!get_q_idx(in, info, &q)) {
                        qparam_list_del(qparams);
                        goto fail;
                    }
                    qparam_list_insert_first(qparams, q);
                    c = fgetc(in);
                    while (isspace(c)) {
                        c = fgetc(in);
                    }

                    if (c == ',') {
                        continue; // additional qubit indices are present in the file
                    }
                    else if (c == ';') {
                        break; // all qubit parameters loaded
                    }
                    else {
                        qparam_list_del(qparams);
                        FAIL(SIM_ERR_SYNTAX, "Invalid 'mcx' gate syntax.");
                    }
                }

                (flags->opt_symb && is_loop)? gate_symb_mcx(&symbc.val, qparams) : gate_mcx(circ, qparams);
                
                qparam_list_del(qparams);
                continue; // ';' already encountered
            }
            else {
                FAIL(SIM_ERR_UNSUPPORTED, "Invalid command '%s'.", cmd);
            }
        }
        else {
            FAIL(SIM_ERR_SYNTAX, "Circuit not initialized.");
        }

        // Skip all remaining characters on the currently read line
        while ((c = fgetc(in)) != ';') {
            if (c == EOF) {
                FAIL(SIM_ERR_SYNTAX, "Invalid format - reached an unexpected end of file (expected ';' to end the current line).");
            }
        }
    } // while

    /* Falling out of the parse loop with a loop still open means the file
     * ended inside a `for` body. Before issue #13 this was silently accepted
     * and the loop dropped, so a truncated circuit simulated as though the
     * missing gates were never meant to be there - a wrong answer with no
     * indication. */
    if (is_loop) {
        FAIL(SIM_ERR_SYNTAX, "Invalid format - reached the end of the file inside a loop "
                             "(missing '}').");
    }

    if (symexp_live) {
        symexp_htab_clear();
        symexp_htab_delete();
    }
    if (!init) {
        info->status = SIM_NO_CIRCUIT;
    }
    return info->status;

fail:
    /* Release what this run set up, so that the caller can go on (and run
     * another circuit) after an error, as a library has to. */
    if (is_loop && flags->opt_symb) {
        symb_abort(&symbc);
    }
    if (symexp_live) {
        symexp_htab_clear();
        symexp_htab_delete();
    }
    if (init) {
        deleteCircuit(circ);
    }
    return info->status;
}

void measure_all(unsigned long samples, FILE *output, qBDD circ, int n, int *bits_to_measure)
{
    prob_t random;
    prob_t p_qt_is_one;
    prob_t norm_coef;
    char curr_state[n+1];
    curr_state[n] = '\0';
    int curr_ct;

    htab_t *state_table = htab_init(n*n*n-1);
    
    for (unsigned long i=0; i < samples; i++) {
        norm_coef = 1;
        for (int j=0; j < n; j++) {
            curr_state[j] = NOT_MEASURED_CHAR;
        }

        for (int j=0; j < n; j++) {
            curr_ct = bits_to_measure[j];
            if (curr_ct ==  Q_NOT_MEASURED) {
                continue;
            }

            p_qt_is_one = measure(&circ, j, curr_state, n) * norm_coef * norm_coef;
            assert(p_qt_is_one <= (1 + EPSILON) && p_qt_is_one >= (0 - EPSILON));
            p_qt_is_one = (p_qt_is_one >= 1)? 1 : ((p_qt_is_one <= 0)? 0 : p_qt_is_one); // round
            random = (prob_t)rand() / RAND_MAX;
            if (random <= p_qt_is_one) {
                curr_state[curr_ct] = '1';
                assert(p_qt_is_one != 0);
                norm_coef *= sqrt(1/p_qt_is_one);
            }
            else {
                curr_state[curr_ct] = '0';
                assert(p_qt_is_one != 1);
                norm_coef *= sqrt(1/(1-p_qt_is_one));
            }
        }
        htab_m_lookup_add(state_table, curr_state);
    }
    htab_m_print_all(state_table, output);
    htab_m_free(state_table);
}

/* end of "sim.c" */