/**
 * @file leaf_reim_double.c
 * re + im
 */

#include "leaf_reim_double.h"
#include "../../hash.h"
#include "../../symexp_list.h"
#include "../../mtbdd_out.h"
#include "../../symb_utils.h"

mpz_t globalSquareRootCoeff;
mpz_t globalSquareRootCoeffSymb;

/* 
 * Internal layout
 */

struct LEAF_TYPE_IMPL {
    leaf_primitive_t re;
    leaf_primitive_t im;
};

typedef struct sl_val {
    /// ptr to a list representing the symbolic expression for the first variable
    symexp_list_t *re;
    symexp_list_t *im;
} sl_val_t;

/// MTBDD leaf value with the variable mapping for symbolic representation
typedef struct sl_map {
    vars_t vre;
    vars_t vim;
} sl_map_t;

/* 
 * Field accessors
 */

static inline leaf_primitive_t* leafRe(const LEAF_TYPE* leaf) { return &leaf->pImpl->re; }
static inline leaf_primitive_t* leafIm(const LEAF_TYPE* leaf) { return &leaf->pImpl->im; }



/* 
 * Tuple lifecycle
 */

static inline void allocPimpl(LEAF_TYPE* result) {
    result->pImpl = malloc(sizeof(LEAF_TYPE_IMPL)); // leaf_reim_double.c:50
    if (!result->pImpl) exit(1);
    init_generic(result->pImpl->re);
    init_generic(result->pImpl->im);
}

static inline void snap_pimpl(LEAF_TYPE_IMPL *p) {
    p->re[0] = snap(p->re[0]);
    p->im[0] = snap(p->im[0]);
}

/**
 * Cloning for non-NULL leafs, otherwise return NULL leaf
 */
LEAF_TYPE clonePimpl(LEAF_TYPE a) {
    if (!a.pImpl) return (LEAF_TYPE){ .pImpl = NULL };
    LEAF_TYPE r;
    allocPimpl(&r);
    set_generic(r.pImpl->re, a.pImpl->re);
    set_generic(r.pImpl->im, a.pImpl->im);
    return r;
}

LEAF_TYPE multiplyByInvSqrt2(LEAF_TYPE a) {
    if (a.pImpl == NULL) return a;
    LEAF_TYPE result;
    allocPimpl(&result);
    mul_sqrt2inv_generic(result.pImpl->re, a.pImpl->re);
    SNAP_CHECK(result.pImpl->re, "multiplyByInvSqrt2 re");
    mul_sqrt2inv_generic(result.pImpl->im, a.pImpl->im);
    SNAP_CHECK(result.pImpl->im, "multiplyByInvSqrt2 im");
    snap_pimpl(result.pImpl);
    return result;
}

void freePimpl(void* leafraw) {
    if (!leafraw) return;
    LEAF_TYPE *leaf = (LEAF_TYPE*) leafraw;
    if (leaf->pImpl) {
        clear_generic(leaf->pImpl->re);
        clear_generic(leaf->pImpl->im);
        free(leaf->pImpl);
        leaf->pImpl = NULL;
    }
    free(leaf);
}


/*
 * Tuple identity (compare + hash)
 */

int terminal_compare_generic(void* a, void* b) {
    LEAF_TYPE *ldata_a = (LEAF_TYPE *) a;
    LEAF_TYPE *ldata_b = (LEAF_TYPE *) b;

    bool null_a = (ldata_a == NULL || ldata_a->pImpl == NULL);
    bool null_b = (ldata_b == NULL || ldata_b->pImpl == NULL);

    if (null_a && null_b) return 1;   // both NULL -- equal
    if (null_a || null_b) return 0;   // one NULL -- not equal

    return !cmp_generic(*leafRe(ldata_a), *leafRe(ldata_b)) &&
           !cmp_generic(*leafIm(ldata_a), *leafIm(ldata_b));
}

unsigned terminal_hash_generic(void* q) {
    LEAF_TYPE *ldata = (LEAF_TYPE*) q;
    uint64_t val = 1;
    val = MY_HASH_COMB(val, hash_comb_generic(*leafRe(ldata)));
    val = MY_HASH_COMB(val, hash_comb_generic(*leafIm(ldata)));
    return (unsigned)(val ^ (val >> 32));
}


/*
 * String representation registered with buddy
 */

// static int _leaf_to_str_output_i(char *buf, leaf_scalar_t re, leaf_scalar_t im, int global_sqrt) {
// #if LEAF_FLOAT_TYPE == LEAF_TYPE_QUAD
//     // quadmath_snprintf only supports ONE %Q specifier per call
//     char re_buf[64], im_buf[64];
//     quadmath_snprintf(re_buf, sizeof(re_buf), "%.30Qg", re);
//     quadmath_snprintf(im_buf, sizeof(im_buf), "%.30Qg", im);

//     // im_buf sign prefix handled manually always show sign
//     char sign = (im < 0.0q) ? '-' : '+';
//     if (im < 0.0q) {
//         // im_buf already has '-', strip it for explicit sign control
//         return snprintf(buf, MAX_LEAF_STR_LEN,
//             "(1/sqrt(2))^(%d) * (%s%c%si)",
//             global_sqrt, re_buf, sign, im_buf + 1);
//     }
//     return snprintf(buf, MAX_LEAF_STR_LEN,
//         "(1/sqrt(2))^(%d) * (%s%c%si)",
//         global_sqrt, re_buf, sign, im_buf);
// #else
//     return snprintf(buf, MAX_LEAF_STR_LEN,
//         "(1/sqrt(2))^(%d) * (%.17Lg%+.17Lgi)", global_sqrt,
//         (long double)re, (long double)im);
// #endif
// }

char* terminal_to_str_generic(void* ldata_raw, char *buddy_buf, size_t buddy_bufsize) {
    LEAF_TYPE *leafValP = (LEAF_TYPE*) ldata_raw;
    if (!leafValP || !leafValP->pImpl) return NULL;

    LEAF_TYPE_IMPL *ldata = leafValP->pImpl;

    char re_buf[64], im_buf[64];
    to_str_generic(ldata->re, re_buf, sizeof(re_buf));
    to_str_generic(ldata->im, im_buf, sizeof(im_buf));

    // determine sign of im for formatting
    char sign = (sgn_generic(ldata->im) < 0) ? '-' : '+';
    const char *im_abs = (sgn_generic(ldata->im) < 0) ? im_buf + 1 : im_buf;

    char ldata_string[MAX_LEAF_STR_LEN] = {0};
    int chars_written = snprintf(ldata_string, MAX_LEAF_STR_LEN,
        "%s%c%si",
        re_buf, sign, im_abs);

    if (chars_written < 0)
        error_exit("An encoding error has occurred when producing leaf value output.\n");
    if (chars_written >= MAX_LEAF_STR_LEN)
        error_exit("Allocated string length for leaf value output has not been sufficient.\n");

    if (chars_written < (int)buddy_bufsize) {
        memcpy(buddy_buf, ldata_string, chars_written);
        buddy_buf[chars_written] = '\0';
        return buddy_buf;
    }

    char *new_buf = (char*)my_malloc((chars_written + 1) * sizeof(char));
    memcpy(new_buf, ldata_string, chars_written);
    new_buf[chars_written] = '\0';
    return new_buf;
}


/*
 * Algebraic operations on leaves re+im
 */

LEAF_TYPE invertLeaf(LEAF_TYPE a) {
    if (a.pImpl == NULL) return a;

    LEAF_TYPE result;
    allocPimpl(&result);
    neg_generic(result.pImpl->re, a.pImpl->re);
    neg_generic(result.pImpl->im, a.pImpl->im);
    snap_pimpl(result.pImpl);
    return result;
}



