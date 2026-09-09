/* leaf_primitive_double.h */
#ifndef LEAF_PRIMITIVE_DOUBLE_H
#define LEAF_PRIMITIVE_DOUBLE_H

#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef LEAF_PRIMITIVE_DEFINED
#define LEAF_PRIMITIVE_DEFINED

/* Select type via -DLEAF_FLOAT_TYPE=0/1/2/3 */
#define LEAF_TYPE_FLOAT       0
#define LEAF_TYPE_DOUBLE      1
#define LEAF_TYPE_LONGDOUBLE  2
#define LEAF_TYPE_QUAD        3

#ifndef LEAF_FLOAT_TYPE
#define LEAF_FLOAT_TYPE LEAF_TYPE_DOUBLE   // default
#endif

/*  float  */
#if LEAF_FLOAT_TYPE == LEAF_TYPE_FLOAT
typedef float           leaf_primitive_t[1];
typedef float           leaf_scalar_t;
#define LEAF_ROUND      roundf
#define LEAF_ZERO       0.0f
#define LEAF_ONE        1.0f
#define LEAF_ABS        fabsf
#define LEAF_SQRT       sqrtf
#define LEAF_POW        powf
#define LEAF_ISNAN(x)   ((x) != (x))
#ifndef LEAF_REL_EPS
#define LEAF_REL_EPS    1e-6f
#endif
#ifndef LEAF_ABS_EPS
#define LEAF_ABS_EPS    1e-6f
#endif
#define LEAF_SQRT2INV   0.707106781f                // 1/sqrt(2), ~7 sig. digits

/*  double  */
#elif LEAF_FLOAT_TYPE == LEAF_TYPE_DOUBLE
typedef double          leaf_primitive_t[1];
typedef double          leaf_scalar_t;
#define LEAF_ROUND      round
#define LEAF_ZERO       0.0
#define LEAF_ONE        1.0
#define LEAF_ABS        fabs
#define LEAF_SQRT       sqrt
#define LEAF_POW        pow
#define LEAF_ISNAN(x)   ((x) != (x))
#ifndef LEAF_REL_EPS
#define LEAF_REL_EPS    1e-9
#endif
#ifndef LEAF_ABS_EPS
#define LEAF_ABS_EPS    1e-9
#endif
#define LEAF_SQRT2INV   M_SQRT1_2                   // 0.7071067811865475... from <math.h>

/*  long double  */
#elif LEAF_FLOAT_TYPE == LEAF_TYPE_LONGDOUBLE
typedef long double     leaf_primitive_t[1];
typedef long double     leaf_scalar_t;
#define LEAF_ROUND      roundl
#define LEAF_ZERO       0.0L
#define LEAF_ONE        1.0L
#define LEAF_ABS        fabsl
#define LEAF_SQRT       sqrtl
#define LEAF_POW        powl
#define LEAF_ISNAN(x)   ((x) != (x))
#ifndef LEAF_REL_EPS
#define LEAF_REL_EPS    1e-12L
#endif
#ifndef LEAF_ABS_EPS
#define LEAF_ABS_EPS    1e-12L
#endif
#define LEAF_SQRT2INV   0.707106781186547524400844362104849039L  // 80-bit full precision

/*  __float128  */
#elif LEAF_FLOAT_TYPE == LEAF_TYPE_QUAD
#include <quadmath.h>
typedef __float128      leaf_primitive_t[1];
typedef __float128      leaf_scalar_t;
#define LEAF_ROUND      roundq
#define LEAF_ZERO       0.0q
#define LEAF_ONE        1.0q
#define LEAF_ABS        fabsq
#define LEAF_SQRT       sqrtq
#define LEAF_POW        powq
#define LEAF_ISNAN(x)   isnanq(x)
#ifndef LEAF_REL_EPS
#define LEAF_REL_EPS    1e-28q
#endif
#ifndef LEAF_ABS_EPS
#define LEAF_ABS_EPS    1e-32q
#endif
#define LEAF_SQRT2INV   M_SQRT1_2q

#else
#error "Unknown LEAF_FLOAT_TYPE"
#endif

/*
 * Bytes of leaf_scalar_t that actually carry the value.
 *
 * This is not always sizeof(leaf_scalar_t). The x87 80-bit long double is ten
 * bytes of value padded out to sixteen for alignment, and the six padding bytes
 * are indeterminate - they hold whatever was in the storage beforehand, since
 * leaves are malloc'd rather than zeroed.
 *
 * That matters because cmp_generic compares scalars numerically (a[0] == b[0])
 * while hash_comb_generic hashes their representation. Folding the padding into
 * the hash breaks the hash/equality contract: two leaves that compare equal can
 * hash differently depending on leftover heap contents, so terminal dedup starts
 * storing duplicates of the same amplitude. Symptoms are intermittent and
 * load-dependent, and only long double is affected - float, double and
 * __float128 have no padding.
 *
 * LDBL_MANT_DIG identifies the representation: 64 is the x87 extended format,
 * 113 IEEE binary128, 53 a long double that is really a double.
 */
#include <float.h>

#if LEAF_FLOAT_TYPE == LEAF_TYPE_LONGDOUBLE && LDBL_MANT_DIG == 64
#define LEAF_SCALAR_HASH_BYTES 10
#else
#define LEAF_SCALAR_HASH_BYTES sizeof(leaf_scalar_t)
#endif

#endif /* LEAF_PRIMITIVE_DEFINED */

static inline leaf_scalar_t snap(leaf_scalar_t x) {
    if (LEAF_ISNAN(x)) {printf("ERROR: NaN detected. Possible numerical instability.\n"); exit(1);}
    if (LEAF_ABS(x) < LEAF_ABS_EPS) return LEAF_ZERO;
    return LEAF_ROUND(x / LEAF_ABS_EPS) * LEAF_ABS_EPS;
}

#endif /* LEAF_PRIMITIVE_DOUBLE_H */