/**
 * @file leaf_algebraic_mpz.c
 * 4-tuple (a + bω + cω² + dω³) leaf arithmetic over GMP integers.
 */

#include "leaf_algebraic_mpz.h"
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
    leaf_primitive_t a;
    leaf_primitive_t b;
    leaf_primitive_t c;
    leaf_primitive_t d;
};

typedef struct sl_val {
    /// ptr to a list representing the symbolic expression for the first variable
    symexp_list_t *a;
    symexp_list_t *b;
    symexp_list_t *c;
    symexp_list_t *d;
} sl_val_t;

/// MTBDD leaf value with the variable mapping for symbolic representation
typedef struct sl_map {
    vars_t va;
    vars_t vb;
    vars_t vc;
    vars_t vd;
} sl_map_t;


/* 
 * Field accessors
 */

static inline mpz_t* leafA(const LEAF_TYPE* leaf) { return &leaf->pImpl->a; }
static inline mpz_t* leafB(const LEAF_TYPE* leaf) { return &leaf->pImpl->b; }
static inline mpz_t* leafC(const LEAF_TYPE* leaf) { return &leaf->pImpl->c; }
static inline mpz_t* leafD(const LEAF_TYPE* leaf) { return &leaf->pImpl->d; }


/* 
 * Tuple lifecycle
 */

static inline void allocPimpl(LEAF_TYPE* result) {
    result->pImpl = malloc(sizeof(LEAF_TYPE_IMPL));
    if (!result->pImpl) exit(1); // TODO EXIT
    mpz_init(result->pImpl->a);
    mpz_init(result->pImpl->b);
    mpz_init(result->pImpl->c);
    mpz_init(result->pImpl->d);
}
LEAF_TYPE clonePimpl(LEAF_TYPE a) {
    if (!a.pImpl)
        return (LEAF_TYPE){ .pImpl = NULL };

    LEAF_TYPE r;
    allocPimpl(&r);
    mpz_set(r.pImpl->a, a.pImpl->a);
    mpz_set(r.pImpl->b, a.pImpl->b);
    mpz_set(r.pImpl->c, a.pImpl->c);
    mpz_set(r.pImpl->d, a.pImpl->d);
    return r;
}

void freePimpl(void* leafraw) {
    return; /* TODO */
    if (!leafraw) return;

    LEAF_TYPE *leaf = (LEAF_TYPE*) leafraw;
    if (!leaf->pImpl) return;

    mpz_clear(leaf->pImpl->a);
    mpz_clear(leaf->pImpl->b);
    mpz_clear(leaf->pImpl->c);
    mpz_clear(leaf->pImpl->d);
    free(leaf->pImpl);
    leaf->pImpl = NULL;
}


/*
 * Tuple identity (compare + hash)
 */

int terminal_compare_generic(void* a, void* b) {
    LEAF_TYPE *ldata_a = (LEAF_TYPE *) a;
    LEAF_TYPE *ldata_b = (LEAF_TYPE *) b;

    bool null_a = (ldata_a == NULL || ldata_a->pImpl == NULL);
    bool null_b = (ldata_b == NULL || ldata_b->pImpl == NULL);

    if (null_a && null_b) return 1;   // both NULL --  equal
    if (null_a || null_b) return 0;   // one NULL -- not equal

    return !mpz_cmp(*leafA(ldata_a), *leafA(ldata_b)) &&
           !mpz_cmp(*leafB(ldata_a), *leafB(ldata_b)) &&
           !mpz_cmp(*leafC(ldata_a), *leafC(ldata_b)) &&
           !mpz_cmp(*leafD(ldata_a), *leafD(ldata_b));
}

unsigned terminal_hash_generic(void* q) {
    LEAF_TYPE *ldata = (LEAF_TYPE*) q;
    uint64_t val = 1;
    val = MY_HASH_COMB_GMP(val, *leafD(ldata));
    val = MY_HASH_COMB_GMP(val, *leafC(ldata));
    val = MY_HASH_COMB_GMP(val, *leafB(ldata));
    val = MY_HASH_COMB_GMP(val, *leafA(ldata));
    return (unsigned)(val ^ (val >> 32));
}


/*
 * String representation registered with buddy
 */