LEAF_TYPE invertLeafS(LEAF_TYPE a) {
    if (a.pImpl == NULL) return a;

    LEAF_TYPE result;
    allocPimpl(&result);
    neg_generic(result.pImpl->re, a.pImpl->re);
    neg_generic(result.pImpl->im, a.pImpl->im);
    mul_sqrt2inv_generic(result.pImpl->re, result.pImpl->re);
    mul_sqrt2inv_generic(result.pImpl->im, result.pImpl->im);
    snap_pimpl(result.pImpl);

    return result;
}

LEAF_TYPE addLeaf(LEAF_TYPE a, LEAF_TYPE b) {
    if (a.pImpl == NULL) return (b);
    if (b.pImpl == NULL) return (a);

    LEAF_TYPE result;
    allocPimpl(&result);
    add_generic(result.pImpl->re, a.pImpl->re, b.pImpl->re);
    SNAP_CHECK(result.pImpl->re, "addLeaf re");
    add_generic(result.pImpl->im, a.pImpl->im, b.pImpl->im);
    SNAP_CHECK(result.pImpl->im, "addLeaf im");
    snap_pimpl(result.pImpl);
    if (!sgn_generic(result.pImpl->re) && !sgn_generic(result.pImpl->im)) {
        free(result.pImpl);
        return (LEAF_TYPE){ .pImpl = NULL };
    }
    
    return result;
}

LEAF_TYPE addLeafS(LEAF_TYPE a, LEAF_TYPE b) {
    if (a.pImpl == NULL) return multiplyByInvSqrt2(b);
    if (b.pImpl == NULL) return multiplyByInvSqrt2(a);

    LEAF_TYPE result;
    allocPimpl(&result);
    add_generic(result.pImpl->re, a.pImpl->re, b.pImpl->re);
        SNAP_CHECK(result.pImpl->re, "addLeafS re");
    add_generic(result.pImpl->im, a.pImpl->im, b.pImpl->im);
    SNAP_CHECK(result.pImpl->im, "addLeafS im");
    if (!sgn_generic(result.pImpl->re) && !sgn_generic(result.pImpl->im)) {
        free(result.pImpl);
        return (LEAF_TYPE){ .pImpl = NULL };
    }

    mul_sqrt2inv_generic(result.pImpl->re, result.pImpl->re);
    SNAP_CHECK(result.pImpl->re, "addLeafS re after mul_sqrt2inv");
    mul_sqrt2inv_generic(result.pImpl->im, result.pImpl->im);
    SNAP_CHECK(result.pImpl->im, "addLeafS im after mul_sqrt2inv");
    snap_pimpl(result.pImpl);

    return result;
}


LEAF_TYPE subLeaf(LEAF_TYPE a, LEAF_TYPE b) {
    if (a.pImpl == NULL) return invertLeaf(b);
    if (b.pImpl == NULL) return (a);
    LEAF_TYPE result;
    allocPimpl(&result);
    sub_generic(result.pImpl->re, a.pImpl->re, b.pImpl->re);
    SNAP_CHECK(result.pImpl->re, "subLeaf re");
    sub_generic(result.pImpl->im, a.pImpl->im, b.pImpl->im);
    SNAP_CHECK(result.pImpl->im, "subLeaf im");
    snap_pimpl(result.pImpl);
    if (!sgn_generic(result.pImpl->re) && !sgn_generic(result.pImpl->im)) {
        free(result.pImpl);
        return (LEAF_TYPE){ .pImpl = NULL };
    }
    return result;
}

LEAF_TYPE subLeafS(LEAF_TYPE a, LEAF_TYPE b) {
    if (a.pImpl == NULL) return invertLeafS(b);
    if (b.pImpl == NULL) return multiplyByInvSqrt2(a);

    LEAF_TYPE result;
    allocPimpl(&result);
    sub_generic(result.pImpl->re, a.pImpl->re, b.pImpl->re);
    SNAP_CHECK(result.pImpl->re, "subLeafS re");
    sub_generic(result.pImpl->im, a.pImpl->im, b.pImpl->im);
    SNAP_CHECK(result.pImpl->im, "subLeafS im");

    if (!sgn_generic(result.pImpl->re) && !sgn_generic(result.pImpl->im)) {
        free(result.pImpl);
        return (LEAF_TYPE){ .pImpl = NULL };
    }

    mul_sqrt2inv_generic(result.pImpl->re, result.pImpl->re);
    mul_sqrt2inv_generic(result.pImpl->im, result.pImpl->im);
    snap_pimpl(result.pImpl);
    return result;
}

LEAF_TYPE mulLeaf(LEAF_TYPE a, LEAF_TYPE b) {
    if (a.pImpl == NULL || b.pImpl == NULL)
        return (LEAF_TYPE){ .pImpl = NULL };

    LEAF_TYPE result;
    allocPimpl(&result);

    // (re_a + im_a·i)(re_b + im_b·i) = (re_a·re_b - im_a·im_b) + (re_a·im_b + im_a·re_b)·i
    leaf_primitive_t tmp;
    init_generic(tmp);

    mul_generic(result.pImpl->re, a.pImpl->re, b.pImpl->re);  // re = re_a*re_b
    SNAP_CHECK(result.pImpl->re, "mulLeaf re after re*re");
    mul_generic(tmp,              a.pImpl->im, b.pImpl->im);  // tmp = im_a*im_b
        SNAP_CHECK(tmp, "mulLeaf tmp after im*im");
    sub_generic(result.pImpl->re, result.pImpl->re, tmp);     // re -= tmp
        SNAP_CHECK(result.pImpl->re, "mulLeaf re after sub");
    mul_generic(result.pImpl->im, a.pImpl->re, b.pImpl->im);  // im = re_a*im_b
    SNAP_CHECK(result.pImpl->im, "mulLeaf im after re*im");
    mul_generic(tmp,              a.pImpl->im, b.pImpl->re);  // tmp = im_a*re_b
    SNAP_CHECK(tmp, "mulLeaf tmp after im*re");
    add_generic(result.pImpl->im, result.pImpl->im, tmp);     // im += tmp
    SNAP_CHECK(result.pImpl->im, "mulLeaf im after add");
    snap_pimpl(result.pImpl);
    clear_generic(tmp);
    return result;
}

LEAF_TYPE mulLeafS(LEAF_TYPE a, LEAF_TYPE b) {
    if (a.pImpl == NULL || b.pImpl == NULL)
        return (LEAF_TYPE){ .pImpl = NULL };

    LEAF_TYPE result;
    allocPimpl(&result);

    // (re_a + im_a·i)(re_b + im_b·i) = (re_a·re_b - im_a·im_b) + (re_a·im_b + im_a·re_b)·i
    leaf_primitive_t tmp;
    init_generic(tmp);

    mul_generic(result.pImpl->re, a.pImpl->re, b.pImpl->re);
    SNAP_CHECK(result.pImpl->re, "mulLeafS re after re*re");
    mul_generic(tmp,              a.pImpl->im, b.pImpl->im);
    SNAP_CHECK(tmp, "mulLeafS tmp after im*im");
    sub_generic(result.pImpl->re, result.pImpl->re, tmp);
    SNAP_CHECK(result.pImpl->re, "mulLeafS re after sub");

    mul_generic(result.pImpl->im, a.pImpl->re, b.pImpl->im);
    SNAP_CHECK(result.pImpl->im, "mulLeafS im after re*im");
    mul_generic(tmp,              a.pImpl->im, b.pImpl->re);
    SNAP_CHECK(tmp, "mulLeafS tmp after im*re");
    add_generic(result.pImpl->im, result.pImpl->im, tmp);
    SNAP_CHECK(result.pImpl->im, "mulLeafS im after add");

    clear_generic(tmp);

    mul_sqrt2inv_generic(result.pImpl->re, result.pImpl->re);
        SNAP_CHECK(result.pImpl->re, "mulLeafS re after mul_sqrt2inv");
    mul_sqrt2inv_generic(result.pImpl->im, result.pImpl->im);
    snap_pimpl(result.pImpl);
    SNAP_CHECK(result.pImpl->im, "mulLeafS im after mul_sqrt2inv");
    return result;
}


