/**
 * @file leaf_algebraic_acn.cpp
 * Leaf arithmetic over acn algebraic complex numbers (buddy_acn target).
 */

extern "C" {
#include "../../symb_utils.h"
#include "../../mtbdd_symb_val.h"
#include "../../medusa_mem_track.h"
#include "../../error.h"
}

#include "../../../lib/algebraic-complex-numbers/include/acn/acn.hpp"

#include <algorithm>
#include <complex>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_set>

// NOTE: will go inside .hpp

// TODO: Finer rotations!

// NOTE: end of .hpp

using acnValue = acn::Acn<ACN_N, acn::BigInt>;

struct sl_map_t { vars_t v[ACN_N]; };

struct sl_val_t { symexp_list_t *e[ACN_N]; };

static int64_t g_sqrt2_exp = 0;
static int64_t g_sqrt2_exp_symb = 0;

// Common K during the evaluation of symbolic loops
static acn::ScaleExp g_symb_common_k = 0;

// BigInt and leaf_primitive_t (i.e mpz) are the same thing
struct LEAF_TYPE_IMPL {
    acnValue acn;
};

static inline LEAF_TYPE allocPimpl(acnValue& from)
{
    medusa_mem_note_pimpl_alloc();
    return { .pImpl = new LEAF_TYPE_IMPL{ from }};
}

static inline LEAF_TYPE allocPimpl(acnValue&& from)
{
    medusa_mem_note_pimpl_alloc();
    return { .pImpl = new LEAF_TYPE_IMPL{ std::move(from) }};
}


