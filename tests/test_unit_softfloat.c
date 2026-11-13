/**
 * Unit tests for f128 on Berkeley SoftFloat (leaf_f128_soft.h, SOFTFLOAT=1).
 *
 * The SoftFloat build has to give the same results as the native binary128
 * one, but it reimplements three things that the native build gets from the
 * compiler and glibc: the leaf helpers and snap() on top of SoftFloat calls,
 * (1/sqrt 2)^k by constructing powers of two (instead of powf128), and "%.30g"
 * printing (instead of strfromf128). The circuit suites compare amplitudes
 * with a tolerance, so a wrong last digit in any of these would pass them.
 *
 * It also checks that SoftFloat's state is per thread, as Sylvan's worker
 * threads need. Two kinds of checks otherwise:
 *  - fixed expectations (bit patterns, strings printed by glibc's strfromf128),
 *    which run everywhere, macOS included;
 *  - where the compiler also has __float128 and glibc its *f128 functions
 *    (x86_64 Linux, the CI's SoftFloat job), a comparison against them on
 *    many random values: SoftFloat is bit-compatible with __float128 there.
 */
#include "test_harness.h"

#include <limits.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "leaf_primitive_double.h"

#ifndef LEAF_SCALAR_IS_SOFT
#error "test_unit_softfloat is for the SoftFloat f128 build (make SOFTFLOAT=1)"
#endif

#if defined(__SIZEOF_FLOAT128__) && defined(__GLIBC__) \
    && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 26))
#define HAVE_NATIVE_F128 1
extern int strfromf128(char *, size_t, const char *, __float128);
extern __float128 roundf128(__float128);
extern __float128 sqrtf128(__float128);
extern __float128 ldexpf128(__float128, int);
#endif

static float128_t bits(uint64_t lo, uint64_t hi) { return LEAF_F128_BITS(lo, hi); }

static int same_bits(float128_t a, float128_t b) {
    return a.v[0] == b.v[0] && a.v[1] == b.v[1];
}

static void to_str(float128_t x, char *buf, size_t n) {
    leaf_primitive_t p = { x };
    to_str_generic(p, buf, n);
}

/* xorshift64*: deterministic, so a failure reproduces. */
static uint64_t rng_state = 0x9e3779b97f4a7c15ULL;
static uint64_t rng(void) {
    rng_state ^= rng_state >> 12;
    rng_state ^= rng_state << 25;
    rng_state ^= rng_state >> 27;
    return rng_state * 0x2545f4914f6cdd1dULL;
}

/* A finite binary128 with an exponent in [lo_e, hi_e] (biased) and a random
 * significand and sign. */
static float128_t random_f128(unsigned lo_e, unsigned hi_e) {
    uint64_t e = lo_e + rng() % (hi_e - lo_e + 1);
    uint64_t hi = (rng() & 0x8000ffffffffffffULL) | (e << 48);
    return bits(rng(), hi);
}

/* -------------------------------------------------------------------------- */

