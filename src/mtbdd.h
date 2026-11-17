/**
 * @file mtbdd.h
 * @brief qBDD types and operations with algebraic complex number representation in leaves
 */

#include <gmp.h>
#include <stdbool.h>
#include "interface.h"
#ifndef MEDUSA_MTBDD_H
#define MEDUSA_MTBDD_H

/// Type of single complex number coefficient
typedef leaf_primitive_t coef_t;

/// Complex number in algebraic representation
typedef struct cnum {
    /// a * 1
    coef_t a;
    /// b * ω
    coef_t b;
    /// c * ω²
    coef_t c;
    /// d * ω³
    coef_t d;
}cnum;

/// Type for the probability that a given qubit is 1
typedef leaf_scalar_t prob_t;

// ==========================================
// Measurement operations:

#define NOT_MEASURED_CHAR 'x'


/**
 * The factor for the variables first .. end-1, which a path in the diagram
 * skips: 2 for each one not yet measured (both of its values lead to the same
 * leaf, and both count), 1 for a measured one and for the target.
 *
 * @param first the first skipped variable
 * @param end the variable of the next node (or n for a terminal)
 * @param target the qubit being measured, counted as 1
 * @param qubit_state per qubit: '0', '1', or NOT_MEASURED_CHAR
 */
double get_coef(uint32_t first, uint32_t end, uint32_t target, const char *qubit_state);

/**
 * Computes the sum of all probabilities in the quantum state t.
 * For a perfectly normalized state this should equal 1.0.
 * The deviation from 1.0 quantifies the floating-point error
 * accumulated during simulation.
 *
 * @param t     The qBDD representing the quantum state
 * @param n     Total number of qubits
 * @return      Sum of all basis state probabilities
 */
prob_t qBDD_total_prob(qBDD t, int n);

qBDD my_mtbdd_b_xt_comp_mul_i(qBDD t, size_t xt);
qBDD my_mtbdd_b_xt_mul_i(qBDD t, size_t xt);
qBDD my_mtbdd_t_xt_comp_i(qBDD t, size_t xt);
qBDD my_mtbdd_t_xt_i(qBDD t, size_t xt);
#endif
/* end of "mtbdd.h" */