/* leaf_primitive_mpz.h */
#ifndef LEAF_PRIMITIVE_MPZ_H
#define LEAF_PRIMITIVE_MPZ_H

#include <stdint.h>
#include <stdbool.h>
#include <gmp.h>

#ifndef LEAF_PRIMITIVE_DEFINED
#define LEAF_PRIMITIVE_DEFINED
typedef mpz_t leaf_primitive_t;
typedef double      leaf_scalar_t;
#endif
#define LEAF_TYPE_MPZ 4
#define LEAF_FLOAT_TYPE LEAF_TYPE_MPZ
#endif /* LEAF_PRIMITIVE_MPZ_H */