extern "C" {

/*
 * Tuple lifecycle
 */

LEAF_TYPE clonePimpl(LEAF_TYPE a)
{
  if (!a.pImpl)
        return (LEAF_TYPE){ .pImpl = NULL };
     return allocPimpl(a.pImpl->acn);
}

void freePimpl(void *leafraw)
{
    if (!leafraw) return;

    LEAF_TYPE *leaf = (LEAF_TYPE*) leafraw;
    if (!leaf->pImpl) return;

    delete leaf->pImpl;
    leaf->pImpl = NULL;
    medusa_mem_note_pimpl_free();
}

/*
 * Tuple identity (compare + hash)
 */

int terminal_compare_generic(void *a, void *b)
{
    LEAF_TYPE *ldata_a = (LEAF_TYPE *) a;
    LEAF_TYPE *ldata_b = (LEAF_TYPE *) b;

    bool null_a = (ldata_a == NULL || ldata_a->pImpl == NULL);
    bool null_b = (ldata_b == NULL || ldata_b->pImpl == NULL);

    if (null_a && null_b) return 1;   // both NULL --  equal
    if (null_a || null_b) return 0;   // one NULL -- not equal

    return ldata_a->pImpl->acn == ldata_b->pImpl->acn;
}

unsigned terminal_hash_generic(void *q)
{
    LEAF_TYPE *ldata = (LEAF_TYPE*) q;
    if (!ldata || !ldata->pImpl) return 0;
    uint64_t val = acn::hash_value(ldata->pImpl->acn);
    return (unsigned)(val ^ (val >> 32));
}

/*
 * Output
 */

static int g_print_leaf_prob = 0;

void setLeafPrintProb(bool is_prob)
{
    g_print_leaf_prob = is_prob ? 1 : 0;
}

static double leaf_prob(const LEAF_TYPE_IMPL *l)
{
    return std::norm(l->acn.scale(static_cast<acn::ScaleExp>(-g_sqrt2_exp)).to_complex());
}

/* "ω", "ω²", "ω³", ... with superscript digits, for any ACN_N. */
static std::string omega_pow(std::size_t j)
{
    static const char *sup[] = {"⁰", "¹", "²", "³", "⁴", "⁵", "⁶", "⁷", "⁸", "⁹"};
    std::string s = "ω";
    if (j > 1) {
        for (char d : std::to_string(j))
            s += sup[d - '0'];
    }
    return s;
}

char *terminal_to_str_generic(void *ldata_raw, char *buddy_buf,
                              size_t buddy_bufsize)
{
    LEAF_TYPE *leaf = (LEAF_TYPE*) ldata_raw;
    std::string s;

    if (!leaf || !leaf->pImpl) {
        s = g_print_leaf_prob ? "0.000000" : "0";
    }
    else if (g_print_leaf_prob) {
        char prob[64];
        std::snprintf(prob, sizeof(prob), "%f", leaf_prob(leaf->pImpl));
        s = prob;
    }
    else {
        // Same label as the mpz leaf: (1/√2)^(k) * (a+bω+cω²+dω³), where k is
        // the amplitude's exponent: the leaf's own plus the global one.
        const acnValue &v = leaf->pImpl->acn;
        s = "(1/√2)^(" + std::to_string((long long)v.scale_exp() + g_sqrt2_exp) + ") * (";
        auto coeffs = v.coeffs();
        for (std::size_t j = 0; j < coeffs.size(); j++) {
            std::string num = coeffs[j].to_string();
            if (j > 0 && num[0] != '-')
                s += "+";
            s += num;
            if (j > 0)
                s += omega_pow(j);
        }
        s += ")";
    }

    if (s.size() + 1 > buddy_bufsize) {
        buddy_buf = (char*) std::realloc(buddy_buf, s.size() + 1);
        if (buddy_buf == NULL)
            error_exit("Bad memory allocation.\n");
    }
    std::memcpy(buddy_buf, s.c_str(), s.size() + 1);
    return buddy_buf;
}

/*
 * Algebraic operations classical
 */

LEAF_TYPE invertLeaf(LEAF_TYPE a)
{
    if (a.pImpl == NULL) return a;
    return allocPimpl(-a.pImpl->acn);
}

LEAF_TYPE addLeaf(LEAF_TYPE a, LEAF_TYPE b)
{
    if (a.pImpl == NULL) return clonePimpl(b);
    if (b.pImpl == NULL) return clonePimpl(a);

    // TODO: check before allocating for zero
    LEAF_TYPE r { allocPimpl(a.pImpl->acn + b.pImpl->acn) };

    if (r.pImpl->acn.is_zero()) {
        freePimpl(reinterpret_cast<void *>(&r));
        return (LEAF_TYPE){ .pImpl = NULL };
    }

    return r;
}

LEAF_TYPE addLeafS(LEAF_TYPE a, LEAF_TYPE b)
{
    // multiplication by 1/sqrt(2) done externally
    return addLeaf(a, b);
}

LEAF_TYPE subLeaf(LEAF_TYPE a, LEAF_TYPE b)
{
    if (a.pImpl == NULL) return invertLeaf(b);
    if (b.pImpl == NULL) return clonePimpl(a);

    LEAF_TYPE r { allocPimpl(a.pImpl->acn - b.pImpl->acn) };

    // TODO: check before allocating for zero
    if (r.pImpl->acn.is_zero()) {
        freePimpl(reinterpret_cast<void *>(&r));
        return (LEAF_TYPE){ .pImpl = NULL };
    }

    return r;
}

LEAF_TYPE subLeafS(LEAF_TYPE a, LEAF_TYPE b)
{
    // multiplication by 1/sqrt(2) done externally
    return subLeaf(a, b);
}

LEAF_TYPE rotateCoef1(LEAF_TYPE l)
{
    if (l.pImpl == NULL) return l;
    return { allocPimpl(l.pImpl->acn.mul_omega(ACN_N / 4)) };
}

LEAF_TYPE rotateCoef1S(LEAF_TYPE l)
{
    // multiplication by 1/sqrt(2) done externally
    return rotateCoef1(l);
}

LEAF_TYPE rotateCoef1S_inv(LEAF_TYPE l)
{
    if (l.pImpl == NULL) return l;
    return { allocPimpl(l.pImpl->acn.mul_omega(-(ACN_N / 4))) };
}

LEAF_TYPE rotateCoef2(LEAF_TYPE l)
{
    if (l.pImpl == NULL) return l;
    return { allocPimpl(l.pImpl->acn.mul_omega(ACN_N * 2 / 4)) };
}

LEAF_TYPE rotateCoef2S(LEAF_TYPE l)
{
    // multiplication by 1/sqrt(2) done externally
    return rotateCoef2(l);
}

LEAF_TYPE times2Leaf(LEAF_TYPE l)
{
    if (l.pImpl == NULL) return l;
    return { allocPimpl( l.pImpl->acn.scale(2) ) };
}

LEAF_TYPE times2LeafS(LEAF_TYPE l)
{
    // multiplication by 1/sqrt(2) done externally
    return times2Leaf(l);
}

/*
 * Symbolic terminals and operations
 */

LEAF_TYPE terminal_symb_val_clone(LEAF_TYPE a) {}

LEAF_TYPE terminal_symb_map_clone(LEAF_TYPE a) {}

LEAF_TYPE mtbdd_symb_neg_i(LEAF_TYPE t) {}

LEAF_TYPE mtbdd_symb_coef_rot1_i(LEAF_TYPE t) {}

LEAF_TYPE mtbdd_symb_coef_rot2_i(LEAF_TYPE t) {}

LEAF_TYPE mtbdd_symb_plus_i(LEAF_TYPE a, LEAF_TYPE b) {}

LEAF_TYPE mtbdd_symb_minus_i(LEAF_TYPE a, LEAF_TYPE b) {}

LEAF_TYPE mtbdd_symb_times_c_i(LEAF_TYPE t, size_t c_raw) {}

qBDD mtbdd_map_to_symb_val_reduced_i(qBDD t, size_t raw_map) {}

qBDD mtbdd_map_to_symb_val_i(qBDD t, size_t raw_map) {}

LEAF_TYPE mtbdd_symb_coef_rot1_i_inv(LEAF_TYPE t) {}

qBDD t_xt_comp_create_i(qBDD t, size_t xt)
{
    if (qBDD_isFalse(t)) {
        validateApplyResult();
        return qBDD_false();
    }


    if (qBDD_isInternal(t)) {
        if(qBDD_getVar(t) == xt) {
            validateApplyResult();
            return qBDD_getLow(t);
        }
    } else {
        validateApplyResult();
        return t;
    }

    invalidateApplyResult(); // further recursion
    return t; // any, not considered
}

qBDD t_xt_create_i(qBDD t, size_t xt)
{
    if (qBDD_isFalse(t)) return qBDD_false();

    if (qBDD_isInternal(t)) {
        if(qBDD_getVar(t) == xt) {
            validateApplyResult();
            return qBDD_getHigh(t);
        }
    } else {
        validateApplyResult();
        return t;
    }

    invalidateApplyResult(); // further recursion
    return t; // any, not considered
}

qBDD mtbdd_symb_refine_i(qBDD map, qBDD val, size_t rd_raw) {}

qBDD mtbdd_to_symb_map_i(qBDD a, size_t raw_m) {}

/*
 * Terminal type registration
 */

void terminal_symb_map_free(void *leafraw) {}

void terminal_symb_val_free(void *leafraw) {}

int terminal_symb_map_compare_generic(void *l_a, void *l_b) {}

unsigned terminal_symb_map_hash_generic(void *l_a) {}

int terminal_symb_val_compare_generic(void *l_a, void *l_b) {}

unsigned terminal_symb_val_hash_generic(void *l_a) {}

qBDD mtbdd_from_symb_i(qBDD t, size_t raw_map) {}

bool can_be_reduced(mtbdd_symb_t *symbc, rdata_t *rdata) {}

/*
 * Package bootstrap
 */

void circuit_init_interface(qBDD *c, const uint32_t n)
{
    unsigned varNum = n;
    if (bdd_varnum() < varNum)
        bdd_setvarnum(varNum);

    g_sqrt2_exp = 0;

    LEAF_TYPE *one = (LEAF_TYPE*) std::malloc(sizeof(LEAF_TYPE));
    if (one == NULL) { bdd_error(BDD_MEMORY); }
    *one = allocPimpl(acnValue::one());

    qBDD leaf1 = qBDD_maketerminal(qBDD_classicLType(), one);
    qBDD_protect(leaf1);

    qBDD cube = leaf1;
    for (uint32_t i = n; i > 0; i--)
        cube = newqBDD(i - 1, cube, qBDD_false());

    qBDD_protect(cube);
    qBDD_unprotect(leaf1);
    *c = cube;
}

static void max_leaf_k(qBDD t, std::unordered_set<qBDD> &seen, acn::ScaleExp &k, bool &found)
{
    if (qBDD_isFalse(t) || !seen.insert(t).second)
        return;
    if (qBDD_isTerminal(t)) {
        LEAF_TYPE tv = qBDD_getTerminalValue(t);
        if (tv.pImpl) {
            acn::ScaleExp lk = tv.pImpl->acn.scale_exp();
            k = found ? std::max(k, lk) : lk;
            found = true;
        }
        return;
    }
    max_leaf_k(qBDD_getLow(t),  seen, k, found);
    max_leaf_k(qBDD_getHigh(t), seen, k, found);
}

void symb_init(qBDD *circ, mtbdd_symb_t *symbc)
{
    std::unordered_set<qBDD> seen;
    acn::ScaleExp k = 0;
    bool found = false;
    max_leaf_k(*circ, seen, k, found);
    g_symb_common_k = k;

    size_t msize = ACN_N * (qBDD_leafcount(*circ));

    vmap_init(&(symbc->vm), msize);
    symbc->is_reduced = true;
    symbc->is_refined = false;

    symbc->map = my_mtbdd_to_symb_map_i(*circ, (size_t)(uintptr_t)symbc->vm);
    qBDD_protect(symbc->map);
    symbc->val = my_mtbdd_map_to_symb_val_i(symbc->map,
                                            (size_t)(uintptr_t)symbc->vm->map,
                                            symbc->is_reduced);
    qBDD_protect(symbc->val);
    initInvSqrtCoeffSymb();
}

/*
 * Probability computation
 */

double qBDD_calculateProb(qBDD terminal)
{
    if (qBDD_isFalse(terminal))
        return 0.0;
    LEAF_TYPE tv = qBDD_getTerminalValue(terminal);
    if (!tv.pImpl)
        return 0.0;
    return leaf_prob(tv.pImpl);
}

/*
 * Global square root coeffs handles
 */

[[noreturn]] static void _fatal_error(int64_t e)
{
    std::fprintf(stderr, "1/sqrt2 exponent %lld exceeds the acn ScaleExp range ", static_cast<long long>(e));
    std::abort();
}

static void check_sqrt2_exp(int64_t e)
{
    if (e > std::numeric_limits<acn::ScaleExp>::max())
    {
        _fatal_error(e);
    }
}

void incInvSqrtCoeff()
{
    check_sqrt2_exp(++g_sqrt2_exp);
}

void incInvSqrtCoeffSymb()
{
    ++g_sqrt2_exp_symb;
}

void initInvSqrtCoeffSymb()
{
    g_sqrt2_exp_symb = 0;
}

void addInvSqrtCoeffs()
{
    int64_t r;
    if (__builtin_add_overflow(g_sqrt2_exp, g_sqrt2_exp_symb, &r))
    {
        _fatal_error(g_sqrt2_exp_symb + g_sqrt2_exp);
    }
    check_sqrt2_exp(r);
    g_sqrt2_exp = r;
}

void mulInvSqrtCoeff(mpz_t res, mpz_t a, unsigned long mul)
{
    mpz_mul_ui(res, a, mul);
}

void clearInvSqrtCoeffSymb()
{
    g_sqrt2_exp_symb = 0;
}

void clearInvSqrtCoeffNormal()
{
    g_sqrt2_exp = 0;
}

void resetInvSqrtCoeffSymb()
{
    g_sqrt2_exp_symb = 0;
}

void mulInvSqrtSymbCoeff(unsigned long a)
{
    int64_t r;
    if (__builtin_mul_overflow(g_sqrt2_exp_symb, (int64_t)a, &r))
    {
        _fatal_error(g_sqrt2_exp_symb * a);
    }
    g_sqrt2_exp_symb = r;
}

/*
 * Rotation and phase gates
 */
[[noreturn]] static void _unsupported(const char *name)
{
    std::fprintf(stderr, "ERROR: %s is not supported by the acn backend (ACN_N=%d). "
                         "Use a floating-point backend for this rotation.\n", name, ACN_N);
    std::abort();
}

LEAF_TYPE rx_low_leaf(LEAF_TYPE low, LEAF_TYPE high, size_t param)
{
    (void)low;(void)high;(void)param; _unsupported("rx_low_leaf");
}

LEAF_TYPE rx_high_leaf(LEAF_TYPE low, LEAF_TYPE high, size_t param)
{
    (void)low;(void)high;(void)param; _unsupported("rx_high_leaf");
}

LEAF_TYPE ry_low_leaf(LEAF_TYPE low, LEAF_TYPE high, size_t param)
{

    (void)low;(void)high;(void)param; _unsupported("ry_low_leaf");
}

LEAF_TYPE ry_high_leaf(LEAF_TYPE low, LEAF_TYPE high, size_t param)
{
    (void)low;(void)high;(void)param; _unsupported("ry_high_leaf");
}

LEAF_TYPE rz_low_leaf(LEAF_TYPE l, size_t param)
{
    (void)l;(void)param; _unsupported("rz_low_leaf");
}

LEAF_TYPE rz_high_leaf(LEAF_TYPE l, size_t param)
{
    (void)l;(void)param; _unsupported("rz_high_leaf");
}

LEAF_TYPE negI_mul(LEAF_TYPE a)
{
    (void)a; _unsupported("negI_mul");
}

LEAF_TYPE mulPhaseLeaf(LEAF_TYPE t, size_t arg)
{
    (void)t;(void)arg; _unsupported("mulPhaseLeaf");
}

} /* extern "C" */
