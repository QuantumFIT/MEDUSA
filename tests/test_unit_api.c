/**
 * @file test_unit_api.c
 * Unit / integration tests for MoToBuddy backend API:
 * leaf ownership, protect/unprotect, apply free-of-unused, gates, circuits.
 */
#include "test_harness.h"

#include "interface.h"
#include "gates.h"
#include "mtbdd.h"
#include "kernel.h" /* bddnodes[].refcou for protect checks */
#include "terminal.h" /* CUSTOMFREE / freefun registration */
#include "medusa_mem_track.h"

#include <stdint.h>

/* Complete the opaque leaf layout used by LEAF_BACKEND_DOUBLES (must match
 * leaf_reim_double.c). Needed only for constructing/inspecting leaves in tests. */
struct LEAF_TYPE_IMPL {
    leaf_primitive_t re;
    leaf_primitive_t im;
};

/* -------------------------------------------------------------------------- */
/* Helpers                                                                    */
/* -------------------------------------------------------------------------- */

static LEAF_TYPE make_leaf(double re, double im) {
    LEAF_TYPE leaf;
    leaf.pImpl = malloc(sizeof(LEAF_TYPE_IMPL));
    if (!leaf.pImpl) abort();
    init_generic(leaf.pImpl->re);
    init_generic(leaf.pImpl->im);
    set_d_generic(leaf.pImpl->re, re);
    set_d_generic(leaf.pImpl->im, im);
    return leaf;
}

static LEAF_TYPE *heap_leaf(double re, double im) {
    LEAF_TYPE *p = malloc(sizeof(LEAF_TYPE));
    if (!p) abort();
    *p = make_leaf(re, im);
    return p;
}

static unsigned refcount(qBDD n) {
    if (n < 0 || n >= bddnodesize) return 0;
    return bddnodes[n].refcou;
}

static void setup_pkg(void) {
    initPackage(0, 0, 0);
    test_silence_gbc();
}

/* -------------------------------------------------------------------------- */
/* Leaf ownership / free-of-unused regression                                 */
/* -------------------------------------------------------------------------- */

static void test_leaf_add_does_not_alias(void) {
    TEST_SECTION("leaf addLeaf/subLeaf clone on NULL operand");

    LEAF_TYPE a = make_leaf(1.0, 2.0);
    LEAF_TYPE z = { .pImpl = NULL };

    LEAF_TYPE r1 = addLeaf(z, a);
    TEST_ASSERT(r1.pImpl != NULL);
    TEST_ASSERT_MSG(r1.pImpl != a.pImpl,
        "addLeaf(NULL,a) must clone — aliasing breaks MoToBuddy free-of-unused");
    TEST_ASSERT_NEAR(to_double_generic(r1.pImpl->re), 1.0, 1e-12);
    TEST_ASSERT_NEAR(to_double_generic(r1.pImpl->im), 2.0, 1e-12);

    /* Simulate MoToBuddy apply path: free unused equal result */
    LEAF_TYPE *wrap = malloc(sizeof(LEAF_TYPE));
    wrap->pImpl = r1.pImpl;
    freePimpl(wrap);
    free(wrap);

    /* Original must still be intact */
    TEST_ASSERT_NEAR(to_double_generic(a.pImpl->re), 1.0, 1e-12);
    TEST_ASSERT_NEAR(to_double_generic(a.pImpl->im), 2.0, 1e-12);

    LEAF_TYPE r2 = subLeaf(a, z);
    TEST_ASSERT(r2.pImpl != a.pImpl);

    LEAF_TYPE *wrap2 = malloc(sizeof(LEAF_TYPE));
    wrap2->pImpl = r2.pImpl;
    freePimpl(wrap2);
    free(wrap2);

    clear_generic(a.pImpl->re);
    clear_generic(a.pImpl->im);
    free(a.pImpl);
}

static void test_apply_free_unused_preserves_terminals(void) {
    TEST_SECTION("apply free-of-unused does not corrupt live terminals");

    setup_pkg();
    qBDD circ;
    circuit_init_interface(&circ, 1);
    qBDD_protect(circ);

    /* |0> --H--> equal superposition; forces addLeafS/subLeafS with false branches */
    gate_h(&circ, 0);
    forceGC();

    TEST_ASSERT(qBDD_isInternal(circ) || qBDD_isTerminal(circ));
    prob_t p = qBDD_total_prob(circ, 1);
    /* After H on |0>, amplitudes are 1/sqrt(2); total prob ~ 1 */
    TEST_ASSERT_NEAR(p, 1.0, 1e-9);

    /* Apply again: results equal to existing leaves must free clones only */
    gate_h(&circ, 0);
    forceGC();
    p = qBDD_total_prob(circ, 1);
    TEST_ASSERT_NEAR(p, 1.0, 1e-9);

    /* Read a terminal value after GC — must not be freed-as-unused */
    qBDD walk = circ;
    while (qBDD_isInternal(walk))
        walk = qBDD_getLow(walk);
    if (qBDD_isTerminal(walk) && !qBDD_isFalse(walk)) {
        LEAF_TYPE v = qBDD_getTerminalValue(walk);
        TEST_ASSERT(v.pImpl != NULL);
        double mag2 = to_double_generic(v.pImpl->re) * to_double_generic(v.pImpl->re)
                    + to_double_generic(v.pImpl->im) * to_double_generic(v.pImpl->im);
        TEST_ASSERT(mag2 > 0.0);
    }

    qBDD_unprotect(circ);
    freePackage();
}

