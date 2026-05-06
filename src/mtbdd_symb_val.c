#include <string.h>
//#include <sylvan_int.h>
#include "mtbdd_symb_val.h"
#include "mtbdd_symb_map.h"
#include "hash.h"
#include "error.h"
#include "interface.h"

/// leaf type id for symbolic representation
//uint32_t ltype_symb_expr_id;
/// coefficient k common for all MTBDD leaf values for symbolic representation
coefs_k_t cs_k;


/// Max. size of string written as leaf value in output file.
#define MAX_SYMB_LEAF_STR_LEN MAX_ST_TO_STR_LEN * 5


void cs_k_reset()
{
    mpz_set_ui(cs_k, 0);
}

/* SETUP */
// void init_my_leaf_symb_val()
// {
//     ltype_symb_expr_id = sylvan_mt_create_type();

//     sylvan_mt_set_create(ltype_symb_expr_id, my_leaf_symb_v_create);
//     sylvan_mt_set_destroy(ltype_symb_expr_id, my_leaf_symb_v_destroy);
//     sylvan_mt_set_equals(ltype_symb_expr_id, my_leaf_symb_v_equals);
//     sylvan_mt_set_to_str(ltype_symb_expr_id, my_leaf_symb_v_to_str);
//     sylvan_mt_set_hash(ltype_symb_expr_id, my_leaf_symb_v_hash);
// }

/* CUSTOM HANDLES */
// void my_leaf_symb_v_create(uint64_t *ldata_p_raw)
// {
//     sl_val_t** ldata_p = (sl_val_t**)ldata_p_raw; // Leaf data type is uint64_t, we store there ptr to our actual data
    
//     sl_val_t* orig_ldata = *ldata_p;
//     sl_val_t* new_ldata = (sl_val_t*)my_malloc(sizeof(sl_val_t));

//     new_ldata->a = orig_ldata->a;
//     new_ldata->b = orig_ldata->b;
//     new_ldata->c = orig_ldata->c;
//     new_ldata->d = orig_ldata->d;

//     *ldata_p = new_ldata;
// }

// void my_leaf_symb_v_destroy(uint64_t ldata)
// {
//     sl_val_t *data_p = (sl_val_t*) ldata; // Data in leaf = pointer to my data
//     // Called only after htab_clear() -> when leaf data has already been deallocated
//     free(data_p);
// }

// int my_leaf_symb_v_equals(const uint64_t ldata_a_raw, const uint64_t ldata_b_raw)
// {
//     sl_val_t *ldata_a = (sl_val_t *) ldata_a_raw;
//     sl_val_t *ldata_b = (sl_val_t *) ldata_b_raw;

//     return symexp_cmp(ldata_a->a, ldata_b->a) && symexp_cmp(ldata_a->b, ldata_b->b) && symexp_cmp(ldata_a->c, ldata_b->c) \
//            && symexp_cmp(ldata_a->d, ldata_b->d);
// }

// char* my_leaf_symb_v_to_str(int complemented, uint64_t ldata_raw, char *sylvan_buf, size_t sylvan_bufsize)
// {
//     (void) complemented;
//     sl_val_t *ldata = (sl_val_t*) ldata_raw;

//     char ldata_string[MAX_SYMB_LEAF_STR_LEN] = {0};
    
//     int chars_written = gmp_snprintf(ldata_string, MAX_SYMB_LEAF_STR_LEN, "(1/√2)^(%Zd) * ((%s) + (%s)ω + (%s)ω² + (%s)ω³)", \
//                                      cs_k, symexp_to_str(ldata->a), symexp_to_str(ldata->b), symexp_to_str(ldata->c), symexp_to_str(ldata->d));
//     // Was string truncated?
//     if (chars_written >= MAX_SYMB_LEAF_STR_LEN) {
//         error_exit("Allocated string length for leaf value output has not been sufficient.\n");
//     }
//     else if (chars_written < 0) {
//         error_exit("An encoding error has occured when producing leaf value output.\n");
//     }

//     // Is buffer large enough?
//     if (chars_written < sylvan_bufsize) {
//         memcpy(sylvan_buf, ldata_string, chars_written * sizeof(char));
//         sylvan_buf[chars_written] = '\0';
//         return sylvan_buf;
//     }
    
//     // Else return newly allocated string
//     char *new_buf = (char*)my_malloc((chars_written + 1) * sizeof(char));
//     memcpy(new_buf, ldata_string, chars_written * sizeof(char));
//     new_buf[chars_written] = '\0';
//     return new_buf;
// }

