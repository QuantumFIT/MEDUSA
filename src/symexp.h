/**
 * @file symexp.h
 * @brief Arithmetic and hash table operations for symbolic expressions
 */

#include <stdbool.h>
#include "symexp_list.h"

#ifndef SYMEXP_H
#define SYMEXP_H


/// Value for distinguishing between uninitialized expression and NULL expression
#define SYMEXP_NULL (void*)1

/// Max. string size when converting expression from tree to string
#define MAX_ST_TO_STR_LEN 2000

/**
 * Initializes the symbolic hash table - must be initialized before any symbolic value MTBDD can be initialized!
 */
void symexp_htab_init(size_t size);

/**
 * Clears the symbolic hash table - deletes all items (the array size is not changed)
 */
void symexp_htab_clear();

/**
 * Deletes the symbolic hash table
 */
void symexp_htab_delete();

/**
 * Creates a new tree for the given complex number coefficient variable
 */
symexp_list_t* symexp_init(vars_t v);

/**
 * Multiplies the tree with the given coefficient
 */
symexp_list_t* symexp_mul_c(symexp_list_t *a, unsigned long c);

/**
 * Performs the given operation on the two trees (a op b) and returns the result
 */
symexp_list_t* symexp_op(symexp_list_t *a, symexp_list_t *b, symexp_op_t op);

/**
 * Returns true if the two trees are equal
 */
bool symexp_cmp(symexp_list_t *a, symexp_list_t *b);

/**
 * Returns the string representation of the expression represented by tree
 */
char* symexp_to_str(symexp_list_t *l);

/**
 * Returns true if the first variable of the expression is true in the given array.
 */
bool symexp_is_first_var_marked(symexp_list_t *l, bool *is_zero);

#endif
/* end of "symexp.h" */