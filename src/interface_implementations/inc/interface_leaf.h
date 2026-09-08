#ifndef INTERFACE_LEAF_H
#define INTERFACE_LEAF_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <gmp.h>

#include "interface_bdd_core.h"

typedef struct PhaseParam { double c, s; } PhaseParam;

typedef struct {
    leaf_primitive_t c;  /* cos(theta/2) */
    leaf_primitive_t s;  /* sin(theta/2) */
} rotation_param_t;


/* *************************************************************************
 * Leaf arithmetic operations
 * ************************************************************************* */

/**
 * @brief Computes the -leaf.
 * @param a The input leaf
 * @return  Sign inverted leaf value
 */
LEAF_TYPE invertLeaf(LEAF_TYPE a);

/**
 * @brief Multiplies a leaf value by -i.
 * @param a The input leaf
 * @return  A new leaf holding -i times a
 */
LEAF_TYPE negI_mul(LEAF_TYPE a);

/**
 * @brief Adds two leaf values.
 * @param a The first operand
 * @param b The second operand
 * @return  A new leaf holding a plus b
 */
LEAF_TYPE addLeaf(LEAF_TYPE a, LEAF_TYPE b);

/**
 * @brief Subtracts one leaf value from another.
 * @param a The minuend
 * @param b The subtrahend
 * @return  A new leaf holding a minus b
 */
LEAF_TYPE subLeaf(LEAF_TYPE a, LEAF_TYPE b);

/**
 * @brief Multiplies two leaf values.
 * @param a The first operand
 * @param b The second operand
 * @return  A new leaf holding a times b
 */
LEAF_TYPE mulLeaf(LEAF_TYPE a, LEAF_TYPE b);

/**
 * @brief Divides one leaf value by another.
 * @param a The dividend
 * @param b The divisor
 * @return  A new leaf holding a divided by b
 */
LEAF_TYPE divLeaf(LEAF_TYPE a, LEAF_TYPE b);

/**
 * @brief Computes the square root of a leaf value.
 * @param a The input leaf
 * @return  A new leaf holding the square root of a
 */
LEAF_TYPE sqrtLeaf(LEAF_TYPE a);

/**
 * @brief Applies the first rotation to the leaf coefficient.
 * @param a The input leaf
 * @return  A new leaf with its coefficient rotated by the first rotation factor
 */
LEAF_TYPE rotateCoef1(LEAF_TYPE a);

/**
 * @brief Applies the second rotation to the leaf coefficient.
 * @param a The input leaf
 * @return  A new leaf with its coefficient rotated by the second rotation factor
 */
LEAF_TYPE rotateCoef2(LEAF_TYPE a);

/**
 * @brief Multiplies a leaf value by two.
 * @param l The input leaf
 * @return  A new leaf holding twice the value of l
 */
LEAF_TYPE times2Leaf(LEAF_TYPE l);

/**
 * @brief Multiplies a leaf value by a complex phase factor defined by the given angle.
 * @param t   The input leaf
 * @param arg The PhaseParam sine and cosine of the angle used to compute the phase factor
 * @return    A new leaf holding the value of t multiplied by the complex phase factor
 *
 * @see PhaseParam
 */
LEAF_TYPE mulPhaseLeaf(LEAF_TYPE t, size_t arg);


/* *************************************************************************
 * Leaf arithmetic operations with inverse square root of 2 factor
 * ************************************************************************* */

/**
 * @brief Adds two leaf values and multiplies the result by 1/sqrt(2).
 * @param a The first operand
 * @param b The second operand
 * @return  A new leaf holding (a plus b) times 1/sqrt(2)
 */
LEAF_TYPE addLeafS(LEAF_TYPE a, LEAF_TYPE b);

/**
 * @brief Subtracts one leaf value from another and multiplies the result by 1/sqrt(2).
 * @param a The minuend
 * @param b The subtrahend
 * @return  A new leaf holding (a minus b) times 1/sqrt(2)
 */
LEAF_TYPE subLeafS(LEAF_TYPE a, LEAF_TYPE b);

/**
 * @brief Multiplies two leaf values and scales the result by 1/sqrt(2).
 * @param a The first operand
 * @param b The second operand
 * @return  A new leaf holding (a times b) times 1/sqrt(2)
 */
LEAF_TYPE mulLeafS(LEAF_TYPE a, LEAF_TYPE b);

/**
 * @brief Divides one leaf value by another and scales the result by 1/sqrt(2).
 * @param a The dividend
 * @param b The divisor
 * @return  A new leaf holding (a divided by b) times 1/sqrt(2)
 */
LEAF_TYPE divLeafS(LEAF_TYPE a, LEAF_TYPE b);