// uint64_t my_leaf_symb_v_hash(const uint64_t ldata_raw, const uint64_t seed)
// {
//     sl_val_t *ldata = (sl_val_t*) ldata_raw;

//     uint64_t val = seed;
//     val = MY_HASH_COMB(val, ldata->a);
//     val = MY_HASH_COMB(val, ldata->b);
//     val = MY_HASH_COMB(val, ldata->c);
//     val = MY_HASH_COMB(val, ldata->d);

//     return val;
// }

/* CUSTOM MTBDD OPERATIONS */
// TASK_IMPL_2(MTBDD, mtbdd_map_to_symb_val, MTBDD, t, size_t, raw_map)
// {
//     // Partial function check not needed, 4 variables were assigned to every base vector

//     if (mtbdd_isleaf(t)) {
//         coef_t* map = (coef_t*) raw_map;
//         sl_map_t *t_data = (sl_map_t*) mtbdd_getvalue(t);
//         sl_val_t new_data;

//         new_data.a = symexp_init(t_data->va);
//         new_data.b = symexp_init(t_data->vb);
//         new_data.c = symexp_init(t_data->vc);
//         new_data.d = symexp_init(t_data->vd);
//         MTBDD res = mtbdd_makeleaf(ltype_symb_expr_id, (uint64_t) &new_data);
//         return res;
//     }

//     return mtbdd_invalid; // Recurse deeper
// }



qBDD my_mtbdd_map_to_symb_val_normal_i(qBDD t, size_t map) {
    return unary_apply_guarded(t, mtbdd_map_to_symb_val_i, map);
}

// TASK_IMPL_2(MTBDD, mtbdd_map_to_symb_val_reduced, MTBDD, t, size_t, raw_map)
// {
//     // Partial function check not needed, 4 variables were assigned to every base vector

//     if (mtbdd_isleaf(t)) {
//         coef_t* map = (coef_t*) raw_map;
//         sl_map_t *t_data = (sl_map_t*) mtbdd_getvalue(t);
//         sl_val_t new_data;

//         if (!mpz_sgn(map[t_data->va]) && !mpz_sgn(map[t_data->vb]) &&
//             !mpz_sgn(map[t_data->vc]) && !mpz_sgn(map[t_data->vd])) {
//             return mtbdd_false;
//         }

//         new_data.a = symexp_init(t_data->va);
//         new_data.b = symexp_init(t_data->vb);
//         new_data.c = symexp_init(t_data->vc);
//         new_data.d = symexp_init(t_data->vd);
//         MTBDD res = mtbdd_makeleaf(ltype_symb_expr_id, (uint64_t) &new_data);
//         return res;
//     }

//     return mtbdd_invalid; // Recurse deeper
// }



qBDD my_mtbdd_map_to_symb_val_reduced_i(qBDD t, size_t raw_map) {
    return unary_apply_guarded(t, mtbdd_map_to_symb_val_reduced_i, raw_map);
}

qBDD my_mtbdd_map_to_symb_val_i(qBDD t, size_t map, bool reduce_zero) {
    if (reduce_zero) {
        return my_mtbdd_map_to_symb_val_reduced_i(t, map);
    } else {
        return my_mtbdd_map_to_symb_val_normal_i(t, map);
    }
}

// TASK_IMPL_2(MTBDD, mtbdd_from_symb, MTBDD, t, size_t, raw_map)
// {
//     // Partial function check
//     if (t == mtbdd_false) return mtbdd_false;

//     if (mtbdd_isleaf(t)) {
//         coef_t* map = (coef_t*) raw_map;
//         sl_map_t *data = (sl_map_t*) mtbdd_getvalue(t);
        
//         cnum new_data; // can be local, mtbdd_makeleaf makes realloc
//         mpz_init_set(new_data.a, map[data->va]);
//         mpz_init_set(new_data.b, map[data->vb]);
//         mpz_init_set(new_data.c, map[data->vc]);
//         mpz_init_set(new_data.d, map[data->vd]);

//         if (!mpz_sgn(new_data.a) && !mpz_sgn(new_data.b) && !mpz_sgn(new_data.c) && !mpz_sgn(new_data.d)) {
//             mpz_clears(new_data.a, new_data.b, new_data.c, new_data.d, NULL);
//             return mtbdd_false;
//         }
        
