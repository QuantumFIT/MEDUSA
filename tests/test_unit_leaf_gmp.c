/**
 * Unit tests for the GMP leaf primitive layer.
 *
 * Why this file exists: test_unit_api is hard-wired to LEAF_BACKEND_DOUBLES
 * (it reads leaf.pImpl->re / ->im throughout), so nothing had ever unit-tested
 * leaf_primitive_mpz.c. It sat at 48.9% line coverage with cmp_generic,
 * hash_comb_generic, mul_generic, neg_generic, mul_ui_generic,
 * init_set_ui_generic and inv_sqrt2_pow_generic never executed by any suite.
 *
 * That gap is what let a real bug live: mulLeaf and divLeaf on this backend
 * multiplied and divided the (a,b,c,d) coefficients componentwise, which is
 * not arithmetic in Z[omega], and the only test that called mulLeaf was a
 * doubles-only one that could never reach them. Both are removed now (#12),
 * but the blind spot they hid in is what this file closes.
 *
 * Scope is the primitive layer - mpz wrappers and the hash/equality contract
 * the terminal table depends on. The algebraic leaf's rotation entry points
 * (rx_/ry_/rz_*_leaf, negI_mul, mulPhaseLeaf) are deliberate abort() stubs on
 * this backend and are not exercised here: a passing test cannot reach them.
 */
#include "test_harness.h"
#include "interface.h"

#include <gmp.h>
#include <stdio.h>
#include <string.h>

/* Every test below is pure arithmetic on leaf_primitive_t, so no BDD package
 * setup is needed - deliberately, since it keeps failures pointing at the
 * primitive rather than at package state. */

static void set_si(leaf_primitive_t x, long v)
{
    if (v >= 0) {
        set_ui_generic(x, (unsigned long)v);
    } else {
        set_ui_generic(x, (unsigned long)(-v));
        neg_generic(x, x);
    }
}

static void test_init_set_clear(void)
{
    TEST_SECTION("gmp primitive: init/set/clear round-trips");

    leaf_primitive_t a, b, c;
    init_generic(a);
    TEST_ASSERT(sgn_generic(a) == 0);          /* init must zero */

    set_ui_generic(a, 42);
    TEST_ASSERT(sgn_generic(a) == 1);

    init_set_generic(b, a);                    /* copy-construct */
    TEST_ASSERT(cmp_generic(a, b) == 0);

    init_set_ui_generic(c, 7);                 /* construct from ulong */
    TEST_ASSERT(sgn_generic(c) == 1);
    set_ui_generic(b, 7);
    TEST_ASSERT_MSG(cmp_generic(c, b) == 0, "init_set_ui_generic(7) != set_ui_generic(7)");

    set_generic(b, a);                         /* assign */
    TEST_ASSERT(cmp_generic(a, b) == 0);

    clear_generic(a);
    clear_generic(b);
    clear_generic(c);
}

static void test_sign_and_negation(void)
{
    TEST_SECTION("gmp primitive: sgn and neg");

    leaf_primitive_t x, y;
    init_generic(x);
    init_generic(y);

    set_ui_generic(x, 0);
    TEST_ASSERT(sgn_generic(x) == 0);

    set_ui_generic(x, 5);
    TEST_ASSERT(sgn_generic(x) == 1);

    neg_generic(y, x);
    TEST_ASSERT_MSG(sgn_generic(y) == -1, "neg of a positive must be negative");

    neg_generic(y, y);                          /* aliased: r and x the same */
    TEST_ASSERT_MSG(cmp_generic(x, y) == 0, "double negation must be the identity");

    set_ui_generic(x, 0);
    neg_generic(y, x);
    TEST_ASSERT_MSG(sgn_generic(y) == 0, "neg of zero stays zero");

    clear_generic(x);
    clear_generic(y);
}

