#include "hash.h"

uint64_t combine_mpz_t(mpz_t x) {
    uint64_t hash = 0;

    const mp_limb_t *limbs = mpz_limbs_read(x);
    size_t size = mpz_size(x);

    for (size_t i = 0; i < size; i++) {
        hash ^= limbs[i];
    }

    return hash;
}

uint64_t my_int_hash(uint64_t x) {
    x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
    x = x ^ (x >> 31);
    return x;
}

uint64_t my_str_hash(const char *str) {
    uint64_t hash = 0;
    int c;
    while ((c = *str++)) {
        hash = c + (hash << 6) + (hash << 16) - hash;
    }
    return hash;
}

/* end of "hash.c" */