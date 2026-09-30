/* leaf_primitive_double.c - out-of-line ops only (hash / to_str / mpz scale).
 * Hot arithmetic lives as static inline in leaf_primitive_double.h. */
#include "../../hash.h"
#include <stdint.h>
#include <stdbool.h>
#include "leaf_primitive_double.h"
#include <string.h>
#include <gmp.h>

uint64_t hash_comb_generic(leaf_primitive_t data) {
    // Only the value-carrying bytes, never the padding: see the comment on
    // LEAF_SCALAR_HASH_BYTES. cmp_generic compares numerically, so hashing
    // indeterminate padding would let equal leaves hash differently and break
    // terminal dedup.
    uint8_t bytes[sizeof(leaf_scalar_t)];
    memcpy(bytes, &data[0], LEAF_SCALAR_HASH_BYTES);

    // edited fmix64 finalizer from MurmurHash3 by Austin Appleby (public domain)
    // https://github.com/aappleby/smhasher/blob/master/src/MurmurHash3.cpp
    uint64_t bits = 0;
    // fold bytes based on the scalar for each size
    for (size_t i = 0; i < LEAF_SCALAR_HASH_BYTES; i++) {
        bits ^= (uint64_t)bytes[i] << ((i % 8) * 8);
    }
    bits ^= bits >> 33;
    bits *= 0xff51afd7ed558ccdULL;
    bits ^= bits >> 33;
    return bits;
}

void mul_mpz_generic(leaf_primitive_t r, leaf_primitive_t x, mpz_t s) {
    r[0] = x[0] * (leaf_scalar_t)mpz_get_d(s);
}

void mul_inv_sqrt2_pow_generic(leaf_primitive_t r, leaf_primitive_t x, mpz_t k) {
    long ki = mpz_get_si(k);
    leaf_scalar_t scale = LEAF_POW(LEAF_SQRT2INV, (leaf_scalar_t)ki);
    r[0] = x[0] * scale;
}

void inv_sqrt2_pow_generic(leaf_primitive_t x, mpz_t k) {
    long ki = mpz_get_si(k);
    leaf_scalar_t scale = LEAF_POW(LEAF_SQRT2INV, (leaf_scalar_t)ki);
    x[0] = scale;
}

void to_str_generic(leaf_primitive_t x, char *buf, size_t bufsize) {
#if LEAF_FLOAT_TYPE == LEAF_TYPE_QUAD
    quadmath_snprintf(buf, bufsize, "%.30Qg", x[0]);
#else
    snprintf(buf, bufsize, "%.17Lg", (long double)x[0]);
#endif
}
