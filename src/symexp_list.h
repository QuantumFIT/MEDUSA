/**
 * @file symexp_list.h
 * @brief List data type and operations representing a symbolic expression
 */

#include <stdint.h>
#include <gmp.h>

#ifndef SYMEXP_LIST_H
#define SYMEXP_LIST_H

/// Type for symbolic variables' coefficients
typedef mpz_t coefs_t;

/// Type for symbolic variables
typedef uint64_t vars_t;

/// Operations that can be performed on the symbolic values when updating the MTBDDs
typedef enum {
    SYMEXP_ADD,
    SYMEXP_SUB,
} symexp_op_t;

/// Data of the symbolic expression list element
typedef struct symexp_val {
    /// the coefficient by which the variable is multiplied
    coefs_t coef;
    /// variable
    vars_t var;
} symexp_val_t;

/// Type for symbolic expression list element
typedef struct symexp_el {
    symexp_val_t *data;
    struct symexp_el *next;
} symexp_el_t;

/// Type for representing the symbolic expression
typedef struct symexp_list {
    symexp_el_t *first;
    symexp_el_t *active;
} symexp_list_t;


/**
 * Creates and initializes a new list.
 */
symexp_list_t* symexp_list_create();

/**
 * Sets first list element as the active element.
 * 
 * @param l list representing a symbolic expression
 */
void symexp_list_first(symexp_list_t *l);

/**
 * Sets the current active element's next as the active element. 
 * If active element isn't set, active stays NULL.
 * 
 * @param l list representing a symbolic expression
 */
void symexp_list_next(symexp_list_t *l);

/**
 * Creates a new list element and inserts it as the first list element.
 * 
 * @param l list representing a symbolic expression
 */
void symexp_list_insert_first(symexp_list_t *l, symexp_val_t *val);

/**
 * Creates a new list element and inserts it as the next element after active
 * (doesn't do anything if active is not set).
 * 
 * @param l list representing a symbolic expression
 */
void symexp_list_insert_after(symexp_list_t *l, symexp_val_t *val);

/**
 * Removes the first list element. If first was active, active is unset.
 * 
 * @param l list representing a symbolic expression
 */
void symexp_list_remove_first(symexp_list_t *l);

/**
 * Removes the next element after active (doesn't do anything if active is not set).
 * 
 * @param l list representing a symbolic expression
 */
void symexp_list_remove_after(symexp_list_t *l);

/**
 * Negates all the coefficients in the expression
 * (instead of subtracting two lists use addition with one list negated).
 * 
 * @param l list representing a symbolic expression
 */
void symexp_list_neg(symexp_list_t *l);

/**
 * Multiplies all the coefficients in the expression with the coefficient.
 * 
 * @param l list representing a symbolic expression
 */
void symexp_list_mul_c(symexp_list_t *l, unsigned long c);

/**
 * Returns a deep copy of the given list.
 * 
 * @param l list representing a symbolic expression
 */
symexp_list_t* symexp_list_mkcpy(symexp_list_t *l);

/**
 * Deletes the given list.
 * 
 * @param l list representing a symbolic expression
 */
void symexp_list_del(symexp_list_t *l);

#endif
/* end of "symexp_list.h" */