static const struct { uint64_t lo, hi; const char *s; } fmt_cases[] = {
    /* strfromf128(buf, n, "%.30g", x) on x86_64 glibc */
    { 0x0000000000000000ULL, 0x3fff000000000000ULL, "1" },
    { 0x0000000000000000ULL, 0x3ffe000000000000ULL, "0.5" },
    { 0x0000000000000000ULL, 0xc000000000000000ULL, "-2" },
    { 0xc908b2fb1366ea95ULL, 0x3ffe6a09e667f3bcULL, "0.707106781186547524400844362105" },
    { 0x2974cfbc31db4b03ULL, 0x3f949f623d5a8a73ULL, "1e-32" },
    { 0xca57a786c226809dULL, 0x3ff1a36e2eb1c432ULL, "0.0001" },          /* E = -4: fixed */
    { 0x0d36496828de459bULL, 0x3ff1a302cee75940ULL, "9.99e-05" },        /* E = -5: exponent */
    { 0x08461f9f01b866e4ULL, 0x3fee4f8b588e368fULL, "1e-05" },
    { 0x0000000000000000ULL, 0x400fe24000000000ULL, "123456" },
    { 0x17caa00000000000ULL, 0x405f431e0fae6d72ULL, "100000000000000000000000000000" }, /* E = 29 */
    { 0x9dbd480000000000ULL, 0x406293e5939a08ceULL, "1e+30" },                          /* E = 30 */
    { 0x9dbd47ffffffe000ULL, 0x406293e5939a08ceULL, "999999999999999999999999999999" },
    { 0x999999999999999aULL, 0x3ffb999999999999ULL, "0.1" },
    { 0x5555555555555555ULL, 0x3ffd555555555555ULL, "0.333333333333333333333333333333" },
    { 0xffffffffffffffffULL, 0x7ffeffffffffffffULL, "1.18973149535723176508575932663e+4932" },
    { 0x0000000000000001ULL, 0x0000000000000000ULL, "6.47517511943802511092443895823e-4966" },
    { 0x0000000000000000ULL, 0x0001000000000000ULL, "3.36210314311209350626267781732e-4932" },
    { 0x17c1bda5119ce076ULL, 0x3ff23a92a3055326ULL, "0.00015" },
    { 0x5b291b823c85c24cULL, 0xbff201f31f46ed24ULL, "-0.000123" },
    { 0x0000000000000000ULL, 0x4002900000000000ULL, "12.5" },
    { 0x0000000000000000ULL, 0x406f000000000000ULL, "5.19229685853482762853049632922e+33" },
    /* special values */
    { 0x0000000000000000ULL, 0x0000000000000000ULL, "0" },
    { 0x0000000000000000ULL, 0x8000000000000000ULL, "-0" },
    { 0x0000000000000000ULL, 0x7fff000000000000ULL, "inf" },
    { 0x0000000000000000ULL, 0xffff000000000000ULL, "-inf" },
    { 0x0000000000000000ULL, 0x7fff800000000000ULL, "nan" },
};

static void test_format_fixed(void) {
    TEST_SECTION("to_str_generic: fixed values (normal, subnormal, %g boundaries, inf/nan)");
    char buf[64];
    for (size_t i = 0; i < sizeof fmt_cases / sizeof fmt_cases[0]; i++) {
        to_str(bits(fmt_cases[i].lo, fmt_cases[i].hi), buf, sizeof buf);
        if (strcmp(buf, fmt_cases[i].s) != 0)
            fprintf(stderr, "  %016llx%016llx: got %s, expected %s\n",
                    (unsigned long long)fmt_cases[i].hi, (unsigned long long)fmt_cases[i].lo,
                    buf, fmt_cases[i].s);
        TEST_ASSERT(strcmp(buf, fmt_cases[i].s) == 0);
    }
}

#ifdef HAVE_NATIVE_F128
/* the same 16 bytes, little-endian {lo, hi} either way */
typedef union { float128_t s; __float128 n; } f128_pun;
static __float128 native(float128_t x) { f128_pun p = { .s = x }; return p.n; }
static float128_t soft(__float128 f) { f128_pun p = { .n = f }; return p.s; }

static void check_format_native(float128_t x, int *bad) {
    char a[64], b[64];
    to_str(x, a, sizeof a);
    strfromf128(b, sizeof b, "%.30g", native(x));
    if (strcmp(a, b) != 0 && (*bad)++ < 5)
        fprintf(stderr, "  %016llx%016llx: SoftFloat %s, strfromf128 %s\n",
                (unsigned long long)x.v[1], (unsigned long long)x.v[0], a, b);
}

