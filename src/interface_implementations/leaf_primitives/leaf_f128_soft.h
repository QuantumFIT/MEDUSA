/* leaf_f128_soft.h - f128 leaves on Berkeley SoftFloat 3e.
 *
 * For targets with no IEEE binary128 type at all (arm64 macOS: no __float128,
 * and long double is a double). Included by leaf_primitive_double.h when
 * LEAF_F128_SOFTFLOAT is defined (make SOFTFLOAT=1, the default on macOS).
 *
 * SoftFloat's operations are correctly rounded IEEE binary128, like the
 * compiler's __float128 soft-float routines, so results match native f128
 * bit for bit. float128_t is a struct, so C operators do not apply: this
 * header provides the *_generic API and snap() on top of SoftFloat calls, and
 * the LEAF_S_* macros that the few call sites outside it use instead of
 * operators. Probabilities (prob_t) are double in this mode.
 */
#ifndef LEAF_F128_SOFT_H
#define LEAF_F128_SOFT_H

#include <string.h>
#define SOFTFLOAT_FAST_INT64   /* what lib/softfloat is built with: pass-by-value API */
#include <softfloat.h>

#define LEAF_SCALAR_IS_SOFT 1

typedef float128_t      leaf_primitive_t[1];
typedef float128_t      leaf_scalar_t;

/* Constants as binary128 bit patterns {low, high} (little-endian, as
 * SoftFloat stores them), equal to the q-suffixed literals of the
 * __float128 build. LEAF_REL_EPS / LEAF_ABS_EPS cannot be overridden here. */
#define LEAF_F128_BITS(lo, hi) ((float128_t){ { (lo), (hi) } })
#define LEAF_ZERO        LEAF_F128_BITS(0, 0)
#define LEAF_ONE         LEAF_F128_BITS(0, 0x3fff000000000000ULL)
#define LEAF_REL_EPS     LEAF_F128_BITS(0x941b17953adc3111ULL, 0x3fa1fb0f6be50601ULL) /* 1e-28 */
#define LEAF_ABS_EPS     LEAF_F128_BITS(0x2974cfbc31db4b03ULL, 0x3f949f623d5a8a73ULL) /* 1e-32 */
#define LEAF_INV_ABS_EPS LEAF_F128_BITS(0x6b3be04000000000ULL, 0x40693b8b5b5056e1ULL) /* 1/fl(1e-32) */
#define LEAF_SQRT2INV    LEAF_F128_BITS(0xc908b2fb1366ea95ULL, 0x3ffe6a09e667f3bcULL) /* 1/sqrt(2) */

static inline float128_t leaf_s_from_d(double d) {
    float64_t f; memcpy(&f, &d, sizeof d); return f64_to_f128(f);
}
static inline double leaf_s_to_d(float128_t x) {
    float64_t f = f128_to_f64(x); double d; memcpy(&d, &f, sizeof d); return d;
}
static inline float128_t leaf_s_neg(float128_t x) { x.v[1] ^= (uint64_t)1 << 63; return x; }
static inline float128_t leaf_s_abs(float128_t x) { x.v[1] &= ~((uint64_t)1 << 63); return x; }

#define LEAF_ISNAN(x)    (!f128_eq((x), (x)))
#define LEAF_ABS(x)      leaf_s_abs(x)
#define LEAF_SQRT(x)     f128_sqrt(x)
#define LEAF_ROUND(x)    f128_roundToInt((x), softfloat_round_near_maxMag, false)

/* Operators for the call sites outside this header. */
#define LEAF_S_ADD(a, b)     f128_add((a), (b))
#define LEAF_S_SUB(a, b)     f128_sub((a), (b))
#define LEAF_S_MUL(a, b)     f128_mul((a), (b))
#define LEAF_S_LT(a, b)      f128_lt((a), (b))
#define LEAF_S_LE(a, b)      f128_le((a), (b))
#define LEAF_S_IS_ZERO(x)    f128_eq((x), LEAF_ZERO)
#define LEAF_S_TO_DOUBLE(x)  leaf_s_to_d(x)
#define LEAF_S_FROM_DOUBLE(d) leaf_s_from_d(d)
#define LEAF_S_TO_PROB(x)    leaf_s_to_d(x)
#define LEAF_S_FROM_PROB(p)  leaf_s_from_d(p)