LEAF_TYPE divLeaf(LEAF_TYPE a, LEAF_TYPE b) {
    if (a.pImpl == NULL)
        return (LEAF_TYPE){ .pImpl = NULL };
    

    leaf_primitive_t denom, tmp;
    init_generic(denom);
    init_generic(tmp);

    // denom = re_b^2 + im_b^2
    mul_generic(denom, b.pImpl->re, b.pImpl->re);
        SNAP_CHECK(denom, "divLeaf denom after re*re");
    mul_generic(tmp,   b.pImpl->im, b.pImpl->im);
    SNAP_CHECK(tmp, "divLeaf tmp after im*im");
    add_generic(denom, denom, tmp);
        SNAP_CHECK(denom, "divLeaf denom after add");

    if (!sgn_generic(denom)) {
        clear_generic(denom);
        clear_generic(tmp);
        return (LEAF_TYPE){ .pImpl = NULL };
    }

    LEAF_TYPE result;
    allocPimpl(&result);

    // re = (re_a*re_b + im_a*im_b) / denom
    mul_generic(result.pImpl->re, a.pImpl->re, b.pImpl->re);
    SNAP_CHECK(result.pImpl->re, "divLeaf re after re*re");
    mul_generic(tmp,              a.pImpl->im, b.pImpl->im);
    SNAP_CHECK(tmp, "divLeaf tmp after im*im");
    add_generic(result.pImpl->re, result.pImpl->re, tmp);
    SNAP_CHECK(result.pImpl->re, "divLeaf re after add");
    div_generic(result.pImpl->re, result.pImpl->re, denom);
    SNAP_CHECK(result.pImpl->re, "divLeaf re after div");

    // im = (im_a*re_b - re_a*im_b) / denom
    mul_generic(result.pImpl->im, a.pImpl->im, b.pImpl->re);
    SNAP_CHECK(result.pImpl->im, "divLeaf im after im*re");
    mul_generic(tmp,              a.pImpl->re, b.pImpl->im);
    SNAP_CHECK(tmp, "divLeaf tmp after re*im");
    sub_generic(result.pImpl->im, result.pImpl->im, tmp);
    SNAP_CHECK(result.pImpl->im, "divLeaf im after sub");
    div_generic(result.pImpl->im, result.pImpl->im, denom);
    SNAP_CHECK(result.pImpl->im, "divLeaf im after div");
    snap_pimpl(result.pImpl);
    clear_generic(denom);
    clear_generic(tmp);
    return result;
}

LEAF_TYPE divLeafS(LEAF_TYPE a, LEAF_TYPE b) {
    if (a.pImpl == NULL || b.pImpl == NULL)
        return (LEAF_TYPE){ .pImpl = NULL };

    leaf_primitive_t denom, tmp;
    init_generic(denom);
    init_generic(tmp);

    // denom = re_b² + im_b²
    mul_generic(denom, b.pImpl->re, b.pImpl->re);
    mul_generic(tmp,   b.pImpl->im, b.pImpl->im);
    add_generic(denom, denom, tmp);

    if (!sgn_generic(denom)) {
        clear_generic(denom);
        clear_generic(tmp);
        return (LEAF_TYPE){ .pImpl = NULL };
    }

    LEAF_TYPE result;
    allocPimpl(&result);

    // re = (re_a*re_b + im_a*im_b) / denom
    mul_generic(result.pImpl->re, a.pImpl->re, b.pImpl->re);
    mul_generic(tmp,              a.pImpl->im, b.pImpl->im);
    add_generic(result.pImpl->re, result.pImpl->re, tmp);
    div_generic(result.pImpl->re, result.pImpl->re, denom);

    // im = (im_a*re_b - re_a*im_b) / denom
    mul_generic(result.pImpl->im, a.pImpl->im, b.pImpl->re);
    mul_generic(tmp,              a.pImpl->re, b.pImpl->im);
    sub_generic(result.pImpl->im, result.pImpl->im, tmp);
    div_generic(result.pImpl->im, result.pImpl->im, denom);

    clear_generic(denom);
    clear_generic(tmp);

    // scale by 1/sqrt(2) after division - consistent with all other S variants
    mul_sqrt2inv_generic(result.pImpl->re, result.pImpl->re);
    mul_sqrt2inv_generic(result.pImpl->im, result.pImpl->im);
    snap_pimpl(result.pImpl);
    return result;
}


LEAF_TYPE sqrtLeaf(LEAF_TYPE a) {
    // TODO
    return a;
}


LEAF_TYPE rotateCoef1(LEAF_TYPE l) {
    if (l.pImpl == NULL) return l;

    LEAF_TYPE result;
    allocPimpl(&result);

    // re' = re - im,  im' = re + im
    // tmp needed: re and im both read before either is written
    leaf_primitive_t tmp;
    init_generic(tmp);
    set_generic(tmp, l.pImpl->re);                            // tmp = re

    sub_generic(result.pImpl->re, l.pImpl->re, l.pImpl->im); // re' = re - im
    add_generic(result.pImpl->im, tmp,          l.pImpl->im); // im' = re + im

    clear_generic(tmp);
    snap_pimpl(result.pImpl);
    return result;
}

LEAF_TYPE rotateCoef1S(LEAF_TYPE l) {
    if (l.pImpl == NULL) return l;

    LEAF_TYPE result;
    allocPimpl(&result);

    leaf_primitive_t tmp;
    init_generic(tmp);
    set_generic(tmp, l.pImpl->re);

    sub_generic(result.pImpl->re, l.pImpl->re, l.pImpl->im);
    SNAP_CHECK(result.pImpl->re, "rotateCoef1S re after sub");
    add_generic(result.pImpl->im, tmp,          l.pImpl->im);
    SNAP_CHECK(result.pImpl->im, "rotateCoef1S im after add");


    clear_generic(tmp);

    // re' = (re - im) / sqrt(2),  im' = (re + im) / sqrt(2)
    mul_sqrt2inv_generic(result.pImpl->re, result.pImpl->re);
    SNAP_CHECK(result.pImpl->re, "rotateCoef1S re after mul_sqrt2inv");
    mul_sqrt2inv_generic(result.pImpl->im, result.pImpl->im);
    SNAP_CHECK(result.pImpl->im, "rotateCoef1S im after mul_sqrt2inv");
    snap_pimpl(result.pImpl);
    return result;
}

LEAF_TYPE rotateCoef1S_inv(LEAF_TYPE l) {
    if (l.pImpl == NULL) return l;

    LEAF_TYPE result;
    allocPimpl(&result);

    leaf_primitive_t tmp;
    init_generic(tmp);
    set_generic(tmp, l.pImpl->re);

    add_generic(result.pImpl->re, l.pImpl->re, l.pImpl->im);
    SNAP_CHECK(result.pImpl->re, "rotateCoef1S_inv re after add");
    sub_generic(result.pImpl->im, l.pImpl->im, tmp);          // im - re
    SNAP_CHECK(result.pImpl->im, "rotateCoef1S_inv im after sub");

    clear_generic(tmp);

    mul_sqrt2inv_generic(result.pImpl->re, result.pImpl->re);
    SNAP_CHECK(result.pImpl->re, "rotateCoef1S_inv re after mul_sqrt2inv");
    mul_sqrt2inv_generic(result.pImpl->im, result.pImpl->im);
    SNAP_CHECK(result.pImpl->im, "rotateCoef1S_inv im after mul_sqrt2inv");
    snap_pimpl(result.pImpl);
    return result;
}