static void test_arithmetic(void)
{
    TEST_SECTION("gmp primitive: add/sub/mul/mul_ui");

    leaf_primitive_t a, b, r, e;
    init_generic(a);
    init_generic(b);
    init_generic(r);
    init_generic(e);

    set_si(a, 6);
    set_si(b, 7);

    mul_generic(r, a, b);
    set_si(e, 42);
    TEST_ASSERT_MSG(cmp_generic(r, e) == 0, "6 * 7 != 42");

    add_generic(r, a, b);
    set_si(e, 13);
    TEST_ASSERT_MSG(cmp_generic(r, e) == 0, "6 + 7 != 13");

    /* No sub_generic here: interface_leaf.h declares one, but only the
     * floating-point primitive defines it - this backend provides add and neg
     * and nothing calls sub_generic on it. Subtraction is add-of-negation. */
    neg_generic(r, b);
    add_generic(r, a, r);
    set_si(e, -1);
    TEST_ASSERT_MSG(cmp_generic(r, e) == 0, "6 + (-7) != -1");

    mul_ui_generic(r, a, 3);
    set_si(e, 18);
    TEST_ASSERT_MSG(cmp_generic(r, e) == 0, "6 * 3u != 18");

    mul_ui_generic(r, a, 0);
    TEST_ASSERT_MSG(sgn_generic(r) == 0, "x * 0u must be zero");

    /* Signed operands: mpz keeps the sign outside the limbs, so this is the
     * case most likely to go wrong in a naive wrapper. */
    set_si(a, -6);
    mul_generic(r, a, b);
    set_si(e, -42);
    TEST_ASSERT_MSG(cmp_generic(r, e) == 0, "-6 * 7 != -42");

    mul_ui_generic(r, a, 3);
    set_si(e, -18);
    TEST_ASSERT_MSG(cmp_generic(r, e) == 0, "-6 * 3u != -18");

    /* Aliased output: r and a the same object. */
    set_si(a, 9);
    mul_generic(a, a, a);
    set_si(e, 81);
    TEST_ASSERT_MSG(cmp_generic(a, e) == 0, "aliased mul_generic(a,a,a) != a^2");

    clear_generic(a);
    clear_generic(b);
    clear_generic(r);
    clear_generic(e);
}

static void test_big_values(void)
{
    TEST_SECTION("gmp primitive: multi-limb values");

    /* The point of this backend is exact arithmetic past 64 bits. Squaring
     * 2^64 + 1 needs more than one limb and would silently wrap on any type
     * that is not arbitrary precision. */
    leaf_primitive_t a, r, e;
    init_generic(a);
    init_generic(r);
    init_generic(e);

    mpz_ui_pow_ui(a, 2, 64);
    add_generic(a, a, a);                       /* 2^65 */
    mul_generic(r, a, a);                       /* 2^130 */
    mpz_ui_pow_ui(e, 2, 130);
    TEST_ASSERT_MSG(cmp_generic(r, e) == 0, "2^65 squared != 2^130 (precision lost?)");
    TEST_ASSERT_MSG(mpz_size(r) > 1, "2^130 should occupy more than one limb");

    clear_generic(a);
    clear_generic(r);
    clear_generic(e);
}

static void test_cmp_ordering(void)
{
    TEST_SECTION("gmp primitive: cmp_generic ordering");

    leaf_primitive_t a, b;
    init_generic(a);
    init_generic(b);

    set_si(a, 3);
    set_si(b, 5);
    TEST_ASSERT_MSG(cmp_generic(a, b) < 0, "3 < 5");
    TEST_ASSERT_MSG(cmp_generic(b, a) > 0, "5 > 3");
    set_si(b, 3);
    TEST_ASSERT_MSG(cmp_generic(a, b) == 0, "3 == 3");

    /* Signed comparison must not fall back to magnitude. */
    set_si(a, -5);
    set_si(b, 3);
    TEST_ASSERT_MSG(cmp_generic(a, b) < 0, "-5 < 3 (magnitude comparison?)");

    set_si(a, -5);
    set_si(b, -3);
    TEST_ASSERT_MSG(cmp_generic(a, b) < 0, "-5 < -3");

    clear_generic(a);
    clear_generic(b);
}