static void test_format_vs_strfromf128(void) {
    TEST_SECTION("to_str_generic agrees with strfromf128 (random values, every exponent)");
    int bad = 0;
    for (int i = 0; i < 200000; i++)
        check_format_native(random_f128(0, 0x7ffe), &bad);
    /* the %g switch-overs and the rounding carries near them */
    for (int i = 0; i < 100000; i++)
        check_format_native(random_f128(0x3ff0, 0x3ff2), &bad);   /* ~1e-5 .. 1e-4 */
    for (int i = 0; i < 100000; i++)
        check_format_native(random_f128(0x405e, 0x4063), &bad);   /* ~1e29 .. 1e30 */
    for (int i = 0; i < 20000; i++) {                             /* subnormals */
        float128_t x = bits(rng(), rng() & 0x8000ffffffffffffULL);
        check_format_native(x, &bad);
    }
    /* ties: values with few significant bits, where round-half-even matters */
    for (int i = 0; i < 50000; i++) {
        float128_t x = random_f128(0x3f00, 0x40ff);
        x.v[0] = 0;
        x.v[1] &= ~((uint64_t)0xffffffff);
        check_format_native(x, &bad);
    }
    TEST_ASSERT(bad == 0);
}

static void test_arith_vs_native(void) {
    TEST_SECTION("SoftFloat helpers, snap and comparisons agree with __float128");
    int bad = 0;
    for (int i = 0; i < 200000; i++) {
        /* operands around 1, where leaves live, and a wider range */
        unsigned lo = (i & 1) ? 0x3f80 : 0x3ff0, hi = (i & 1) ? 0x4080 : 0x4010;
        leaf_primitive_t a = { random_f128(lo, hi) }, b = { random_f128(lo, hi) }, r;
        __float128 na = native(a[0]), nb = native(b[0]);

        add_generic(r, a, b);  bad += !same_bits(r[0], soft(na + nb));
        sub_generic(r, a, b);  bad += !same_bits(r[0], soft(na - nb));
        mul_generic(r, a, b);  bad += !same_bits(r[0], soft(na * nb));
        div_generic(r, a, b);  bad += !same_bits(r[0], soft(na / nb));
        neg_generic(r, a);     bad += !same_bits(r[0], soft(-na));
        mul_sqrt2inv_generic(r, a);
        bad += !same_bits(r[0], soft(na * 0.707106781186547524400844362104849039q));
        double d = (double)(int64_t)rng() / 9.2e18;
        mul_d_generic(r, a, d);   bad += !same_bits(r[0], soft(na * (__float128)d));
        set_d_generic(r, d);      bad += !same_bits(r[0], soft((__float128)d));
        bad += to_double_generic(a) != (double)na;
        unsigned long u = (unsigned long)(rng() >> 20);
        mul_ui_generic(r, a, u);  bad += !same_bits(r[0], soft(na * (__float128)u));

        bad += !same_bits(LEAF_SQRT(LEAF_ABS(a[0])), soft(sqrtf128(na < 0 ? -na : na)));
        bad += !same_bits(LEAF_ROUND(a[0]), soft(roundf128(na)));
        bad += !same_bits(LEAF_ABS(a[0]), soft(na < 0 ? -na : na));
        bad += sgn_generic(a) != ((na > 0) - (na < 0));
        bad += cmp_generic(a, b) != (na != nb);
        bad += LEAF_S_LT(a[0], b[0]) != (na < nb);
        bad += LEAF_S_LE(a[0], b[0]) != (na <= nb);

        /* snap: the native header's formula, on values near the grid */
        float128_t x = random_f128(0x3f90, 0x3fff);
        __float128 nx = native(x), ns;
        __float128 ae = 1e-32q, inv = 1 / ae;
        ns = ((nx < 0 ? -nx : nx) < ae) ? 0 : roundf128(nx * inv) * ae;
        bad += !same_bits(snap(x), soft(ns));
    }
    if (bad) fprintf(stderr, "  %d mismatches\n", bad);
    TEST_ASSERT(bad == 0);
}

