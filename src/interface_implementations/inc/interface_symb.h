#ifndef INTERFACE_SYMB_H
#define INTERFACE_SYMB_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>

#include "interface_bdd_core.h"
#include "interface_leaf.h"
#include "symb_utils.h"


/* *************************************************************************
 * Leaf type identifiers
 * ************************************************************************* */

/**
 * @brief Returns the leaf type identifier for the symbolic map type.
 * @return Leaf type index for symbolic map leaves
 */
size_t qBDD_symbolicMapLType();

/**
 * @brief Returns the leaf type identifier for the symbolic value type.
 * @return Leaf type index for symbolic value leaves
 */
size_t qBDD_symbolicValLType();

/**
 * @brief Returns the leaf type identifier for the classical numeric type.
 * @return Leaf type index for classical leaves
 */
size_t qBDD_classicLType();


/* *************************************************************************
 * Symbolic terminal initialisation
 * ************************************************************************* */

/**
 * @brief Initialises the symbolic value terminal type for use in computations.
 */
void init_terminal_symb_val_i();

/**
 * @brief Initialises the symbolic map terminal type for use in computations.
 */
void init_terminal_symb_map_i();


/* *************************************************************************
 * Symbolic MTBDD leaf operations
 * ************************************************************************* */

/**
 * @brief Negates a symbolic leaf value.
 * @param t The input symbolic leaf
 * @return  A new leaf with the negated value
 */
LEAF_TYPE mtbdd_symb_neg_i(LEAF_TYPE t);

/**
 * @brief Applies the first coefficient rotation to a symbolic leaf.
 * @param t The input symbolic leaf
 * @return  A new leaf with its coefficient rotated by the first factor
 */
LEAF_TYPE mtbdd_symb_coef_rot1_i(LEAF_TYPE t);

/**
 * @brief Applies the inverse of the first coefficient rotation to a symbolic leaf.
 * @param t The input symbolic leaf
 * @return  A new leaf with its coefficient rotated by the inverse of the first factor
 */
LEAF_TYPE mtbdd_symb_coef_rot1_i_inv(LEAF_TYPE t);

/**
 * @brief Applies the second coefficient rotation to a symbolic leaf.
 * @param t The input symbolic leaf
 * @return  A new leaf with its coefficient rotated by the second factor
 */
LEAF_TYPE mtbdd_symb_coef_rot2_i(LEAF_TYPE t);

/**
 * @brief Adds two symbolic leaf values within the MTBDD framework.
 * @param a The first symbolic leaf
 * @param b The second symbolic leaf
 * @return  A new leaf holding a plus b
 */
LEAF_TYPE mtbdd_symb_plus_i(LEAF_TYPE a, LEAF_TYPE b);

/**
 * @brief Subtracts one symbolic leaf value from another within the MTBDD framework.
 * @param a The symbolic minuend
 * @param b The symbolic subtrahend
 * @return  A new leaf holding a minus b
 */
LEAF_TYPE mtbdd_symb_minus_i(LEAF_TYPE a, LEAF_TYPE b);

/**
 * @brief Multiplies a symbolic leaf by a raw integer constant.
 * @param t     The input symbolic leaf
 * @param c_raw Raw encoding of the integer constant
 * @return      A new leaf holding t times the constant
 */
LEAF_TYPE mtbdd_symb_times_c_i(LEAF_TYPE t, size_t c_raw);


/* *************************************************************************
 * Symbolic MTBDD structural operations
 * ************************************************************************* */

/**
 * @brief Multiplies a qBDD by a boolean variable xt.
 * @param t  The input qBDD
 * @param b  The boolean variable qBDD to multiply with
 * @return   A new qBDD representing the product
 */
qBDD mtbdd_b_xt_mul_i(qBDD t, qBDD b);

/**
 * @brief Creates a complement variable node for xt in a qBDD.
 * @param t  The input qBDD
 * @param xt Index of the variable to complement
 * @return   A new qBDD with the complemented xt node inserted
 */
qBDD t_xt_comp_create_i(qBDD t, size_t xt);

