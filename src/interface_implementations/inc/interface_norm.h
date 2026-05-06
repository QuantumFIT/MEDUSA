#ifndef INTERFACE_NORM_H
#define INTERFACE_NORM_H

#ifdef __cplusplus
#include <gmp.h>
extern "C" {
#endif


/* *************************************************************************
 * Inverse square root coefficient management
 * ************************************************************************* */

/**
 * @brief Increments the global inverse square root coefficient by one step.
 */
void incInvSqrtCoeff();

/**
 * @brief Increments the symbolic inverse square root coefficient by one step.
 */
void incInvSqrtCoeffSymb();

/**
 * @brief Initialises the symbolic inverse square root coefficient to its base value.
 */
void initInvSqrtCoeffSymb();

/**
 * @brief Resets and frees the normal inverse square root coefficient.
 */
void clearInvSqrtCoeffNormal();

/**
 * @brief Resets and frees the symbolic inverse square root coefficient.
 */
void clearInvSqrtCoeffSymb();

/**
 * @brief Multiplies the symbolic inverse square root coefficient by an unsigned integer.
 * @param mul The multiplier to apply
 */
void mulInvSqrtSymbCoeff(unsigned long mul);

/**
 * @brief Adds the symbolic coefficient into the normal coefficient accumulator.
 */
void addInvSqrtCoeffs();

/**
 * @brief Resets the symbolic coefficient back to its initial state without freeing.
 */
void resetInvSqrtCoeffSymb();


#ifdef __cplusplus
}
#endif

#endif /* INTERFACE_NORM_H */
