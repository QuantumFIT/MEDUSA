/* leaf_primitive_mpz.c */
#include "../../hash.h"
#include <stdint.h>
#include <stdbool.h>
#include <gmp.h>

/**
 * Taken from original Medusa
 */
uint64_t hash_comb_generic(leaf_primitive_t data) {
    uint64_t hash = 0;

    const mp_limb_t *limbs = mpz_limbs_read(data);
    size_t size = mpz_size(data);

    for (size_t i = 0; i < size; i++) {
        hash ^= limbs[i];
    }

    return hash;
}

int cmp_generic(leaf_primitive_t a, leaf_primitive_t b) {
    return mpz_cmp(a, b);
}

void init_generic(leaf_primitive_t x) {
    mpz_init(x);
}

void init_set_generic(leaf_primitive_t dst, leaf_primitive_t src) {
    mpz_init_set(dst, src);
}

void clear_generic(leaf_primitive_t x) {
    mpz_clear(x);
}

void mul_ui_generic(leaf_primitive_t r, leaf_primitive_t x, unsigned long c) {
    mpz_mul_ui(r, x, c);
}

void neg_generic(leaf_primitive_t r, leaf_primitive_t x) {
    mpz_neg(r, x);
}

void add_generic(leaf_primitive_t r, leaf_primitive_t a, leaf_primitive_t b) {
    mpz_add(r, a, b);
}

void init_set_ui_generic(leaf_primitive_t x, unsigned long v) {
    mpz_init_set_ui(x, v);
}

int sgn_generic(leaf_primitive_t x) {
    return mpz_sgn(x);
}

void set_ui_generic(leaf_primitive_t x, unsigned long v) {
    mpz_set_ui(x, v);
}

void set_generic(leaf_primitive_t dst, leaf_primitive_t src) {
    mpz_set(dst, src);
}

void mul_generic(leaf_primitive_t r, leaf_primitive_t a, leaf_primitive_t b) {
    mpz_mul(r, a, b);
}

void mul_mpz_generic(leaf_primitive_t r, leaf_primitive_t x, mpz_t s) {
    mpz_mul(r, x, s);
}

void inv_sqrt2_pow_generic(leaf_primitive_t x, mpz_t k) {
    mpz_set(x, k);
}