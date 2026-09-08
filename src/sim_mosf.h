/**
 * @file sim_mosf.h
 * @brief Module performing quantum circuit simulation of MOSF circuits using tree traversal
 */

#include <stdbool.h>
#include <time.h>
#include "mtbdd.h"
#include "sim.h"

#ifndef SIM_MOSF_H
#define SIM_MOSF_H
#ifdef __cplusplus
extern "C" {
#endif
/**
 * Parses a given MOSF file and simulates this circuit
 * 
 * @param in input MOSF file
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
bool sim_mosf_file(FILE *in, qBDD *circ, const sim_flags_t *flags, sim_info_t *info);


#ifdef __cplusplus
}
#endif
#endif
/* end of "sim_mosf.h" */