//         MTBDD res = mtbdd_makeleaf(ltype_id, (uint64_t) &new_data);
//         mpz_clears(new_data.a, new_data.b, new_data.c, new_data.d, NULL);

//         return res;
//     }

//     return mtbdd_invalid; // Recurse deeper
// }


qBDD my_mtbdd_from_symb_i(qBDD t, size_t raw_map) {
    return unary_apply_guarded(t, mtbdd_from_symb_i, raw_map);
}

// Operations needed for gate representation:
// TASK_IMPL_2(MTBDD, mtbdd_symb_plus, MTBDD*, p_a, MTBDD*, p_b)
// {
//     MTBDD a = *p_a;
//     MTBDD b = *p_b;

//     // Partial function check
//     if (a == mtbdd_false) return b;
//     if (b == mtbdd_false) return a;

//     // Compute a + b if both mtbdds are leaves
//     if (mtbdd_isleaf(a) && mtbdd_isleaf(b)) {
//         sl_val_t *a_data = (sl_val_t*) mtbdd_getvalue(a);
//         sl_val_t *b_data = (sl_val_t*) mtbdd_getvalue(b);

//         sl_val_t res_data;
//         res_data.a = symexp_op(a_data->a, b_data->a, SYMEXP_ADD);
//         res_data.b = symexp_op(a_data->b, b_data->b, SYMEXP_ADD);
//         res_data.c = symexp_op(a_data->c, b_data->c, SYMEXP_ADD);
//         res_data.d = symexp_op(a_data->d, b_data->d, SYMEXP_ADD);

//         if (!res_data.a && !res_data.b && !res_data.c && !res_data.d) {
//             return mtbdd_false;
//         }
        
//         MTBDD res = mtbdd_makeleaf(ltype_symb_expr_id, (uint64_t) &res_data);
//         return res;
//     }

//     /* Commutative, so swap a,b for better cache performance*/
//     if (a < b) {
//         *p_a = b;
//         *p_b = a;
//     }

//     return mtbdd_invalid; // Recurse deeper
// }



qBDD my_mtbdd_symb_plus_i(qBDD a, qBDD b) {
    return binary_apply(a, b, mtbdd_symb_plus_i);
}

// TASK_IMPL_2(MTBDD, mtbdd_symb_minus, MTBDD*, p_a, MTBDD*, p_b)
// {
//     MTBDD a = *p_a;
//     MTBDD b = *p_b;

//     // Partial function check
//     if (a == mtbdd_false){
//         MTBDD b_minus = my_mtbdd_symb_neg(b);
//         return b_minus;
//     } 
//     if (b == mtbdd_false) return a;

//     // Compute a - b if both mtbdds are leaves
//     if (mtbdd_isleaf(a) && mtbdd_isleaf(b)) {
//         sl_val_t *a_data = (sl_val_t*) mtbdd_getvalue(a);
//         sl_val_t *b_data = (sl_val_t*) mtbdd_getvalue(b);
        
//         sl_val_t res_data;
//         res_data.a = symexp_op(a_data->a, b_data->a, SYMEXP_SUB);
//         res_data.b = symexp_op(a_data->b, b_data->b, SYMEXP_SUB);
//         res_data.c = symexp_op(a_data->c, b_data->c, SYMEXP_SUB);
//         res_data.d = symexp_op(a_data->d, b_data->d, SYMEXP_SUB);

//         if (!res_data.a && !res_data.b && !res_data.c && !res_data.d) {
//             return mtbdd_false;
//         }
        
//         MTBDD res = mtbdd_makeleaf(ltype_symb_expr_id, (uint64_t) &res_data);
//         return res;
//     }

//     return mtbdd_invalid; // Recurse deeper
// }


qBDD my_mtbdd_symb_minus_i(qBDD a, qBDD b) {
    return binary_apply(a, b, mtbdd_symb_minus_i);
}

// qBDD my_mtbdd_symb_minus_s_i(qBDD a, qBDD b) {
//     return binary_apply(a, b, mtbdd_symb_minus_s_i);
// }

// qBDD my_mtbdd_symb_plus_s_i(qBDD a, qBDD b) {
//     return binary_apply(a, b, mtbdd_symb_plus_s_i);
// }
// TASK_IMPL_2(MTBDD, mtbdd_symb_times_c, MTBDD, t, size_t, c_raw)
// {
//     // Partial function check
//     if (t == mtbdd_false) return mtbdd_false;

