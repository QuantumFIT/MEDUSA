// #ifndef INTERFACE_H
// #define INTERFACE_H


// #ifdef __cplusplus
// #include <gmp.h>
// extern "C" {
// #endif

// #include <stdlib.h>
// #include <stddef.h>
// #include <stdio.h>
// #include <stdint.h>
// #include <stdbool.h>

// #include "symb_utils.h"

// typedef struct LEAF_TYPE_IMPL LEAF_TYPE_IMPL;

// #ifndef QBDD_TYPE_DEFINED
// # error "qBDD not defined, include a leaf type header before this file"
// typedef void* qBDD;
// #endif

// #ifndef LEAF_PRIMITIVE_DEFINED
// # error "leaf_primitive_t not defined, include a leaf type header before this file"
// typedef void* leaf_primitive_t;
// #endif

// typedef struct LEAF_TYPE {
//     LEAF_TYPE_IMPL* pImpl;
// } LEAF_TYPE;

// typedef struct PhaseParam { double c, s; } PhaseParam;

// typedef struct {
//     leaf_primitive_t c;  // cos(theta/2)
//     leaf_primitive_t s;  // sin(theta/2)
// } rotation_param_t;

// /* *************************************************************************
//  * BDD node construction and traversal
//  * ************************************************************************* */

// /**
//  * @brief Creates a new internal qBDD node.
//  * @param target Target variable number
//  * @param low    qBDD representing the low branch of the new node
//  * @param high   qBDD representing the high branch of the new node
//  * @return       A new qBDD node branching on the given target variable
//  */
// qBDD newqBDD(unsigned int target, qBDD low, qBDD high);

// /**
//  * @brief Protects a qBDD node from garbage collection.
//  * @param  A qBDD node to protect
//  * @return The same node with an incremented reference count
//  */
// qBDD qBDD_protect(qBDD);

// /**
//  * @brief Releases a previously protected qBDD node.
//  * @param  A qBDD node to unprotect
//  * @return The same node with a decremented reference count
//  */
// qBDD qBDD_unprotect(qBDD);

// /**
//  * @brief Returns the canonical false terminal node.
//  * @return The false terminal qBDD
//  */
// qBDD qBDD_false();

// /**
//  * @brief Returns the canonical true terminal node.
//  * @return The true terminal qBDD
//  */
// qBDD qBDD_true();

// /**
//  * @brief Tests whether a qBDD node is the false terminal.
//  * @param  A qBDD node to test
//  * @return Non-zero if the node is the false terminal
//  */
// int qBDD_isFalse(qBDD);

// /**
//  * @brief Tests whether a qBDD node is an internal decision node.
//  * @param  A qBDD node to test
//  * @return Non-zero if the node is an internal node
//  */
// int qBDD_isInternal(qBDD);

// /**
//  * @brief Tests whether a qBDD node is a terminal leaf node.
//  * @param  A qBDD node to test
//  * @return Non-zero if the node is a terminal node
//  */
// int qBDD_isTerminal(qBDD);

// /**
//  * @brief Returns the low child of an internal qBDD node.
//  * @param  An internal qBDD node
//  * @return The low branch child node
//  */
// qBDD qBDD_getLow(qBDD);

// /**
//  * @brief Returns the high child of an internal qBDD node.
//  * @param  An internal qBDD node
//  * @return The high branch child node
//  */
// qBDD qBDD_getHigh(qBDD);

// /**
//  * @brief Returns the variable index of an internal qBDD node.
//  * @param  An internal qBDD node
//  * @return The variable index this node branches on
//  */
// size_t qBDD_getVar(qBDD);

// /**
//  * @brief Retrieves the leaf value stored in a terminal qBDD node.
//  * @param  A terminal qBDD node
//  * @return The LEAF_TYPE value associated with the terminal
//  */
// LEAF_TYPE qBDD_getTerminalValue(qBDD);

// /**
//  * @brief Creates a new terminal qBDD node with the given type and value.
//  * @param  Leaf type identifier
//  * @param  Pointer to the raw leaf data
//  * @return A terminal qBDD node holding the given value
//  */
// qBDD qBDD_maketerminal(size_t, void*);

// /**
//  * @brief Counts the number of distinct leaf nodes in a qBDD.
//  * @param  A qBDD to count leaves in
//  * @return The number of terminal nodes reachable from the root
//  */
// int qBDD_leafcount(qBDD);

// /**
//  * @brief Returns the level of a qBDD node in the decision diagram.
//  * @param  A qBDD node to query
//  * @return The level of the node, where 0 is the root level and higher numbers are deeper in the diagram
//  */
// size_t qBDD_level(qBDD node);