LEAF_TYPE rotateCoef2(LEAF_TYPE l) {
    if (l.pImpl == NULL) return l;

    LEAF_TYPE result;
    allocPimpl(&result);

    // (re + im·i) * i = -im + re·i
    neg_generic(result.pImpl->re, l.pImpl->im); // re' = -im
    SNAP_CHECK(result.pImpl->re, "rotateCoef2 re after neg");
    set_generic(result.pImpl->im, l.pImpl->re); // im' =  re
        SNAP_CHECK(result.pImpl->im, "rotateCoef2 im after set");
    snap_pimpl(result.pImpl);
    return result;
}

LEAF_TYPE negI_mul(LEAF_TYPE l) {
    if (l.pImpl == NULL) return l;
    
    LEAF_TYPE result;
    allocPimpl(&result);
    
    set_generic(result.pImpl->re, l.pImpl->im);        // re' =  im
    neg_generic(result.pImpl->im, l.pImpl->re);        // im' = -re
    snap_pimpl(result.pImpl);
    return result;
}

LEAF_TYPE mulPhaseLeaf(LEAF_TYPE t, size_t arg) {
    if (t.pImpl == NULL) return t;

    LEAF_TYPE_IMPL *ldata = t.pImpl;
    const PhaseParam *p = (const PhaseParam *)arg;
    double c = p->c;
    leaf_primitive_t primitive_c = {(leaf_scalar_t)c};
    double s = p->s;
    leaf_primitive_t primitive_s = {(leaf_scalar_t)s};

    LEAF_TYPE result;
    allocPimpl(&result);

    leaf_primitive_t rc, is_, rs, ic;
    init_generic(rc);  init_generic(is_);
    init_generic(rs);  init_generic(ic);

    mul_generic(rc,  ldata->re, primitive_c);
    mul_generic(is_, ldata->im, primitive_s);
    mul_generic(rs,  ldata->re, primitive_s);
    mul_generic(ic,  ldata->im, primitive_c);

    sub_generic(result.pImpl->re, rc, is_);
    add_generic(result.pImpl->im, rs, ic);

    clear_generic(rc);  clear_generic(is_);
    clear_generic(rs);  clear_generic(ic);
    snap_pimpl(result.pImpl);
    return result;
}

LEAF_TYPE rotateCoef2S(LEAF_TYPE l) {
    if (l.pImpl == NULL) return l;

    LEAF_TYPE result;
    allocPimpl(&result);

    neg_generic(result.pImpl->re, l.pImpl->im);
    set_generic(result.pImpl->im, l.pImpl->re);

    // (re + im·i) * i / sqrt(2)
    mul_sqrt2inv_generic(result.pImpl->re, result.pImpl->re);
    SNAP_CHECK(result.pImpl->re, "rotateCoef2S re after mul_sqrt2inv");
    mul_sqrt2inv_generic(result.pImpl->im, result.pImpl->im);
    SNAP_CHECK(result.pImpl->im, "rotateCoef2S im after mul_sqrt2inv");
    snap_pimpl(result.pImpl);
    return result;
}

LEAF_TYPE ry_low_leaf(LEAF_TYPE low, LEAF_TYPE high, size_t param) {
    if (low.pImpl == NULL && high.pImpl == NULL) return (LEAF_TYPE){ .pImpl = NULL };
    double theta;
    memcpy(&theta, &param, sizeof(double));
    leaf_scalar_t c = (leaf_scalar_t)cos(theta / 2.0);
    leaf_scalar_t s = (leaf_scalar_t)sin(theta / 2.0);
    leaf_primitive_t cp = {c};
    leaf_primitive_t sp = {s};
    LEAF_TYPE result;
    allocPimpl(&result);
    leaf_primitive_t tmp1, tmp2;
    // re' = c*re(low) - s*re(high)
    if (low.pImpl != NULL) mul_generic(tmp1, low.pImpl->re, cp); else tmp1[0] = LEAF_ZERO;
    if (high.pImpl != NULL) mul_generic(tmp2, high.pImpl->re, sp); else tmp2[0] = LEAF_ZERO;
    sub_generic(result.pImpl->re, tmp1, tmp2);
    // im' = c*im(low) - s*im(high)
    if (low.pImpl != NULL) mul_generic(tmp1, low.pImpl->im, cp); else tmp1[0] = LEAF_ZERO;
    if (high.pImpl != NULL) mul_generic(tmp2, high.pImpl->im, sp); else tmp2[0] = LEAF_ZERO;
    sub_generic(result.pImpl->im, tmp1, tmp2);
    snap_pimpl(result.pImpl);
    if (!sgn_generic(result.pImpl->re) && !sgn_generic(result.pImpl->im)) {
        free(result.pImpl);
        return (LEAF_TYPE){ .pImpl = NULL };
    }
    return result;
}

LEAF_TYPE ry_high_leaf(LEAF_TYPE low, LEAF_TYPE high, size_t param) {
    if (low.pImpl == NULL && high.pImpl == NULL) return (LEAF_TYPE){ .pImpl = NULL };
    double theta;
    memcpy(&theta, &param, sizeof(double));
    leaf_scalar_t c = (leaf_scalar_t)cos(theta / 2.0);
    leaf_scalar_t s = (leaf_scalar_t)sin(theta / 2.0);
    leaf_primitive_t cp = {c};
    leaf_primitive_t sp = {s};
    LEAF_TYPE result;
    allocPimpl(&result);
    leaf_primitive_t tmp1, tmp2;
    // re' = s*re(low) + c*re(high)
    if (low.pImpl != NULL) mul_generic(tmp1, low.pImpl->re, sp); else tmp1[0] = LEAF_ZERO;
    if (high.pImpl != NULL) mul_generic(tmp2, high.pImpl->re, cp); else tmp2[0] = LEAF_ZERO;
    add_generic(result.pImpl->re, tmp1, tmp2);
    // im' = s*im(low) + c*im(high)
    if (low.pImpl != NULL) mul_generic(tmp1, low.pImpl->im, sp); else tmp1[0] = LEAF_ZERO;
    if (high.pImpl != NULL) mul_generic(tmp2, high.pImpl->im, cp); else tmp2[0] = LEAF_ZERO;
    add_generic(result.pImpl->im, tmp1, tmp2);
    snap_pimpl(result.pImpl);
    if (!sgn_generic(result.pImpl->re) && !sgn_generic(result.pImpl->im)) {
        free(result.pImpl);
        return (LEAF_TYPE){ .pImpl = NULL };
    }
    return result;
}

LEAF_TYPE rx_low_leaf(LEAF_TYPE low, LEAF_TYPE high, size_t param) {
    if (low.pImpl == NULL && high.pImpl == NULL) return (LEAF_TYPE){ .pImpl = NULL };
    double theta;
    memcpy(&theta, &param, sizeof(double));
    leaf_scalar_t c = (leaf_scalar_t)cos(theta / 2.0);
    leaf_scalar_t s = (leaf_scalar_t)sin(theta / 2.0);
    leaf_primitive_t cp = {c};
    leaf_primitive_t sp = {s};
    LEAF_TYPE result;
    allocPimpl(&result);
    leaf_primitive_t tmp1, tmp2;
    // re' = c*re(low) + s*im(high)
    if (low.pImpl != NULL) mul_generic(tmp1, low.pImpl->re, cp); else tmp1[0] = LEAF_ZERO;
    if (high.pImpl != NULL) mul_generic(tmp2, high.pImpl->im, sp); else tmp2[0] = LEAF_ZERO;
    add_generic(result.pImpl->re, tmp1, tmp2);
    // im' = c*im(low) - s*re(high)
    if (low.pImpl != NULL) mul_generic(tmp1, low.pImpl->im, cp); else tmp1[0] = LEAF_ZERO;
    if (high.pImpl != NULL) mul_generic(tmp2, high.pImpl->re, sp); else tmp2[0] = LEAF_ZERO;
    sub_generic(result.pImpl->im, tmp1, tmp2);
    snap_pimpl(result.pImpl);
    if (!sgn_generic(result.pImpl->re) && !sgn_generic(result.pImpl->im)) {
        free(result.pImpl);
        return (LEAF_TYPE){ .pImpl = NULL };
    }
    return result;
}

