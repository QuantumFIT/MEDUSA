#include <ctype.h>  // For isspace(), isdigit()
#include <string.h>
#include <math.h>
#include <errno.h>
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
 * Function for number parsing from the input file (reads the number from the input until the end character is encountered)
 * Checks for two possible end characters. If only one character should be checked agains, set alt_end to NO_ALT_END.
 */
static long long parse_num(FILE *in, char end, char alt_end)
{
    int c = fgetc(in);
    char num[NUM_MAX_LEN] = {0};
    long long n;

    // Skip leading whitespace
    while (isspace(c)) {
        c = fgetc(in);
    }

    // Load number to string
    while ( c != end && c != alt_end) {
        if (c == EOF) {
            error_exit("Invalid format - reached an unexpected end of file when converting a number.\n");
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
                error_exit("Invalid format - not a valid number (a non-digit character '%c' encountered while parsing a number).\n", c);
            }
        }
        else if (strlen(num) + 1 < NUM_MAX_LEN) {
            int *temp = &c;
            strncat(num, (char*)temp, 1);
        }
        else {
            error_exit("Invalid format - not a valid number (too many digits).\n");
        }
        c = fgetc(in);
    }

    // Convert to integer value
    char *ptr;
    errno = 0;
    n = strtoll(num, &ptr, 10);
    if (num == ptr || errno != 0) {
        error_exit("Invalid format - not a valid number.\n");
    }
    return n;
}

/** 
 * Function for getting the next qubit index for the command on the given line.
 */
static uint32_t get_q_num(FILE *in)
{
    int c;
    long long n;

    while ((c = fgetc(in)) != '[') {
        if (c == EOF) {
            error_exit("Invalid format - reached an unexpected end of file (expected a qubit index).\n");
        }
    }

    n = parse_num(in, ']', NO_ALT_END);
    if (n > UINT32_MAX || n < 0) {
        error_exit("Invalid format - not a valid qubit identifier.\n");
    }

    return ((uint32_t)n);
}

/**
 * Returns the number of iterations, should be called when a for loop is encountered
 */
static uint64_t get_iters(FILE *in)
{
    int c;
    long long start, end;
    long long step = 1;
    fpos_t second_num_pos;
    uint64_t iters;

    while ((c = fgetc(in)) != '[') {
        if (c == EOF) {
            error_exit("Invalid format - reached an unexpected end of file (expected a number of loop iterations).\n");
        }
    }

    start = parse_num(in, ':', NO_ALT_END);
    end = parse_num(in, ']', ':');
    if (fseek(in, -1, SEEK_CUR) != 0) {
        error_exit("Error has occured when parsing loop parameters (fseek error). Circuits with loops cannot be simulated from stdin.\n");
    }
    if ((c = fgetc(in)) == ':') {
        step = end;
        end = parse_num(in, ']', NO_ALT_END);
    }
    
    // Note: expects 64bit long long
    if (step == 0) {
        error_exit("Invalid number of loop iterations - step must be non-zero.\n");
    }
    else if ((end == LLONG_MAX && start == LLONG_MIN) || (end == LLONG_MIN && start == LLONG_MAX)) {
        error_exit("Invalid number of loop iterations - overflow detected.\n");
    }
    else if ((end + 1 - start) % step != 0) {
        error_exit("Invalid number of loop iterations - not an integer.\n");
    }
    else if ((end < start && step > 0) || (end > start && step < 0)) {
        error_exit("Invalid number of loop iterations.\n");
    }

    iters = (end + 1 - start) / step;
    
    return ((uint64_t) iters);
}

/**
 * Skips a one line comment (checks if the given char doesn't mark a start of a comment)
 * 
 * @return Returns 1 if comment has been skipped, -1 if EOF was reached, 0 if there is no comment
 */
static int skip_one_line_comments(char c, FILE *in)
{
    if (c == '/') {
        if ((c = fgetc(in)) == '/') {
            while ((c = fgetc(in)) != '\n') {
                if (c == EOF) {
                    return -1;
                }
            }
            return 1;
        }
        else {
            error_exit("Invalid command, expected a one-line comment.\n");
        }
    }
    return 0;
}