static void test_hash_equality_contract(void)
{
    TEST_SECTION("gmp primitive: hash/equality contract");

    /* The contract the terminal table relies on: values that compare equal
     * must hash equal. Issue #6 was exactly this contract breaking on the f80
     * leaf, where the hash folded in uninitialised padding - so it is worth
     * asserting directly on this backend rather than inferring it from
     * simulation results. */
    leaf_primitive_t a, b;
    init_generic(a);
    init_generic(b);

    const long vals[] = { 0, 1, -1, 7, -7, 1000003, -1000003 };
    for (size_t i = 0; i < sizeof vals / sizeof vals[0]; i++) {
        set_si(a, vals[i]);
        set_si(b, vals[i]);
        char msg[96];
        snprintf(msg, sizeof msg, "equal values %ld must hash equally", vals[i]);
        TEST_ASSERT_MSG(cmp_generic(a, b) == 0, msg);
        TEST_ASSERT_MSG(hash_comb_generic(a) == hash_comb_generic(b), msg);
    }

    /* Reached by two different routes, the same value must still hash the
     * same - a copy must not hash differently from a freshly set value. */
    set_si(a, 123456789);
    leaf_primitive_t copy;
    init_set_generic(copy, a);
    TEST_ASSERT(cmp_generic(a, copy) == 0);
    TEST_ASSERT_MSG(hash_comb_generic(a) == hash_comb_generic(copy),
                    "a copy must hash like its source");
    clear_generic(copy);

    set_ui_generic(a, 0);
    set_si(b, 0);
    TEST_ASSERT_MSG(hash_comb_generic(a) == hash_comb_generic(b),
                    "zero must hash consistently however it was produced");

    /* Documented collision, asserted so it is a decision rather than a
     * surprise: hash_comb_generic XORs the limbs, and mpz stores the sign
     * outside them, so +n and -n hash identically while comparing unequal.
     * That is legal - the contract is equal => same hash, not the converse -
     * and the terminal table resolves collisions with cmp_generic. If the
     * hash is ever changed to fold in the sign, this assertion should be
     * updated rather than deleted, so the property stays explicit. */
    set_si(a, 42);
    set_si(b, -42);
    TEST_ASSERT_MSG(cmp_generic(a, b) != 0, "+42 and -42 must not compare equal");
    TEST_ASSERT_MSG(hash_comb_generic(a) == hash_comb_generic(b),
                    "sign is outside the limbs, so +n and -n collide by design");

    clear_generic(a);
    clear_generic(b);
}

static void test_inv_sqrt2_pow_stores_exponent(void)
{
    TEST_SECTION("gmp primitive: inv_sqrt2_pow_generic stores k");

    /* Unlike the floating-point primitive, which computes (1/sqrt2)^k, the
     * algebraic backend keeps the power symbolically: the leaf carries the
     * exponent and the 1/sqrt2 factor is applied outside. So this is a set,
     * not a pow, and a "fix" that made it compute a value would be wrong. */
    leaf_primitive_t x;
    mpz_t k;
    init_generic(x);
    mpz_init_set_ui(k, 5);

    inv_sqrt2_pow_generic(x, k);
    TEST_ASSERT_MSG(mpz_cmp(x, k) == 0, "inv_sqrt2_pow_generic must store the exponent");

    mpz_set_ui(k, 0);
    inv_sqrt2_pow_generic(x, k);
    TEST_ASSERT_MSG(sgn_generic(x) == 0, "exponent 0 stores 0");

    mpz_clear(k);
    clear_generic(x);
}

static void test_mul_mpz_and_scaling(void)
{
    TEST_SECTION("gmp primitive: mul_mpz_generic");

    leaf_primitive_t x, r, e;
    mpz_t s;
    init_generic(x);
    init_generic(r);
    init_generic(e);
    mpz_init_set_ui(s, 11);

    set_si(x, -4);
    mul_mpz_generic(r, x, s);
    set_si(e, -44);
    TEST_ASSERT_MSG(cmp_generic(r, e) == 0, "-4 * 11 != -44");

    mpz_set_ui(s, 0);
    mul_mpz_generic(r, x, s);
    TEST_ASSERT_MSG(sgn_generic(r) == 0, "scaling by zero must give zero");

    mpz_clear(s);
    clear_generic(x);
    clear_generic(r);
    clear_generic(e);
}

int main(void)
{
    printf("MEDUSA GMP leaf primitive unit tests\n");

    test_init_set_clear();
    test_sign_and_negation();
    test_arithmetic();
    test_big_values();
    test_cmp_ordering();
    test_hash_equality_contract();
    test_inv_sqrt2_pow_stores_exponent();
    test_mul_mpz_and_scaling();

    return test_report("test_unit_leaf_gmp");
}