// /* *************************************************************************
//  * Probability computation
//  * ************************************************************************* */

// /**
//  * @brief Computes the total probability represented by a qBDD.
//  * @param  A qBDD representing a quantum state
//  * @return The sum of squared absolute values of all leaf amplitudes
//  */
// leaf_scalar_t qBDD_calculateProb(qBDD);

// /**
//  * @brief Recursively computes the probability for a given partial assignment.
//  * @param t          The qBDD representing the quantum state
//  * @param xt         The current variable index being evaluated
//  * @param curr_state The current partial assignment string
//  * @param n          Total number of variables
//  * @return           The probability contribution from paths matching curr_state
//  */
// leaf_scalar_t interface_prob_sum(qBDD t, uint32_t xt, char* curr_state, int n);


// /* *************************************************************************
//  * Leaf type identifiers
//  * ************************************************************************* */

// /**
//  * @brief Returns the leaf type identifier for the symbolic map type.
//  * @return Leaf type index for symbolic map leaves
//  */
// size_t qBDD_symbolicMapLType();

// /**
//  * @brief Returns the leaf type identifier for the symbolic value type.
//  * @return Leaf type index for symbolic value leaves
//  */
// size_t qBDD_symbolicValLType();

// /**
//  * @brief Returns the leaf type identifier for the classical numeric type.
//  * @return Leaf type index for classical leaves
//  */
// size_t qBDD_classicLType();


// /* *************************************************************************
//  * Gate application and BDD traversal operations
//  * ************************************************************************* */

// /**
//  * @brief Applies a gate operation to a target variable in a qBDD.
//  * @param operand    The qBDD representing the current quantum state
//  * @param targets    Array of target variable indices
//  * @param controlNum Number of control variables in the operation
//  * @param op         Gate operation function taking a target and two child qBDDs
//  * @return           A new qBDD representing the state after the gate
//  */
// qBDD bdd_operation(qBDD operand, size_t* targets, size_t controlNum,
//                    qBDD(*op)(size_t tgt, qBDD, qBDD));

// /**
//  * @brief Applies a gate operation to a target variable in a qBDD with additional parameter.
//  * @param operand    The qBDD representing the current quantum state
//  * @param targets    Array of target variable indices
//  * @param controlNum Number of control variables in the operation
//  * @param op         Gate operation function taking a target and two child qBDDs
//  * @param param      Extra parameter to pass to op
//  * @return           A new qBDD representing the state after the gate
//  */
// qBDD bdd_operation_param(qBDD operand, size_t *targets, size_t controlNum,
//                    qBDD (*op)(size_t, qBDD, qBDD, size_t), size_t param);
// /**
//  * @brief Applies a guarded gate operation to a target variable in a qBDD.
//  * @param operand    The qBDD representing the current quantum state
//  * @param targets    Array of target variable indices
//  * @param controlNum Number of control variables in the operation
//  * @param op         Gate operation function taking a target and a single qBDD
//  * @return           A new qBDD representing the state after the operation
//  */
// qBDD bdd_operation_guarded(qBDD operand, size_t* targets, size_t controlNum,
//                             qBDD(*op)(size_t tgt, qBDD));

// /**
//  * @brief Applies a binary leaf operation pointwise over two qBDDs.
//  * @param l  The left qBDD operand
//  * @param r  The right qBDD operand
//  * @param op Leaf operation applied to corresponding terminal pairs
//  * @return   A new qBDD with leaves computed by op
//  */
// qBDD binary_apply(qBDD l, qBDD r, LEAF_TYPE(*op)(LEAF_TYPE, LEAF_TYPE));

// /**
//  * @brief Applies a parameterised binary leaf operation pointwise over two qBDDs.
//  * @param l     The left qBDD operand
//  * @param r     The right qBDD operand
//  * @param op    Leaf operation applied to corresponding terminal pairs with an additional parameter
//  * @param param Additional parameter forwarded to op for each leaf pair
//  * @return      A new qBDD with leaves computed by op
//  */
// qBDD binary_apply_param(qBDD l, qBDD r, LEAF_TYPE(*op)(LEAF_TYPE, LEAF_TYPE, size_t), size_t param);

// /**
//  * @brief Applies a guarded binary operation over two qBDDs.
//  * @param l  The left qBDD operand
//  * @param r  The right qBDD operand
//  * @param op Operation function taking two qBDDs and returning a qBDD
//  * @return   A new qBDD produced by the operation
//  */
// qBDD binary_apply_guarded(qBDD l, qBDD r, qBDD(*op)(qBDD, qBDD));