/* -------------------------------------------------------------------------- */
/* Protect / unprotect                                                        */
/* -------------------------------------------------------------------------- */

static void test_protect_unprotect_refcount(void) {
    TEST_SECTION("protect/unprotect reference counts");

    setup_pkg();
    qBDD circ;
    circuit_init_interface(&circ, 2);
    qBDD_protect(circ);

    unsigned base = refcount(circ);
    TEST_ASSERT(base >= 1);

    qBDD_protect(circ);
    TEST_ASSERT(refcount(circ) == base + 1 || refcount(circ) == 0x3FF /* MAXREF */);

    qBDD_unprotect(circ);
    TEST_ASSERT(refcount(circ) == base || refcount(circ) == 0x3FF);

    qBDD old = circ;
    unsigned old_ref = refcount(old);
    gate_x(&circ, 0);
    /* gate_x protects res and unprotects old */
    TEST_ASSERT(circ != 0);
    TEST_ASSERT(refcount(circ) >= 1);

    /* After replace, old should not keep an extra protect from gate_x */
    if (old != circ && old_ref != 0x3FF) {
        TEST_ASSERT(refcount(old) <= old_ref);
    }

    qBDD_unprotect(circ);
    freePackage();
}

static void test_cnot_protect_balance(void) {
    TEST_SECTION("CNOT protect balance + norm");

    setup_pkg();
    qBDD circ;
    circuit_init_interface(&circ, 3);
    qBDD_protect(circ);

    gate_h(&circ, 0);
    gate_cnot(&circ, 1, 0);
    forceGC();
    TEST_ASSERT_NEAR(qBDD_total_prob(circ, 3), 1.0, 1e-9);
    TEST_ASSERT(refcount(circ) >= 1);

    qBDD_unprotect(circ);
    freePackage();
}

/* Known bug fixed: Toffoli after H+CNOT must preserve total_prob == 1. */
#ifndef EXPECT_TOFFOLI_OK
#define EXPECT_TOFFOLI_OK 1
#endif

static void test_toffoli_norm_regression(void) {
    TEST_SECTION("Toffoli norm (H+CNOT+Toffoli GHZ)");

    setup_pkg();
    qBDD circ;
    circuit_init_interface(&circ, 3);
    qBDD_protect(circ);

    gate_h(&circ, 0);
    gate_cnot(&circ, 1, 0);
    gate_toffoli(&circ, 2, 0, 1);
    forceGC();

    prob_t p = qBDD_total_prob(circ, 3);
#if EXPECT_TOFFOLI_OK
    TEST_ASSERT_NEAR(p, 1.0, 1e-9);
#else
    TEST_ASSERT_MSG(fabs((double)p - 0.5) < 1e-9 || fabs((double)p - 1.0) < 1e-9,
        "Toffoli total_prob neither 0.5 (known bug) nor 1.0 (fixed)");
    if (fabs((double)p - 1.0) < 1e-9) {
        fprintf(stdout, "    NOTE: Toffoli norm looks fixed — set EXPECT_TOFFOLI_OK=1\n");
    } else {
        fprintf(stdout, "    KNOWN BUG: Toffoli total_prob=%g (expected 1.0)\n", (double)p);
    }
#endif

    qBDD_unprotect(circ);
    freePackage();
}

/* -------------------------------------------------------------------------- */
/* Gate / API correctness                                                     */
/* -------------------------------------------------------------------------- */

static void test_gates_norm_and_idempotence(void) {
    TEST_SECTION("gates preserve norm; X/X and H/H identity");

    setup_pkg();
    qBDD circ;
    circuit_init_interface(&circ, 2);
    qBDD_protect(circ);

    TEST_ASSERT_NEAR(qBDD_total_prob(circ, 2), 1.0, 1e-9);

    gate_x(&circ, 0);
    TEST_ASSERT_NEAR(qBDD_total_prob(circ, 2), 1.0, 1e-9);
    gate_x(&circ, 0);
    TEST_ASSERT_NEAR(qBDD_total_prob(circ, 2), 1.0, 1e-9);

    gate_z(&circ, 0);
    gate_z(&circ, 0);
    TEST_ASSERT_NEAR(qBDD_total_prob(circ, 2), 1.0, 1e-9);

    gate_y(&circ, 1);
    gate_y(&circ, 1);
    TEST_ASSERT_NEAR(qBDD_total_prob(circ, 2), 1.0, 1e-9);

    gate_h(&circ, 0);
    gate_h(&circ, 0);
    TEST_ASSERT_NEAR(qBDD_total_prob(circ, 2), 1.0, 1e-9);

    gate_s(&circ, 0);
    gate_s(&circ, 0);
    gate_s(&circ, 0);
    gate_s(&circ, 0); /* S^4 = I */
    TEST_ASSERT_NEAR(qBDD_total_prob(circ, 2), 1.0, 1e-9);

    qBDD_unprotect(circ);
    freePackage();
}

static prob_t test_basis_prob(qBDD t, const char *bits)
{
    while (!qBDD_isTerminal(t) && !qBDD_isFalse(t)) {
        uint32_t v = (uint32_t)qBDD_getVar(t);
        t = (bits[v] == '1') ? qBDD_getHigh(t) : qBDD_getLow(t);
    }
    if (qBDD_isFalse(t))
        return 0.0;
    return (prob_t)qBDD_calculateProb(t);
}