static int _leaf_to_str_output_i(char *buf, LEAF_TYPE_IMPL *ldata,
                                  mpz_t a, mpz_t b, mpz_t c, mpz_t d,
                                  mpz_t k, mp_bitcnt_t shift_cnt)
{
    char buf_a[MAX_NUM_LEN + 2] = {0};
    char buf_b[MAX_NUM_LEN + 2] = {0};
    char buf_c[MAX_NUM_LEN + 2] = {0};
    char buf_d[MAX_NUM_LEN + 2] = {0};
    int chars_written;

    if (mpz_sizeinbase(a, 10) > MAX_NUM_LEN) {
        chars_written = snprintf(buf_a, MAX_NUM_LEN + 2, VAR_NAME_FMT, lnum_map_add(&(ldata->a), shift_cnt));
        assert(chars_written < MAX_NUM_LEN + 2 && chars_written >= 0);
    } else {
        gmp_snprintf(buf_a, MAX_NUM_LEN + 2, "%Zd", a);
    }

    if (mpz_sizeinbase(b, 10) > MAX_NUM_LEN) {
        chars_written = snprintf(buf_b, MAX_NUM_LEN + 2, "+"VAR_NAME_FMT, lnum_map_add(&(ldata->b), shift_cnt));
        assert(chars_written < MAX_NUM_LEN + 2 && chars_written >= 0);
    } else {
        gmp_snprintf(buf_b, MAX_NUM_LEN + 2, "%+Zd", b);
    }

    if (mpz_sizeinbase(c, 10) > MAX_NUM_LEN) {
        chars_written = snprintf(buf_c, MAX_NUM_LEN + 2, "+"VAR_NAME_FMT, lnum_map_add(&(ldata->c), shift_cnt));
        assert(chars_written < MAX_NUM_LEN + 2 && chars_written >= 0);
    } else {
        gmp_snprintf(buf_c, MAX_NUM_LEN + 2, "%+Zd", c);
    }

    if (mpz_sizeinbase(d, 10) > MAX_NUM_LEN) {
        chars_written = snprintf(buf_d, MAX_NUM_LEN + 2, "+"VAR_NAME_FMT, lnum_map_add(&(ldata->d), shift_cnt));
        assert(chars_written < MAX_NUM_LEN + 2 && chars_written >= 0);
    } else {
        gmp_snprintf(buf_d, MAX_NUM_LEN + 2, "%+Zd", d);
    }

    chars_written = gmp_snprintf(buf, MAX_LEAF_STR_LEN,
        "(1/√2)^(%Zd) * (%s%sω%sω²%sω³)", k, buf_a, buf_b, buf_c, buf_d);
    return chars_written;
}

char* terminal_to_str_generic(void* ldata_raw, char *buddy_buf, size_t buddy_bufsize) {
    LEAF_TYPE *leafValP = (LEAF_TYPE*) ldata_raw;
    LEAF_TYPE_IMPL *ldata = leafValP->pImpl;
    char ldata_string[MAX_LEAF_STR_LEN] = {0};
    int chars_written;

    if (mpz_even_p(ldata->a) && mpz_even_p(ldata->b) &&
        mpz_even_p(ldata->c) && mpz_even_p(ldata->d))
    {
        mp_bitcnt_t greatest_pow2 = GET_MIN(GET_MIN(GET_MIN(
            mpz_scan1(ldata->a, 0),
            mpz_scan1(ldata->b, 0)),
            mpz_scan1(ldata->c, 0)),
            mpz_scan1(ldata->d, 0));

        assert(mpz_fits_uint_p(globalSquareRootCoeff));
        mp_bitcnt_t pow2 = mpz_get_ui(globalSquareRootCoeff) >> 1;
        mp_bitcnt_t shift_cnt = GET_MIN(greatest_pow2, pow2);

        mpz_t a, b, c, d, k;
        mpz_inits(a, b, c, d, k, NULL);
        mpz_fdiv_q_2exp(a, ldata->a, shift_cnt);
        mpz_fdiv_q_2exp(b, ldata->b, shift_cnt);
        mpz_fdiv_q_2exp(c, ldata->c, shift_cnt);
        mpz_fdiv_q_2exp(d, ldata->d, shift_cnt);
        mpz_sub_ui(k, globalSquareRootCoeff, shift_cnt << 1);

        chars_written = _leaf_to_str_output_i(ldata_string, ldata, a, b, c, d, k, shift_cnt);
        mpz_clears(a, b, c, d, k, NULL);
    } else {
        chars_written = _leaf_to_str_output_i(ldata_string, ldata,
            ldata->a, ldata->b, ldata->c, ldata->d, globalSquareRootCoeff, 0);
    }

    if (chars_written >= MAX_LEAF_STR_LEN)
        error_exit("Allocated string length for leaf value output has not been sufficient.\n");
    else if (chars_written < 0)
        error_exit("An encoding error has occured when producing leaf value output.\n");

    if (chars_written < (int)buddy_bufsize) {
        memcpy(buddy_buf, ldata_string, chars_written * sizeof(char));
        buddy_buf[chars_written] = '\0';
        return buddy_buf;
    }

    char *new_buf = (char*)my_malloc((chars_written + 1) * sizeof(char));
    memcpy(new_buf, ldata_string, chars_written * sizeof(char));
    new_buf[chars_written] = '\0';
    return new_buf;
}


