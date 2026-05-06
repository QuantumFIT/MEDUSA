#ifndef INTERFACE_LIFECYCLE_H
#define INTERFACE_LIFECYCLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>

#include "interface_bdd_core.h"


/* *************************************************************************
 * Package and circuit lifecycle
 * ************************************************************************* */

/**
 * @brief Initialises the BDD package with given cache and node table sizes.
 * @param cacheSize Size of the operation cache
 * @param nodeSize  Size of the unique node table
 * @param varNum    Number of variables to support
 */
void initPackage(unsigned cacheSize, unsigned nodeSize, unsigned varNum);

/**
 * @brief Initialises a quantum circuit via the interface layer.
 * @param c Pointer to the qBDD to initialise
 * @param n Number of qubits
 */
void circuit_init_interface(qBDD *c, const uint32_t n);

/**
 * @brief Frees all resources held by a quantum circuit.
 * @param circ Pointer to the qBDD circuit to delete
 */
void deleteCircuit(qBDD *circ);

/**
 * @brief Shuts down the BDD package and releases all allocated memory.
 */
void freePackage();

/**
 * @brief Releases all resources held by a raw leaf implementation pointer.
 * @param leafraw Pointer to the raw leaf data to free
 */
void freePimpl(void* leafraw);


/* *************************************************************************
 * Garbage collection
 * ************************************************************************* */

/**
 * @brief Forces an immediate garbage collection pass over the node table.
 */
void forceGC();


/* *************************************************************************
 * Debugging and visualisation
 * ************************************************************************* */

/**
 * @brief Writes a Graphviz dot representation of a qBDD to a file.
 * @param out Output file stream to write to
 * @param a   The qBDD to serialise
 */
void q_fprintdot(FILE* out, qBDD a);

#ifdef __cplusplus
}
#endif

#endif /* INTERFACE_LIFECYCLE_H */
