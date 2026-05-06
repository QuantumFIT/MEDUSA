/* leaf_primitive_double.c */
#include "../../hash.h"
#include <stdint.h>
#include <stdbool.h>
#include "leaf_primitive_double.h"
#include <string.h>


int cmp_generic(leaf_primitive_t a, leaf_primitive_t b) {
    return !((a[0]) == (b[0]));
}

uint64_t hash_comb_generic(leaf_primitive_t data) {
    uint8_t bytes[sizeof(leaf_scalar_t)];
    memcpy(bytes, &data[0], sizeof(leaf_scalar_t));

    // edited fmix64 finalizer from MurmurHash3 by Austin Appleby (public domain)
    // https://github.com/aappleby/smhasher/blob/master/src/MurmurHash3.cpp
    uint64_t bits = 0;
    // fold bytes based on the scalar for each size
    for (size_t i = 0; i < sizeof(leaf_scalar_t); i++) {
        bits ^= (uint64_t)bytes[i] << ((i % 8) * 8);
    }
    bits ^= bits >> 33;
    bits *= 0xff51afd7ed558ccdULL;
    bits ^= bits >> 33;
    return bits;
}



void init_generic(leaf_primitive_t x) {
    x[0] = 0.0;
}

void init_set_generic(leaf_primitive_t dst, leaf_primitive_t src) {
    dst[0] = src[0];
}

void clear_generic(leaf_primitive_t x) {
    x[0] = 0.0;
}

void mul_ui_generic(leaf_primitive_t r, leaf_primitive_t x, unsigned long c) {
    r[0] = x[0] * (leaf_scalar_t)c;
}

void neg_generic(leaf_primitive_t r, leaf_primitive_t x) {
    r[0] = -x[0];
}

void add_generic(leaf_primitive_t r, leaf_primitive_t a, leaf_primitive_t b) {
    r[0] = a[0] + b[0];
}

void init_set_ui_generic(leaf_primitive_t x, unsigned long v) {
    x[0] = (leaf_scalar_t)v;
}

int sgn_generic(leaf_primitive_t x) {
    leaf_scalar_t s = (x[0]);
    if (s > LEAF_ZERO) return 1;
    if (s < LEAF_ZERO) return -1;
    return 0;
}

void set_ui_generic(leaf_primitive_t x, unsigned long v) {
    x[0] = (leaf_scalar_t)v;
}

void set_generic(leaf_primitive_t dst, leaf_primitive_t src) {
    dst[0] = src[0];
}

void mul_generic(leaf_primitive_t r, leaf_primitive_t a, leaf_primitive_t b) {
    r[0] = a[0] * b[0];
}

void mul_mpz_generic(leaf_primitive_t r, leaf_primitive_t x, mpz_t s) {
    r[0] = x[0] * (leaf_scalar_t)mpz_get_d(s);
}

void set_d_generic(leaf_primitive_t x, double v) {
    x[0] = (leaf_scalar_t)v;
}

void sub_generic(leaf_primitive_t r, leaf_primitive_t a, leaf_primitive_t b) {
    r[0] = a[0] - b[0];
}

void div_generic(leaf_primitive_t r, leaf_primitive_t a, leaf_primitive_t b) {
    r[0] = a[0] / b[0];
}

void mul_d_generic(leaf_primitive_t r, leaf_primitive_t x, double s) {
    r[0] = x[0] * (leaf_scalar_t)s;
}

void mul_sqrt2inv_generic(leaf_primitive_t r, leaf_primitive_t x) {
    r[0] = x[0] * (leaf_scalar_t)LEAF_SQRT2INV;
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

double to_double_generic(leaf_primitive_t x) {
    return (double)x[0];
}

void to_str_generic(leaf_primitive_t x, char *buf, size_t bufsize) {
#if LEAF_FLOAT_TYPE == LEAF_TYPE_QUAD
    quadmath_snprintf(buf, bufsize, "%.30Qg", x[0]);
#else
    snprintf(buf, bufsize, "%.17Lg", (long double)x[0]);
#endif
}