// /**
//  * @brief Applies a parameterised guarded binary operation over two qBDDs.
//  * @param l     The left qBDD operand
//  * @param r     The right qBDD operand
//  * @param op    Operation function taking two qBDDs and a size_t parameter
//  * @param param Additional parameter forwarded to op
//  * @return      A new qBDD produced by the operation
//  */
// qBDD binary_apply_guarded_param(qBDD l, qBDD r,
//                                  qBDD(*op)(qBDD, qBDD, size_t), size_t param);

// /**
//  * @brief Applies a unary leaf operation to every terminal of a qBDD.
//  * @param l  The input qBDD
//  * @param op Leaf operation applied to each terminal value
//  * @return   A new qBDD with transformed leaf values
//  */
// qBDD unary_apply(qBDD l, LEAF_TYPE(*op)(LEAF_TYPE));

// /**
//  * @brief Applies a guarded unary operation with an argument to a qBDD.
//  * @param l   The input qBDD
//  * @param op  Operation function taking a qBDD and a size_t argument
//  * @param arg Argument forwarded to op at each recursive step
//  * @return    A new qBDD produced by the operation
//  */
// qBDD unary_apply_guarded(qBDD l, qBDD(*op)(qBDD, size_t), size_t arg);

// /**
//  * @brief Applies a parameterised unary operation to a qBDD.
//  * @param l   The input qBDD
//  * @param op  Operation function taking a void* leaf value and a size_t argument
//  * @param arg Argument forwarded to op for each leaf value
//  * @return    A new qBDD with transformed leaf values
//  */
// qBDD unary_apply_param(qBDD l, LEAF_TYPE(*op)(LEAF_TYPE, size_t), size_t arg);

// /**
//  * @brief Constructs a cube qBDD over a set of variables with given leaf values.
//  * @param value     Integer encoding of the variable assignment
//  * @param width     Number of variables in the cube
//  * @param variables Array of qBDD variable nodes
//  * @param leaf1     Leaf value assigned to the true path
//  * @param leaf0     Leaf value assigned to the false path
//  * @return          A qBDD representing the cube over the given variables
//  */
// qBDD cube(int value, int width, qBDD *variables, qBDD leaf1, qBDD leaf0);


// /* *************************************************************************
//  * Inverse square root coefficient management
//  * ************************************************************************* */

// /**
//  * @brief Increments the global inverse square root coefficient by one step.
//  */
// void incInvSqrtCoeff();

// /**
//  * @brief Increments the symbolic inverse square root coefficient by one step.
//  */
// void incInvSqrtCoeffSymb();

// /**
//  * @brief Initialises the symbolic inverse square root coefficient to its base value.
//  */
// void initInvSqrtCoeffSymb();

// /**
//  * @brief Resets and frees the normal inverse square root coefficient.
//  */
// void clearInvSqrtCoeffNormal();

// /**
//  * @brief Resets and frees the symbolic inverse square root coefficient.
//  */
// void clearInvSqrtCoeffSymb();

// /**
//  * @brief Multiplies the symbolic inverse square root coefficient by an unsigned integer.
//  * @param mul The multiplier to apply
//  */
// void mulInvSqrtSymbCoeff(unsigned long mul);

// /**
//  * @brief Adds the symbolic coefficient into the normal coefficient accumulator.
//  */
// void addInvSqrtCoeffs();

// /**
//  * @brief Resets the symbolic coefficient back to its initial state without freeing.
//  */
// void resetInvSqrtCoeffSymb();

// /**
//  * @brief Returns a read-only pointer to the current normal inverse sqrt coefficient.
//  * @return Pointer to the GMP integer holding the coefficient
//  */
// mpz_srcptr getInvSqrtCoeff(void);

// /**
//  * @brief Returns a read-only pointer to the current symbolic inverse sqrt coefficient.
//  * @return Pointer to the GMP integer holding the symbolic coefficient
//  */
// mpz_srcptr getInvSqrtCoeffSymb(void);


// /* *************************************************************************
//  * Operation result validation
//  * ************************************************************************* */

// /**
//  * @brief Marks the most recent operation result as valid.
//  */
// void validateOperationResult();

// /**
//  * @brief Invalidates the cached operation result forcing further traversal.
//  */
// void invalidateOperationResult();

// /**
//  * @brief Marks the most recent apply result as valid.
//  */
// void validateApplyResult();

// /**
//  * @brief Invalidates the cached apply result forcing further traversal.
//  */
// void invalidateApplyResult();


// /* *************************************************************************
//  * Leaf arithmetic operations
//  * ************************************************************************* */