static void test_x_flips_computational_basis(void) {
    TEST_SECTION("X actually flips |0> -> |1> (not a no-op)");

    setup_pkg();
    qBDD circ;
    circuit_init_interface(&circ, 2);
    qBDD_protect(circ);

    TEST_ASSERT_NEAR(test_basis_prob(circ, "00"), 1.0, 1e-9);
    gate_x(&circ, 0);
    TEST_ASSERT_NEAR(test_basis_prob(circ, "10"), 1.0, 1e-9);
    TEST_ASSERT_NEAR(test_basis_prob(circ, "00"), 0.0, 1e-9);
    gate_x(&circ, 1);
    TEST_ASSERT_NEAR(test_basis_prob(circ, "11"), 1.0, 1e-9);
    gate_x(&circ, 0);
    TEST_ASSERT_NEAR(test_basis_prob(circ, "01"), 1.0, 1e-9);

    qBDD_unprotect(circ);
    freePackage();
}

static void test_grover_2q_marks_11(void) {
    TEST_SECTION("2-qubit Grover (classic gates) concentrates on |11>");

    setup_pkg();
    qBDD circ;
    circuit_init_interface(&circ, 2);
    qBDD_protect(circ);

    gate_h(&circ, 0);
    gate_h(&circ, 1);
    gate_cz(&circ, 1, 0);
    gate_h(&circ, 0);
    gate_h(&circ, 1);
    gate_x(&circ, 0);
    gate_x(&circ, 1);
    gate_cz(&circ, 1, 0);
    gate_x(&circ, 0);
    gate_x(&circ, 1);
    gate_h(&circ, 0);
    gate_h(&circ, 1);

    TEST_ASSERT_NEAR(qBDD_total_prob(circ, 2), 1.0, 1e-9);
    TEST_ASSERT_MSG(test_basis_prob(circ, "11") > 0.9,
        "classic X/CZ Grover must amplify |11>");

    qBDD_unprotect(circ);
    freePackage();
}

static void test_bell_state(void) {
    TEST_SECTION("Bell state H+CNOT has total prob 1");

    setup_pkg();
    qBDD circ;
    circuit_init_interface(&circ, 2);
    qBDD_protect(circ);

    gate_h(&circ, 0);
    gate_cnot(&circ, 1, 0);
    forceGC();

    TEST_ASSERT_NEAR(qBDD_total_prob(circ, 2), 1.0, 1e-9);
    TEST_ASSERT(qBDD_leafcount(circ) >= 1);

    qBDD_unprotect(circ);
    freePackage();
}

static void test_terminal_compare_and_maketerminal(void) {
    TEST_SECTION("terminal compare / maketerminal dedup");

    setup_pkg();

    LEAF_TYPE *a = heap_leaf(0.5, -0.25);
    LEAF_TYPE *b = heap_leaf(0.5, -0.25);
    LEAF_TYPE *c = heap_leaf(1.0, 0.0);

    TEST_ASSERT(terminal_compare_generic(a, b));
    TEST_ASSERT(!terminal_compare_generic(a, c));

    qBDD t1 = qBDD_maketerminal(qBDD_classicLType(), a);
    qBDD t2 = qBDD_maketerminal(qBDD_classicLType(), b);
    /* equal values must share the same terminal node; unused b is freed */
    TEST_ASSERT(t1 == t2);
    TEST_ASSERT(qBDD_isTerminal(t1));

    LEAF_TYPE v = qBDD_getTerminalValue(t1);
    TEST_ASSERT_NEAR(to_double_generic(v.pImpl->re), 0.5, 1e-12);

    qBDD t3 = qBDD_maketerminal(qBDD_classicLType(), c);
    TEST_ASSERT(t3 != t1);

    freePackage();
}

static void test_node_classification(void) {
    TEST_SECTION("node classification API");

    setup_pkg();
    qBDD circ;
    circuit_init_interface(&circ, 1);
    qBDD_protect(circ);

    TEST_ASSERT(!qBDD_isFalse(circ));
    TEST_ASSERT(qBDD_isInternal(circ) || qBDD_isTerminal(circ));
    TEST_ASSERT(qBDD_isFalse(qBDD_false()));
    TEST_ASSERT(qBDD_isTerminal(qBDD_false()) || qBDD_isFalse(qBDD_false()));

    if (qBDD_isInternal(circ)) {
        qBDD lo = qBDD_getLow(circ);
        qBDD hi = qBDD_getHigh(circ);
        TEST_ASSERT(qBDD_isTerminal(lo) || qBDD_isFalse(lo) || qBDD_isInternal(lo));
        TEST_ASSERT(qBDD_isTerminal(hi) || qBDD_isFalse(hi) || qBDD_isInternal(hi));
        TEST_ASSERT(qBDD_getVar(circ) == 0);
    }

    qBDD_unprotect(circ);
    freePackage();
}

static void test_binary_apply_with_false(void) {
    TEST_SECTION("binary_apply with bdd_false (NULL leaf path)");

    setup_pkg();
    qBDD circ;
    circuit_init_interface(&circ, 1);
    qBDD_protect(circ);

    /* Walk to the |1> leaf (false on |0> cube) and add with false */
    qBDD leaf = circ;
    while (qBDD_isInternal(leaf))
        leaf = qBDD_getHigh(leaf);

    qBDD_protect(leaf);
    qBDD sum = binary_apply(leaf, qBDD_false(), addLeaf);
    qBDD_protect(sum);
    forceGC();

    /* leaf + 0 == leaf (same terminal after dedup) */
    TEST_ASSERT(sum == leaf || (qBDD_isTerminal(sum) && qBDD_isTerminal(leaf)));
    if (qBDD_isTerminal(sum) && !qBDD_isFalse(sum)) {
        LEAF_TYPE v = qBDD_getTerminalValue(sum);
        TEST_ASSERT(v.pImpl != NULL);
        TEST_ASSERT_NEAR(to_double_generic(v.pImpl->re), 1.0, 1e-12);
    }

    qBDD_unprotect(sum);
    qBDD_unprotect(leaf);
    qBDD_unprotect(circ);
    freePackage();
}