/**
 * @brief Applies the first coefficient rotation and scales the result by 1/sqrt(2).
 * @param a The input leaf
 * @return  A new leaf with its coefficient rotated by the first factor and scaled by 1/sqrt(2)
 */
LEAF_TYPE rotateCoef1S(LEAF_TYPE a);

/**
 * @brief Applies the inverse of the first coefficient rotation and scales the result by 1/sqrt(2).
 * @param l The input leaf
 * @return  A new leaf with its coefficient rotated by the inverse of the first factor and scaled by 1/sqrt(2)
 */
LEAF_TYPE rotateCoef1S_inv(LEAF_TYPE l);

/**
 * @brief Applies the second coefficient rotation and scales the result by 1/sqrt(2).
 * @param a The input leaf
 * @return  A new leaf with its coefficient rotated by the second factor and scaled by 1/sqrt(2)
 */
LEAF_TYPE rotateCoef2S(LEAF_TYPE a);

/**
 * @brief Multiplies a leaf value by two and scales the result by 1/sqrt(2).
 * @param l The input leaf
 * @return  A new leaf holding twice the value of l scaled by 1/sqrt(2),
 *          equivalent to l times sqrt(2)
 */
LEAF_TYPE times2LeafS(LEAF_TYPE l);

/**
 * @brief Computes the low branch leaf for an Ry rotation gate.
 */
LEAF_TYPE ry_low_leaf(LEAF_TYPE low, LEAF_TYPE high, size_t param);

/**
 * @brief Computes the high branch leaf for an Ry rotation gate.
 */
LEAF_TYPE ry_high_leaf(LEAF_TYPE low, LEAF_TYPE high, size_t param);

/**
 * @brief Computes the low branch leaf for an Rx rotation gate.
 */
LEAF_TYPE rx_low_leaf(LEAF_TYPE low, LEAF_TYPE high, size_t param);

/**
 * @brief Computes the high branch leaf for an Rx rotation gate.
 */
LEAF_TYPE rx_high_leaf(LEAF_TYPE low, LEAF_TYPE high, size_t param);

/**
 * @brief Computes the low branch leaf for an Rz rotation gate.
 */
LEAF_TYPE rz_low_leaf(LEAF_TYPE l, size_t param);

/**
 * @brief Computes the high branch leaf for an Rz rotation gate.
 */
LEAF_TYPE rz_high_leaf(LEAF_TYPE l, size_t param);


/* *************************************************************************
 * Primitive leaf backend interface
 * ************************************************************************* */

/**
 * @brief Hashes the raw data of a primitive leaf node.
 * @param data The primitive leaf value to hash
 * @return     A 64-bit hash of the leaf data
 */
uint64_t hash_comb_generic(leaf_primitive_t data);

/**
 * @brief Compares two primitive leaf values for ordering.
 * @param a The first operand
 * @param b The second operand
 * @return  Negative if a < b, zero if equal, positive if a > b
 */
int cmp_generic(leaf_primitive_t a, leaf_primitive_t b);

/**
 * @brief Initialises dst and copies the value of src into it.
 * @param dst The destination primitive to initialise
 * @param src The source primitive to copy from
 */
void init_set_generic(leaf_primitive_t dst, leaf_primitive_t src);

/**
 * @brief Initialises a primitive leaf to zero without setting a value.
 * @param x The primitive leaf to initialise
 */
void init_generic(leaf_primitive_t x);

/**
 * @brief Releases all resources held by a primitive leaf.
 * @param x The primitive leaf to clear
 */
void clear_generic(leaf_primitive_t x);

/**
 * @brief Multiplies a primitive leaf by an unsigned integer.
 * @param r The destination primitive to store the result
 * @param x The input primitive operand
 * @param c The unsigned integer multiplier
 */
void mul_ui_generic(leaf_primitive_t r, leaf_primitive_t x, unsigned long c);

/**
 * @brief Negates a primitive leaf value.
 * @param r The destination primitive to store the result
 * @param x The input primitive to negate
 */
void neg_generic(leaf_primitive_t r, leaf_primitive_t x);

/**
 * @brief Adds two primitive leaf values.
 * @param r The destination primitive to store the result
 * @param a The first operand
 * @param b The second operand
 */
void add_generic(leaf_primitive_t r, leaf_primitive_t a, leaf_primitive_t b);

/**
 * @brief Initialises a primitive leaf and sets it to an unsigned integer value.
 * @param x The primitive leaf to initialise
 * @param v The unsigned integer value to assign
 */
void init_set_ui_generic(leaf_primitive_t x, unsigned long v);

/**
 * @brief Sets a primitive leaf to an unsigned integer value without initialisation.
 * @param dst The destination primitive, already initialised
 * @param v   The unsigned integer value to assign
 */
