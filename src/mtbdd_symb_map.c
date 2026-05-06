#include <string.h>
#include "mtbdd_symb_map.h"
#include "hash.h"
#include "error.h"
#include "interface.h"
#include "mtbdd.h"

/// String length limit used for mtbdd_to_str
#define MAX_SYMB_MAP_LEAF_STR_LEN 1000
/// Realloc step for vmap
#define REALLOC_COEF 2

/// leaf type id for symbolic representation
uint32_t ltype_symb_map_id;

void vmap_init(vmap_t **vm, size_t size)
{
    *vm = my_malloc(sizeof(vmap_t));
    (*vm)->msize = size;
    (*vm)->map = my_malloc(sizeof(coef_t) * size); // REDO
    (*vm)->next_var = 0;
    (*vm)->mappings = NULL;
}

void vmap_add(vmap_t *vm, vars_t old)
{
    if (vm->next_var >= vm->msize) {
        int old_msize = vm->msize;
        vm->msize *= REALLOC_COEF;
        vm->map = my_realloc(vm->map, sizeof(coef_t) * vm->msize);  // REDO

        // Initialize all new mpz_t slots
        for (int i = old_msize; i < vm->msize; i++) {
            init_generic(vm->map[i]);
        }
    }

    init_set_generic(vm->map[vm->next_var], vm->map[old]);
    vm->next_var++;
}


void vmap_clear(vmap_t *vm)
{
    for (size_t i = 0; i < vm->next_var; i++) {
        clear_generic(vm->map[i]);
    }
    free(vm->map);
    vm->msize = 0;
}

void vmap_delete(vmap_t *vm)
{
    vmap_clear(vm);
    free(vm);
}

qBDD vmap_lookup(vmap_t *m, qBDD a) {
    for (mapping_entry_t *e = m->mappings; e; e = e->next) {
        if (e->original == a) return e->mapped;
    }
    return 0;
}

void vmap_insert(vmap_t *m, qBDD orig, qBDD mapped) {
    mapping_entry_t *e = malloc(sizeof(mapping_entry_t));
    if (e == NULL) printf("Malloc failed\n");
    e->original = orig;
    e->mapped = mapped;
    e->next = m->mappings;
    m->mappings = e;
}

/* SETUP */
// void init_my_leaf_symb_map()
// {
//     ltype_symb_map_id = sylvan_mt_create_type();

//     sylvan_mt_set_create(ltype_symb_map_id, my_leaf_symb_m_create);
//     sylvan_mt_set_destroy(ltype_symb_map_id, my_leaf_symb_m_destroy);
//     sylvan_mt_set_equals(ltype_symb_map_id, my_leaf_symb_m_equals);
//     sylvan_mt_set_to_str(ltype_symb_map_id, my_leaf_symb_m_to_str);
//     sylvan_mt_set_hash(ltype_symb_map_id, my_leaf_symb_m_hash);
// }

/* CUSTOM HANDLES */
// void my_leaf_symb_m_create(uint64_t *ldata_p_raw)
// {
//     sl_map_t** ldata_p = (sl_map_t**)ldata_p_raw; // Leaf data type is uint64_t, we store there ptr to our actual data
    
//     sl_map_t* orig_ldata = *ldata_p;
//     sl_map_t* new_ldata = (sl_map_t*)my_malloc(sizeof(sl_map_t));

//     new_ldata->va = orig_ldata->va;
//     new_ldata->vb = orig_ldata->vb;
//     new_ldata->vc = orig_ldata->vc;
//     new_ldata->vd = orig_ldata->vd;

//     *ldata_p = new_ldata;
// }

// void my_leaf_symb_m_destroy(uint64_t ldata)
// {
//     sl_map_t *data_p = (sl_map_t*) ldata; // Data in leaf = pointer to my data
//     free(data_p);
// }

// int my_leaf_symb_m_equals(const uint64_t ldata_a_raw, const uint64_t ldata_b_raw)
// {
//     sl_map_t *ldata_a = (sl_map_t*) ldata_a_raw;
//     sl_map_t *ldata_b = (sl_map_t*) ldata_b_raw;

//     return (ldata_a->va == ldata_b->va) && (ldata_a->vb == ldata_b->vb) && (ldata_a->vc == ldata_b->vc) \
//            && (ldata_a->vd == ldata_b->vd);
// }

// char* my_leaf_symb_m_to_str(int complemented, uint64_t ldata_raw, char *sylvan_buf, size_t sylvan_bufsize)
// {
//     (void) complemented;
//     sl_map_t *ldata = (sl_map_t*) ldata_raw;

//     char ldata_string[MAX_SYMB_MAP_LEAF_STR_LEN] = {0};
    
//     int chars_written = snprintf(ldata_string, MAX_SYMB_MAP_LEAF_STR_LEN, "(v[%ld], v[%ld]ω, v[%ld]ω², v[%ld]ω³)", \
//                                  ldata->va, ldata->vb, ldata->vc, ldata->vd);
//     // Was string truncated?
//     if (chars_written >= MAX_SYMB_MAP_LEAF_STR_LEN) {
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

// uint64_t my_leaf_symb_m_hash(const uint64_t ldata_raw, const uint64_t seed)
// {
//     sl_map_t *ldata = (sl_map_t*) ldata_raw;

//     uint64_t val = seed;
//     val = MY_HASH_COMB(val, ldata->va);
//     val = MY_HASH_COMB(val, ldata->vb);
//     val = MY_HASH_COMB(val, ldata->vc);
//     val = MY_HASH_COMB(val, ldata->vd);

//     return val;
// }

/* CUSTOM MTBDD OPERATIONS */
// TASK_IMPL_2(MTBDD, mtbdd_to_symb_map, MTBDD, a, size_t, raw_m)
// {
//     if (a != mtbdd_false && !mtbdd_isleaf(a)) {
//         return mtbdd_invalid; // Recurse deeper
//     }

//     vmap_t* m = (vmap_t*) raw_m;
//     vars_t var_a = m->next_var;
//     vars_t var_b = m->next_var + 1;
//     vars_t var_c = m->next_var + 2;
//     vars_t var_d = m->next_var + 3;

//     // Partial function check
//     if (a == mtbdd_false) {
//         // initial vmap size does not count with 'mtbdd_false' leaves, so vmap needs to be resized
//         m->map = my_realloc(m->map, sizeof(coef_t) * (m->msize + 4));
//         m->msize += 4;
//         mpz_inits(m->map[var_a], m->map[var_b], m->map[var_c], m->map[var_d], 0);
//     }
//     else if (mtbdd_isleaf(a)) {
//         cnum *orig_data = (cnum*) mtbdd_getvalue(a);
//         mpz_init_set(m->map[var_a], orig_data->a);
//         mpz_init_set(m->map[var_b], orig_data->b);
//         mpz_init_set(m->map[var_c], orig_data->c);
//         mpz_init_set(m->map[var_d], orig_data->d);
//     }

//     sl_map_t new_data;
//     new_data.va = var_a;
//     new_data.vb = var_b;
//     new_data.vc = var_c;
//     new_data.vd = var_d;

//     MTBDD res = mtbdd_makeleaf(ltype_symb_map_id, (uint64_t) &new_data);
//     m->next_var += 4;

//     return res;
// }

// creating different type of terminal - has to return node
// as buddy uapply takes type from the previous

 // REDO


qBDD my_mtbdd_to_symb_map_i(qBDD t, size_t m) {
    return unary_apply_guarded(t, mtbdd_to_symb_map_i, m);
}

/* end of "mtbdd_symb_map.c" */