//     // Compute c*t if mtbdd is a leaf
//     if (mtbdd_isleaf(t)) {
//         sl_val_t *ldata = (sl_val_t*) mtbdd_getvalue(t);
//         unsigned long c = (unsigned long)c_raw;

//         sl_val_t res_data;
//         res_data.a = symexp_mul_c(ldata->a, c);
//         res_data.b = symexp_mul_c(ldata->b,c);
//         res_data.c = symexp_mul_c(ldata->c, c);
//         res_data.d = symexp_mul_c(ldata->d, c);

//         MTBDD res = mtbdd_makeleaf(ltype_symb_expr_id, (uint64_t) &res_data);
//         return res;
//     }

//     return mtbdd_invalid; // Recurse deeper
// }

qBDD my_mtbdd_symb_times_c_i(qBDD t, size_t c_raw) {
    return unary_apply_param(t, mtbdd_symb_times_c_i, c_raw);
}

// TASK_IMPL_2(MTBDD, mtbdd_symb_neg, MTBDD, t, size_t, x)
// {
//     (void)x; // extra parameter needed for task - not needed

//     // Partial function check
//     if (t == mtbdd_false) return mtbdd_false;

//     // Compute -a if mtbdd is a leaf
//     if (mtbdd_isleaf(t)) {
//         sl_val_t *ldata = (sl_val_t*) mtbdd_getvalue(t);

//         sl_val_t res_data;
//         res_data.a = symexp_op(NULL, ldata->a, SYMEXP_SUB);
//         res_data.b = symexp_op(NULL, ldata->b, SYMEXP_SUB);
//         res_data.c = symexp_op(NULL, ldata->c, SYMEXP_SUB);
//         res_data.d = symexp_op(NULL, ldata->d, SYMEXP_SUB);

//         MTBDD res = mtbdd_makeleaf(ltype_symb_expr_id, (uint64_t) &res_data);
//         return res;
//     }

//     return mtbdd_invalid; // Recurse deeper
// }



qBDD my_mtbdd_symb_neg_i(qBDD t) {
    return unary_apply(t, mtbdd_symb_neg_i);
}

// TASK_IMPL_2(MTBDD, mtbdd_symb_coef_rot1, MTBDD, t, size_t, x)
// {
//     (void)x; // extra parameter needed for task - not needed

//     // Partial function check
//     if (t == mtbdd_false) return mtbdd_false;

//     // Compute coeficient rotation if mtbdd is a leaf
//     if (mtbdd_isleaf(t)) {
//         sl_val_t *ldata = (sl_val_t*) mtbdd_getvalue(t);

//         sl_val_t res_data;
//         res_data.a = symexp_op(NULL, ldata->d, SYMEXP_SUB);
//         res_data.b = ldata->a;
//         res_data.c = ldata->b;
//         res_data.d = ldata->c;

//         MTBDD res = mtbdd_makeleaf(ltype_symb_expr_id, (uint64_t) &res_data);
//         return res;
//     }

//     return mtbdd_invalid; // Recurse deeper
// }



qBDD my_mtbdd_symb_coef_rot1_i(qBDD t) {
    return unary_apply(t, mtbdd_symb_coef_rot1_i);
}

qBDD my_mtbdd_symb_coef_rot1_i_inv(qBDD t) {
    return unary_apply(t, mtbdd_symb_coef_rot1_i_inv);
}

// TASK_IMPL_2(MTBDD, mtbdd_symb_coef_rot2, MTBDD, t, size_t, x)
// {
//     (void)x; // extra parameter needed for task - not needed

//     // Partial function check
//     if (t == mtbdd_false) return mtbdd_false;

//     // Compute coeficient rotation if mtbdd is a leaf
//     if (mtbdd_isleaf(t)) {
//         sl_val_t *ldata = (sl_val_t*) mtbdd_getvalue(t);

//         sl_val_t res_data;
//         res_data.a = symexp_op(NULL, ldata->c, SYMEXP_SUB);
//         res_data.b = symexp_op(NULL, ldata->d, SYMEXP_SUB);
//         res_data.c = ldata->a;
//         res_data.d = ldata->b;

//         MTBDD res = mtbdd_makeleaf(ltype_symb_expr_id, (uint64_t) &res_data);
//         return res;
//     }

//     return mtbdd_invalid; // Recurse deeper
// }


qBDD my_mtbdd_symb_coef_rot2_i(qBDD t) {
    return unary_apply(t, mtbdd_symb_coef_rot2_i);
}

/* end of "mtbdd_symb_val.c" */