LEAF_TYPE rx_high_leaf(LEAF_TYPE low, LEAF_TYPE high, size_t param) {
    if (low.pImpl == NULL && high.pImpl == NULL) return (LEAF_TYPE){ .pImpl = NULL };
    double theta;
    memcpy(&theta, &param, sizeof(double));
    leaf_scalar_t c = (leaf_scalar_t)cos(theta / 2.0);
    leaf_scalar_t s = (leaf_scalar_t)sin(theta / 2.0);
    leaf_primitive_t cp = {c};
    leaf_primitive_t sp = {s};
    LEAF_TYPE result;
    allocPimpl(&result);
    leaf_primitive_t tmp1, tmp2;
    // re' = c*re(high) + s*im(low)
    if (high.pImpl != NULL) mul_generic(tmp1, high.pImpl->re, cp); else tmp1[0] = LEAF_ZERO;
    if (low.pImpl != NULL) mul_generic(tmp2, low.pImpl->im, sp); else tmp2[0] = LEAF_ZERO;
    add_generic(result.pImpl->re, tmp1, tmp2);
    // im' = c*im(high) - s*re(low)
    if (high.pImpl != NULL) mul_generic(tmp1, high.pImpl->im, cp); else tmp1[0] = LEAF_ZERO;
    if (low.pImpl != NULL) mul_generic(tmp2, low.pImpl->re, sp); else tmp2[0] = LEAF_ZERO;
    sub_generic(result.pImpl->im, tmp1, tmp2);
    snap_pimpl(result.pImpl);
    if (!sgn_generic(result.pImpl->re) && !sgn_generic(result.pImpl->im)) {
        free(result.pImpl);
        return (LEAF_TYPE){ .pImpl = NULL };
    }
    return result;
}

LEAF_TYPE rz_low_leaf(LEAF_TYPE l, size_t param) {
    if (l.pImpl == NULL) return (LEAF_TYPE){ .pImpl = NULL };
    double theta;
    memcpy(&theta, &param, sizeof(double));
    leaf_scalar_t c = (leaf_scalar_t)cos(theta / 2.0);
    leaf_scalar_t s = (leaf_scalar_t)sin(theta / 2.0);
    leaf_primitive_t cp = {c};
    leaf_primitive_t sp = {s};
    LEAF_TYPE result;
    allocPimpl(&result);
    leaf_primitive_t tmp1, tmp2;
    mul_generic(tmp1, l.pImpl->re, cp);
    mul_generic(tmp2, l.pImpl->im, sp);
    add_generic(result.pImpl->re, tmp1, tmp2);
    mul_generic(tmp1, l.pImpl->im, cp);
    mul_generic(tmp2, l.pImpl->re, sp);
    sub_generic(result.pImpl->im, tmp1, tmp2);

    snap_pimpl(result.pImpl);
    if (!sgn_generic(result.pImpl->re) && !sgn_generic(result.pImpl->im)) {
        free(result.pImpl);
        return (LEAF_TYPE){ .pImpl = NULL };
    }
    return result;
}

LEAF_TYPE rz_high_leaf(LEAF_TYPE l, size_t param) {
    if (l.pImpl == NULL) return (LEAF_TYPE){ .pImpl = NULL };
    double theta;
    memcpy(&theta, &param, sizeof(double));
    leaf_scalar_t c = (leaf_scalar_t)cos(theta / 2.0);
    leaf_scalar_t s = (leaf_scalar_t)sin(theta / 2.0);
    leaf_primitive_t cp = {c};
    leaf_primitive_t sp = {s};
    LEAF_TYPE result;
    allocPimpl(&result);
    leaf_primitive_t tmp1, tmp2;
    mul_generic(tmp1, l.pImpl->re, cp);
    mul_generic(tmp2, l.pImpl->im, sp);
    sub_generic(result.pImpl->re, tmp1, tmp2);
    mul_generic(tmp1, l.pImpl->im, cp);
    mul_generic(tmp2, l.pImpl->re, sp);
    add_generic(result.pImpl->im, tmp1, tmp2);
    snap_pimpl(result.pImpl);
    if (!sgn_generic(result.pImpl->re) && !sgn_generic(result.pImpl->im)) {
        free(result.pImpl);
        return (LEAF_TYPE){ .pImpl = NULL };
    }
    return result;
}

LEAF_TYPE times2Leaf(LEAF_TYPE l) {
    if (l.pImpl == NULL) return l;

    LEAF_TYPE result;
    allocPimpl(&result);
    mul_ui_generic(result.pImpl->re, l.pImpl->re, 2);
    SNAP_CHECK(result.pImpl->re, "times2Leaf re");
    mul_ui_generic(result.pImpl->im, l.pImpl->im, 2);
    SNAP_CHECK(result.pImpl->im, "times2Leaf im");
    snap_pimpl(result.pImpl);
    return result;
}

LEAF_TYPE times2LeafS(LEAF_TYPE l) {
    if (l.pImpl == NULL) return l;

    LEAF_TYPE result;
    allocPimpl(&result);

    // 2 * (1/sqrt(2)) = sqrt(2)
    mul_ui_generic(result.pImpl->re, l.pImpl->re, 2);
        SNAP_CHECK(result.pImpl->re, "times2LeafS re after mul_ui");
    mul_ui_generic(result.pImpl->im, l.pImpl->im, 2);
    SNAP_CHECK(result.pImpl->im, "times2LeafS im after mul_ui");
    mul_sqrt2inv_generic(result.pImpl->re, result.pImpl->re);
        SNAP_CHECK(result.pImpl->re, "times2LeafS re after mul_sqrt2inv");
    mul_sqrt2inv_generic(result.pImpl->im, result.pImpl->im);
    SNAP_CHECK(result.pImpl->im, "times2LeafS im after mul_sqrt2inv");
    snap_pimpl(result.pImpl);
    return result;
}
/* 
 * Algebraic operations symbolic (sl_val_t / symexp)
 */

LEAF_TYPE mtbdd_symb_neg_i(LEAF_TYPE t) {
    if (t.pImpl == NULL) return t;
    sl_val_t *ldata = (sl_val_t*) t.pImpl;
    sl_val_t *res_data = (sl_val_t*)malloc(sizeof(sl_val_t));
    if (!res_data) {
        printf("ERROR ALLOCATION\n");
        exit(EXIT_FAILURE);
    }
    res_data->re = symexp_op(NULL, ldata->re, SYMEXP_SUB);
    res_data->im = symexp_op(NULL, ldata->im, SYMEXP_SUB);
    return (LEAF_TYPE){ .pImpl = (LEAF_TYPE_IMPL*)res_data };
}

// LEAF_TYPE mtbdd_symb_neg_s_i(LEAF_TYPE t) {
//     if (t.pImpl == NULL) return t;
//     sl_val_t *ldata = (sl_val_t*) t.pImpl;
//     sl_val_t *res_data = (sl_val_t*)malloc(sizeof(sl_val_t));
//     res_data->re = symexp_op(NULL, ldata->re, SYMEXP_SUB_MUL_BY_SQRT2_INV);
//     res_data->im = symexp_op(NULL, ldata->im, SYMEXP_SUB_MUL_BY_SQRT2_INV);
//     return (LEAF_TYPE){ .pImpl = (LEAF_TYPE_IMPL*)res_data };
// }

