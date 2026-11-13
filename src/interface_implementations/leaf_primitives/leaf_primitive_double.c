/* leaf_primitive_double.c - out-of-line ops only (hash / to_str / mpz scale).
 * Hot arithmetic lives as static inline in leaf_primitive_double.h. */
#include "../../hash.h"
#include <stdint.h>
#include <stdbool.h>
#include "leaf_primitive_double.h"
#include <string.h>
#include <gmp.h>

/* The hash copies LEAF_SCALAR_HASH_BYTES out of one scalar. */
_Static_assert(LEAF_SCALAR_HASH_BYTES <= sizeof(leaf_scalar_t),
               "LEAF_SCALAR_HASH_BYTES must not exceed the size of leaf_scalar_t");

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
enum { LEAF_S_DIGITS = 30 };

/* The significand M of a finite nonzero x into num; returns ex, x = M * 2^ex.
 * M = [implicit 1] . hi (48 bits) . lo (64 bits), built in 32-bit pieces so
 * it does not depend on the width of unsigned long. */
static long leaf_s_significand(mpz_t num, float128_t x) {
    unsigned e = (unsigned)(x.v[1] >> 48) & 0x7fff;
    uint64_t hi = x.v[1] & 0xffffffffffffULL, lo = x.v[0];
    mpz_set_ui(num, e ? 1 : 0);
    mpz_mul_2exp(num, num, 16);  mpz_add_ui(num, num, (unsigned long)(hi >> 32));
    mpz_mul_2exp(num, num, 32);  mpz_add_ui(num, num, (unsigned long)(hi & 0xffffffffULL));
    mpz_mul_2exp(num, num, 32);  mpz_add_ui(num, num, (unsigned long)(lo >> 32));
    mpz_mul_2exp(num, num, 32);  mpz_add_ui(num, num, (unsigned long)(lo & 0xffffffffULL));
    return (long)(e ? e : 1) - 16383 - 112;
}

/* q = round-half-even(M * 2^ex * 10^sc) */
static void leaf_s_scaled(mpz_t q, const mpz_t num, long ex, long sc) {
    mpz_t den, r;
    mpz_inits(den, r, NULL);
    mpz_set(q, num);
    mpz_set_ui(den, 1);
    if (ex >= 0) mpz_mul_2exp(q, q, (mp_bitcnt_t)ex);
    else         mpz_mul_2exp(den, den, (mp_bitcnt_t)-ex);
    mpz_ui_pow_ui(r, 10, (unsigned long)(sc >= 0 ? sc : -sc));
    if (sc >= 0) mpz_mul(q, q, r);
    else         mpz_mul(den, den, r);
    mpz_tdiv_qr(q, r, q, den);
    mpz_mul_2exp(r, r, 1);
    int c = mpz_cmp(r, den);
    if (c > 0 || (c == 0 && mpz_odd_p(q))) mpz_add_ui(q, q, 1);
    mpz_clears(den, r, NULL);
}

/* The LEAF_S_DIGITS significant digits of M * 2^ex into d, *n of them once
 * trailing zeros are dropped (as %g does); returns the decimal exponent E of
 * the first digit. */
static long leaf_s_decimal(char *d, int *n, const mpz_t num, long ex) {
    mpz_t q, lim, low;
    mpz_inits(q, lim, low, NULL);
    mpz_ui_pow_ui(lim, 10, LEAF_S_DIGITS);
    mpz_tdiv_q_ui(low, lim, 10);
    /* estimate from the bit length, then correct by one where it is off */
    long E = (long)floor(((double)mpz_sizeinbase(num, 2) - 1 + (double)ex) * 0.30102999566398120);
    for (;;) {
        leaf_s_scaled(q, num, ex, LEAF_S_DIGITS - 1 - E);
        if (mpz_cmp(q, lim) >= 0)      E++;     /* one digit too many */
        else if (mpz_cmp(q, low) < 0)  E--;     /* one too few */
        else break;
    }
    mpz_get_str(d, 10, q);
    *n = LEAF_S_DIGITS;
    while (*n > 1 && d[*n - 1] == '0') (*n)--;
    d[*n] = '\0';
    mpz_clears(q, lim, low, NULL);
    return E;
}

/* Lay the digits d (n of them, first at 10^E) out as %g does. */
static void leaf_s_layout(char *buf, size_t bufsize, int neg, const char *d, long n, long E) {
    static const char zeros[] = "0000000000000000000000000000000000";  /* > LEAF_S_DIGITS + 3 */
    const char *sg = neg ? "-" : "";
    if (E < -4 || E >= LEAF_S_DIGITS)           /* d[.ddd]e+XX */
        snprintf(buf, bufsize, "%s%c%s%se%c%02ld", sg, d[0], n > 1 ? "." : "", d + 1,
                 E < 0 ? '-' : '+', E < 0 ? -E : E);
    else if (E < 0)                             /* 0.000ddd */
        snprintf(buf, bufsize, "%s0.%.*s%s", sg, (int)(-E - 1), zeros, d);
    else if (n <= E + 1)                        /* ddd000 */
        snprintf(buf, bufsize, "%s%s%.*s", sg, d, (int)(E + 1 - n), zeros);
    else                                        /* ddd.ddd */
        snprintf(buf, bufsize, "%s%.*s.%s", sg, (int)(E + 1), d, d + E + 1);
}

static void leaf_s_to_str(float128_t x, char *buf, size_t bufsize) {
    int neg = (int)(x.v[1] >> 63);
    unsigned e = (unsigned)(x.v[1] >> 48) & 0x7fff;
    int frac = (x.v[1] & 0xffffffffffffULL) != 0 || x.v[0] != 0;
    if (e == 0x7fff) {
        snprintf(buf, bufsize, "%s", frac ? "nan" : (neg ? "-inf" : "inf"));
        return;
    }
    if (e == 0 && !frac) {
        snprintf(buf, bufsize, "%s", neg ? "-0" : "0");
        return;
    }
    char d[LEAF_S_DIGITS + 1];
    mpz_t num;
    mpz_init(num);
    long ex = leaf_s_significand(num, x);
    int n;
    long E = leaf_s_decimal(d, &n, num, ex);
    mpz_clear(num);
    leaf_s_layout(buf, bufsize, neg, d, n, E);
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
