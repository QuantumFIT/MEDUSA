/**
 * @file gates_symb.h
 * @brief Symbolic gate operations
 */

#include "mtbdd_symb_val.h"
#include "qparam.h"

#ifndef GATES_SYMB_H
#define GATES_SYMB_H
#ifdef __cplusplus
extern "C" {
#endif
/**
 * Apply gate X on the symbolic state vector.
 * 
 * @param p_t pointer to a symbolic value MTBDD
 * 
 * @param xt target qubit index
 * 
 */
void gate_symb_x(qBDD *p_t, uint32_t xt);

/**
 * Apply gate Y on the symbolic state vector.
 * 
 * @param p_t pointer to a symbolic value MTBDD
 * 
 * @param xt target qubit index
 * 
 */
void gate_symb_y(qBDD *p_t, uint32_t xt);

/**
 * Apply gate Z on the symbolic state vector.
 * 
 * @param p_t pointer to a symbolic value MTBDD
 * 
 * @param xt target qubit index
 * 
 */
void gate_symb_z(qBDD *p_t, uint32_t xt);

/**
 * Apply gate S on the symbolic state vector.
 * 
 * @param p_t pointer to a symbolic value MTBDD
 * 
 * @param xt target qubit index
 * 
 */
void gate_symb_s(qBDD *p_t, uint32_t xt);

/**
 * Apply gate T on the symbolic state vector.
 * 
 * @param p_t pointer to a symbolic value MTBDD
 * 
 * @param xt target qubit index
 * 
 */
void gate_symb_t(qBDD *p_t, uint32_t xt);

/**
 * Apply gate Tdg on the symbolic state vector.
 * 
 * @param p_t pointer to a symbolic value MTBDD
 * 
 * @param xt target qubit index
 * 
 */
void gate_symb_tdg(qBDD *p_t, uint32_t xt);

/**
 * Apply Hadamard gate on the symbolic state vector.
 * 
 * @param p_t pointer to a symbolic value MTBDD
 * 
 * @param xt target qubit index
 */
void gate_symb_h(qBDD *p_t, uint32_t xt);

/**
 * Apply Rx(π/2) gate on the symbolic state vector.
 * 
 * @param p_t pointer to a symbolic value MTBDD
 * 
 * @param xt target qubit index
 */
void gate_symb_rx_pihalf(qBDD *p_t, uint32_t xt);

/**
 * Apply Ry(π/2) gate on the symbolic state vector.
 * 
 * @param p_t pointer to a symbolic value MTBDD
 * 
 * @param xt target qubit index
 */
void gate_symb_ry_pihalf(qBDD *p_t, uint32_t xt);

/**
 * Apply Controlled NOT gate on the symbolic state vector.
 * 
 * @param p_t pointer to a symbolic value MTBDD
 * 
 * @param xt target qubit index
 * 
 * @param xc control qubit index
 */
void gate_symb_cnot(qBDD *p_t, uint32_t xt, uint32_t xc);

/**
 * Apply Controlled Z gate on the symbolic state vector.
 * 
 * @param p_t pointer to a symbolic value MTBDD
 * 
 * @param xt target qubit index
 * 
 * @param xc control qubit index
 */
void gate_symb_cz(qBDD *p_t, uint32_t xt, uint32_t xc);

/**
 * Apply Toffoli gate on the symbolic state vector.
 * 
 * @param p_t pointer to a symbolic value MTBDD
 * 
 * @param xt target qubit index
 * 
 * @param xc1 first control qubit index
 * 
 * @param xc2 second control qubit index
 */
void gate_symb_toffoli(qBDD *p_t, uint32_t xt, uint32_t xc1, uint32_t xc2);

/**
 * Apply Multicontrol NOT gate on the symbolic state vector.
 * 
 * @param p_t pointer to a symbolic value MTBDD
 * 
 * @param qparams list of all the target + control qubit indices (first index is assumed to be the target index)
 */
void gate_symb_mcx(qBDD *p_t, qparam_list_t *qparams);

#endif
#ifdef __cplusplus
}
#endif
/* end of "gates_symb.h" */