void set_ui_generic(leaf_primitive_t dst, unsigned long v);

/**
 * @brief Returns the sign of a primitive leaf value.
 * @param x The primitive leaf to query
 * @return  -1 if x is negative, 0 if zero, 1 if positive
 */
int sgn_generic(leaf_primitive_t x);

/**
 * @brief Sets a primitive leaf to the value of a double.
 * @param x The destination primitive
 * @param v The double value to assign
 */
void set_d_generic(leaf_primitive_t x, double v);

/**
 * @brief Subtracts one primitive leaf from another.
 * @param r The destination primitive to store the result
 * @param a The minuend
 * @param b The subtrahend
 */
void sub_generic(leaf_primitive_t r, leaf_primitive_t a, leaf_primitive_t b);

/**
 * @brief Divides one primitive leaf by another.
 * @param r The destination primitive to store the result
 * @param a The dividend
 * @param b The divisor
 */
void div_generic(leaf_primitive_t r, leaf_primitive_t a, leaf_primitive_t b);

/**
 * @brief Multiplies a primitive leaf by a double scalar.
 * @param r The destination primitive to store the result
 * @param x The input primitive operand
 * @param s The double scalar multiplier
 */
void mul_d_generic(leaf_primitive_t r, leaf_primitive_t x, double s);

/**
 * @brief Multiplies a primitive leaf by 1/sqrt(2).
 * @param r The destination primitive to store the result
 * @param x The input primitive operand
 */
void mul_sqrt2inv_generic(leaf_primitive_t r, leaf_primitive_t x);

/**
 * @brief Converts a primitive leaf to its nearest double representation.
 * @param x The primitive leaf to convert
 * @return  The double approximation of x
 */
double to_double_generic(leaf_primitive_t x);

/**
 * @brief Copies the value of src into dst without reinitialising dst.
 * @param dst The destination primitive, already initialised
 * @param src The source primitive to copy from
 */
void set_generic(leaf_primitive_t dst, leaf_primitive_t src);

/**
 * @brief Multiplies two primitive leaf values.
 * @param r The destination primitive to store the result
 * @param a The first operand
 * @param b The second operand
 */
void mul_generic(leaf_primitive_t r, leaf_primitive_t a, leaf_primitive_t b);

/**
 * @brief Multiplies a primitive leaf by a GMP integer scalar.
 * @param r The destination primitive to store the result
 * @param x The input primitive operand
 * @param s The GMP integer multiplier
 */
void mul_mpz_generic(leaf_primitive_t r, leaf_primitive_t x, mpz_t s);

/**
 * @brief Multiplies a primitive leaf by the inverse square root of 2 raised to a power.
 * @param r The destination primitive to store the result
 * @param x The input primitive operand
 * @param k The exponent of the inverse square root of 2 factor
 */
void mul_inv_sqrt2_pow_generic(leaf_primitive_t r, leaf_primitive_t x, mpz_t k);

/**
 * @brief Sets a primitive leaf to the inverse square root of 2 raised to the power k.
 * @param x The primitive leaf to set (must already be initialised)
 * @param k The exponent of the inverse square root of 2 factor
 *
 * @warning x must be initialised before calling this function.
 */
void inv_sqrt2_pow_generic(leaf_primitive_t x, mpz_t k);

/**
 * @brief Computes the hash of a raw terminal node for use in the node table.
 * @param q Pointer to the raw terminal leaf data
 * @return  An unsigned hash value for the terminal
 */
unsigned terminal_hash_generic(void* q);

/**
 * @brief Compares two raw terminal nodes for equality in the node table.
 * @param a Pointer to the first raw terminal
 * @param b Pointer to the second raw terminal
 * @return  Zero if the terminals are equal, non-zero otherwise
 */
int terminal_compare_generic(void* a, void* b);

/**
 * @brief Serialises a raw terminal leaf to a human-readable string.
 * @param ldata_raw    Pointer to the raw terminal leaf data
 * @param buddy_buf    Output buffer to write the string into
 * @param buddy_bufsize Size of the output buffer in bytes
 * @return             Pointer to the written string within buddy_buf
 */
char* terminal_to_str_generic(void* ldata_raw, char *buddy_buf, size_t buddy_bufsize);

/**
 * @brief Serialises a primitive leaf value to a human-readable string.
 * @param x       The primitive leaf to serialise
 * @param buf     Output buffer to write the string into
 * @param bufsize Size of the output buffer in bytes
 */
void to_str_generic(leaf_primitive_t x, char *buf, size_t bufsize);

#ifdef __cplusplus
}
#endif

#endif /* INTERFACE_LEAF_H */