// /**
//  * @brief Computes the -leaf.
//  * @param a The input leaf
//  * @return  Sign inverted leaf value
//  */
// LEAF_TYPE invertLeaf(LEAF_TYPE a);

// /**
//  * @brief Multiplies a leaf value by -i.
//  * @param a The input leaf
//  * @return  A new leaf holding -i times a
//  */
// LEAF_TYPE negI_mul(LEAF_TYPE a);

// /**
//  * @brief Adds two leaf values.
//  * @param a The first operand
//  * @param b The second operand
//  * @return  A new leaf holding a plus b
//  */
// LEAF_TYPE addLeaf(LEAF_TYPE a, LEAF_TYPE b);

// /**
//  * @brief Subtracts one leaf value from another.
//  * @param a The minuend
//  * @param b The subtrahend
//  * @return  A new leaf holding a minus b
//  */
// LEAF_TYPE subLeaf(LEAF_TYPE a, LEAF_TYPE b);

// /**
//  * @brief Multiplies two leaf values.
//  * @param a The first operand
//  * @param b The second operand
//  * @return  A new leaf holding a times b
//  */
// LEAF_TYPE mulLeaf(LEAF_TYPE a, LEAF_TYPE b);

// /**
//  * @brief Divides one leaf value by another.
//  * @param a The dividend
//  * @param b The divisor
//  * @return  A new leaf holding a divided by b
//  */
// LEAF_TYPE divLeaf(LEAF_TYPE a, LEAF_TYPE b);

// /**
//  * @brief Computes the square root of a leaf value.
//  * @param a The input leaf
//  * @return  A new leaf holding the square root of a
//  */
// LEAF_TYPE sqrtLeaf(LEAF_TYPE a);

// /**
//  * @brief Applies the first rotation to the leaf coefficient.
//  * @param a The input leaf
//  * @return  A new leaf with its coefficient rotated by the first rotation factor
//  */
// LEAF_TYPE rotateCoef1(LEAF_TYPE a);

// /**
//  * @brief Applies the second rotation to the leaf coefficient.
//  * @param a The input leaf
//  * @return  A new leaf with its coefficient rotated by the second rotation factor
//  */
// LEAF_TYPE rotateCoef2(LEAF_TYPE a);

// /**
//  * @brief Multiplies a leaf value by two.
//  * @param l The input leaf
//  * @return  A new leaf holding twice the value of l
//  */
// LEAF_TYPE times2Leaf(LEAF_TYPE l);

// /**
//  * @brief Multiplies a leaf value by a complex phase factor defined by the given angle.
//  * @param t   The input leaf
//  * @param arg The PhaseParam sine and cosine of the angle are used to compute the phase factor
//  * @return    A new leaf holding the value of t multiplied by the complex phase factor
//  * 
//  * @see PhaseParam
//  */
// LEAF_TYPE mulPhaseLeaf(LEAF_TYPE t, size_t arg);


// /* *************************************************************************
//  * Leaf arithmetic operations with inverse square root of 2 factor
//  * ************************************************************************* */

// /**
//  * @brief Adds two leaf values and multiplies the result by 1/sqrt(2).
//  * @param a The first operand
//  * @param b The second operand
//  * @return  A new leaf holding (a plus b) times 1/sqrt(2)
//  */
// LEAF_TYPE addLeafS(LEAF_TYPE a, LEAF_TYPE b);

// /**
//  * @brief Subtracts one leaf value from another and multiplies the result by 1/sqrt(2).
//  * @param a The minuend
//  * @param b The subtrahend
//  * @return  A new leaf holding (a minus b) times 1/sqrt(2)
//  */
// LEAF_TYPE subLeafS(LEAF_TYPE a, LEAF_TYPE b);

// /**
//  * @brief Multiplies two leaf values and scales the result by 1/sqrt(2).
//  * @param a The first operand
//  * @param b The second operand
//  * @return  A new leaf holding (a times b) times 1/sqrt(2)
//  */
// LEAF_TYPE mulLeafS(LEAF_TYPE a, LEAF_TYPE b);

// /**
//  * @brief Divides one leaf value by another and scales the result by 1/sqrt(2).
//  * @param a The dividend
//  * @param b The divisor
//  * @return  A new leaf holding (a divided by b) times 1/sqrt(2)
//  */
// LEAF_TYPE divLeafS(LEAF_TYPE a, LEAF_TYPE b);

// /**
//  * @brief Applies the first coefficient rotation and scales the result by 1/sqrt(2).
//  * @param a The input leaf
//  * @return  A new leaf with its coefficient rotated by the first factor and scaled by 1/sqrt(2)
//  */
// LEAF_TYPE rotateCoef1S(LEAF_TYPE a);