LEAF_TYPE mtbdd_symb_coef_rot1_i(LEAF_TYPE t) {
    if (t.pImpl == NULL) return t;
    sl_val_t *ldata = (sl_val_t*) t.pImpl;
    sl_val_t *res_data = (sl_val_t*)malloc(sizeof(sl_val_t));
    if (!res_data) {
        printf("ERROR ALLOCATION\n");
        exit(EXIT_FAILURE);
    }

    // multiply by ω = e^(iπ/4) = (1+i)/√2, 1/sqrt(2) per gate later
    // re' = re - im,  im' = re + im
    res_data->re = symexp_op(ldata->re, ldata->im, SYMEXP_SUB);
    res_data->im = symexp_op(ldata->re, ldata->im, SYMEXP_ADD);
    return (LEAF_TYPE){ .pImpl = (LEAF_TYPE_IMPL*)res_data };
}

LEAF_TYPE mtbdd_symb_coef_rot1_i_inv(LEAF_TYPE t) {
    if (t.pImpl == NULL) return t;
    sl_val_t *ldata = (sl_val_t*) t.pImpl;
    sl_val_t *res_data = (sl_val_t*)malloc(sizeof(sl_val_t));
    if (!res_data) {
        printf("ERROR ALLOCATION\n");
        exit(EXIT_FAILURE);
    }

    // multiply by ω* = e^(-iπ/4) = (1-i)/√2, 1/sqrt(2) tracked globally
    // re' = re + im,  im' = im - re
    res_data->re = symexp_op(ldata->re, ldata->im, SYMEXP_ADD);
    res_data->im = symexp_op(ldata->im, ldata->re, SYMEXP_SUB);  // note: im - re
    return (LEAF_TYPE){ .pImpl = (LEAF_TYPE_IMPL*)res_data };
}

LEAF_TYPE mtbdd_symb_coef_rot2_i(LEAF_TYPE t) {
    if (t.pImpl == NULL) return t;
    sl_val_t *ldata = (sl_val_t*) t.pImpl;
    sl_val_t *res_data = (sl_val_t*)malloc(sizeof(sl_val_t));
    if (!res_data) {
        printf("ERROR ALLOCATION\n");
        exit(EXIT_FAILURE);
    }

    // multiply by ω^2 = i
    // re' = -im,  im' = re
    res_data->re = symexp_op(NULL, ldata->im, SYMEXP_SUB);
    res_data->im = ldata->re;
    return (LEAF_TYPE){ .pImpl = (LEAF_TYPE_IMPL*)res_data };
}

// LEAF_TYPE mtbdd_symb_multiply_1_sqrt(LEAF_TYPE t) {
//     if (t.pImpl == NULL) return t;
//     sl_val_t *ldata = (sl_val_t*) t.pImpl;
//     sl_val_t *res_data = (sl_val_t*)malloc(sizeof(sl_val_t));
//     res_data->re = symexp_op(ldata->re, NULL, SYMEXP_ADD_MUL_BY_SQRT2_INV);
//     res_data->im = symexp_op(ldata->im, NULL, SYMEXP_ADD_MUL_BY_SQRT2_INV);

//     if (!res_data->re && !res_data->im) {
//         free(res_data);
//         return (LEAF_TYPE){ .pImpl = NULL };
//     }

//     return (LEAF_TYPE){ .pImpl = (LEAF_TYPE_IMPL*)res_data };
// }

LEAF_TYPE mtbdd_symb_plus_i(LEAF_TYPE a, LEAF_TYPE b) {
    if (a.pImpl == NULL) return b;
    if (b.pImpl == NULL) return a;
    sl_val_t *a_data = (sl_val_t*) a.pImpl;
    sl_val_t *b_data = (sl_val_t*) b.pImpl;
    sl_val_t *res_data = (sl_val_t*)malloc(sizeof(sl_val_t));
    if (!res_data) {
        printf("ERROR ALLOCATION\n");
        exit(EXIT_FAILURE);
    }
    res_data->re = symexp_op(a_data->re, b_data->re, SYMEXP_ADD);
    res_data->im = symexp_op(a_data->im, b_data->im, SYMEXP_ADD);

    // if (!res_data->re && !res_data->im) {
    //     free(res_data);
    //     return (LEAF_TYPE){ .pImpl = NULL };
    // }
    return (LEAF_TYPE){ .pImpl = (LEAF_TYPE_IMPL*)res_data };
}

// LEAF_TYPE mtbdd_symb_plus_s_i(LEAF_TYPE a, LEAF_TYPE b) {
//     if (a.pImpl == NULL) return mtbdd_symb_multiply_1_sqrt(b);
//     if (b.pImpl == NULL) return mtbdd_symb_multiply_1_sqrt(a);
//     sl_val_t *a_data = (sl_val_t*) a.pImpl;
//     sl_val_t *b_data = (sl_val_t*) b.pImpl;
//     sl_val_t *res_data = (sl_val_t*)malloc(sizeof(sl_val_t));
//     res_data->re = symexp_op(a_data->re, b_data->re, SYMEXP_ADD_MUL_BY_SQRT2_INV);
//     res_data->im = symexp_op(a_data->im, b_data->im, SYMEXP_ADD_MUL_BY_SQRT2_INV);

//     if (!res_data->re && !res_data->im) {
//         free(res_data);
//         return (LEAF_TYPE){ .pImpl = NULL };
//     }
//     return (LEAF_TYPE){ .pImpl = (LEAF_TYPE_IMPL*)res_data };
// }

LEAF_TYPE mtbdd_symb_minus_i(LEAF_TYPE a, LEAF_TYPE b) {
    if (a.pImpl == NULL) return mtbdd_symb_neg_i(b);
    if (b.pImpl == NULL) return a;
    sl_val_t *a_data = (sl_val_t*) a.pImpl;
    sl_val_t *b_data = (sl_val_t*) b.pImpl;
    sl_val_t *res_data = (sl_val_t*)malloc(sizeof(sl_val_t));
    if (!res_data) {
        printf("ERROR ALLOCATION\n");
        exit(EXIT_FAILURE);
    }
    res_data->re = symexp_op(a_data->re, b_data->re, SYMEXP_SUB);
    res_data->im = symexp_op(a_data->im, b_data->im, SYMEXP_SUB);

    if (!res_data->re && !res_data->im) {
        free(res_data);
        return (LEAF_TYPE){ .pImpl = NULL };
    }
    return (LEAF_TYPE){ .pImpl = (LEAF_TYPE_IMPL*)res_data };
}

// LEAF_TYPE mtbdd_symb_minus_s_i(LEAF_TYPE a, LEAF_TYPE b) {
//     if (a.pImpl == NULL) return mtbdd_symb_neg_s_i(b);
//     if (b.pImpl == NULL) return mtbdd_symb_multiply_1_sqrt(a); 
//     sl_val_t *a_data = (sl_val_t*) a.pImpl;
//     sl_val_t *b_data = (sl_val_t*) b.pImpl;
//     sl_val_t *res_data = (sl_val_t*)malloc(sizeof(sl_val_t));
//     res_data->re = symexp_op(a_data->re, b_data->re, SYMEXP_SUB_MUL_BY_SQRT2_INV);
//     res_data->im = symexp_op(a_data->im, b_data->im, SYMEXP_SUB_MUL_BY_SQRT2_INV);

//     if (!res_data->re && !res_data->im) {
//         free(res_data);
//         return (LEAF_TYPE){ .pImpl = NULL };
//     }
//     return (LEAF_TYPE){ .pImpl = (LEAF_TYPE_IMPL*)res_data };
// }

