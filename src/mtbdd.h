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

/// Type for the probability that a given qubit is 1: the leaves' own scalar,
/// so measurement and norm tracking keep their precision.
typedef leaf_scalar_t prob_t;

/* Arithmetic on prob_t. SoftFloat's float128_t (leaf_f128_soft.h) is a struct
 * without C operators, so code that computes with probabilities goes through
 * these; everywhere else they are the plain operators and conversions, and
 * compile to what the operators did. PROB_SQRT_D is a square root in double,
 * as measure_all has always taken it. */
#ifdef LEAF_SCALAR_IS_SOFT
#define PROB_ZERO           LEAF_ZERO
#define PROB_ONE            LEAF_ONE
#define PROB_ADD(a, b)      f128_add((a), (b))
#define PROB_SUB(a, b)      f128_sub((a), (b))
#define PROB_MUL(a, b)      f128_mul((a), (b))
#define PROB_DIV(a, b)      f128_div((a), (b))
#define PROB_LE(a, b)       f128_le((a), (b))
#define PROB_EQ(a, b)       f128_eq((a), (b))
#define PROB_FROM_I(i)      i64_to_f128((int64_t)(i))
#define PROB_FROM_U(u)      ui64_to_f128((uint64_t)(u))
#define PROB_FROM_D(d)      leaf_s_from_d(d)
#define PROB_TO_D(p)        leaf_s_to_d(p)
#else
#define PROB_ZERO           ((prob_t)0)
#define PROB_ONE            ((prob_t)1)
#define PROB_ADD(a, b)      ((a) + (b))
#define PROB_SUB(a, b)      ((a) - (b))
#define PROB_MUL(a, b)      ((a) * (b))
#define PROB_DIV(a, b)      ((a) / (b))
#define PROB_LE(a, b)       ((a) <= (b))
#define PROB_EQ(a, b)       ((a) == (b))
#define PROB_FROM_I(i)      ((prob_t)(i))
#define PROB_FROM_U(u)      ((prob_t)(u))
#define PROB_FROM_D(d)      ((prob_t)(d))
#define PROB_TO_D(p)        ((double)(p))
#endif
#define PROB_SQRT_D(p)      PROB_FROM_D(sqrt(PROB_TO_D(p)))

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