// /**
//  * @brief Applies the inverse of the first coefficient rotation and scales the result by 1/sqrt(2).
//  * @param l The input leaf
//  * @return  A new leaf with its coefficient rotated by the inverse of the first factor and scaled by 1/sqrt(2)
//  */
// LEAF_TYPE rotateCoef1S_inv(LEAF_TYPE l);
// /**
//  * @brief Applies the second coefficient rotation and scales the result by 1/sqrt(2).
//  * @param a The input leaf
//  * @return  A new leaf with its coefficient rotated by the second factor and scaled by 1/sqrt(2)
//  */
// LEAF_TYPE rotateCoef2S(LEAF_TYPE a);

// /**
//  * @brief Multiplies a leaf value by two and scales the result by 1/sqrt(2).
//  * @param l The input leaf
//  * @return  A new leaf holding twice the value of l scaled by 1/sqrt(2),
//  *          equivalent to l times sqrt(2)
//  */
// LEAF_TYPE times2LeafS(LEAF_TYPE l);

// /**
//  * @brief 
//  */
// LEAF_TYPE ry_low_leaf(LEAF_TYPE low, LEAF_TYPE high, size_t param);

// /**
//  * @brief
//  */
// LEAF_TYPE ry_high_leaf(LEAF_TYPE low, LEAF_TYPE high, size_t param);

// /**
//  * @brief
//  */
// LEAF_TYPE rx_low_leaf(LEAF_TYPE low, LEAF_TYPE high, size_t param);

// /**
//  * @brief
//  */
// LEAF_TYPE rx_high_leaf(LEAF_TYPE low, LEAF_TYPE high, size_t param);

// LEAF_TYPE rz_low_leaf(LEAF_TYPE l, size_t param);
// LEAF_TYPE rz_high_leaf(LEAF_TYPE l, size_t param);

// /* *************************************************************************
//  * Debugging and visualisation
//  * ************************************************************************* */

// /**
//  * @brief Writes a Graphviz dot representation of a qBDD to a file.
//  * @param out Output file stream to write to
//  * @param a   The qBDD to serialise
//  */
// void q_fprintdot(FILE* out, qBDD a);


// /* *************************************************************************
//  * Package and circuit lifecycle
//  * ************************************************************************* */

// /**
//  * @brief Initialises a quantum circuit via the interface layer.
//  * @param c Pointer to the qBDD to initialise
//  * @param n Number of qubits
//  */
// void circuit_init_interface(qBDD *c, const uint32_t n);

// /**
//  * @brief Initialises the BDD package with given cache and node table sizes.
//  * @param cacheSize Size of the operation cache
//  * @param nodeSize  Size of the unique node table
//  * @param varNum    Number of variables to support
//  */
// void initPackage(unsigned cacheSize, unsigned nodeSize, unsigned varNum);

// /**
//  * @brief Frees all resources held by a quantum circuit.
//  * @param circ Pointer to the qBDD circuit to delete
//  */
// void deleteCircuit(qBDD *circ);

// /**
//  * @brief Shuts down the BDD package and releases all allocated memory.
//  */
// void freePackage();
// void freePimpl(void* leafraw);

// /* *************************************************************************
//  * Symbolic terminal initialisation
//  * ************************************************************************* */

// /**
//  * @brief Initialises the symbolic value terminal type for use in computations.
//  */
// void init_terminal_symb_val_i();

// /**
//  * @brief Initialises the symbolic map terminal type for use in computations.
//  */
// void init_terminal_symb_map_i();


// /* *************************************************************************
//  * Garbage collection
//  * ************************************************************************* */

// /**
//  * @brief Forces an immediate garbage collection pass over the node table.
//  */
// void forceGC();


// /* *************************************************************************
//  * Terminal type query
//  * ************************************************************************* */

// /**
//  * @brief Returns the leaf type identifier of a terminal qBDD node.
//  * @param terminal A terminal qBDD node
//  * @return         The type index of the leaf stored in the terminal
//  */
// size_t qBDD_getTerminalType(qBDD terminal);


// /* *************************************************************************
//  * Symbolic MTBDD operations
//  * ************************************************************************* */

// /**
//  * @brief Negates a symbolic leaf value.
//  * @param t The input symbolic leaf
//  * @return  A new leaf with the negated value
//  */
// LEAF_TYPE mtbdd_symb_neg_i(LEAF_TYPE t);