static void test_pow_vs_native(void) {
    TEST_SECTION("(1/sqrt 2)^k is 2^-floor(k/2) (times 1/sqrt 2) in native arithmetic");
    const __float128 c = 0.707106781186547524400844362104849039q;
    int bad = 0;
    for (long k = -33000; k <= 33100; k++) {
        long q = k / 2 - (k % 2 < 0);
        __float128 want;
        if (k % 2 == 0)
            want = ldexpf128(1, (int)-q);
        else if (-q > 16383)   /* 2^-q overflows; 2^(-q-1) * sqrt 2 does not */
            want = ldexpf128(2 * c, (int)(-q - 1));
        else
            want = ldexpf128(1, (int)-q) * c;
        if (!same_bits(leaf_sqrt2inv_pow(k), soft(want)) && bad++ < 5)
            fprintf(stderr, "  k = %ld\n", k);
    }
    TEST_ASSERT(bad == 0);
}
#endif /* HAVE_NATIVE_F128 */

static void test_pow_fixed(void) {
    TEST_SECTION("(1/sqrt 2)^k: exact values, overflow to inf, subnormals, underflow to 0");
    const uint64_t c_lo = 0xc908b2fb1366ea95ULL, c_hi = 0x3ffe6a09e667f3bcULL;
    TEST_ASSERT(same_bits(leaf_sqrt2inv_pow(0), LEAF_ONE));
    TEST_ASSERT(same_bits(leaf_sqrt2inv_pow(1), bits(c_lo, c_hi)));
    TEST_ASSERT(same_bits(leaf_sqrt2inv_pow(2), bits(0, 0x3ffe000000000000ULL)));     /* 1/2 */
    TEST_ASSERT(same_bits(leaf_sqrt2inv_pow(3), bits(c_lo, c_hi - ((uint64_t)1 << 48))));
    TEST_ASSERT(same_bits(leaf_sqrt2inv_pow(-1), bits(c_lo, c_hi + ((uint64_t)1 << 48)))); /* sqrt 2 */
    TEST_ASSERT(same_bits(leaf_sqrt2inv_pow(-2), bits(0, 0x4000000000000000ULL)));    /* 2 */
    /* the largest finite results */
    TEST_ASSERT(same_bits(leaf_sqrt2inv_pow(-32766), bits(0, 0x7ffe000000000000ULL))); /* 2^16383 */
    TEST_ASSERT(same_bits(leaf_sqrt2inv_pow(-32767), bits(c_lo, 0x7ffe6a09e667f3bcULL)));
    /* overflow: +inf, never a sign or exponent wrap-around (was -0 at -32770) */
    const float128_t inf = bits(0, 0x7fff000000000000ULL);
    TEST_ASSERT(same_bits(leaf_sqrt2inv_pow(-32768), inf));
    TEST_ASSERT(same_bits(leaf_sqrt2inv_pow(-32769), inf));
    TEST_ASSERT(same_bits(leaf_sqrt2inv_pow(-32770), inf));
    TEST_ASSERT(same_bits(leaf_sqrt2inv_pow(-1000000), inf));
    /* smallest normal, then subnormals (was +0 from 32766 on), then 0 */
    TEST_ASSERT(same_bits(leaf_sqrt2inv_pow(32764), bits(0, 0x0001000000000000ULL)));  /* 2^-16382 */
    TEST_ASSERT(same_bits(leaf_sqrt2inv_pow(32766), bits(0, 0x0000800000000000ULL)));  /* 2^-16383 */
    TEST_ASSERT(same_bits(leaf_sqrt2inv_pow(2 * 16494), bits(1, 0)));                 /* 2^-16494 */
    TEST_ASSERT(same_bits(leaf_sqrt2inv_pow(2 * 16494 + 1), bits(1, 0)));  /* 0.707 ulp: rounds up */
    TEST_ASSERT(same_bits(leaf_sqrt2inv_pow(2 * 16495), LEAF_ZERO));       /* half an ulp: to even */
    TEST_ASSERT(same_bits(leaf_sqrt2inv_pow(1000000), LEAF_ZERO));
    /* the most negative k must not overflow computing floor(k / 2) */
    TEST_ASSERT(same_bits(leaf_sqrt2inv_pow(LONG_MIN), inf));
}