/**
 * @brief Creates a variable node for xt in a qBDD.
 * @param t  The input qBDD
 * @param xt Index of the variable node to create
 * @return   A new qBDD with the xt node inserted
 */
qBDD t_xt_create_i(qBDD t, size_t xt);

/**
 * @brief Refines a symbolic map using a value qBDD and reduction data.
 * @param map    The symbolic map qBDD to refine
 * @param val    The symbolic value qBDD to apply
 * @param rd_raw Raw encoding of the reduction data
 * @return       A new qBDD representing the refined symbolic map
 */
qBDD mtbdd_symb_refine_i(qBDD map, qBDD val, size_t rd_raw);

/**
 * @brief Converts an MTBDD to a reduced symbolic value using a given map.
 * @param t       The input MTBDD
 * @param raw_map Raw encoding of the symbolic map to apply
 * @return        A new qBDD with symbolic values reduced under the map
 */
qBDD mtbdd_map_to_symb_val_reduced_i(qBDD t, size_t raw_map);

/**
 * @brief Converts an MTBDD to a symbolic value using a given map.
 * @param t       The input MTBDD
 * @param raw_map Raw encoding of the symbolic map to apply
 * @return        A new qBDD with symbolic values computed from the map
 */
qBDD mtbdd_map_to_symb_val_i(qBDD t, size_t raw_map);

/**
 * @brief Converts an MTBDD to a symbolic map representation.
 * @param a     The input MTBDD
 * @param raw_m Raw encoding of the target map format
 * @return      A new qBDD representing the symbolic map form of a
 */
qBDD mtbdd_to_symb_map_i(qBDD a, size_t raw_m);

/**
 * @brief Converts a symbolic qBDD back to its standard MTBDD form.
 * @param t       The symbolic qBDD to convert
 * @param raw_map Raw encoding of the symbolic map used during conversion
 * @return        A new qBDD in standard MTBDD form
 */
qBDD mtbdd_from_symb_i(qBDD t, size_t raw_map);


/* *************************************************************************
 * Symbolic map terminal backend
 * ************************************************************************* */

/**
 * @brief Compares two raw symbolic map terminals for equality in the node table.
 * @param l_a Pointer to the first symbolic map terminal
 * @param l_b Pointer to the second symbolic map terminal
 * @return    Zero if equal, non-zero otherwise
 */
int terminal_symb_map_compare_generic(void* l_a, void* l_b);

/**
 * @brief Computes the hash of a raw symbolic map terminal for use in the node table.
 * @param l_a Pointer to the symbolic map terminal to hash
 * @return    An unsigned hash value for the terminal
 */
unsigned terminal_symb_map_hash_generic(void* l_a);

/**
 * @brief Compares two raw symbolic value terminals for equality in the node table.
 * @param l_a Pointer to the first symbolic value terminal
 * @param l_b Pointer to the second symbolic value terminal
 * @return    Zero if equal, non-zero otherwise
 */
int terminal_symb_val_compare_generic(void* l_a, void* l_b);

/**
 * @brief Computes the hash of a raw symbolic value terminal for use in the node table.
 * @param l_a Pointer to the symbolic value terminal to hash
 * @return    An unsigned hash value for the terminal
 */
unsigned terminal_symb_val_hash_generic(void* l_a);


/* *************************************************************************
 * Symbolic coefficient reduction
 * ************************************************************************* */

/**
 * @brief Tests whether a symbolic coefficient can be reduced under given reduction data.
 * @param symbc Pointer to the symbolic coefficient structure
 * @param rdata Pointer to the reduction data to test against
 * @return      True if the coefficient is reducible, false otherwise
 */
bool can_be_reduced(mtbdd_symb_t *symbc, rdata_t *rdata);

/**
 * @brief Initialises a symbolic circuit state.
 * @param circ  Pointer to the qBDD circuit to initialise
 * @param symbc Pointer to the symbolic coefficient structure to initialise
 */
void symb_init(qBDD *circ, mtbdd_symb_t *symbc);

#ifdef __cplusplus
}
#endif

#endif /* INTERFACE_SYMB_H */
