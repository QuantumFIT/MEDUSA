/**
 * @file symb_utils.h
 * @brief Symbolic simulation conversion from/to regular simulation and other symbolic sim. utilities
 */

#include <sylvan.h>
#include <stdint.h>
#include "mtbdd_symb_map.h"

#ifndef SYMB_UTILS_H
#define SYMB_UTILS_H

/// Type for encapsulating the mtbdd tuple and map function needed for symbolic representation
typedef struct mtbdd_symb {
    MTBDD map;  /// MTBDD with the symoblic variable mapping
    MTBDD val;  /// MTBDD with symbolic values of the variables
    vmap_t *vm; /// Array mapping variables (index) to their initial values (needed for the final evaluation)
    bool is_reduced; /// If true, val is initialized with 'mtbdd_false' leaves if all variables have value 0
    bool is_refined; /// False only before first refine attempt (to check for reduction errors only once)
} mtbdd_symb_t;

// Refine internal:

/**
 * Type for elements of the update array
 */
typedef symexp_list_t* upd_elem_t;

/// Type for elements of the refined list (old_var -> new_var mapping)
typedef struct ref_elem {
    vars_t old;
    vars_t new;
    struct ref_elem *next;
} ref_elem_t;

/// Type for the refined list
typedef struct ref_list {
    ref_elem_t *first;
    ref_elem_t *cur;
} ref_list_t;

/// Type for storing the new variable values (in order to check for conflicts)
typedef struct upd_list{
    upd_elem_t *arr;
    size_t size;
} upd_list_t;

/// Type for encapsulating all data needed during refine
typedef struct rdata {
    upd_list_t *upd; /// Symbolic values of all variables, including the ones added during this refine
    ref_list_t *ref; /// Information about variable refinement (splitting)
    vmap_t *vm;      /// Only ptr to the original variable mapping, so it can be updated during refine operations. \
                         Holds initial numeric values of all variables.
} rdata_t;

/**
 * Function for Sylvan setup for symbolic simulation.
 */
void init_sylvan_symb();

/**
 * Creates an symbolic MTBDD from the given MTBDD and returns the mapping of variables to their values
 */
void symb_init(MTBDD *circ, mtbdd_symb_t *symbc);

/**
 * Creates and initializes refine data
 */
rdata_t* rdata_create(vmap_t *vm);

/**
 * Disposes refine data except for vmap (as it is used also outside refine)
 */
void rdata_delete(rdata_t *rd);

/**
 * Executes refine on the symbolic MTBDDs and returns true if the MTBDDs haven't been changed.
 * Fills update, refined data structures and updates variable map.
 */
bool symb_refine(mtbdd_symb_t *symbc, rdata_t *rdata);

/**
 * Updates the circuits MTBDD according to the symbolic MTBDD, variable mapping and the number of iterations
 */
void symb_eval(MTBDD *circ,  mtbdd_symb_t *symbc, uint64_t iters, rdata_t *rdata);

/**
 * Writes the update formulae into the given file
 */
void print_update(const char *filename, upd_list_t *upd);

#endif
/* end of "symb_utils.h" */