/*
 * Algebraic operations classical (mpz_t)
 */

LEAF_TYPE invertLeaf(LEAF_TYPE a) {
    if (a.pImpl == NULL) return a;
    LEAF_TYPE result;
    allocPimpl(&result);
    mpz_neg(result.pImpl->a, *leafA(&a));
    mpz_neg(result.pImpl->b, *leafB(&a));
    mpz_neg(result.pImpl->c, *leafC(&a));
    mpz_neg(result.pImpl->d, *leafD(&a));
    return result;
}

LEAF_TYPE addLeaf(LEAF_TYPE a, LEAF_TYPE b) {
    if (a.pImpl == NULL) return clonePimpl(b);
    if (b.pImpl == NULL) return clonePimpl(a);
    LEAF_TYPE result;
    allocPimpl(&result);
    mpz_add(result.pImpl->a, *leafA(&a), *leafA(&b));
    mpz_add(result.pImpl->b, *leafB(&a), *leafB(&b));
    mpz_add(result.pImpl->c, *leafC(&a), *leafC(&b));
    mpz_add(result.pImpl->d, *leafD(&a), *leafD(&b));

    if (!mpz_sgn(result.pImpl->a) && !mpz_sgn(result.pImpl->b) &&
        !mpz_sgn(result.pImpl->c) && !mpz_sgn(result.pImpl->d)) {
        mpz_clears(result.pImpl->a, result.pImpl->b, result.pImpl->c, result.pImpl->d, NULL);
        free(result.pImpl);
        return (LEAF_TYPE){ .pImpl = NULL };
    }
    return result;
}

LEAF_TYPE addLeafS(LEAF_TYPE a, LEAF_TYPE b) {
    // multiplication by 1/sqrt(2) done externally
    return addLeaf(a, b);
}

LEAF_TYPE subLeaf(LEAF_TYPE a, LEAF_TYPE b) {
    if (a.pImpl == NULL) return invertLeaf(b);
    if (b.pImpl == NULL) return clonePimpl(a);
    LEAF_TYPE result;
    allocPimpl(&result);
    mpz_sub(result.pImpl->a, *leafA(&a), *leafA(&b));
    mpz_sub(result.pImpl->b, *leafB(&a), *leafB(&b));
    mpz_sub(result.pImpl->c, *leafC(&a), *leafC(&b));
    mpz_sub(result.pImpl->d, *leafD(&a), *leafD(&b));

    if (!mpz_sgn(result.pImpl->a) && !mpz_sgn(result.pImpl->b) &&
        !mpz_sgn(result.pImpl->c) && !mpz_sgn(result.pImpl->d)) {
        mpz_clears(result.pImpl->a, result.pImpl->b, result.pImpl->c, result.pImpl->d, NULL);
        free(result.pImpl);
        return (LEAF_TYPE){ .pImpl = NULL };
    }
    return result;
}

LEAF_TYPE subLeafS(LEAF_TYPE a, LEAF_TYPE b) {
    // multiplication by 1/sqrt(2) done externally
    return subLeaf(a, b);
}

LEAF_TYPE mulLeaf(LEAF_TYPE a, LEAF_TYPE b) {
    LEAF_TYPE result;
    allocPimpl(&result);
    mpz_mul(result.pImpl->a, *leafA(&a), *leafA(&b));
    mpz_mul(result.pImpl->b, *leafB(&a), *leafB(&b));
    mpz_mul(result.pImpl->c, *leafC(&a), *leafC(&b));
    mpz_mul(result.pImpl->d, *leafD(&a), *leafD(&b));
    return result;
}

LEAF_TYPE mulLeafS(LEAF_TYPE a, LEAF_TYPE b) {
    // multiplication by 1/sqrt(2) done externally
    return mulLeaf(a, b);
}

LEAF_TYPE divLeaf(LEAF_TYPE a, LEAF_TYPE b) {
    LEAF_TYPE result;
    allocPimpl(&result);
    mpz_div(result.pImpl->a, *leafA(&a), *leafA(&b));
    mpz_div(result.pImpl->b, *leafB(&a), *leafB(&b));
    mpz_div(result.pImpl->c, *leafC(&a), *leafC(&b));
    mpz_div(result.pImpl->d, *leafD(&a), *leafD(&b));
    return result;
}

LEAF_TYPE divLeafS(LEAF_TYPE a, LEAF_TYPE b) {
    // multiplication by 1/sqrt(2) done externally
    return divLeaf(a, b);
}