qBDD mtbdd_to_symb_map_i(qBDD a, size_t raw_m) {
    if (!qBDD_isFalse(a) && !qBDD_isTerminal(a)) {
        invalidateApplyResult();
        return qBDD_false();
    }

    vmap_t* m = (vmap_t*) raw_m;

    qBDD found = vmap_lookup(m, a);
    if (!qBDD_isFalse(found)) {
        validateApplyResult();
        return found;
    }


    vars_t var_re = m->next_var;
    vars_t var_im = m->next_var + 1;

    // Partial function check
    if (qBDD_isFalse(a)) {
        // initial vmap size does not count with 'mtbdd_false' leaves, so vmap needs to be resized
        m->map = my_realloc(m->map, sizeof(coef_t) * (m->msize + 2));
        m->msize += 2;
        init_generic(m->map[var_re]);
        init_generic(m->map[var_im]);
    }
    else if (qBDD_isTerminal(a)) {
        LEAF_TYPE leafData = qBDD_getTerminalValue(a);
        LEAF_TYPE_IMPL *orig_data = (LEAF_TYPE_IMPL*) leafData.pImpl;
        init_set_generic(m->map[var_re], orig_data->re);
        init_set_generic(m->map[var_im], orig_data->im);
    }

    sl_map_t *new_data = (sl_map_t*) malloc(sizeof(sl_map_t));
    if (new_data == NULL) {
        printf("ERROR ALLOCATION\n");
        exit(EXIT_FAILURE);
    }
    new_data->vre = var_re;
    new_data->vim = var_im;

    LEAF_TYPE *newLeaf = (LEAF_TYPE*)malloc(sizeof(LEAF_TYPE));
    newLeaf->pImpl = new_data;

    qBDD res = qBDD_maketerminal(qBDD_symbolicMapLType(), (void *) newLeaf);
    m->next_var += 2;
    validateApplyResult();
    vmap_insert(m, a, res);
    return res;
}

qBDD mtbdd_from_symb_i(qBDD t, size_t raw_map) {
    // Partial function check
    if (qBDD_isFalse(t)) {
        validateApplyResult();
        return t;
    }

    if (qBDD_isTerminal(t)) {
        coef_t* map = (coef_t*) raw_map;
        LEAF_TYPE leaf = qBDD_getTerminalValue(t);
        sl_map_t *data = (sl_map_t*) leaf.pImpl;
        
        LEAF_TYPE_IMPL *new_data = (LEAF_TYPE_IMPL*) malloc(sizeof(LEAF_TYPE_IMPL)); 
        init_set_generic(new_data->re, map[data->vre]);
        init_set_generic(new_data->im, map[data->vim]);
        
        snap_pimpl(new_data);

        if (!sgn_generic(new_data->re) && !sgn_generic(new_data->im)) {
            clear_generic(new_data->re);
            clear_generic(new_data->im);
            free(new_data);
            validateApplyResult();
            return qBDD_false();
        }
        
        LEAF_TYPE *newLeaf = (LEAF_TYPE*)malloc(sizeof(LEAF_TYPE));
        newLeaf->pImpl = new_data;

        qBDD res = qBDD_maketerminal(qBDD_classicLType(), (void*) newLeaf);
        validateApplyResult();
        return res;
    }
    invalidateApplyResult();
    return qBDD_false(); // Recurse deeper
}