// /**
//  * @brief Applies the first coefficient rotation to a symbolic leaf.
//  * @param t The input symbolic leaf
//  * @return  A new leaf with its coefficient rotated by the first factor
//  */
// LEAF_TYPE mtbdd_symb_coef_rot1_i(LEAF_TYPE t);

// /**
//  * @brief Applies the inverse of the first coefficient rotation to a symbolic leaf.
//  * @param t The input symbolic leaf
//  * @return  A new leaf with its coefficient rotated by the inverse of the first factor
//  */
// LEAF_TYPE mtbdd_symb_coef_rot1_i_inv(LEAF_TYPE t);
// /**
//  * @brief Applies the second coefficient rotation to a symbolic leaf.
//  * @param t The input symbolic leaf
//  * @return  A new leaf with its coefficient rotated by the second factor
//  */
// LEAF_TYPE mtbdd_symb_coef_rot2_i(LEAF_TYPE t);

// /**
//  * @brief Adds two symbolic leaf values within the MTBDD framework.
//  * @param a The first symbolic leaf
//  * @param b The second symbolic leaf
//  * @return  A new leaf holding a plus b
//  */
// LEAF_TYPE mtbdd_symb_plus_i(LEAF_TYPE a, LEAF_TYPE b);

// /**
//  * @brief Subtracts one symbolic leaf value from another within the MTBDD framework.
//  * @param a The symbolic minuend
//  * @param b The symbolic subtrahend
//  * @return  A new leaf holding a minus b
//  */
// LEAF_TYPE mtbdd_symb_minus_i(LEAF_TYPE a, LEAF_TYPE b);

// /**
//  * @brief Multiplies a qBDD by a boolean variable xt.
//  * @param t  The input qBDD
//  * @param b  The boolean variable qBDD to multiply with
//  * @return   A new qBDD representing the product
//  */
// qBDD mtbdd_b_xt_mul_i(qBDD t, qBDD b);

// /**
//  * @brief Multiplies a symbolic leaf by a raw integer constant.
//  * @param t     The input symbolic leaf
//  * @param c_raw Raw encoding of the integer constant
//  * @return      A new leaf holding t times the constant
//  */
// LEAF_TYPE mtbdd_symb_times_c_i(LEAF_TYPE t, size_t c_raw);

// /**
//  * @brief Creates a complement variable node for xt in a qBDD.
//  * @param t  The input qBDD
//  * @param xt Index of the variable to complement
//  * @return   A new qBDD with the complemented xt node inserted
//  */
// qBDD t_xt_comp_create_i(qBDD t, size_t xt);

// /**
//  * @brief Creates a variable node for xt in a qBDD.
//  * @param t  The input qBDD
//  * @param xt Index of the variable node to create
//  * @return   A new qBDD with the xt node inserted
//  */
// qBDD t_xt_create_i(qBDD t, size_t xt);

// /**
//  * @brief Refines a symbolic map using a value qBDD and reduction data.
//  * @param map    The symbolic map qBDD to refine
//  * @param val    The symbolic value qBDD to apply
//  * @param rd_raw Raw encoding of the reduction data
//  * @return       A new qBDD representing the refined symbolic map
//  */
// qBDD mtbdd_symb_refine_i(qBDD map, qBDD val, size_t rd_raw);

// /**
//  * @brief Converts an MTBDD to a reduced symbolic value using a given map.
//  * @param t       The input MTBDD
//  * @param raw_map Raw encoding of the symbolic map to apply
//  * @return        A new qBDD with symbolic values reduced under the map
//  */
// qBDD mtbdd_map_to_symb_val_reduced_i(qBDD t, size_t raw_map);

// /**
//  * @brief Converts an MTBDD to a symbolic value using a given map.
//  * @param t       The input MTBDD
//  * @param raw_map Raw encoding of the symbolic map to apply
//  * @return        A new qBDD with symbolic values computed from the map
//  */
// qBDD mtbdd_map_to_symb_val_i(qBDD t, size_t raw_map);

// /**
//  * @brief Converts an MTBDD to a symbolic map representation.
//  * @param a     The input MTBDD
//  * @param raw_m Raw encoding of the target map format
//  * @return      A new qBDD representing the symbolic map form of a
//  */
// qBDD mtbdd_to_symb_map_i(qBDD a, size_t raw_m);

// /**
//  * @brief Converts a symbolic qBDD back to its standard MTBDD form.
//  * @param t       The symbolic qBDD to convert
//  * @param raw_map Raw encoding of the symbolic map used during conversion
//  * @return        A new qBDD in standard MTBDD form
//  */
// qBDD mtbdd_from_symb_i(qBDD t, size_t raw_map);