/* -------------------------------------------------------------------------- */
/* Terminal memory handling                                                   */
/* -------------------------------------------------------------------------- */

/** MoToBuddy apply free-of-unused: freefun(result); free(result); */
static void buddy_free_unused_result(LEAF_TYPE *result_wrap) {
    freePimpl(result_wrap);
    free(result_wrap);
}

static LEAF_TYPE *wrap_owned(LEAF_TYPE leaf) {
    LEAF_TYPE *w = malloc(sizeof(LEAF_TYPE));
    if (!w) abort();
    w->pImpl = leaf.pImpl;
    return w;
}

static void destroy_owned_leaf(LEAF_TYPE *leaf) {
    if (!leaf || !leaf->pImpl) return;
    clear_generic(leaf->pImpl->re);
    clear_generic(leaf->pImpl->im);
    free(leaf->pImpl);
    leaf->pImpl = NULL;
}

static void test_freepimpl_registered_with_motobuddy(void) {
    TEST_SECTION("freePimpl is registered as classic terminal freefun");

    setup_pkg();
    size_t ty = qBDD_classicLType();
    TEST_ASSERT(mtbdd_terminal_functions_list != NULL);
    TEST_ASSERT_MSG(
        mtbdd_terminal_functions_list[ty].freefun == freePimpl,
        "lt_classic freefun must be freePimpl — revert with "
        "patches/revert-classic-freepimpl.patch if this breaks");

    /* Exercise real MoToBuddy equal-result path: freefun(result)+free(wrapper) */
    LEAF_TYPE *a = heap_leaf(7.0, 1.0);
    qBDD t = qBDD_maketerminal(qBDD_classicLType(), a);
    qBDD_protect(t);

    for (int i = 0; i < 64; i++) {
        qBDD same = binary_apply(t, qBDD_false(), addLeaf);
        TEST_ASSERT(same == t);
    }
    forceGC();
    LEAF_TYPE v = qBDD_getTerminalValue(t);
    TEST_ASSERT(v.pImpl != NULL);
    TEST_ASSERT_NEAR(to_double_generic(v.pImpl->re), 7.0, 1e-12);

    qBDD_unprotect(t);
    freePackage();
}

static void test_motobuddy_freepimpl_frees_unused_pimpl(void) {
    TEST_SECTION("MoToBuddy freefun frees unused pImpl (counter)");

    setup_pkg();
    medusa_mem_reset();

    LEAF_TYPE *a = heap_leaf(1.0, 0.0);
    /* heap_leaf bypasses allocPimpl counters — note manually for the stored one */
    medusa_mem_note_pimpl_alloc();
    qBDD t = qBDD_maketerminal(qBDD_classicLType(), a);
    qBDD_protect(t);

    size_t a0, f0, w0;
    medusa_mem_get(&a0, &f0, &w0);

    const int N = 128;
    for (int i = 0; i < N; i++) {
        qBDD same = binary_apply(t, qBDD_false(), addLeaf);
        TEST_ASSERT(same == t);
    }

    size_t a1, f1, w1;
    medusa_mem_get(&a1, &f1, &w1);

    /* Each addLeaf(t, false) clones then MoToBuddy freePimpls the unused clone */
    TEST_ASSERT_MSG(f1 >= f0 + (size_t)N,
        "expected freePimpl called once per equal-result apply");
    TEST_ASSERT_MSG(w1 >= w0 + (size_t)N,
        "expected apply wrapper alloc per iteration");
    /* Net live pImpl from this loop should not grow by N (clones were freed) */
    TEST_ASSERT_MSG((a1 - f1) <= (a0 - f0) + 2,
        "live pImpl grew — unused results not freed via freePimpl");

    LEAF_TYPE v = qBDD_getTerminalValue(t);
    TEST_ASSERT(v.pImpl != NULL);
    TEST_ASSERT_NEAR(to_double_generic(v.pImpl->re), 1.0, 1e-12);

    qBDD_unprotect(t);
    freePackage();
}

static void test_maketerminal_dedup_frees_via_freepimpl(void) {
    TEST_SECTION("maketerminal dedup calls freePimpl on unused value");

    setup_pkg();
    medusa_mem_reset();

    LEAF_TYPE *first = heap_leaf(0.25, 0.75);
    medusa_mem_note_pimpl_alloc();
    qBDD t = qBDD_maketerminal(qBDD_classicLType(), first);

    size_t a0, f0, w0;
    medusa_mem_get(&a0, &f0, &w0);

    const int N = 64;
    for (int i = 0; i < N; i++) {
        LEAF_TYPE *dup = heap_leaf(0.25, 0.75);
        medusa_mem_note_pimpl_alloc();
        qBDD t2 = qBDD_maketerminal(qBDD_classicLType(), dup);
        TEST_ASSERT(t2 == t);
    }

    size_t a1, f1, w1;
    medusa_mem_get(&a1, &f1, &w1);
    (void)w0; (void)w1;

    TEST_ASSERT_MSG(f1 >= f0 + (size_t)N,
        "dedup must freePimpl each unused duplicate payload");
    TEST_ASSERT_MSG((a1 - f1) == (a0 - f0),
        "live pImpl count must be unchanged after dedup flood");

    LEAF_TYPE v = qBDD_getTerminalValue(t);
    TEST_ASSERT_NEAR(to_double_generic(v.pImpl->re), 0.25, 1e-12);

    freePackage();
}