static void test_conversions_fixed(void) {
    TEST_SECTION("double conversions and the constants' bit patterns");
    TEST_ASSERT(to_double_generic((leaf_primitive_t){ LEAF_ONE }) == 1.0);
    TEST_ASSERT(leaf_s_to_d(LEAF_SQRT2INV) == 0.70710678118654752440);
    TEST_ASSERT(leaf_s_to_d(LEAF_ABS_EPS) == 1e-32);
    TEST_ASSERT(leaf_s_to_d(LEAF_REL_EPS) == 1e-28);
    TEST_ASSERT(same_bits(leaf_s_from_d(0.5), bits(0, 0x3ffe000000000000ULL)));
    TEST_ASSERT(same_bits(leaf_s_from_d(-0.0), bits(0, 0x8000000000000000ULL)));
    /* 1/sqrt 2 squared is 1/2 within an ulp; LEAF_INV_ABS_EPS is 1/fl(1e-32) */
    float128_t h = f128_mul(LEAF_SQRT2INV, LEAF_SQRT2INV);
    TEST_ASSERT(f128_lt(leaf_s_abs(f128_sub(h, bits(0, 0x3ffe000000000000ULL))),
                        bits(0, 0x3f8e000000000000ULL)));                   /* < 2^-113 */
    TEST_ASSERT(same_bits(f128_div(LEAF_ONE, LEAF_ABS_EPS), LEAF_INV_ABS_EPS));
    /* snap: below the grid -> 0, on the grid unchanged, NaN-free */
    TEST_ASSERT(same_bits(snap(f128_mul(LEAF_ABS_EPS, leaf_s_from_d(0.4))), LEAF_ZERO));
    TEST_ASSERT(same_bits(snap(LEAF_ONE), LEAF_ONE));
    TEST_ASSERT(same_bits(snap(LEAF_ZERO), LEAF_ZERO));
}

/* One thread raises "inexact" (1/3 is not exact); with per-thread state the
 * other thread's flags stay clear. With SoftFloat's default, shared globals,
 * they would not. */
static void *raise_inexact(void *seen) {
    softfloat_exceptionFlags = 0;
    (void)f128_div(LEAF_ONE, ui64_to_f128(3));
    *(uint_fast8_t *)seen = softfloat_exceptionFlags;
    return NULL;
}

static void test_state_per_thread(void) {
    TEST_SECTION("SoftFloat state is per thread (exception flags)");
    uint_fast8_t seen = 0;
    softfloat_exceptionFlags = 0;
    pthread_t t;
    TEST_ASSERT(pthread_create(&t, NULL, raise_inexact, &seen) == 0);
    TEST_ASSERT(pthread_join(t, NULL) == 0);
    TEST_ASSERT(seen & softfloat_flag_inexact);          /* the other thread's own flag */
    TEST_ASSERT(softfloat_exceptionFlags == 0);          /* and not this thread's */
    (void)f128_div(LEAF_ONE, ui64_to_f128(3));
    TEST_ASSERT(softfloat_exceptionFlags & softfloat_flag_inexact);
}

int main(void)
{
    printf("MEDUSA SoftFloat f128 unit tests%s\n",
#ifdef HAVE_NATIVE_F128
           " (with __float128 comparisons)"
#else
           " (fixed expectations only: no __float128 here)"
#endif
    );
    test_format_fixed();
    test_pow_fixed();
    test_conversions_fixed();
    test_state_per_thread();
#ifdef HAVE_NATIVE_F128
    test_format_vs_strfromf128();
    test_arith_vs_native();
    test_pow_vs_native();
#endif
    return test_report("test_unit_softfloat");
}