// /**
//  * @brief Tests whether a symbolic coefficient can be reduced under given reduction data.
//  * @param symbc Pointer to the symbolic coefficient structure
//  * @param rdata Pointer to the reduction data to test against
//  * @return      True if the coefficient is reducible, false otherwise
//  */
// bool can_be_reduced(mtbdd_symb_t *symbc, rdata_t *rdata);


// /* *************************************************************************
//  * Primitive leaf backend interface
//  * ************************************************************************* */

// /**
//  * @brief Hashes the raw data of a primitive leaf node.
//  * @param data The primitive leaf value to hash
//  * @return     A 64-bit hash of the leaf data
//  */
// uint64_t hash_comb_generic(leaf_primitive_t data);

// /**
//  * @brief Compares two primitive leaf values for ordering.
//  * @param a The first operand
//  * @param b The second operand
//  * @return  Negative if a is less than b, zero if equal, positive if greater
//  */
// int cmp_generic(leaf_primitive_t a, leaf_primitive_t b);

// /**
//  * @brief Initialises dst and copies the value of src into it.
//  * @param dst The destination primitive to initialise
//  * @param src The source primitive to copy from
//  */
// void init_set_generic(leaf_primitive_t dst, leaf_primitive_t src);

// /**
//  * @brief Initialises a primitive leaf to zero without setting a value.
//  * @param x The primitive leaf to initialise
//  */
// void init_generic(leaf_primitive_t x);

// /**
//  * @brief Releases all resources held by a primitive leaf.
//  * @param x The primitive leaf to clear
//  */
// void clear_generic(leaf_primitive_t x);

// /**
//  * @brief Multiplies a primitive leaf by an unsigned integer.
//  * @param r The destination primitive to store the result
//  * @param x The input primitive operand
//  * @param c The unsigned integer multiplier
//  */
// void mul_ui_generic(leaf_primitive_t r, leaf_primitive_t x, unsigned long c);

// /**
//  * @brief Negates a primitive leaf value.
//  * @param r The destination primitive to store the result
//  * @param x The input primitive to negate
//  */
// void neg_generic(leaf_primitive_t r, leaf_primitive_t x);

// /**
//  * @brief Adds two primitive leaf values.
//  * @param r The destination primitive to store the result
//  * @param a The first operand
//  * @param b The second operand
//  */
// void add_generic(leaf_primitive_t r, leaf_primitive_t a, leaf_primitive_t b);

// /**
//  * @brief Initialises a primitive leaf and sets it to an unsigned integer value.
//  * @param x The primitive leaf to initialise
//  * @param v The unsigned integer value to assign
//  */
// void init_set_ui_generic(leaf_primitive_t x, unsigned long v);

// /**
//     * @brief Sets a primitive leaf to an unsigned integer value without initialisation
//     * @param dst The destination primitive, already initialised
//     * @param v   The unsigned integer value to assign
//  */
// void set_ui_generic(leaf_primitive_t dst, unsigned long v);

// /**
//  * @brief Returns the sign of a primitive leaf value.
//  * @param x The primitive leaf to query
//  * @return  Negative one if x is negative, zero if x is zero, one if x is positive
//  */
// int sgn_generic(leaf_primitive_t x);

// /**
//  * @brief Sets a primitive leaf to the value of a double.
//  * @param x The destination primitive
//  * @param v The double value to assign
//  */
// void set_d_generic(leaf_primitive_t x, double v);

// /**
//  * @brief Subtracts one primitive leaf from another.
//  * @param r The destination primitive to store the result
//  * @param a The minuend
//  * @param b The subtrahend
//  */
// void sub_generic(leaf_primitive_t r, leaf_primitive_t a, leaf_primitive_t b);

// /**
//  * @brief Divides one primitive leaf by another.
//  * @param r The destination primitive to store the result
//  * @param a The dividend
//  * @param b The divisor
//  */
// void div_generic(leaf_primitive_t r, leaf_primitive_t a, leaf_primitive_t b);

// /**
//  * @brief Multiplies a primitive leaf by a double scalar.
//  * @param r The destination primitive to store the result
//  * @param x The input primitive operand
//  * @param s The double scalar multiplier
//  */
// void mul_d_generic(leaf_primitive_t r, leaf_primitive_t x, double s);

// void mul_sqrt2inv_generic(leaf_primitive_t r, leaf_primitive_t x);

// /**
//  * @brief Converts a primitive leaf to its nearest double representation.
//  * @param x The primitive leaf to convert
//  * @return  The double approximation of x
//  */
// double to_double_generic(leaf_primitive_t x);

