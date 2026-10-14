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
    mul_d_generic(r, x, mpz_get_d(s));
}

void mul_inv_sqrt2_pow_generic(leaf_primitive_t r, leaf_primitive_t x, mpz_t k) {
    long ki = mpz_get_si(k);
    leaf_scalar_t scale = leaf_sqrt2inv_pow(ki);
    leaf_primitive_t sp = { scale };
    mul_generic(r, x, sp);
}

void inv_sqrt2_pow_generic(leaf_primitive_t x, mpz_t k) {
    long ki = mpz_get_si(k);
    leaf_scalar_t scale = leaf_sqrt2inv_pow(ki);
    x[0] = scale;
}

#if defined(LEAF_SCALAR_IS_SOFT)
/* "%.30g" of a SoftFloat binary128, digit for digit as glibc's strfromf128
 * prints it: the value M * 2^ex is exact, so 30 significant digits are found
 * by integer division with round-half-even, then laid out by the %g rules
 * (exponent form below 1e-4 or from 1e30, trailing zeros dropped). */
static void leaf_s_to_str(float128_t x, char *buf, size_t bufsize) {
    enum { P = 30 };
    int neg = (int)(x.v[1] >> 63);
    unsigned e = (unsigned)(x.v[1] >> 48) & 0x7fff;
    uint64_t hi = x.v[1] & 0xffffffffffffULL, lo = x.v[0];
    if (e == 0x7fff) {
        snprintf(buf, bufsize, (hi | lo) ? "nan" : (neg ? "-inf" : "inf"));
        return;
    }
    if (e == 0 && (hi | lo) == 0) {
        snprintf(buf, bufsize, neg ? "-0" : "0");
        return;
    }
    mpz_t num, den, q, r, lim;
    mpz_inits(num, den, q, r, lim, NULL);
    /* significand = [implicit 1] . hi (48 bits) . lo (64 bits), built in
     * 32-bit pieces so it does not depend on the width of unsigned long */
    mpz_set_ui(num, e ? 1 : 0);
    mpz_mul_2exp(num, num, 16);  mpz_add_ui(num, num, (unsigned long)(hi >> 32));
    mpz_mul_2exp(num, num, 32);  mpz_add_ui(num, num, (unsigned long)(hi & 0xffffffffULL));
    mpz_mul_2exp(num, num, 32);  mpz_add_ui(num, num, (unsigned long)(lo >> 32));
    mpz_mul_2exp(num, num, 32);  mpz_add_ui(num, num, (unsigned long)(lo & 0xffffffffULL));
    long ex = (long)(e ? e : 1) - 16383 - 112;
    /* decimal exponent estimate, corrected below */
    long E = (long)floor(((double)mpz_sizeinbase(num, 2) - 1 + (double)ex) * 0.30102999566398120);
    mpz_ui_pow_ui(lim, 10, P);
    for (;;) {
        long sc = P - 1 - E;                    /* q = round(v * 10^sc) */
        mpz_set(q, num);
        mpz_set_ui(den, 1);
        if (ex >= 0) mpz_mul_2exp(q, q, (mp_bitcnt_t)ex);
        else         mpz_mul_2exp(den, den, (mp_bitcnt_t)-ex);
        if (sc >= 0) { mpz_ui_pow_ui(r, 10, (unsigned long)sc);  mpz_mul(q, q, r); }
        else         { mpz_ui_pow_ui(r, 10, (unsigned long)-sc); mpz_mul(den, den, r); }
        mpz_tdiv_qr(q, r, q, den);
        mpz_mul_2exp(r, r, 1);
        int c = mpz_cmp(r, den);
        if (c > 0 || (c == 0 && mpz_odd_p(q))) mpz_add_ui(q, q, 1);
        if (mpz_cmp(q, lim) >= 0) { E++; continue; }          /* 10^P digits */
        mpz_tdiv_q_ui(r, lim, 10);
        if (mpz_cmp(q, r) < 0) { E--; continue; }             /* P-1 digits */
        break;
    }
    char d[P + 1];
    mpz_get_str(d, 10, q);                      /* exactly P digits */
    int n = P;
    while (n > 1 && d[n - 1] == '0') n--;       /* %g drops trailing zeros */
    d[n] = '\0';
    const char *sg = neg ? "-" : "";
    char z[P + 5];                              /* zeros for fixed notation */
    if (E < -4 || E >= P) {                     /* d[.ddd]e+XX */
        snprintf(buf, bufsize, "%s%c%s%se%c%02ld", sg, d[0], n > 1 ? "." : "", d + 1,
                 E < 0 ? '-' : '+', E < 0 ? -E : E);
    } else if (E < 0) {                         /* 0.000ddd */
        memset(z, '0', (size_t)(-E - 1)); z[-E - 1] = '\0';
        snprintf(buf, bufsize, "%s0.%s%s", sg, z, d);
    } else if (n <= E + 1) {                    /* ddd000 */
        memset(z, '0', (size_t)(E + 1 - n)); z[E + 1 - n] = '\0';
        snprintf(buf, bufsize, "%s%s%s", sg, d, z);
    } else {                                    /* ddd.ddd */
        snprintf(buf, bufsize, "%s%.*s.%s", sg, (int)(E + 1), d, d + E + 1);
    }
    mpz_clears(num, den, q, r, lim, NULL);
}
#endif

void to_str_generic(leaf_primitive_t x, char *buf, size_t bufsize) {
#if defined(LEAF_SCALAR_IS_SOFT)
    leaf_s_to_str(x[0], buf, bufsize);
#elif LEAF_FLOAT_TYPE == LEAF_TYPE_QUAD && defined(LEAF_QUAD_IS_LONG_DOUBLE)
    snprintf(buf, bufsize, "%.30Lg", x[0]);
#elif LEAF_FLOAT_TYPE == LEAF_TYPE_QUAD
    strfromf128(buf, bufsize, "%.30g", x[0]);
#else
    snprintf(buf, bufsize, "%.17Lg", (long double)x[0]);
#endif
}
