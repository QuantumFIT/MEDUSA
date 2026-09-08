/**
 * @file leaf_algebraic_mpz.h
 */

#ifndef LEAF_ALGEBRAIC_MPZ_H
#define LEAF_ALGEBRAIC_MPZ_H

#include "../leaf_primitives/leaf_primitive_mpz.h"
#include "../../interface.h"
#include "../../mtbdd_symb_map.h"
#include "../../mtbdd_symb_val.h"
#include "../../mtbdd_out.h"
#include "../../error.h"
#include "../../symexp.h"
#include "../../symexp_list.h"
#include "bdd.h"
#include <math.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include "mtbdd.h"

#define MAX_LEAF_STR_LEN  250
#define MAX_NUM_LEN       50
#define GET_MIN(a, b)     (((a) < (b)) ? (a) : (b))

extern mtbdd_terminal_type lt_classic;
extern mtbdd_terminal_type lt_symb_map;
extern mtbdd_terminal_type lt_symb_val;


#endif /* LEAF_ALGEBRAIC_MPZ_H */
