/**
 * @file sim.h
 * @brief Module performing quantum circuit simulation
 */

#include <stdbool.h>
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
 */
sim_status_t sim_file(FILE *in, qBDD *circ, const sim_flags_t *flags, sim_info_t *info);

/**
 * Measures all bits in the given array (compatible only with measurement at the end of the circuit)
 * 
 * @param samples the total number of samples
 * 
 * @param output stream for the output of results
 * 
 * @param circ MTBDD representation of the end circuit state
 * 
 * @param n number of qubits in the circuit
 * 
 * @param bits_to_measure array for storing the qubits that are to be measured
 * 
 */
void measure_all(unsigned long samples, FILE *output, qBDD circ, int n, int *bits_to_measure);

/**
 * Calculates the time elapsed between two timestamps in seconds
 */
static inline double get_time_el(struct timespec start, struct timespec fin)
{
    return fin.tv_sec - start.tv_sec + (fin.tv_nsec - start.tv_nsec) * 1.0e-9;
}

#endif
/* end of "sim.h" */
