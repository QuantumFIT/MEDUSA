/**
 * @file mtbdd_symb_val.h
 * @brief Custom Sylvan MTBDD type and operations for symbolic variable values (symbolic expressions)
 */


#include <gmp.h>
#include "symexp.h"
#include "mtbdd.h"
#include "interface.h"

#ifndef MTBDD_SYMB_VAL_H
#define MTBDD_SYMB_VAL_H
#ifdef __cplusplus
extern "C" {
#endif
/// Global variable for my custom symbolic expression mtbdd leaf type id
extern uint32_t ltype_symb_expr_id;


/// Type for the coefficient k for symbolic representation
typedef mpz_t coefs_k_t;

/// Complex number coefficient k for symbolic representation
extern coefs_k_t cs_k;

/**
 * Sets symbolic coefficient k's value to 0
 */
void cs_k_reset();

/**
 * Converts the given symbolic map MTBDD to a symbolic value MTBDD
 * 
 * @param t a regular MTBDD
 * 
 * @param map array with the variable mapping to their values
 * 
 * @param reduce_zero if true, variables that have value 0 will produce 'mtbdd_false' leaves 
 *                    in symbolic value MTBDD (else the leaf contains the full symbolic expression as usual)
 * 
 */
qBDD my_mtbdd_map_to_symb_val_i(qBDD t, size_t map, bool reduce_zero);

/**
 * Converts the given symbolic MTBDD to a regular MTBDD according to the variable mapping
 * 
 * @param t a symbolic map MTBDD
 * 
 * @param map array with the variable mapping to their values
 * 
 */
 qBDD my_mtbdd_from_symb_i(qBDD t, size_t map);

// ==========================================
// Operations needed for gate representation:

/**
 * Computes a + b with symbolic MTBDDs
 * 
 * @param p_a pointer to a symbolic value MTBDD
 * 
 * @param p_b pointer to a symbolic value MTBDD
 * 
 */
qBDD my_mtbdd_symb_plus_i(qBDD a, qBDD b);

/**
 * Computes a - b with symbolic MTBDDs
 * 
 * @param p_a pointer to a symbolic value MTBDD
 * 
 * @param p_b pointer to a symbolic value MTBDD
 * 
 */
qBDD my_mtbdd_symb_minus_i(qBDD a, qBDD b);

/**
 * Computes c * t for a symbolic MTBDD
 * 
 * @param t symbolic value MTBDD
 * 
 * @param c unsigned integer (the multiplication coefficient)
 * 
 */
qBDD my_mtbdd_symb_times_c_i(qBDD t, size_t c);

/**
 * Computes -a for a symbolic MTBDD
 * 
 * @param t symbolic value MTBDD
 * 
 */
qBDD my_mtbdd_symb_neg_i(qBDD t);

/**
 * Computes t * ω a symbolic MTBDD (rotate coefficients)
 * 
 * @param t a symbolic value MTBDD
 * 
 */
qBDD my_mtbdd_symb_coef_rot1_i(qBDD t);

qBDD my_mtbdd_symb_coef_rot1_i_inv(qBDD t);

/**
 * Computes t * ω² a symbolic MTBDD (rotate coefficients twice)
 * 
 * @param t a symbolic value MTBDD
 * 
 */
qBDD my_mtbdd_symb_coef_rot2_i(qBDD t);

#ifdef __cplusplus
}
#endif
#endif
/* end of "mtbdd_symb_val.h" */