/**
 * @file leaf_reim_double.h
 */

#ifndef LEAF_REIM_DOUBLE_H
#define LEAF_REIM_DOUBLE_H

#if defined(LEAF_BACKEND_MPFR)
  #include "../leaf_primitives/leaf_primitive_mpfr.h"
#elif defined(LEAF_BACKEND_GMP)
  #include "../leaf_primitives/leaf_primitive_mpz.h"
#else
  #include "../leaf_primitives/leaf_primitive_double.h"
#endif
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

#define SNAP_CHECK(x, label) \
    do { if (LEAF_ISNAN((x)[0])) { \
        printf( "NaN at: %s\n", label); abort(); } } while(0)

#endif /* LEAF_REIM_DOUBLE_H */