static void test_cancel_apply_no_pimpl_leak(void) {
    TEST_SECTION("cancel apply (a + -a) does not leak pImpl");

    setup_pkg();
    qBDD circ;
    circuit_init_interface(&circ, 1);
    qBDD_protect(circ);

    qBDD leaf = circ;
    while (qBDD_isInternal(leaf))
        leaf = qBDD_getLow(leaf);
    qBDD_protect(leaf);

    medusa_mem_reset();
    size_t a0, f0, w0;
    medusa_mem_get(&a0, &f0, &w0);

    for (int i = 0; i < 64; i++) {
        qBDD neg = unary_apply(leaf, invertLeaf);
        qBDD_protect(neg);
        qBDD z = binary_apply(leaf, neg, addLeaf);
        TEST_ASSERT(qBDD_isFalse(z));
        qBDD_unprotect(neg);
        forceGC();
    }

    size_t a1, f1, w1;
    medusa_mem_get(&a1, &f1, &w1);
    (void)w0; (void)w1;

    /* Temps from invert should be freed when unprotected+GC and/or when
     * unused apply results are discarded — live delta must stay small. */
    TEST_ASSERT_MSG((a1 - f1) <= (a0 - f0) + 8,
        "cancel loop leaked many pImpl payloads");
    TEST_ASSERT(f1 > f0);

    LEAF_TYPE v = qBDD_getTerminalValue(leaf);
    TEST_ASSERT(v.pImpl != NULL);

    qBDD_unprotect(leaf);
    qBDD_unprotect(circ);
    freePackage();
}

static void test_freepimpl_contract(void) {
    TEST_SECTION("freePimpl: null-safe, frees pImpl only (not outer wrapper)");

    freePimpl(NULL); /* must not crash */

    LEAF_TYPE *wrap = heap_leaf(3.0, -1.5);
    void *outer = wrap;
    freePimpl(wrap);
    /* Outer LEAF_TYPE* must still be freeable by caller (MoToBuddy does free()). */
    TEST_ASSERT(wrap == outer);
    TEST_ASSERT(wrap->pImpl == NULL);
    free(wrap);

    /* Second freePimpl on emptied wrapper is safe */
    LEAF_TYPE empty = { .pImpl = NULL };
    LEAF_TYPE *ew = malloc(sizeof(LEAF_TYPE));
    *ew = empty;
    freePimpl(ew);
    free(ew);
}

static void test_clone_independent_of_source(void) {
    TEST_SECTION("clonePimpl is deep-independent");

    LEAF_TYPE src = make_leaf(2.0, 4.0);
    LEAF_TYPE c = clonePimpl(src);
    TEST_ASSERT(c.pImpl != NULL);
    TEST_ASSERT(c.pImpl != src.pImpl);

    set_d_generic(src.pImpl->re, 99.0);
    TEST_ASSERT_NEAR(to_double_generic(c.pImpl->re), 2.0, 1e-12);

    LEAF_TYPE *cw = wrap_owned(c);
    buddy_free_unused_result(cw);
    TEST_ASSERT_NEAR(to_double_generic(src.pImpl->re), 99.0, 1e-12);

    destroy_owned_leaf(&src);
}

static void test_apply_equal_result_free_does_not_touch_live(void) {
    TEST_SECTION("apply equal-result free path (simulate MoToBuddy)");

    /* Case: addLeaf(NULL, live) clones; freeing the clone must leave live intact */
    LEAF_TYPE live = make_leaf(1.25, -0.5);
    LEAF_TYPE z = { .pImpl = NULL };

    LEAF_TYPE r = addLeaf(z, live);
    TEST_ASSERT(terminal_compare_generic(
        &(LEAF_TYPE){ .pImpl = r.pImpl },
        &(LEAF_TYPE){ .pImpl = live.pImpl }));
    TEST_ASSERT(r.pImpl != live.pImpl);

    buddy_free_unused_result(wrap_owned(r));
    TEST_ASSERT_NEAR(to_double_generic(live.pImpl->re), 1.25, 1e-12);
    TEST_ASSERT_NEAR(to_double_generic(live.pImpl->im), -0.5, 1e-12);

    /* subLeaf(live, NULL) same contract */
    LEAF_TYPE r2 = subLeaf(live, z);
    TEST_ASSERT(r2.pImpl != live.pImpl);
    buddy_free_unused_result(wrap_owned(r2));
    TEST_ASSERT_NEAR(to_double_generic(live.pImpl->re), 1.25, 1e-12);

    /* unary invert: free unused result after compare-equal to another invert */
    LEAF_TYPE inv = invertLeaf(live);
    LEAF_TYPE inv2 = invertLeaf(live);
    TEST_ASSERT(inv.pImpl != inv2.pImpl);
    TEST_ASSERT(terminal_compare_generic(
        &(LEAF_TYPE){ .pImpl = inv.pImpl },
        &(LEAF_TYPE){ .pImpl = inv2.pImpl }));
    buddy_free_unused_result(wrap_owned(inv2));
    TEST_ASSERT_NEAR(to_double_generic(inv.pImpl->re), -1.25, 1e-12);

    buddy_free_unused_result(wrap_owned(inv));
    destroy_owned_leaf(&live);
}

