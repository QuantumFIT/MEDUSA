/**
 * @file gates.h
 * @brief Gate operations on the classic MTBDD
 */

#include <stdbool.h>
#include "mtbdd.h"
#include "qparam.h"
#include "interface.h"
#ifndef GATES_H
#define GATES_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Apply a gate operation <op> to <dd>. Custom apply needed because xt nodes may not be present in the reduced <dd>.
 * Otherwise it's basically the standard uapply.
 */
qBDD interface_gate_x(size_t xt, qBDD low, qBDD high);

/**
 * Returns the probability the given qubit's state will be 1.
 * This implementation supports only a measurement of all the qubits at the end of the circuit.
 * 
 * @param p_t pointer to an MTBDD
 * 
 * @param xt target qubit index
 * 
 * @param curr_state current state vector (determined by previous measurements)
 * 
 * @param n number of qubits in the circuit
 * 
 */
prob_t measure(qBDD *p_t, uint32_t xt, char *curr_state, int n);

/**
 * Apply quantum gate X on the state vector.
 * 
 * @param p_t pointer to an MTBDD
 * 
 * @param xt target qubit index
 * 
 */
void gate_x(qBDD *p_t, uint32_t xt);

/**
 * Apply quantum gate Y on the state vector.
 * 
 * @param p_t pointer to an MTBDD
 * 
 * @param xt target qubit index
 * 
 */
void gate_y(qBDD *p_t, uint32_t xt);

/**
 * Apply quantum gate Z on the state vector.
 * 
 * @param p_t pointer to an MTBDD
 * 
 * @param xt target qubit index
 * 
 */
void gate_z(qBDD *p_t, uint32_t xt);

/**
 * Apply quantum gate S on the state vector.
 * 
 * @param p_t pointer to an MTBDD
 * 
 * @param xt target qubit index
 * 
 */
void gate_s(qBDD *p_t, uint32_t xt);

/**
 * Apply quantum gate T on the state vector.
 * 
 * @param p_t pointer to an MTBDD
 * 
 * @param xt target qubit index
 * 
 */
void gate_t(qBDD *p_t, uint32_t xt);

/**
 * Apply quantum gate Tdg on the state vector.
 * 
 * @param p_t pointer to an MTBDD
 * 
 * @param xt target qubit index
 * 
 */
void gate_tdg(qBDD *p_t, uint32_t xt);

/**
 * Function implementing quantum Hadamard gate for a given MTBDD.
 * 
 * @param p_t pointer to an MTBDD
 * 
 * @param xt target qubit index
 */
void gate_h(qBDD *p_t, uint32_t xt);

/**
 * Function implementing quantum Rx(π/2) gate for a given MTBDD.
 * 
 * @param p_t pointer to an MTBDD
 * 
 * @param xt target qubit index
 */
void gate_rx_pihalf(qBDD *p_t, uint32_t xt);

/**
 * Function implementing quantum Ry(π/2) gate for a given MTBDD.
 * 
 * @param p_t pointer to an MTBDD
 * 
 * @param xt target qubit index
 */
void gate_ry_pihalf(qBDD *p_t, uint32_t xt);


/**
 * Function implementing quantum Rx(theta) gate for a given MTBDD.
 * 
 * @param p_t pointer to an MTBDD
 * 
 * @param xt target qubit index
 * 
 * @param theta rotation angle
 */
void gate_rx(qBDD *p_t, uint32_t xt, double theta);

/**
 * Function implementing quantum Ry(theta) gate for a given MTBDD.
 * 
 * @param p_t pointer to an MTBDD
 * 
 * @param xt target qubit index
 * 
 * @param theta rotation angle
 */
void gate_ry(qBDD *p_t, uint32_t xt, double theta);


/**
 * Function implementing quantum Rz(theta) gate for a given MTBDD.
 * 
 * @param p_t pointer to an MTBDD
 * 
 * @param xt target qubit index
 * 
 * @param theta rotation angle
 */
void gate_rz(qBDD *p_t, uint32_t xt, double theta);

/**
 * Function implementing quantum Controlled NOT gate for a given MTBDD.
 * 
 * @param p_t custom MTBDD
 * 
 * @param xt target qubit index
 * 
 * @param xc control qubit index
 */
void gate_cnot(qBDD *p_t, uint32_t xt, uint32_t xc);

/**
 * Function implementing quantum Controlled Z gate for a given MTBDD.
 * 
 * @param p_t custom MTBDD
 * 
 * @param xt target qubit index
 * 
 * @param xc control qubit index
 */
void gate_cz(qBDD *p_t, uint32_t xt, uint32_t xc);

/**
 * Function implementing quantum Toffoli gate for a given MTBDD.
 * 
 * @param p_t custom MTBDD
 * 
 * @param xt target qubit index
 * 
 * @param xc1 first control qubit index
 * 
 * @param xc2 second control qubit index
 */
void gate_toffoli(qBDD *p_t, uint32_t xt, uint32_t xc1, uint32_t xc2);

/**
 * Function implementing quantum Multicontrol NOT gate for a given MTBDD.
 * 
 * @param p_t custom MTBDD
 * 
 * @param qparams list of all the target + control qubit indices (first index is assumed to be the target index)
 * 
 */
void gate_mcx(qBDD *p_t, qparam_list_t *qparams);

#endif
#ifdef __cplusplus
}
#endif
/* end of "gates.h" */