LEAF_TYPE sqrtLeaf(LEAF_TYPE a) {
    // TODO
    return a;
}

LEAF_TYPE rotateCoef1(LEAF_TYPE l) {
    if (l.pImpl == NULL) return l;
    LEAF_TYPE result;
    allocPimpl(&result);
    mpz_neg(result.pImpl->a, *leafD(&l));       /* a' = -d */
    mpz_set(result.pImpl->b, *leafA(&l));        /* b' =  a */
    mpz_set(result.pImpl->c, *leafB(&l));        /* c' =  b */
    mpz_set(result.pImpl->d, *leafC(&l));        /* d' =  c */
    return result;
}

LEAF_TYPE rotateCoef1S(LEAF_TYPE l) {
    // multiplication by 1/sqrt(2) done externally
    return rotateCoef1(l);
}

LEAF_TYPE rotateCoef2(LEAF_TYPE l) {
    if (l.pImpl == NULL) return l;
    LEAF_TYPE result;
    allocPimpl(&result);
    mpz_neg(result.pImpl->a, *leafC(&l));        /* a' = -c */
    mpz_neg(result.pImpl->b, *leafD(&l));        /* b' = -d */
    mpz_set(result.pImpl->c, *leafA(&l));        /* c' =  a */
    mpz_set(result.pImpl->d, *leafB(&l));        /* d' =  b */
    return result;
}

LEAF_TYPE rotateCoef1S_inv(LEAF_TYPE l) {
    if (l.pImpl == NULL) return l;
    LEAF_TYPE result;
    allocPimpl(&result);
    mpz_set(result.pImpl->a, *leafB(&l));        /* a' =  b */
    mpz_set(result.pImpl->b, *leafC(&l));        /* b' =  c */
    mpz_set(result.pImpl->c, *leafD(&l));        /* c' =  d */
    mpz_neg(result.pImpl->d, *leafA(&l));        /* d' = -a */
    return result;
}

LEAF_TYPE rotateCoef2S(LEAF_TYPE l) {
    // multiplication by 1/sqrt(2) done externally
    return rotateCoef2(l);
}

LEAF_TYPE times2Leaf(LEAF_TYPE l) {
    if (l.pImpl == NULL) return l;
    LEAF_TYPE result;
    allocPimpl(&result);
    mpz_mul_ui(result.pImpl->a, *leafA(&l), 2);
    mpz_mul_ui(result.pImpl->b, *leafB(&l), 2);
    mpz_mul_ui(result.pImpl->c, *leafC(&l), 2);
    mpz_mul_ui(result.pImpl->d, *leafD(&l), 2);
    return result;
}

LEAF_TYPE times2LeafS(LEAF_TYPE l) {
    // multiplication by 1/sqrt(2) done externally
    return times2Leaf(l);
}


/* 
 * Algebraic operations symbolic (sl_val_t / symexp)
 */

LEAF_TYPE mtbdd_symb_neg_i(LEAF_TYPE t) {
    if (t.pImpl == NULL) return t;
    sl_val_t *ldata = (sl_val_t*) t.pImpl;
    sl_val_t *res_data = (sl_val_t*)malloc(sizeof(sl_val_t));
    res_data->a = symexp_op(NULL, ldata->a, SYMEXP_SUB);
    res_data->b = symexp_op(NULL, ldata->b, SYMEXP_SUB);
    res_data->c = symexp_op(NULL, ldata->c, SYMEXP_SUB);
    res_data->d = symexp_op(NULL, ldata->d, SYMEXP_SUB);
    return (LEAF_TYPE){ .pImpl = (LEAF_TYPE_IMPL*)res_data };
}

LEAF_TYPE mtbdd_symb_coef_rot1_i(LEAF_TYPE t) {
    if (t.pImpl == NULL) return t;
    sl_val_t *ldata = (sl_val_t*) t.pImpl;
    sl_val_t *res_data = (sl_val_t*)malloc(sizeof(sl_val_t));
    res_data->a = symexp_op(NULL, ldata->d, SYMEXP_SUB);
    res_data->b = ldata->a;
    res_data->c = ldata->b;
    res_data->d = ldata->c;
    return (LEAF_TYPE){ .pImpl = (LEAF_TYPE_IMPL*)res_data };
}