bool sim_file(FILE *in, qBDD *circ, const sim_flags_t *flags, sim_info_t *info)
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
        int comment_check = skip_one_line_comments(c, in);
        if (comment_check == -1) {
            break;
        }
        else if (comment_check == 1) {
            continue;
        }

        // Load the command
        do {
            if (c == EOF) {
                error_exit("Invalid format - reached an unexpected end of file when loading a command.\n");
            }
            int len = strlen(cmd);
            if (len + 1 < CMD_MAX_LEN) {
                cmd[len] = (char) c;
            }
            else {
                error_exit("Invalid command (command too long).\n");
            }
        } while (!isspace(c = fgetc(in)) && (c != '['));
        if (c == '[') {
            // get_q_num() needs to see '[' (needed because of OpenQASM 3 new syntax)
            ungetc(c, in);
        }
        else if (c == EOF) {
            error_exit("Invalid format - reached an unexpected end of file immediately after a command.\n");
        }

        // Identify the command
        if (strcmp(cmd, "OPENQASM") == 0) {}
        else if (strcmp(cmd, "include") == 0) {}
        else if ((strcmp(cmd, "creg") == 0) || (strcmp(cmd, "bit") == 0)) {
            if (n_bits != 0) {
                error_exit("Multiple bit register definitions encountered - currently not supported.\n");
            }

            uint32_t n = get_q_num(in);
            n_bits = (int)n;
            if (info->n_qubits != 0 && n != info->n_qubits) { // != 0 check because maybe it's not initialized yet
                error_exit("Bit register size is different than the size of the qubit register - currently not supported.\n");
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
                    error_exit("Invalid format - reached an unexpected end of file (bit register name expected).\n");
                }
                int len = strlen(bit_reg);
                if (len + 1 < BIT_REG_ID_MAX_LEN) {
                    bit_reg[len] = (char) c;
                }
                else {
                    error_exit("Invalid bit register identifier (identifier is too long, max supported length is %d).\n", BIT_REG_ID_MAX_LEN-1);
                }
            } while (!isspace(c = fgetc(in)));
            if (c == ';') {
                continue; // end of command
            }
        }
        else if ((strcmp(cmd, "qreg") == 0) || (strcmp(cmd, "qubit") == 0)) {
            if (init) {
                error_exit("Multiple qubit register definitions encountered - currently not supported.\n");
            }

            uint32_t n = get_q_num(in);
            info->n_qubits = (int)n;
            g_num_qubits = (int)n; // for norm tracking
            if (n_bits != 0 && n != n_bits) { // != 0 check because maybe it's not initialized yet
                error_exit("Bit register size is different than the size of the qubit register - currently not supported.\n");
            }

            circuit_init_interface(circ, n);
            qBDD_protect(*circ);
            init = true;
        }
        else if (init) {
            if (strcmp(cmd, "for") == 0) {
                if (is_loop) { // currently doesn't allow nested loops
                    error_exit("Nested loops are not allowed, aborting.\n");
                }
                iters = get_iters(in);
                if (iters == 0) {
                    // skip symbolic
                    while ((c = fgetc(in)) != '}') {
                        if (c == EOF || skip_one_line_comments(c, in) == -1) {
                            error_exit("Invalid format - reached an unexpected end of file (there is an unfinished loop).\n");
                        }
                    }
                    continue;
                }
                while ((c = fgetc(in)) != '{') {
                    if (c == EOF) {
                        error_exit("Invalid format - reached an unexpected end of file at the start of a loop.\n");
                    }
                }
                is_loop = true;
                if (flags->opt_symb) {
                    if (info->n_loops == 0) {
                        symexp_htab_init(1LL<<17);
                    }
                    symb_init(circ, &symbc);
                }
                if (fgetpos(in, &loop_start) != 0) {
                    error_exit("Could not get the current position of the stream to mark the start of a loop.\n");
                }
                if (info->n_loops == info->t_len) {
                    sim_info_times_addsize(info, TIMES_RESIZE_COEF);
                }
                clock_gettime(CLOCK_MONOTONIC, &t_loop_start);
                continue; // ';' not expected
            }
            else if (strcmp(cmd, "}") == 0) {
                if (!is_loop) {
                    error_exit("Invalid loop syntax - reached an unexpected end of a loop.\n");
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
                            error_exit("Could not set a new position of the stream to start the next iteration.\n");
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
                        info->n_loops++;

                        if (g_norm_track_enabled) {
                            char label[32];
                            snprintf(label, sizeof(label), "LOOP (%lu iters)", (unsigned long)iters);
                            norm_track_record(label, *circ, g_num_qubits);
                        }
                    }
                    else {
                        if (fsetpos(in, &loop_start) != 0) {
                            error_exit("Could not set a new position of the stream during symbolic simulation.\n");
                        }
                    }
                    rdata_delete(rdata);
                }
                continue; // ';' not expected
            }
            else if ((strcmp(cmd, "measure") == 0) || (strcmp(cmd, bit_reg) == 0)) {
                if (info->bits_to_measure == NULL) {
                    error_exit("Measuring into an uninitialized bit register.\n");
                }
                uint32_t qt, ct;
                if ((strcmp(cmd, "measure") == 0)) {
                    qt = get_q_num(in);
                    ct = get_q_num(in);
                }
                else {
                    ct = get_q_num(in);
                    qt = get_q_num(in);
                }
                
                info->is_measure = true;
                (info->bits_to_measure)[qt] = ct;
            }
            else if (strcasecmp(cmd, "x") == 0) {
                uint32_t qt = get_q_num(in);
                (flags->opt_symb && is_loop)? gate_symb_x(&symbc.val, qt) : gate_x(circ, qt);
            }
            else if (strcasecmp(cmd, "y") == 0) {
                uint32_t qt = get_q_num(in);
                (flags->opt_symb && is_loop)? gate_symb_y(&symbc.val, qt) : gate_y(circ, qt);
            }
            else if (strcasecmp(cmd, "z") == 0) {
                uint32_t qt = get_q_num(in);
                (flags->opt_symb && is_loop)? gate_symb_z(&symbc.val, qt) : gate_z(circ, qt);
            }
            else if (strcasecmp(cmd, "h") == 0) {
                uint32_t qt = get_q_num(in);
                (flags->opt_symb && is_loop)? gate_symb_h(&symbc.val, qt) : gate_h(circ, qt);
            }
            else if (strcasecmp(cmd, "s") == 0) {
                uint32_t qt = get_q_num(in);
                (flags->opt_symb && is_loop)? gate_symb_s(&symbc.val, qt) : gate_s(circ, qt);
            }
            else if (strcasecmp(cmd, "t") == 0) {
                uint32_t qt = get_q_num(in);
                (flags->opt_symb && is_loop)? gate_symb_t(&symbc.val, qt) : gate_t(circ, qt);
            }
            else if (strcasecmp(cmd, "tdg") == 0) {
                uint32_t qt = get_q_num(in);
                (flags->opt_symb && is_loop)? gate_symb_tdg(&symbc.val, qt) : gate_tdg(circ, qt);
            }
            else if (strcasecmp(cmd, "rx(pi/2)") == 0) {
                uint32_t qt = get_q_num(in);
                (flags->opt_symb && is_loop)? gate_symb_rx_pihalf(&symbc.val, qt) : gate_rx_pihalf(circ, qt);
            }
            else if (strcasecmp(cmd, "ry(pi/2)") == 0) {
                uint32_t qt = get_q_num(in);
                (flags->opt_symb && is_loop)? gate_symb_ry_pihalf(&symbc.val, qt) : gate_ry_pihalf(circ, qt);
            }
            else if (strncasecmp(cmd, "rx(", 3) == 0 && strcasecmp(cmd, "rx(pi/2)") != 0) {
                double angle;
                if (sscanf(cmd + 3, "%lf", &angle) != 1) {
                    error_exit("Invalid rx angle in command '%s'.\n", cmd);
                }
                uint32_t qt = get_q_num(in);
                gate_rx(circ, qt, angle);
            }
            else if (strncasecmp(cmd, "ry(", 3) == 0 && strcasecmp(cmd, "ry(pi/2)") != 0) {
                double angle;
                if (sscanf(cmd + 3, "%lf", &angle) != 1) {
                    error_exit("Invalid ry angle in command '%s'.\n", cmd);
                }
                uint32_t qt = get_q_num(in);
                gate_ry(circ, qt, angle);
            }
            else if (strncasecmp(cmd, "rz(", 3) == 0) {
                double angle;
                if (sscanf(cmd + 3, "%lf", &angle) != 1) {
                    error_exit("Invalid rz angle in command '%s'.\n", cmd);
                }
                uint32_t qt = get_q_num(in);
                gate_rz(circ, qt, angle);
            }
            else if (strcasecmp(cmd, "cx") == 0) {
                uint32_t qc = get_q_num(in);
                uint32_t qt = get_q_num(in);
                (flags->opt_symb && is_loop)? gate_symb_cnot(&symbc.val, qt, qc) : gate_cnot(circ, qt, qc);
            }
            else if (strcasecmp(cmd, "cz") == 0) {
                uint32_t qc = get_q_num(in);
                uint32_t qt = get_q_num(in);
                if (qc > qt) { // can swap as it is only a controlled rotation
                    uint32_t temp = qt;
                    qt = qc;
                    qc = temp;
                }
                assert(qc != qt);
                (flags->opt_symb && is_loop)? gate_symb_cz(&symbc.val, qt, qc) : gate_cz(circ, qt, qc);
            }
            else if (strcasecmp(cmd, "ccx") == 0) {
                uint32_t qc1 = get_q_num(in);
                uint32_t qc2 = get_q_num(in);
                uint32_t qt = get_q_num(in);
                (flags->opt_symb && is_loop)? gate_symb_toffoli(&symbc.val, qt, qc1, qc2) : gate_toffoli(circ, qt, qc1, qc2);
            }
            else if (strcasecmp(cmd, "mcx") == 0) { // supports 2 and 3 control qubits
                qparam_list_t* qparams = qparam_list_create();
                
                // Read all control qubits and the target qubit and save it in qparams
                while(true) {
                    qparam_list_insert_first(qparams, get_q_num(in));
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
                        error_exit("Invalid 'mcx' gate syntax.\n");
                    }
                }

                (flags->opt_symb && is_loop)? gate_symb_mcx(&symbc.val, qparams) : gate_mcx(circ, qparams);
                
                qparam_list_del(qparams);
                continue; // ';' already encountered
            }
            else {
                error_exit("Invalid command '%s'.\n", cmd);
            }
        }
        else {
            error_exit("Circuit not initialized.\n");
        }

        // Skip all remaining characters on the currently read line
        while ((c = fgetc(in)) != ';') {
            if (c == EOF) {
                error_exit("Invalid format - reached an unexpected end of file (expected ';' to end the current line).\n");
            }
        }
    } // while

    if (flags->opt_symb && info->n_loops > 0) {
        symexp_htab_clear();
    }
    return init;
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