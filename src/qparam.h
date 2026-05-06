/**
 * @file qparam.h
 * @brief Data structure and operations for list of qubit parameters (for MCX gate)
 */
#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>

#ifndef QPARAM_H
#define QPARAM_H

/// Type for qparam list element
typedef struct qparam {
    uint32_t q_index;
    struct qparam *next;
} qparam_t;

///  Type for list for gate parameters (target/control qubit indices)
typedef struct qparam_list {
    qparam_t *first;
    qparam_t *active;
} qparam_list_t;


/**
 * Creates and initializes a new list.
 */
qparam_list_t* qparam_list_create();

/**
 * Sets first list element as the active element.
 * 
 * @param l list of qubit indices
 */
void qparam_list_first(qparam_list_t *l);

/**
 * Sets the current active element's next as the active element. 
 * If active element isn't set, active stays NULL.
 * 
 * @param l list of qubit indices
 */
void qparam_list_next(qparam_list_t *l);

/**
 * Creates a new list element and inserts it as the first list element.
 * 
 * @param l list of qubit indices
 * 
 * @param q_index index of the new element
 */
void qparam_list_insert_first(qparam_list_t *l, uint32_t q_index);

/**
 * Deletes the given list.
 * 
 * @param l list of qubit indices
 */
void qparam_list_del(qparam_list_t *l);

#endif
#ifdef __cplusplus
}
#endif
/* end of "qparam.h" */