LEAF_TYPE mtbdd_symb_coef_rot2_i(LEAF_TYPE t) {
    if (t.pImpl == NULL) return t;
    sl_val_t *ldata = (sl_val_t*) t.pImpl;
    sl_val_t *res_data = (sl_val_t*)malloc(sizeof(sl_val_t));
    res_data->a = symexp_op(NULL, ldata->c, SYMEXP_SUB);
    res_data->b = symexp_op(NULL, ldata->d, SYMEXP_SUB);
    res_data->c = ldata->a;
    res_data->d = ldata->b;
    return (LEAF_TYPE){ .pImpl = (LEAF_TYPE_IMPL*)res_data };
}

LEAF_TYPE mtbdd_symb_plus_i(LEAF_TYPE a, LEAF_TYPE b) {
    if (a.pImpl == NULL) return b;
    if (b.pImpl == NULL) return a;
    sl_val_t *a_data = (sl_val_t*) a.pImpl;
    sl_val_t *b_data = (sl_val_t*) b.pImpl;
    sl_val_t *res_data = (sl_val_t*)malloc(sizeof(sl_val_t));
    res_data->a = symexp_op(a_data->a, b_data->a, SYMEXP_ADD);
    res_data->b = symexp_op(a_data->b, b_data->b, SYMEXP_ADD);
    res_data->c = symexp_op(a_data->c, b_data->c, SYMEXP_ADD);
    res_data->d = symexp_op(a_data->d, b_data->d, SYMEXP_ADD);

    if (!res_data->a && !res_data->b && !res_data->c && !res_data->d) {
        free(res_data);
        return (LEAF_TYPE){ .pImpl = NULL };
    }
    return (LEAF_TYPE){ .pImpl = (LEAF_TYPE_IMPL*)res_data };
}

LEAF_TYPE mtbdd_symb_minus_i(LEAF_TYPE a, LEAF_TYPE b) {
    if (a.pImpl == NULL) return mtbdd_symb_neg_i(b);
    if (b.pImpl == NULL) return a;
    sl_val_t *a_data = (sl_val_t*) a.pImpl;
    sl_val_t *b_data = (sl_val_t*) b.pImpl;
    sl_val_t *res_data = (sl_val_t*)malloc(sizeof(sl_val_t));
    res_data->a = symexp_op(a_data->a, b_data->a, SYMEXP_SUB);
    res_data->b = symexp_op(a_data->b, b_data->b, SYMEXP_SUB);
    res_data->c = symexp_op(a_data->c, b_data->c, SYMEXP_SUB);
    res_data->d = symexp_op(a_data->d, b_data->d, SYMEXP_SUB);

    if (!res_data->a && !res_data->b && !res_data->c && !res_data->d) {
        free(res_data);
        return (LEAF_TYPE){ .pImpl = NULL };
    }
    return (LEAF_TYPE){ .pImpl = (LEAF_TYPE_IMPL*)res_data };
}

LEAF_TYPE mtbdd_symb_times_c_i(LEAF_TYPE t, size_t c_raw) {
    // Partial function check
    if (t.pImpl == NULL) return t;

    // Compute c*t if mtbdd is a leaf
    sl_val_t *ldata = (sl_val_t*) t.pImpl;
    unsigned long c = (unsigned long)c_raw;

    sl_val_t *res_data = (sl_val_t *)malloc(sizeof(sl_val_t));
    res_data->a = symexp_mul_c(ldata->a, c);
    res_data->b = symexp_mul_c(ldata->b, c);
    res_data->c = symexp_mul_c(ldata->c, c);
    res_data->d = symexp_mul_c(ldata->d, c);

    LEAF_TYPE res = {.pImpl = (LEAF_TYPE_IMPL*)res_data};
    return res;
}

qBDD mtbdd_map_to_symb_val_reduced_i(qBDD t, size_t raw_map) {
    // Partial function check not needed, 4 variables were assigned to every base vector

    if (qBDD_isTerminal(t)) {
        coef_t* map = (coef_t*) raw_map;
        LEAF_TYPE leaf = qBDD_getTerminalValue(t);
        sl_map_t *t_data = (sl_map_t*) leaf.pImpl;
        

        if (!mpz_sgn(map[t_data->va]) && !mpz_sgn(map[t_data->vb]) &&
            !mpz_sgn(map[t_data->vc]) && !mpz_sgn(map[t_data->vd])) {
            validateApplyResult();
            return qBDD_false();
        }
        sl_val_t *new_data = (sl_val_t*)malloc(sizeof(sl_val_t));
        new_data->a = symexp_init(t_data->va);
        new_data->b = symexp_init(t_data->vb);
        new_data->c = symexp_init(t_data->vc);
        new_data->d = symexp_init(t_data->vd);
        
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
        coef_t* map = (coef_t*) raw_map;
        LEAF_TYPE leaf = qBDD_getTerminalValue(t);
        sl_map_t *t_data = (sl_map_t*) leaf.pImpl;
        sl_val_t *new_data = (sl_val_t*)malloc(sizeof(sl_val_t));

        new_data->a = symexp_init(t_data->va);
        new_data->b = symexp_init(t_data->vb);
        new_data->c = symexp_init(t_data->vc);
        new_data->d = symexp_init(t_data->vd);

        LEAF_TYPE *newLeaf = (LEAF_TYPE*)malloc(sizeof(LEAF_TYPE));
        newLeaf->pImpl = new_data;

        qBDD res = qBDD_maketerminal(qBDD_symbolicValLType(), (void*) newLeaf);
        validateApplyResult();
        return res;
    }
    invalidateApplyResult();
    return qBDD_false(); // Recurse deeper
}