qBDD t_xt_comp_create_i(qBDD t, size_t xt) {
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

qBDD t_xt_create_i(qBDD t, size_t xt) {
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

qBDD mtbdd_symb_refine_i(qBDD map, qBDD val, size_t rd_raw) {

    rdata_t *rd = (rdata_t*) rd_raw;
    if (qBDD_isTerminal(map) && qBDD_isTerminal(val)) {
        LEAF_TYPE mapVal = qBDD_getTerminalValue(map); // never qBDD_false, as all leaves were changed
        sl_map_t *mdata = (sl_map_t*) mapVal.pImpl; 
        vars_t new_re, new_im;

        if (qBDD_isFalse(val)) {
            new_re = refine_var_check(mdata->vre, SYMEXP_NULL, rd);
            new_im = refine_var_check(mdata->vim, SYMEXP_NULL, rd);
        }
        else {
            LEAF_TYPE valVal = qBDD_getTerminalValue(val);
            sl_val_t *vdata = (sl_val_t*) valVal.pImpl;
            new_re = refine_var_check(mdata->vre, vdata->re, rd);
            new_im = refine_var_check(mdata->vim, vdata->im, rd);
        }

        if (new_re == mdata->vre && new_im == mdata->vim) {
            validateApplyResult();
            return map;
        }

        // new symbolic var needed
        sl_map_t *new_data = (sl_map_t*)malloc(sizeof(sl_map_t));
        new_data->vre = new_re;
        new_data->vim = new_im;

        LEAF_TYPE *newLeaf = (LEAF_TYPE*)malloc(sizeof(LEAF_TYPE));
        newLeaf->pImpl = new_data;

        qBDD res = qBDD_maketerminal(qBDD_symbolicMapLType(), (void*)newLeaf);
        validateApplyResult();
        return res;
    }
    invalidateApplyResult();
    return qBDD_false(); // Recurse deeper
}

LEAF_TYPE mtbdd_symb_times_c_i(LEAF_TYPE t, size_t c_raw) {
    // Partial function check
    if (t.pImpl == NULL) return t;

    // Compute c*t if mtbdd is a leaf
    sl_val_t *ldata = (sl_val_t*) t.pImpl;
    unsigned long c = (unsigned long)c_raw;

    sl_val_t *res_data = (sl_val_t *)malloc(sizeof(sl_val_t));
    if (res_data == NULL) {
        printf("ERROR ALLOCATION\n");
        exit(1);
    }
    res_data->re = symexp_mul_c(ldata->re, c);
    res_data->im = symexp_mul_c(ldata->im, c);

    LEAF_TYPE res = {.pImpl = (LEAF_TYPE_IMPL*)res_data};
    return res;
}

qBDD mtbdd_map_to_symb_val_reduced_i(qBDD t, size_t raw_map) {
    // Partial function check not needed, 2 variables were assigned to every base vector

    if (qBDD_isTerminal(t)) {

        leaf_primitive_t* map = (leaf_primitive_t*) raw_map;
        LEAF_TYPE leaf = qBDD_getTerminalValue(t);
        sl_map_t *t_data = (sl_map_t*) leaf.pImpl;
        

        if (!sgn_generic(map[t_data->vre]) && !sgn_generic(map[t_data->vim])) {
            validateApplyResult();
            return qBDD_false();
        }
        sl_val_t *new_data = (sl_val_t*)malloc(sizeof(sl_val_t)); // 906
        if (new_data == NULL) {
            printf("ERROR ALLOCATION\n");
            exit(EXIT_FAILURE);
        }
        new_data->re = symexp_init(t_data->vre);
        new_data->im = symexp_init(t_data->vim);
        
        LEAF_TYPE *newLeaf = (LEAF_TYPE*)malloc(sizeof(LEAF_TYPE));
        newLeaf->pImpl = new_data;

        qBDD res = qBDD_maketerminal(qBDD_symbolicValLType(), (void *) newLeaf);
        validateApplyResult();
        return res;
    }
    invalidateApplyResult();
    return qBDD_false(); // Recurse deeper
}

qBDD mtbdd_map_to_symb_val_i(qBDD t, size_t raw_map) {
    // Partial function check not needed, 4 variables were assigned to every base vector

    if (qBDD_isTerminal(t)) {
        leaf_primitive_t* map = (leaf_primitive_t*) raw_map;
        LEAF_TYPE leaf = qBDD_getTerminalValue(t);
        sl_map_t *t_data = (sl_map_t*) leaf.pImpl;
        sl_val_t *new_data = (sl_val_t*)malloc(sizeof(sl_val_t));
        if (new_data == NULL) {
            printf("ERROR ALLOCATION\n");
            exit(EXIT_FAILURE);
        }

        new_data->re = symexp_init(t_data->vre);
        new_data->im = symexp_init(t_data->vim);

        LEAF_TYPE *newLeaf = (LEAF_TYPE*)malloc(sizeof(LEAF_TYPE));
        newLeaf->pImpl = new_data;

        qBDD res = qBDD_maketerminal(qBDD_symbolicValLType(), (void*) newLeaf);
        validateApplyResult();
        return res;
    }
    invalidateApplyResult();
    return qBDD_false(); // Recurse deeper
}



/*
 * Terminal type registration
 */

void terminal_symb_map_free(void *val) {
    return; /* TODO */
}

int terminal_symb_map_compare_generic(void* l_a, void* l_b) {
    if (l_a == NULL && l_b == NULL) return 1;
    if ((l_a == NULL) != (l_b == NULL)) return 0;
    sl_map_t *ldata_a = (sl_map_t*) ((LEAF_TYPE*)l_a)->pImpl;
    sl_map_t *ldata_b = (sl_map_t*) ((LEAF_TYPE*)l_b)->pImpl;
    return (ldata_a->vre == ldata_b->vre) && (ldata_a->vim == ldata_b->vim);
}

unsigned terminal_symb_map_hash_generic(void* l_a) {
    sl_map_t *ldata = (sl_map_t*) ((LEAF_TYPE*)l_a)->pImpl;
    uint64_t val = 1;
    val = MY_HASH_COMB(val, ldata->vre);
    val = MY_HASH_COMB(val, ldata->vim);
    return (unsigned)val;
}


int terminal_symb_val_compare_generic(void* l_a, void* l_b) {
    if (l_a == NULL && l_b == NULL) return 1;
    if ((l_a == NULL) != (l_b == NULL)) return 0;
    sl_val_t *ldata_a = (sl_val_t*) ((LEAF_TYPE*)l_a)->pImpl;
    sl_val_t *ldata_b = (sl_val_t*) ((LEAF_TYPE*)l_b)->pImpl;
    return symexp_cmp(ldata_a->re, ldata_b->re) && symexp_cmp(ldata_a->im, ldata_b->im);
}

unsigned terminal_symb_val_hash_generic(void* l_a) {
    sl_val_t *ldata = (sl_val_t*) ((LEAF_TYPE*)l_a)->pImpl;
    uint64_t val = 1;
    val = MY_HASH_COMB(val, ldata->re);
    val = MY_HASH_COMB(val, ldata->im);
    return val;
}


bool can_be_reduced(mtbdd_symb_t *symbc, rdata_t *rdata)
{
    bool is_correct = true;
    bool is_zero[rdata->vm->next_var];
    for (int i = 0; i < rdata->vm->next_var; i++) {
        is_zero[i] = false;
    }

    // The whole leaf behaves the same way, so checking every 2nd variable is sufficient
    for (int i = 0; i < rdata->vm->next_var; i += 2) {
        // If leaf is initially 0:
        if (!sgn_generic(rdata->vm->map[i]) && !sgn_generic(rdata->vm->map[i+1])){
            is_zero[i] = true;
            is_zero[i+1] = true;

            // Check if the right side of update equation for these variables is 0
            // (eg. change of value caused by H)
            if (rdata->upd->arr[i] != SYMEXP_NULL && rdata->upd->arr[i+1] != SYMEXP_NULL) {
                is_correct = false;
                break;
            }
        }
    }

    // Check if swap with 0 leaf occurs 
    // (i.e., if these variables appear alone on some right side of update equation)
    for(int i = 0; i < rdata->vm->next_var; i +=2) {
        // Check for permutations as well, first variable of the leaf is sufficient
        // (we always swap the whole leaf)
        if (rdata->upd->arr[i] != SYMEXP_NULL) {
          //  printf("Checking var %u, is_zero: %d\n", i, is_zero[i]);
            if (symexp_is_first_var_marked(rdata->upd->arr[i], is_zero)) {
                is_correct = false;
                break;
            }
        }
    }

    if (!is_correct) {
        symbc->is_reduced = false;
    }
    return is_correct;
}



/*
 * Package bootstrap
 */

void circuit_init_interface(qBDD *c, const uint32_t n) {
    unsigned varNum = n;
    if (bdd_varnum() < varNum)
        bdd_setvarnum(varNum);

    mpz_init(globalSquareRootCoeff);

    LEAF_TYPE *onePtr = malloc(sizeof(LEAF_TYPE));
    if (onePtr == NULL) { bdd_error(BDD_MEMORY); }
    onePtr->pImpl = NULL;
    allocPimpl(onePtr);
    set_d_generic(onePtr->pImpl->re, 1.0);
    /* im already zeroed by allocPimpl */

    qBDD *variables = malloc(varNum * sizeof(qBDD));
    if (variables == NULL) { bdd_error(BDD_MEMORY); }
    for (unsigned i = 0; i < varNum; i++)
        variables[i] = bdd_ithvar(i);

    qBDD leaf1   = mtbdd_maketerminal(onePtr, lt_classic);
    qBDD_protect(leaf1); 
    qBDD cube_bdd = mtbdd_cube2(0x0, varNum, variables, leaf1, bdd_false());
    qBDD_unprotect(leaf1);  
    free(variables);
    *c = cube_bdd;
}

void symb_init(qBDD *circ, mtbdd_symb_t *symbc)
{
    size_t msize = 2 * (qBDD_leafcount(*circ)); // multiplied because one var is needed for every coefficient
                                                 // !doesn't count F ... needs to be allocated manually
    vmap_init(&(symbc->vm), msize);
    symbc->is_reduced = true;  // initially tries to reduce symb. leaves into F
    symbc->is_refined = false;
    
    symbc->map = my_mtbdd_to_symb_map_i(*circ, symbc->vm);
    qBDD_protect((symbc->map));
    symbc->val = my_mtbdd_map_to_symb_val_i(symbc->map, symbc->vm->map, symbc->is_reduced);
    qBDD_protect((symbc->val));
    initInvSqrtCoeffSymb();
}

/*
 * Probability computation
 */

leaf_scalar_t qBDD_calculateProb(qBDD terminal) {

    LEAF_TYPE_IMPL *leaf = ((LEAF_TYPE*)mtbdd_getTerminalValue(terminal))->pImpl;

    return leaf->re[0] * leaf->re[0] + leaf->im[0] * leaf->im[0];   // |z|^2
}
/**
 * Global square root coeffs handles
 */

void incInvSqrtCoeff() {
    //mpz_add_ui(globalSquareRootCoeff, globalSquareRootCoeff, 1);
    // not used, kept for compliance with algebraic
    return;
}

void incInvSqrtCoeffSymb() {
    mpz_add_ui(globalSquareRootCoeffSymb, globalSquareRootCoeffSymb, 1);
    return;
}

void initInvSqrtCoeffSymb() {
    mpz_init(globalSquareRootCoeffSymb);
}

void addInvSqrtCoeffs() {
    mpz_add(globalSquareRootCoeff, globalSquareRootCoeff, globalSquareRootCoeffSymb);
}

void mulInvSqrtCoeff(mpz_t res, mpz_t a, unsigned long mul) {
   mpz_mul_ui(res, a, mul);
}

void clearInvSqrtCoeffSymb() {
    mpz_clear(globalSquareRootCoeffSymb);
}

void clearInvSqrtCoeffNormal() {
    mpz_clear(globalSquareRootCoeff);
}

void resetInvSqrtCoeffSymb() {
    mpz_set_ui(globalSquareRootCoeffSymb, 0);
}

void mulInvSqrtSymbCoeff(unsigned long a) {
    mpz_mul_ui(globalSquareRootCoeffSymb, globalSquareRootCoeffSymb, a);
    return;
}


/* EOF leaf_algebraic_mpz.c */