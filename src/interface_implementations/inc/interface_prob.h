#ifndef INTERFACE_PROB_H
#define INTERFACE_PROB_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "interface_bdd_core.h"


/* *************************************************************************
 * Probability computation
 * ************************************************************************* */

/**
 * @brief Computes the total probability represented by a qBDD.
 * @param  A qBDD representing a quantum state
 * @return The sum of squared absolute values of all leaf amplitudes
 */
leaf_scalar_t qBDD_calculateProb(qBDD);

/**
 * @brief Recursively computes the probability for a given partial assignment.
 * @param t          The qBDD representing the quantum state
 * @param xt         The current variable index being evaluated
 * @param curr_state The current partial assignment string
 * @param n          Total number of variables
 * @return           The probability contribution from paths matching curr_state
 */
leaf_scalar_t interface_prob_sum(qBDD t, uint32_t xt, char* curr_state, int n);

#ifdef __cplusplus
}
#endif

#endif /* INTERFACE_PROB_H */