LEAF_TYPE mtbdd_symb_coef_rot1_i_inv(LEAF_TYPE t) {
    if (t.pImpl == NULL) return t;
    sl_val_t *ldata = (sl_val_t*) t.pImpl;
    sl_val_t *res_data = (sl_val_t*)malloc(sizeof(sl_val_t));
    res_data->a = ldata->b;
    res_data->b = ldata->c;
    res_data->c = ldata->d;
    res_data->d = symexp_op(NULL, ldata->a, SYMEXP_SUB);
    return (LEAF_TYPE){ .pImpl = (LEAF_TYPE_IMPL*)res_data };
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
        vars_t new_a, new_b, new_c, new_d;

        if (qBDD_isFalse(val)) {
            new_a = refine_var_check(mdata->va, SYMEXP_NULL, rd);
            new_b = refine_var_check(mdata->vb, SYMEXP_NULL, rd);
            new_c = refine_var_check(mdata->vc, SYMEXP_NULL, rd);
            new_d = refine_var_check(mdata->vd, SYMEXP_NULL, rd);
        }
        else {
            LEAF_TYPE valVal = qBDD_getTerminalValue(val);
            sl_val_t *vdata = (sl_val_t*) valVal.pImpl;
            new_a = refine_var_check(mdata->va, vdata->a, rd);
            new_b = refine_var_check(mdata->vb, vdata->b, rd);
            new_c = refine_var_check(mdata->vc, vdata->c, rd);
            new_d = refine_var_check(mdata->vd, vdata->d, rd);
        }

        if (new_a == mdata->va && new_b == mdata->vb && new_c == mdata->vc && new_d == mdata->vd) {
            validateApplyResult();
            return map;
        }

        // new symbolic var needed
        sl_map_t *new_data = (sl_map_t*)malloc(sizeof(sl_map_t));
        new_data->va = new_a;
        new_data->vb = new_b;
        new_data->vc = new_c;
        new_data->vd = new_d;

        LEAF_TYPE *newLeaf = (LEAF_TYPE*)malloc(sizeof(LEAF_TYPE));
        newLeaf->pImpl = new_data;

        qBDD res = qBDD_maketerminal(qBDD_symbolicMapLType(), (void*)newLeaf);
        validateApplyResult();
        return res;
    }
    invalidateApplyResult();
    return qBDD_false(); // Recurse deeper
}

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


    vars_t var_a = m->next_var;
    vars_t var_b = m->next_var + 1;
    vars_t var_c = m->next_var + 2;
    vars_t var_d = m->next_var + 3;

    // Partial function check
    if (qBDD_isFalse(a)) {
        // initial vmap size does not count with 'mtbdd_false' leaves, so vmap needs to be resized
        m->map = my_realloc(m->map, sizeof(coef_t) * (m->msize + 4));
        m->msize += 4;
        mpz_inits(m->map[var_a], m->map[var_b], m->map[var_c], m->map[var_d], 0);
    }
    else if (qBDD_isTerminal(a)) {
        LEAF_TYPE leafData = qBDD_getTerminalValue(a);
        cnum *orig_data = (cnum*) leafData.pImpl;
        mpz_init_set(m->map[var_a], orig_data->a);
        mpz_init_set(m->map[var_b], orig_data->b);
        mpz_init_set(m->map[var_c], orig_data->c);
        mpz_init_set(m->map[var_d], orig_data->d);
    }

    sl_map_t *new_data = (sl_map_t*) malloc(sizeof(sl_map_t));
    if (new_data == NULL) {
        printf("ERROR ALLOCATION\n");
        exit(EXIT_FAILURE);
    }
    new_data->va = var_a;
    new_data->vb = var_b;
    new_data->vc = var_c;
    new_data->vd = var_d;
    
    LEAF_TYPE *newLeaf = (LEAF_TYPE*)malloc(sizeof(LEAF_TYPE));
    newLeaf->pImpl = new_data;

    qBDD res = qBDD_maketerminal(qBDD_symbolicMapLType(), (void *) newLeaf);
    m->next_var += 4;
    validateApplyResult();
    vmap_insert(m, a, res);
    return res;
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
    return (ldata_a->va == ldata_b->va) && (ldata_a->vb == ldata_b->vb) &&
           (ldata_a->vc == ldata_b->vc) && (ldata_a->vd == ldata_b->vd);
}

