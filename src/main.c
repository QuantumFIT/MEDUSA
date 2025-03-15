#include <stdio.h>
#include <getopt.h>
#include <time.h>
#include <sys/resource.h>
#include "mtbdd.h"
#include "symb_utils.h"
#include "sim.h"
#include "mtbdd_out.h"
#include "error.h"

/// Name of the output .dot file
#define OUT_FILE "res"
/// Name of the output file with the large numbers
#define LONG_NUMS_OUT_FILE "res-vars.txt"
/// Name of the output file with update formulae
#define UPDATE_OUT_FILE "update.txt"
/// Initial size of array for the separate output of large numbers
#define LONG_NUMS_MAP_INIT_SIZE 5
#define HELP_MSG \
" Usage: MEDUSA [options] \n\
\n\
 Options with no argument:\n\
 --help,        -h          show this message\n\
 --info,        -i          measure the simulation runtime and peak memory usage\n\
 --symbolic,    -s          perform symbolic simulation if possible\n\
 --probability, -p          show probabilities of measuring the basis state in the result MTBDD instead\n\
 --update,      -u          the result will be the update formulae of the first loop encountered (symbolic simulation must be enabled)\n\
 \n\
 Options with a required argument:\n\
 --file,        -f          specify the input QASM file (default STDIN)\n\
 --nsamples,    -n          specify the number of samples used for measurement (default 1024)\n\
 \n\
 Options with an optional argument:\n\
 --measure,     -m          perform the measure operations encountered in the circuit, \n\
                         optional arg specifies the file for saving the measurement result (default STDOUT)\n\
 \n\
 The MTBDD result is saved in the file 'res.dot'.\n\
 The evaluation of variables for large numbers is saved (if necessary) in 'res-vars.txt'.\n"

/**
 * Returns the peak physical memory usage of the process in kilobytes. For an unsupported OS returns -1.
 */
static long get_peak_mem()
{
    long peak = 0;
    #if defined(__unix__) || defined(__APPLE__)
        struct rusage rs_usage;
        if (getrusage(RUSAGE_SELF, &rs_usage) == 0) {
            peak = rs_usage.ru_maxrss;
        }
    #else
        // Unknown OS
        peak = -1;
    #endif
    return peak;
}

int main(int argc, char *argv[])
{
    FILE *input = stdin;
    FILE *measure_output = stdout;
    bool opt_infile = false;
    bool opt_measure = false;
    bool opt_probability = false;
    sim_flags_t flags = { .opt_symb = false,
                          .opt_info = false,
                          .opt_update = false,
                          .upd_filename = UPDATE_OUT_FILE };
    unsigned long samples = 1024;
    
    int opt;
    static struct option long_options[] = {
        {"help",     no_argument,        0, 'h'},
        {"info",     no_argument,        0, 'i'},
        {"file",     required_argument,  0, 'f'},
        {"measure",  optional_argument,  0, 'm'},
        {"nsamples", required_argument,  0, 'n'},
        {"symbolic", no_argument,        0, 's'},
        {"probability", no_argument,     0, 'p'},
        {"update", no_argument,          0, 'u'},
        {0, 0, 0, 0}
    };
    char *endptr;
    while((opt = getopt_long(argc, argv, "hif:m::n:spu", long_options, 0)) != -1) {
        switch(opt) {
            case 'h':
                printf("%s\n", HELP_MSG);
                exit(0);
            case 'i':
                flags.opt_info = true;
                break; 
            case 'f':
                opt_infile = true;
                input = fopen(optarg, "r");
                if (input == NULL) {
                    error_exit("Invalid input file '%s'.\n", optarg);
                }
                break;
            case 'm':
                opt_measure = true;
                if (!optarg && optind < argc && argv[optind][0] != '-') {
                    optarg = argv[optind++];
                    measure_output = fopen(optarg, "w");
                    if (measure_output == NULL) {
                        error_exit("Invalid output file '%s'.\n", optarg);
                    }
                }
                break;
            case 'n':
                samples = strtoul(optarg, &endptr, 10);
                if (*endptr != '\0') {
                    error_exit("Invalid number of samples.\n");
                }
                break;
            case 's':
                flags.opt_symb = true;
                break;
            case 'p':
                opt_probability = true;
                break;
            case 'u':
                flags.opt_update = true;
                break;
            case '?':
                exit(1); // error msg already printed by getopt_long
        }
    }

    // Init:
    init_sylvan();
    init_my_leaf(opt_probability);
    if (flags.opt_symb) {
        init_sylvan_symb();
    }
    FILE *out = fopen(OUT_FILE".dot", "w");
    if (out == NULL) {
        error_exit("Cannot open the output file.\n");
    }
    MTBDD circ;
    sim_info_t info;
    init_sim_info(&info);

    // Sim:
    struct timespec t_start, t_finish;
    double t_el;
    clock_gettime(CLOCK_MONOTONIC, &t_start); // Start the timer

    bool sim_successful = sim_file(input, &circ, &flags, &info);

    if (opt_measure && info.is_measure) {
        measure_all(samples, measure_output, circ, info.n_qubits, info.bits_to_measure);
    }

    clock_gettime(CLOCK_MONOTONIC, &t_finish); // End the timer
    
    // Output:
    lnum_map_init(LONG_NUMS_MAP_INIT_SIZE);
    mtbdd_fprintdot(out, circ);
    // Check if there are any large numbers outputted only as variables in the .dot file
    if (!lnum_map_is_empty()) {
        FILE *lnums_out = fopen(LONG_NUMS_OUT_FILE, "w");
        if (lnums_out == NULL) {
            error_exit("Cannot open the output file for the separate output for large numbers.\n");
        }
        lnum_map_print(lnums_out);
        fclose(lnums_out);
    }
    lnum_map_clear();

    t_el = get_time_el(t_start, t_finish);
    if (flags.opt_info) {
        printf("Time=%.3gs\n", t_el);
        #if defined(__unix__) || defined(__APPLE__)
            printf("Peak Memory Usage=%ldkB\n", get_peak_mem());
        #else
            printf("Peak Memory Usage not supported for this OS.\n");
        #endif
        if (info.n_loops > 0) {
            printf("\nLoop time stats:\n");
            for (size_t i = 0; i < info.n_loops; i++) {
                printf("  Loop[%ld] - Total time=%.3gs", i, info.t_el_loop[i]);
                if (flags.opt_symb) {
                    printf(", Eval time=%.3gs", info.t_el_eval[i]);
                }
                printf("\n");
            }
        }
    }

    // Finish:
    if (sim_successful) {
        circuit_delete(&circ);
    }
    stop_sylvan();
    fclose(out);
    if (opt_infile) {
        fclose(input);
    }

    return 0;
}