#ifndef INTERFACE_LIFECYCLE_H
#define INTERFACE_LIFECYCLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

#include "interface_bdd_core.h"


/* *************************************************************************
 * Package and circuit lifecycle
 * ************************************************************************* */

/**
 * @brief Initialises the BDD package with given cache and node table sizes.
 * @param cacheSize Size of the operation cache (0 = default 10000)
 * @param nodeSize  Size of the unique node table (0 = default 10000)
 * @param varNum    Number of variables to support (0 = default 1)
 */
void initPackage(unsigned cacheSize, unsigned nodeSize, unsigned varNum);

/**
 * @brief When true, classic res.dot terminals print |amp|^2 instead of the amplitude.
 * Matches original MEDUSA --probability. Call after initPackage; reset on freePackage.
 */
void setLeafPrintProb(bool is_prob);

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

/**
 * @brief Reset / query pImpl allocation counters (for leak tests).
 */
void medusa_mem_reset(void);
void medusa_mem_get(size_t *pimpl_allocs, size_t *pimpl_frees, size_t *wrap_allocs);


/* *************************************************************************
 * Garbage collection
 * ************************************************************************* */

/**
 * @brief Forces an immediate garbage collection pass over the node table.
 */
void forceGC();

/**
 * Clear MoToBuddy MTBDD apply/operation caches.
 * Required before side-effecting applies (e.g. symb_refine → rdata).
 */
void clearOpCache(void);


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
