/**
 * @file mtbdd.h
 * @brief Custom Sylvan MTBDD type and operations with algebraic complex number representation in leaves
 */

//#include <sylvan.h>
#include <gmp.h>
#include <stdbool.h>
#include "interface.h"
#ifndef MEDUSA_MTBDD_H
#define MEDUSA_MTBDD_H

/// Global variable for my custom leaf type id
extern uint32_t ltype_id;

/// Opid of mtbdd_apply_gate needed for operation caching
extern uint64_t mtbdd_apply_gate_id;

/// Opid of mtbdd_apply_cgate needed for operation caching
extern uint64_t mtbdd_apply_cgate_id;

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

/// Complex number coefficient k
extern coef_t c_k;

// ==========================================
// Measurement operations:

#define NOT_MEASURED_CHAR 'x'


/**
 * Computes the skip coefficient value for measure
 * 
 * @param start index of the current node
 * 
 * @param end index of next node present
 * 
 * @param target target qubit index
 * 
 * @param curr_state current state vector (determined by previous measurements)
 * 
 */
long get_coef(uint32_t start, uint32_t end, uint32_t target, char *curr_state);

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