/* (1/sqrt 2)^k, exactly rounded: 2^-q, times 1/sqrt 2 when k is odd. */
static inline float128_t leaf_sqrt2inv_pow(long k) {
    long q = k >= 0 ? k / 2 : -((-k + 1) / 2);       /* floor(k / 2) */
    float128_t r = LEAF_F128_BITS(0, (uint64_t)(16383 - q) << 48);
    return (k - 2 * q) ? f128_mul(r, LEAF_SQRT2INV) : r;
}

#define LEAF_SCALAR_HASH_BYTES sizeof(leaf_scalar_t)
#define LEAF_PRIMITIVE_OPS_INLINED 1

static inline int cmp_generic(leaf_primitive_t a, leaf_primitive_t b) {
    return !f128_eq(a[0], b[0]);
}
static inline void init_generic(leaf_primitive_t x) { x[0] = LEAF_ZERO; }
static inline void init_set_generic(leaf_primitive_t dst, leaf_primitive_t src) { dst[0] = src[0]; }
static inline void clear_generic(leaf_primitive_t x) { x[0] = LEAF_ZERO; }
static inline void mul_ui_generic(leaf_primitive_t r, leaf_primitive_t x, unsigned long c) {
    r[0] = f128_mul(x[0], ui64_to_f128(c));
}
static inline void neg_generic(leaf_primitive_t r, leaf_primitive_t x) { r[0] = leaf_s_neg(x[0]); }
static inline void add_generic(leaf_primitive_t r, leaf_primitive_t a, leaf_primitive_t b) {
    r[0] = f128_add(a[0], b[0]);
}
static inline void init_set_ui_generic(leaf_primitive_t x, unsigned long v) { x[0] = ui64_to_f128(v); }
static inline int sgn_generic(leaf_primitive_t x) {
    if (f128_lt(LEAF_ZERO, x[0])) return 1;
    if (f128_lt(x[0], LEAF_ZERO)) return -1;
    return 0;
}
static inline void set_ui_generic(leaf_primitive_t x, unsigned long v) { x[0] = ui64_to_f128(v); }
static inline void set_generic(leaf_primitive_t dst, leaf_primitive_t src) { dst[0] = src[0]; }
static inline void mul_generic(leaf_primitive_t r, leaf_primitive_t a, leaf_primitive_t b) {
    r[0] = f128_mul(a[0], b[0]);
}
static inline void set_d_generic(leaf_primitive_t x, double v) { x[0] = leaf_s_from_d(v); }
static inline void sub_generic(leaf_primitive_t r, leaf_primitive_t a, leaf_primitive_t b) {
    r[0] = f128_sub(a[0], b[0]);
}
static inline void div_generic(leaf_primitive_t r, leaf_primitive_t a, leaf_primitive_t b) {
    r[0] = f128_div(a[0], b[0]);
}
static inline void mul_d_generic(leaf_primitive_t r, leaf_primitive_t x, double s) {
    r[0] = f128_mul(x[0], leaf_s_from_d(s));
}
static inline void mul_sqrt2inv_generic(leaf_primitive_t r, leaf_primitive_t x) {
    r[0] = f128_mul(x[0], LEAF_SQRT2INV);
}
static inline double to_double_generic(leaf_primitive_t x) { return leaf_s_to_d(x[0]); }

/* Same grid snap as the native header (see the comment there). */
static inline leaf_scalar_t snap(leaf_scalar_t x) {
    if (LEAF_ISNAN(x)) {
        printf("ERROR: NaN detected. Possible numerical instability.\n");
        exit(1);
    }
    if (f128_lt(LEAF_ABS(x), LEAF_ABS_EPS)) return LEAF_ZERO;
    return f128_mul(LEAF_ROUND(f128_mul(x, LEAF_INV_ABS_EPS)), LEAF_ABS_EPS);
}

#endif /* LEAF_F128_SOFT_H */
