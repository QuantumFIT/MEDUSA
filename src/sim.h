/**
 * @file sim.h
 * @brief Module performing quantum circuit simulation
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include "mtbdd.h"

#ifndef SIMULATOR_H
#define SIMULATOR_H

/// Flags to run the simulation with
typedef struct sim_flags {
    bool opt_symb;   /// If true, loops will be simulated symbolically
    bool opt_info;   /// If true, time statistics will be collected for loop simulation
} sim_flags_t;

/// Outcome of sim_file
typedef enum sim_status {
    SIM_OK = 0,          /// The circuit was simulated
    SIM_NO_CIRCUIT,      /// The input declares no qubit register, so there is nothing to simulate
    SIM_ERR_SYNTAX,      /// The input is not a valid circuit
    SIM_ERR_UNSUPPORTED, /// The input is valid, but uses something MEDUSA does not support
    SIM_ERR_STREAM,      /// The input cannot be repositioned, which loops need (e.g. stdin)
} sim_status_t;

/// Max. length of the error message in sim_info (includes '\0')
#define SIM_ERROR_LEN 256

/// Additional simulation info (set during the simulation)
typedef struct sim_info {
    int n_qubits;         /// Number of qubits in the circuit
    bool is_measure;      /// True if some measure operation is encountered
    int *bits_to_measure; /// Array for storing the qubits that are to be measured
    size_t n_loops;       /// Total number of loops in the circuit
    size_t t_len;         /// Lenght of both time arrays
    double *t_el_loop;    /// Array for the total elapsed time for each loop
    double *t_el_eval;    /// Array for the eval time for each loop (for symbolic sim only)
    sim_status_t status;  /// Outcome of sim_file, the same as its return value
    char error[SIM_ERROR_LEN]; /// For a SIM_ERR_* status: what went wrong, one line without '\n'
    size_t line;          /// For a SIM_ERR_* status: the input line (from 1) the error was found on
} sim_info_t;

/**
 * Initializes the structure for the data collected during the simulation
 */
void init_sim_info(sim_info_t *i);

/** Free arrays owned by sim_info_t (timing + measure map). */
void free_sim_info(sim_info_t *i);

/**
 * Parses a given QASM file and simulates this circuit
 * 
 * @param in input QASM file
 * 
 * @param circ MTBDD that will represent the final quantum state
 * 
 * @param flags Flags to run the simulation with
 * 
 * @param info Structure to store the simulation info to (needs to be initialized beforehand)
 * 
 * @return true if the circuit has been properly initialized and simulated
 * 
 */
/**
 * Simulates the OpenQASM circuit read from in.
 *
 * Invalid input does not end the process: sim_file returns a SIM_ERR_* status
 * and leaves the message in info->error. On any status other than SIM_OK, *circ
 * holds no circuit and everything sim_file allocated for the run is released,
 * except the arrays in info, which free_sim_info releases as on success.
 *
 * All four pointers must be non-NULL (asserted).
 */
sim_status_t sim_file(FILE *in, qBDD *circ, const sim_flags_t *flags, sim_info_t *info);

/**
 * Simulates the OpenQASM circuit in the string qasm, as sim_file does for a
 * stream (loops included: the string is read through a memory stream, which
 * can be rewound). An empty string is SIM_NO_CIRCUIT; a memory stream that
 * cannot be opened is SIM_ERR_STREAM.
 *
 * All four pointers must be non-NULL (asserted).
 */
sim_status_t sim_string(const char *qasm, qBDD *circ, const sim_flags_t *flags, sim_info_t *info);

/// Outcome of sim_loop_count
typedef enum sim_loop_count_result {
    SIM_LOOP_OK = 0,     /// *iters holds the count (0 for an empty range)
    SIM_LOOP_STEP_ZERO,  /// the step is 0
    SIM_LOOP_OVERFLOW,   /// end - start does not fit in a long long
} sim_loop_count_t;

/**
 * Number of iterations of the OpenQASM 3 range [start:step:end], which
 * includes both ends: (end - start) / step + 1 when step points from start
 * towards end (or start == end), 0 for a range that runs away from its end.
 * Exact for every representable range; the largest count is 2^63 + 1.
 * *iters is set only for SIM_LOOP_OK.
 */
sim_loop_count_t sim_loop_count(long long start, long long step, long long end, uint64_t *iters);

/// In sim_info_t.bits_to_measure: this qubit is not measured
#define Q_NOT_MEASURED -1

/// One outcome drawn by measure_all, and how often it was drawn
typedef struct sim_sample {
    char *bits;           /// The classical register, highest bit first ('0', '1'; 'x' for a bit nothing is measured into)
    unsigned long count;  /// How many of the samples gave this outcome
} sim_sample_t;

/// The outcomes drawn by measure_all, each once, in no particular order
typedef struct sim_samples {
    sim_sample_t *v;      /// The outcomes
    size_t n;             /// How many there are
} sim_samples_t;

/**
 * Samples the measurement of the qubits listed in bits_to_measure (at the end
 * of the circuit) and fills out with the outcomes and their counts. The
 * random numbers come from MEDUSA's own generator, started from seed, so the
 * same seed gives the same samples on every platform. Nothing is printed;
 * print_samples does that. Release out with free_samples.
 *
 * @param samples number of samples to draw
 * @param seed seed of the random number generator
 * @param circ the final state
 * @param n number of qubits (and classical bits)
 * @param bits_to_measure for each qubit, the classical bit it is measured into, or Q_NOT_MEASURED
 * @param out filled with the outcomes; its previous contents are not freed
 */
void measure_all(unsigned long samples, uint64_t seed, qBDD circ, int n,
                 const int *bits_to_measure, sim_samples_t *out);

/** Prints s as the CLI does ("Sampled results:", then one outcome per line). */
void print_samples(FILE *output, const sim_samples_t *s);

/** Releases what measure_all put in s, and empties it. */
void free_samples(sim_samples_t *s);

/**
 * Calculates the time elapsed between two timestamps in seconds
 */
static inline double get_time_el(struct timespec start, struct timespec fin)
{
    return fin.tv_sec - start.tv_sec + (fin.tv_nsec - start.tv_nsec) * 1.0e-9;
}

#endif
/* end of "sim.h" */
