#ifndef INTERFACE_GATE_OPS_H
#define INTERFACE_GATE_OPS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

#include "interface_bdd_core.h"
#include "interface_leaf.h"


/* *************************************************************************
 * Gate application and BDD traversal operations
 * ************************************************************************* */

/**
 * @brief Applies a gate operation to a target variable in a qBDD.
 * @param operand    The qBDD representing the current quantum state
 * @param targets    Array of target variable indices
 * @param controlNum Number of control variables in the operation
 * @param op         Gate operation function taking a target and two child qBDDs
 * @return           A new qBDD representing the state after the gate
 */
qBDD bdd_operation(qBDD operand, size_t* targets, size_t controlNum,
                   qBDD(*op)(size_t tgt, qBDD, qBDD));

/**
 * @brief Applies a gate operation to a target variable in a qBDD with an additional parameter.
 * @param operand    The qBDD representing the current quantum state
 * @param targets    Array of target variable indices
 * @param controlNum Number of control variables in the operation
 * @param op         Gate operation function taking a target and two child qBDDs
 * @param param      Extra parameter to pass to op
 * @return           A new qBDD representing the state after the gate
 */
qBDD bdd_operation_param(qBDD operand, size_t *targets, size_t controlNum,
                         qBDD (*op)(size_t, qBDD, qBDD, size_t), size_t param);

/**
 * @brief Applies a guarded gate operation to a target variable in a qBDD.
 * @param operand    The qBDD representing the current quantum state
 * @param targets    Array of target variable indices
 * @param controlNum Number of control variables in the operation
 * @param op         Gate operation function taking a target and a single qBDD
 * @return           A new qBDD representing the state after the operation
 */
qBDD bdd_operation_guarded(qBDD operand, size_t* targets, size_t controlNum,
                           qBDD(*op)(size_t tgt, qBDD));

/**
 * @brief Applies a binary leaf operation pointwise over two qBDDs.
 * @param l  The left qBDD operand
 * @param r  The right qBDD operand
 * @param op Leaf operation applied to corresponding terminal pairs
 * @return   A new qBDD with leaves computed by op
 */
qBDD binary_apply(qBDD l, qBDD r, LEAF_TYPE(*op)(LEAF_TYPE, LEAF_TYPE));

/**
 * @brief Applies a parameterised binary leaf operation pointwise over two qBDDs.
 * @param l     The left qBDD operand
 * @param r     The right qBDD operand
 * @param op    Leaf operation applied to corresponding terminal pairs with an additional parameter
 * @param param Additional parameter forwarded to op for each leaf pair
 * @return      A new qBDD with leaves computed by op
 */
qBDD binary_apply_param(qBDD l, qBDD r,
                        LEAF_TYPE(*op)(LEAF_TYPE, LEAF_TYPE, size_t), size_t param);

/**
 * @brief Applies a guarded binary operation over two qBDDs.
 * @param l  The left qBDD operand
 * @param r  The right qBDD operand
 * @param op Operation function taking two qBDDs and returning a qBDD
 * @return   A new qBDD produced by the operation
 */
qBDD binary_apply_guarded(qBDD l, qBDD r, qBDD(*op)(qBDD, qBDD));

/**
 * @brief Applies a parameterised guarded binary operation over two qBDDs.
 * @param l     The left qBDD operand
 * @param r     The right qBDD operand
 * @param op    Operation function taking two qBDDs and a size_t parameter
 * @param param Additional parameter forwarded to op
 * @return      A new qBDD produced by the operation
 */
qBDD binary_apply_guarded_param(qBDD l, qBDD r,
                                qBDD(*op)(qBDD, qBDD, size_t), size_t param);

/**
 * @brief Applies a unary leaf operation to every terminal of a qBDD.
 * @param l  The input qBDD
 * @param op Leaf operation applied to each terminal value
 * @return   A new qBDD with transformed leaf values
 */
qBDD unary_apply(qBDD l, LEAF_TYPE(*op)(LEAF_TYPE));

/**
 * @brief Applies a guarded unary operation with an argument to a qBDD.
 * @param l   The input qBDD
 * @param op  Operation function taking a qBDD and a size_t argument
 * @param arg Argument forwarded to op at each recursive step
 * @return    A new qBDD produced by the operation
 */
qBDD unary_apply_guarded(qBDD l, qBDD(*op)(qBDD, size_t), size_t arg);

/**
 * @brief Applies a parameterised unary operation to a qBDD.
 * @param l   The input qBDD
 * @param op  Operation function taking a LEAF_TYPE and a size_t argument
 * @param arg Argument forwarded to op for each leaf value
 * @return    A new qBDD with transformed leaf values
 */
qBDD unary_apply_param(qBDD l, LEAF_TYPE(*op)(LEAF_TYPE, size_t), size_t arg);


/* *************************************************************************
 * Operation result validation
 * ************************************************************************* */

/**
 * @brief Marks the most recent operation result as valid.
 */
void validateOperationResult();

/**
 * @brief Invalidates the cached operation result, forcing further traversal.
 */
void invalidateOperationResult();

/**
 * @brief Marks the most recent apply result as valid.
 */
void validateApplyResult();

/**
 * @brief Invalidates the cached apply result, forcing further traversal.
 */
void invalidateApplyResult();

/**
 * @brief Constructs a cube qBDD over a set of variables with given leaf values.
 * @param value     Integer encoding of the variable assignment
 * @param width     Number of variables in the cube
 * @param variables Array of qBDD variable nodes
 * @param leaf1     Leaf value assigned to the true path
 * @param leaf0     Leaf value assigned to the false path
 * @return          A qBDD representing the cube over the given variables
 */
qBDD cube(int value, int width, qBDD *variables, qBDD leaf1, qBDD leaf0);

#ifdef __cplusplus
}
#endif

#endif /* INTERFACE_GATE_OPS_H */
