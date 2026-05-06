/**
 * @file mtbdd_symb_map.h
 * @brief Custom Sylvan MTBDD type and operations for symbolic variable mapping
 */

//#include "mtbdd.h"
#include "symexp_list.h"
#include "interface.h"

#ifndef MTBDD_SYMB_MAP_H
#define MTBDD_SYMB_MAP_H

/// Global variable for my custom symbolic map mtbdd leaf type id
extern uint32_t ltype_symb_map_id;

typedef struct mapping_entry {
    qBDD original;
    qBDD mapped;
    struct mapping_entry *next;
} mapping_entry_t;

/// Type for saving and using the symbolic variable to value mapping
typedef struct vmap {
    /// array for saving the variable mapping to their values (complex numbers)
    leaf_primitive_t *map;
    size_t msize; 
    /// next variable index to be assigned
    vars_t next_var;
    /// mapping of original terminals to symbolic ones
    mapping_entry_t *mappings; 
} vmap_t;

/**
 * Allocates and initializes the variable mapping structure (the map itself only allocated)
 */
void vmap_init(vmap_t **vm, size_t size);

/**
 * Appends the existing map with the next_var (with the same value as the 'old' variable)
 */
void vmap_add(vmap_t *vm, vars_t old);

/**
 * Clears and deallocates the map in the given mapping structure
 */
void vmap_clear(vmap_t *vm);

/**
 * Deallocates the given mapping structure
 */
void vmap_delete(vmap_t *vm);

/**
 * Looks for a mapping original - symbolic terminal
 */
qBDD vmap_lookup(vmap_t *m, qBDD a);

/**
 * Adds an entry to singly-linked list of mappings.
 */
void vmap_insert(vmap_t *m, qBDD orig, qBDD mapped);

/* CUSTOM MTBDD OPERATIONS */
// Basic operations:

/**
 * Converts the given MTBDD to a symbolic map MTBDD
 * 
 * @param t a regular MTBDD
 * 
 * @param m pointer to a vmap_t mapping
 * 
 */
qBDD my_mtbdd_to_symb_map_i(qBDD t, size_t m);
#endif
/* end of "mtbdd_symb_map.h" */