static void test_leaf_ops_null_and_cancel(void) {
    TEST_SECTION("leaf ops: NULL/NULL, cancel-to-zero");

    LEAF_TYPE z = { .pImpl = NULL };
    LEAF_TYPE r = addLeaf(z, z);
    TEST_ASSERT(r.pImpl == NULL);

    LEAF_TYPE one = make_leaf(1.0, 0.0);
    r = mulLeaf(z, one);
    TEST_ASSERT(r.pImpl == NULL);

    LEAF_TYPE a = make_leaf(0.5, 0.25);
    LEAF_TYPE neg = invertLeaf(a);
    LEAF_TYPE sum = addLeaf(a, neg);
    TEST_ASSERT_MSG(sum.pImpl == NULL, "a + (-a) must be NULL leaf (false)");

    buddy_free_unused_result(wrap_owned(neg));
    destroy_owned_leaf(&a);
    destroy_owned_leaf(&one);
}

static void test_maketerminal_dedup_preserves_stored_value(void) {
    TEST_SECTION("maketerminal dedup frees unused without corrupting store");

    setup_pkg();

    LEAF_TYPE *first = heap_leaf(0.3, 0.4);
    qBDD t = qBDD_maketerminal(qBDD_classicLType(), first);
    TEST_ASSERT(qBDD_isTerminal(t));

    /* Many equal inserts — each unused copy must be freed; stored value stays */
    for (int i = 0; i < 32; i++) {
        LEAF_TYPE *dup = heap_leaf(0.3, 0.4);
        qBDD t2 = qBDD_maketerminal(qBDD_classicLType(), dup);
        TEST_ASSERT(t2 == t);
    }

    forceGC();
    LEAF_TYPE v = qBDD_getTerminalValue(t);
    TEST_ASSERT(v.pImpl != NULL);
    TEST_ASSERT_NEAR(to_double_generic(v.pImpl->re), 0.3, 1e-12);
    TEST_ASSERT_NEAR(to_double_generic(v.pImpl->im), 0.4, 1e-12);

    /* Distinct value must allocate a new terminal */
    LEAF_TYPE *other = heap_leaf(-0.3, 0.4);
    qBDD t3 = qBDD_maketerminal(qBDD_classicLType(), other);
    TEST_ASSERT(t3 != t);
    LEAF_TYPE v3 = qBDD_getTerminalValue(t3);
    TEST_ASSERT_NEAR(to_double_generic(v3.pImpl->re), -0.3, 1e-12);

    /* Original still intact after peer creation */
    v = qBDD_getTerminalValue(t);
    TEST_ASSERT_NEAR(to_double_generic(v.pImpl->re), 0.3, 1e-12);

    freePackage();
}

/**
 * MoToBuddy grows customPointers when mtbddTerminalUsed hits INITIAL_TERMINAL_SIZE
 * (10000). Past bugs corrupted or lost earlier terminal payloads across that realloc.
 */
static void test_terminal_table_realloc_preserves_values(void) {
    TEST_SECTION("terminal table realloc keeps early terminals readable");

    setup_pkg();
    const unsigned boundary = (unsigned)INITIAL_TERMINAL_SIZE;
    const unsigned N = boundary + 128;
    TEST_ASSERT(N > boundary);

    qBDD first = 0, early = 0, at_boundary_prev = 0, at_boundary = 0, last = 0;
    int size0 = mtbddmaxTerminalSize;
    int saw_realloc = 0;

    for (unsigned i = 0; i < N; i++) {
        double re = (double)i + 0.125;
        double im = -((double)i) * 0.03125;
        LEAF_TYPE *leaf = heap_leaf(re, im);
        qBDD t = qBDD_maketerminal(qBDD_classicLType(), leaf);
        TEST_ASSERT(qBDD_isTerminal(t));

        if (mtbddmaxTerminalSize > size0) {
            saw_realloc = 1;
            size0 = mtbddmaxTerminalSize;
            /* Values inserted before growth must still be intact. */
            if (first) {
                LEAF_TYPE v = qBDD_getTerminalValue(first);
                TEST_ASSERT(v.pImpl != NULL);
                TEST_ASSERT_NEAR(to_double_generic(v.pImpl->re), 0.125, 1e-12);
            }
            if (early) {
                LEAF_TYPE v = qBDD_getTerminalValue(early);
                TEST_ASSERT_NEAR(to_double_generic(v.pImpl->re), 1.125, 1e-12);
                TEST_ASSERT_NEAR(to_double_generic(v.pImpl->im), -0.03125, 1e-12);
            }
            if (at_boundary_prev) {
                LEAF_TYPE v = qBDD_getTerminalValue(at_boundary_prev);
                TEST_ASSERT_NEAR(to_double_generic(v.pImpl->re),
                                 (double)(boundary - 1) + 0.125, 1e-9);
            }
        }

        if (i == 0) {
            first = qBDD_protect(t);
        } else if (i == 1) {
            early = qBDD_protect(t);
        } else if (i == boundary - 1) {
            at_boundary_prev = qBDD_protect(t);
        } else if (i == boundary) {
            at_boundary = qBDD_protect(t);
            /* Dedup across the realloc edge (same bit-pattern as stored leaf). */
            LEAF_TYPE *dup = heap_leaf(re, im);
            qBDD t2 = qBDD_maketerminal(qBDD_classicLType(), dup);
            TEST_ASSERT(t2 == t);
        } else if (i == N - 1) {
            last = qBDD_protect(t);
        }
    }

    TEST_ASSERT_MSG(saw_realloc, "expected customPointers realloc past INITIAL_TERMINAL_SIZE");
    TEST_ASSERT(mtbddmaxTerminalSize >= (int)N);

    /* Dedup a pre-realloc terminal after the table has already grown. */
    {
        LEAF_TYPE *dup = heap_leaf(1.125, -0.03125);
        qBDD t2 = qBDD_maketerminal(qBDD_classicLType(), dup);
        TEST_ASSERT(t2 == early);
        LEAF_TYPE v = qBDD_getTerminalValue(early);
        TEST_ASSERT_NEAR(to_double_generic(v.pImpl->re), 1.125, 1e-12);
        TEST_ASSERT_NEAR(to_double_generic(v.pImpl->im), -0.03125, 1e-12);
    }

    forceGC();
    forceGC();

    {
        LEAF_TYPE v = qBDD_getTerminalValue(first);
        TEST_ASSERT_NEAR(to_double_generic(v.pImpl->re), 0.125, 1e-12);
    }
    {
        LEAF_TYPE v = qBDD_getTerminalValue(early);
        TEST_ASSERT_NEAR(to_double_generic(v.pImpl->re), 1.125, 1e-12);
        TEST_ASSERT_NEAR(to_double_generic(v.pImpl->im), -0.03125, 1e-12);
    }
    {
        LEAF_TYPE v = qBDD_getTerminalValue(at_boundary_prev);
        TEST_ASSERT_NEAR(to_double_generic(v.pImpl->re),
                         (double)(boundary - 1) + 0.125, 1e-9);
    }
    {
        LEAF_TYPE v = qBDD_getTerminalValue(at_boundary);
        TEST_ASSERT_NEAR(to_double_generic(v.pImpl->re),
                         (double)boundary + 0.125, 1e-9);
    }
    {
        LEAF_TYPE v = qBDD_getTerminalValue(last);
        TEST_ASSERT_NEAR(to_double_generic(v.pImpl->re),
                         (double)(N - 1) + 0.125, 1e-9);
    }

    qBDD_unprotect(first);
    qBDD_unprotect(early);
    qBDD_unprotect(at_boundary_prev);
    qBDD_unprotect(at_boundary);
    qBDD_unprotect(last);
    freePackage();
}