// /**
//  * @brief Copies the value of src into dst without reinitialising dst.
//  * @param dst The destination primitive, already initialised
//  * @param src The source primitive to copy from
//  */
// void set_generic(leaf_primitive_t dst, leaf_primitive_t src);

// /**
//  * @brief Multiplies two primitive leaf values.
//  * @param r The destination primitive to store the result
//  * @param a The first operand
//  * @param b The second operand
//  */
// void mul_generic(leaf_primitive_t r, leaf_primitive_t a, leaf_primitive_t b);

// /**
//  * @brief Multiplies a primitive leaf by a GMP integer scalar.
//  * @param r The destination primitive to store the result
//  * @param x The input primitive operand
//  * @param s The GMP integer multiplier
//  */
// void mul_mpz_generic(leaf_primitive_t r, leaf_primitive_t x, mpz_t s);

// /**
//  * @brief Multiplies a primitive leaf by the inverse square root of 2 raised to a power.
//  * @param r The destination primitive to store the result
//  * @param x The input primitive operand
//  * @param k The exponent of the inverse square root of 2 factor
//  */
// void mul_inv_sqrt2_pow_generic(leaf_primitive_t r, leaf_primitive_t x, mpz_t k);

// /**
//  * @brief Sets a primitive leaf to the inverse square root of 2 raised to a power k.
//  * @param x The primitive leaf to set
//  * @param k The exponent of the inverse square root of 2 factor
//  * 
//  * @warning This function does not initialise x, so it must be initialised before calling.
//  */
// void inv_sqrt2_pow_generic(leaf_primitive_t x, mpz_t k);


// /**
//  * @brief Computes the hash of a raw terminal node for use in the node table.
//  * @param q Pointer to the raw terminal leaf data
//  * @return  An unsigned hash value for the terminal
//  */
// unsigned terminal_hash_generic(void* q);

// /**
//  * @brief Compares two raw terminal nodes for equality in the node table.
//  * @param a Pointer to the first raw terminal
//  * @param b Pointer to the second raw terminal
//  * @return  Zero if the terminals are equal, non-zero otherwise
//  */
// int terminal_compare_generic(void* a, void* b);

// /**
//  * @brief Serialises a raw terminal leaf to a human-readable string.
//  * @param ldata_raw   Pointer to the raw terminal leaf data
//  * @param buddy_buf   Output buffer to write the string into
//  * @param buddy_bufsize Size of the output buffer in bytes
//  * @return            Pointer to the written string within buddy_buf
//  */
// char* terminal_to_str_generic(void* ldata_raw, char *buddy_buf, size_t buddy_bufsize);

// /**
//  * @brief Serialises a primitive leaf value to a human-readable string.
//  * @param x       The primitive leaf to serialise
//  * @param buf     Output buffer to write the string into
//  * @param bufsize Size of the output buffer in bytes
//  */
// void to_str_generic(leaf_primitive_t x, char *buf, size_t bufsize);

// /**
//  * @brief Compares two raw symbolic map terminals for equality in the node table.
//  * @param l_a Pointer to the first symbolic map terminal
//  * @param l_b Pointer to the second symbolic map terminal
//  * @return    Zero if equal, non-zero otherwise
//  */
// int terminal_symb_map_compare_generic(void* l_a, void* l_b);

// /**
//  * @brief Computes the hash of a raw symbolic map terminal for use in the node table.
//  * @param l_a Pointer to the symbolic map terminal to hash
//  * @return    An unsigned hash value for the terminal
//  */
// unsigned terminal_symb_map_hash_generic(void* l_a);

// /**
//  * @brief Compares two raw symbolic value terminals for equality in the node table.
//  * @param l_a Pointer to the first symbolic value terminal
//  * @param l_b Pointer to the second symbolic value terminal
//  * @return    Zero if equal, non-zero otherwise
//  */
// int terminal_symb_val_compare_generic(void* l_a, void* l_b);

// /**
//  * @brief Computes the hash of a raw symbolic value terminal for use in the node table.
//  * @param l_a Pointer to the symbolic value terminal to hash
//  * @return    An unsigned hash value for the terminal
//  */
// unsigned terminal_symb_val_hash_generic(void* l_a);

// void symb_init(qBDD *circ, mtbdd_symb_t *symbc);

// #ifdef __cplusplus
// }
// #endif

// #endif

#ifndef INTERFACE_H
#define INTERFACE_H

#include "interface_bdd_core.h"
#include "interface_leaf.h"
#include "interface_gate_ops.h"
#include "interface_prob.h"
#include "interface_norm.h"
#include "interface_symb.h"
#include "interface_lifecycle.h"

#endif