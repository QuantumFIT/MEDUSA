#ifndef INTERFACE_BDD_CORE_H
#define INTERFACE_BDD_CORE_H

#ifdef __cplusplus
#include <gmp.h>
extern "C" {
#endif

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>


/*
 * Consumers must define qBDD and leaf_primitive_t before including this header
 * by including the appropriate leaf type header first.
 */
#ifndef QBDD_TYPE_DEFINED
# error "qBDD not defined, include a leaf type header before this file"
typedef void* qBDD;
#endif

#ifndef LEAF_PRIMITIVE_DEFINED
# error "leaf_primitive_t not defined, include a leaf type header before this file"
typedef void* leaf_primitive_t;
#endif

typedef struct LEAF_TYPE_IMPL LEAF_TYPE_IMPL;

typedef struct LEAF_TYPE {
    LEAF_TYPE_IMPL* pImpl;
} LEAF_TYPE;


/* *************************************************************************
 * BDD node construction and traversal
 * ************************************************************************* */

/**
 * @brief Creates a new internal qBDD node.
 * @param target Target variable number
 * @param low    qBDD representing the low branch of the new node
 * @param high   qBDD representing the high branch of the new node
 * @return       A new qBDD node branching on the given target variable
 */
qBDD newqBDD(unsigned int target, qBDD low, qBDD high);

/**
 * @brief Protects a qBDD node from garbage collection.
 * @param  A qBDD node to protect
 * @return The same node with an incremented reference count
 */
qBDD qBDD_protect(qBDD);

/**
 * @brief Releases a previously protected qBDD node.
 * @param  A qBDD node to unprotect
 * @return The same node with a decremented reference count
 */
qBDD qBDD_unprotect(qBDD);

/**
 * @brief Returns the canonical false terminal node.
 * @return The false terminal qBDD
 */
qBDD qBDD_false();

/**
 * @brief Returns the canonical true terminal node.
 * @return The true terminal qBDD
 */
qBDD qBDD_true();

/**
 * @brief Tests whether a qBDD node is the false terminal.
 * @param  A qBDD node to test
 * @return Non-zero if the node is the false terminal
 */
int qBDD_isFalse(qBDD);

/**
 * @brief Tests whether a qBDD node is an internal decision node.
 * @param  A qBDD node to test
 * @return Non-zero if the node is an internal node
 */
int qBDD_isInternal(qBDD);

/**
 * @brief Tests whether a qBDD node is a terminal leaf node.
 * @param  A qBDD node to test
 * @return Non-zero if the node is a terminal node
 */
int qBDD_isTerminal(qBDD);

/**
 * @brief Returns the low child of an internal qBDD node.
 * @param  An internal qBDD node
 * @return The low branch child node
 */
qBDD qBDD_getLow(qBDD);

/**
 * @brief Returns the high child of an internal qBDD node.
 * @param  An internal qBDD node
 * @return The high branch child node
 */
qBDD qBDD_getHigh(qBDD);

/**
 * @brief Returns the variable index of an internal qBDD node.
 * @param  An internal qBDD node
 * @return The variable index this node branches on
 */
size_t qBDD_getVar(qBDD);

/**
 * @brief Retrieves the leaf value stored in a terminal qBDD node.
 * @param  A terminal qBDD node
 * @return The LEAF_TYPE value associated with the terminal
 */
LEAF_TYPE qBDD_getTerminalValue(qBDD);

/**
 * @brief Creates a new terminal qBDD node with the given type and value.
 * @param  Leaf type identifier
 * @param  Pointer to the raw leaf data
 * @return A terminal qBDD node holding the given value
 */
qBDD qBDD_maketerminal(size_t, void*);

/**
 * @brief Counts the number of distinct leaf nodes in a qBDD.
 * @param  A qBDD to count leaves in
 * @return The number of terminal nodes reachable from the root
 */
int qBDD_leafcount(qBDD);

/**
 * @brief Returns the level of a qBDD node in the decision diagram.
 * @param  A qBDD node to query
 * @return The level of the node, where 0 is the root level and higher numbers are deeper
 */
size_t qBDD_level(qBDD node);

/**
 * @brief Returns the leaf type identifier of a terminal qBDD node.
 * @param terminal A terminal qBDD node
 * @return         The type index of the leaf stored in the terminal
 */
size_t qBDD_getTerminalType(qBDD terminal);

#ifdef __cplusplus
}
#endif

#endif /* INTERFACE_BDD_CORE_H */
