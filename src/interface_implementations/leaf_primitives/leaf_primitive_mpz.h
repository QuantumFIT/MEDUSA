/* leaf_primitive_mpz.h */
#ifndef LEAF_PRIMITIVE_MPZ_H
#define LEAF_PRIMITIVE_MPZ_H

#include <stdint.h>
#include <stdbool.h>
/* The GMP build force-includes this header (-include) ahead of every
 * source file, so it decides what gmp.h declares. <stdio.h> must come
 * first: gmp.h declares its FILE functions (mpz_out_str, gmp_fprintf, ...)
 * only if stdio.h was already included. Without that, calls to them are
 * implicit declarations, an error since GCC 14. */
#include <stdio.h>
#include <gmp.h>

#ifndef LEAF_PRIMITIVE_DEFINED
#define LEAF_PRIMITIVE_DEFINED
typedef mpz_t leaf_primitive_t;
typedef double      leaf_scalar_t;
#endif
#define LEAF_TYPE_MPZ 4
#define LEAF_FLOAT_TYPE LEAF_TYPE_MPZ
#endif /* LEAF_PRIMITIVE_MPZ_H */