static void test_apply_cancel_and_reuse_terminals(void) {
    TEST_SECTION("binary_apply cancel + reuse live terminal after free-of-unused");

    setup_pkg();
    qBDD circ;
    circuit_init_interface(&circ, 1);
    qBDD_protect(circ);

    qBDD leaf = circ;
    while (qBDD_isInternal(leaf))
        leaf = qBDD_getLow(leaf);
    qBDD_protect(leaf);

    qBDD neg = unary_apply(leaf, invertLeaf);
    qBDD_protect(neg);
    qBDD zero = binary_apply(leaf, neg, addLeaf);
    forceGC();
    TEST_ASSERT(qBDD_isFalse(zero));

    /* Live leaf must still be readable after cancel produced unused temps */
    LEAF_TYPE v = qBDD_getTerminalValue(leaf);
    TEST_ASSERT(v.pImpl != NULL);
    TEST_ASSERT_NEAR(to_double_generic(v.pImpl->re), 1.0, 1e-12);

    /* add(leaf, false) returns same terminal (equal-result free path) */
    qBDD same = binary_apply(leaf, qBDD_false(), addLeaf);
    qBDD_protect(same);
    forceGC();
    TEST_ASSERT(same == leaf);
    v = qBDD_getTerminalValue(leaf);
    TEST_ASSERT_NEAR(to_double_generic(v.pImpl->re), 1.0, 1e-12);

    qBDD_unprotect(same);
    qBDD_unprotect(neg);
    qBDD_unprotect(leaf);
    qBDD_unprotect(circ);
    freePackage();
}

static void test_unary_apply_identity_free_path(void) {
    TEST_SECTION("unary_apply free-of-unused when result equals input");

    setup_pkg();
    qBDD circ;
    circuit_init_interface(&circ, 1);
    qBDD_protect(circ);

    qBDD leaf = circ;
    while (qBDD_isInternal(leaf))
        leaf = qBDD_getLow(leaf);
    qBDD_protect(leaf);

    /* invert twice → back to original value; second invert of invert may dedup */
    qBDD n1 = qBDD_protect(unary_apply(leaf, invertLeaf));
    qBDD n2 = qBDD_protect(unary_apply(n1, invertLeaf));
    forceGC();

    TEST_ASSERT(qBDD_isTerminal(n2));
    LEAF_TYPE v = qBDD_getTerminalValue(n2);
    TEST_ASSERT_NEAR(to_double_generic(v.pImpl->re), 1.0, 1e-12);

    /* Original leaf still valid */
    v = qBDD_getTerminalValue(leaf);
    TEST_ASSERT_NEAR(to_double_generic(v.pImpl->re), 1.0, 1e-12);

    qBDD_unprotect(n2);
    qBDD_unprotect(n1);
    qBDD_unprotect(leaf);
    qBDD_unprotect(circ);
    freePackage();
}