unsigned terminal_symb_map_hash_generic(void* l_a) {
    sl_map_t *ldata = (sl_map_t*) ((LEAF_TYPE*)l_a)->pImpl;
    uint64_t val = 1;
    val = MY_HASH_COMB(val, ldata->va);
    val = MY_HASH_COMB(val, ldata->vb);
    val = MY_HASH_COMB(val, ldata->vc);
    val = MY_HASH_COMB(val, ldata->vd);
    return (unsigned)val;
}


int terminal_symb_val_compare_generic(void* l_a, void* l_b) {
    if (l_a == NULL && l_b == NULL) return 1;
    if ((l_a == NULL) != (l_b == NULL)) return 0;
    sl_val_t *ldata_a = (sl_val_t*) ((LEAF_TYPE*)l_a)->pImpl;
    sl_val_t *ldata_b = (sl_val_t*) ((LEAF_TYPE*)l_b)->pImpl;
    return symexp_cmp(ldata_a->a, ldata_b->a) && symexp_cmp(ldata_a->b, ldata_b->b) &&
           symexp_cmp(ldata_a->c, ldata_b->c) && symexp_cmp(ldata_a->d, ldata_b->d);
}

unsigned terminal_symb_val_hash_generic(void* l_a) {
    sl_val_t *ldata = (sl_val_t*) ((LEAF_TYPE*)l_a)->pImpl;
    uint64_t val = 1;
    val = MY_HASH_COMB(val, ldata->a);
    val = MY_HASH_COMB(val, ldata->b);
    val = MY_HASH_COMB(val, ldata->c);
    val = MY_HASH_COMB(val, ldata->d);
    return val;
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
        
        cnum *new_data = (cnum*) malloc(sizeof(cnum)); 
        mpz_init_set(new_data->a, map[data->va]);
        mpz_init_set(new_data->b, map[data->vb]);
        mpz_init_set(new_data->c, map[data->vc]);
        mpz_init_set(new_data->d, map[data->vd]);

        if (!mpz_sgn(new_data->a) && !mpz_sgn(new_data->b) && !mpz_sgn(new_data->c) && !mpz_sgn(new_data->d)) {
            mpz_clears(new_data->a, new_data->b, new_data->c, new_data->d, NULL);
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

bool can_be_reduced(mtbdd_symb_t *symbc, rdata_t *rdata)
{
    bool is_correct = true;
    bool is_zero[rdata->vm->next_var];
    for (int i = 0; i < rdata->vm->next_var; i++) {
        is_zero[i] = false;
    }

    // The whole leaf behaves the same way, so checking every 4th variable is sufficient
    for (int i = 0; i < rdata->vm->next_var; i += 4) {
        // If leaf is initially 0:
        if (!sgn_generic(rdata->vm->map[i]) && !sgn_generic(rdata->vm->map[i+1]) 
            && !sgn_generic(rdata->vm->map[i+2]) && !sgn_generic(rdata->vm->map[i+3])) {
            is_zero[i] = true;
            is_zero[i+1] = true;
            is_zero[i+2] = true;
            is_zero[i+3] = true;

            // Check if the right side of update equation for these variables is 0
            // (eg. change of value caused by H)
            if (rdata->upd->arr[i] != SYMEXP_NULL && rdata->upd->arr[i+1] != SYMEXP_NULL
                && rdata->upd->arr[i+2] != SYMEXP_NULL && rdata->upd->arr[i+3] != SYMEXP_NULL) {
                is_correct = false;
                break;
            }
        }
    }

    // Check if swap with 0 leaf occurs 
    // (i.e., if these variables appear alone on some right side of update equation)
    for(int i = 0; i < rdata->vm->next_var; i +=4) {
        // Check for permutations as well, first variable of the leaf is sufficient
        // (we always swap the whole leaf)
        if (rdata->upd->arr[i] != SYMEXP_NULL) {
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
    mpz_set_ui(*leafA(onePtr), 1);
    /* b, c, d already zeroed by mpz_init inside allocPimpl */

    qBDD *variables = malloc(varNum * sizeof(BDD));
    if (variables == NULL) { bdd_error(BDD_MEMORY); }
    for (unsigned i = 0; i < varNum; i++)
        variables[i] = bdd_ithvar(i);

    qBDD leaf1   = mtbdd_maketerminal(onePtr, lt_classic);
    qBDD cube_bdd = mtbdd_cube2(0x0, varNum, variables, leaf1, bdd_false());
    free(variables);
    *c = cube_bdd;
}

void symb_init(qBDD *circ, mtbdd_symb_t *symbc)
{
    size_t msize = 4 * (qBDD_leafcount(*circ)); // multiplied because one var is needed for every coefficient
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

double qBDD_calculateProb(qBDD terminal) {
    mpf_t a, b, c, d;
    LEAF_TYPE_IMPL *prob = ((LEAF_TYPE*)mtbdd_getTerminalValue(terminal))->pImpl;
    mpf_inits(a, b, c, d, NULL);
    mpf_set_z(a, prob->a);
    mpf_set_z(b, prob->b);
    mpf_set_z(c, prob->c);
    mpf_set_z(d, prob->d);

    assert(mpz_fits_uint_p(globalSquareRootCoeff));
    mp_bitcnt_t shift_cnt = mpz_get_ui(globalSquareRootCoeff) >> 1;

    double prob_re, prob_im;
    double c_a, c_b, c_c, c_d;

    if (mpz_even_p(globalSquareRootCoeff) != 0) {
        mpf_div_2exp(a, a, shift_cnt);
        mpf_div_2exp(b, b, shift_cnt);
        mpf_div_2exp(c, c, shift_cnt);
        mpf_div_2exp(d, d, shift_cnt);
        c_a = mpf_get_d(a); c_b = mpf_get_d(b);
        c_c = mpf_get_d(c); c_d = mpf_get_d(d);
        prob_re = pow(c_a + c_b * M_SQRT1_2 - c_d * M_SQRT1_2, 2);
        prob_im = pow(c_c + c_b * M_SQRT1_2 + c_d * M_SQRT1_2, 2);
    } else {
        mpf_div_2exp(a, a, shift_cnt);
        mpf_div_2exp(b, b, shift_cnt + 1);
        mpf_div_2exp(c, c, shift_cnt);
        mpf_div_2exp(d, d, shift_cnt + 1);
        c_a = mpf_get_d(a); c_b = mpf_get_d(b);
        c_c = mpf_get_d(c); c_d = mpf_get_d(d);
        prob_re = pow(c_a * M_SQRT1_2 + c_b - c_d, 2);
        prob_im = pow(c_c * M_SQRT1_2 + c_b + c_d, 2);
    }

    mpf_clears(a, b, c, d, NULL);
    return prob_re + prob_im;
}

/**
 * Global square root coeffs handles
 */

void incInvSqrtCoeff() {
    mpz_add_ui(globalSquareRootCoeff, globalSquareRootCoeff, 1);
}

void incInvSqrtCoeffSymb() {
    mpz_add_ui(globalSquareRootCoeffSymb, globalSquareRootCoeffSymb, 1);
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
}

static void _unsupported(const char *name) {
    fprintf(stderr, "ERROR: %s is not supported by the GMP algebraic backend. "
                    "Use a floating-point backend for rotation gates.\n", name);
    abort();
}

LEAF_TYPE rx_low_leaf(LEAF_TYPE low, LEAF_TYPE high, size_t param)
    { (void)low;(void)high;(void)param; _unsupported("rx_low_leaf");  return low; }
LEAF_TYPE rx_high_leaf(LEAF_TYPE low, LEAF_TYPE high, size_t param)
    { (void)low;(void)high;(void)param; _unsupported("rx_high_leaf"); return low; }
LEAF_TYPE ry_low_leaf(LEAF_TYPE low, LEAF_TYPE high, size_t param)
    { (void)low;(void)high;(void)param; _unsupported("ry_low_leaf");  return low; }
LEAF_TYPE ry_high_leaf(LEAF_TYPE low, LEAF_TYPE high, size_t param)
    { (void)low;(void)high;(void)param; _unsupported("ry_high_leaf"); return low; }
LEAF_TYPE rz_low_leaf(LEAF_TYPE l, size_t param)
    { (void)l;(void)param; _unsupported("rz_low_leaf");  return l; }
LEAF_TYPE rz_high_leaf(LEAF_TYPE l, size_t param)
    { (void)l;(void)param; _unsupported("rz_high_leaf"); return l; }
LEAF_TYPE negI_mul(LEAF_TYPE a)
    { (void)a; _unsupported("negI_mul"); return a; }
LEAF_TYPE mulPhaseLeaf(LEAF_TYPE t, size_t arg)
    { (void)t;(void)arg; _unsupported("mulPhaseLeaf"); return t; }



/* EOF leaf_algebraic_mpz.c */