static void test_many_terminals_survive_gc_when_protected(void) {
    TEST_SECTION("many distinct terminals survive GC while rooted");

    setup_pkg();
    qBDD circ;
    circuit_init_interface(&circ, 3);
    qBDD_protect(circ);

    /* Create a spread of leaf values via gates */
    gate_h(&circ, 0);
    gate_h(&circ, 1);
    gate_x(&circ, 2);
    gate_s(&circ, 0);
    gate_t(&circ, 1);
    forceGC();
    TEST_ASSERT_NEAR(qBDD_total_prob(circ, 3), 1.0, 1e-8);

    /* Walk all terminals reachable from root — none may have NULL pImpl */
    qBDD stack[64];
    int sp = 0;
    stack[sp++] = circ;
    int terminals = 0;
    while (sp > 0) {
        qBDD n = stack[--sp];
        if (qBDD_isFalse(n)) continue;
        if (qBDD_isTerminal(n)) {
            LEAF_TYPE v = qBDD_getTerminalValue(n);
            TEST_ASSERT_MSG(v.pImpl != NULL, "protected terminal has NULL pImpl after GC");
            terminals++;
            continue;
        }
        if (qBDD_isInternal(n) && sp + 2 < 64) {
            stack[sp++] = qBDD_getLow(n);
            stack[sp++] = qBDD_getHigh(n);
        }
    }
    TEST_ASSERT(terminals >= 1);

    qBDD_unprotect(circ);
    freePackage();
}

static void test_stress_gates_with_gc_terminals_intact(void) {
    TEST_SECTION("stress gates + GC: terminals stay readable");

    setup_pkg();
    qBDD circ;
    circuit_init_interface(&circ, 4);
    qBDD_protect(circ);

    for (int round = 0; round < 8; round++) {
        gate_h(&circ, (uint32_t)(round % 4));
        gate_x(&circ, (uint32_t)((round + 1) % 4));
        gate_z(&circ, (uint32_t)((round + 2) % 4));
        if (round % 2 == 0)
            gate_cnot(&circ, (uint32_t)(round % 4), (uint32_t)((round + 1) % 4));
        forceGC();
        TEST_ASSERT_NEAR(qBDD_total_prob(circ, 4), 1.0, 1e-7);

        qBDD walk = circ;
        int steps = 0;
        while (qBDD_isInternal(walk) && steps++ < 16)
            walk = qBDD_getLow(walk);
        if (qBDD_isTerminal(walk) && !qBDD_isFalse(walk)) {
            LEAF_TYPE v = qBDD_getTerminalValue(walk);
            TEST_ASSERT(v.pImpl != NULL);
        }
    }

    qBDD_unprotect(circ);
    freePackage();
}

static void test_terminal_hash_compare_consistency(void) {
    TEST_SECTION("terminal hash/compare consistency for equal/unequal");

    LEAF_TYPE *a = heap_leaf(1.0, 2.0);
    LEAF_TYPE *b = heap_leaf(1.0, 2.0);
    LEAF_TYPE *c = heap_leaf(1.0, 2.0000001);

    TEST_ASSERT(terminal_compare_generic(a, b));
    TEST_ASSERT(terminal_hash_generic(a) == terminal_hash_generic(b));

    /* Very close but not equal under exact float compare used by cmp_generic */
    int eq = terminal_compare_generic(a, c);
    if (eq) {
        TEST_ASSERT(terminal_hash_generic(a) == terminal_hash_generic(c));
    }

    TEST_ASSERT(!terminal_compare_generic(a, NULL));
    TEST_ASSERT(terminal_compare_generic(NULL, NULL));

    destroy_owned_leaf(a); free(a);
    destroy_owned_leaf(b); free(b);
    destroy_owned_leaf(c); free(c);
}

static void test_get_terminal_value_null_safe_on_false(void) {
    TEST_SECTION("false terminal / NULL leaf handling in apply");

    setup_pkg();
    qBDD f = qBDD_false();
    TEST_ASSERT(qBDD_isFalse(f));

    qBDD both = binary_apply(f, f, addLeaf);
    TEST_ASSERT(qBDD_isFalse(both));

    LEAF_TYPE *one = heap_leaf(1.0, 0.0);
    qBDD t = qBDD_maketerminal(qBDD_classicLType(), one);
    qBDD_protect(t);

    qBDD sum = binary_apply(t, f, addLeaf);
    qBDD_protect(sum);
    forceGC();
    TEST_ASSERT(sum == t);
    LEAF_TYPE v = qBDD_getTerminalValue(t);
    TEST_ASSERT(v.pImpl != NULL);

    qBDD_unprotect(sum);
    qBDD_unprotect(t);
    freePackage();
}

/* -------------------------------------------------------------------------- */

int main(void) {
    printf("MEDUSA MoToBuddy API tests\n");

    test_leaf_add_does_not_alias();
    test_freepimpl_registered_with_motobuddy();
    test_motobuddy_freepimpl_frees_unused_pimpl();
    test_maketerminal_dedup_frees_via_freepimpl();
    test_cancel_apply_no_pimpl_leak();
    test_freepimpl_contract();
    test_clone_independent_of_source();
    test_apply_equal_result_free_does_not_touch_live();
    test_leaf_ops_null_and_cancel();
    test_terminal_hash_compare_consistency();
    test_terminal_compare_and_maketerminal();
    test_maketerminal_dedup_preserves_stored_value();
    test_terminal_table_realloc_preserves_values();
    test_node_classification();
    test_protect_unprotect_refcount();
    test_apply_free_unused_preserves_terminals();
    test_binary_apply_with_false();
    test_apply_cancel_and_reuse_terminals();
    test_unary_apply_identity_free_path();
    test_get_terminal_value_null_safe_on_false();
    test_many_terminals_survive_gc_when_protected();
    test_stress_gates_with_gc_terminals_intact();
    test_gates_norm_and_idempotence();
    test_x_flips_computational_basis();
    test_grover_2q_marks_11();
    test_bell_state();
    test_cnot_protect_balance();
    test_toffoli_norm_regression();

    return test_report("test_unit_api");
}
