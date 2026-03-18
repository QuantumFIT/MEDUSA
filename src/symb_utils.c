#include <string.h>
#include "symb_utils.h"
#include "mtbdd_symb_val.h"
#include "sylvan_int.h" // for cache_next_opid()
#include "error.h"

/// Opid for mtbdd_symb_refine (needed for mtbdd_applyp)
static uint64_t apply_mtbdd_symb_refine_id;

/// Coefficient for resizing refine data's update array
#define UPDATE_RESIZE_COEF 2

// =================
// Refine internal:
// =================

rdata_t* rdata_create(vmap_t *vm)
{
    rdata_t *rd = my_malloc(sizeof(rdata_t));

    rd->ref = my_malloc(sizeof(ref_list_t));
    rd->ref->first = NULL;
    rd->ref->cur = NULL;

    rd->upd = my_malloc(sizeof(upd_list_t));
    rd->upd->size = vm->msize;
    rd->upd->arr = my_malloc(sizeof(upd_elem_t) * rd->upd->size);
    memset(rd->upd->arr, 0, sizeof(upd_elem_t) * rd->upd->size);

    rd->vm = vm;
    return rd;
}

void rdata_delete(rdata_t *rd)
{
    ref_elem_t *temp;
    while (rd->ref->first != NULL) {
        temp = rd->ref->first;
        rd->ref->first = temp->next;
        free(temp);
    }

    free(rd->upd->arr); // stores only stree*, trees are freed when the value MTBDD is deleted
    free(rd->upd);
    free(rd);
}

/**
 * Adds new variable and its value to the update array, refine list and vmap array
 */
static void rdata_add(rdata_t *rd, vars_t old, vars_t new, symexp_list_t *data)
{
    ref_elem_t *new_ref_elem = my_malloc(sizeof(ref_elem_t));
    new_ref_elem->old = old;
    new_ref_elem->new = new;
    new_ref_elem->next = rd->ref->first;
    rd->ref->first = new_ref_elem;
    
    if (new >= rd->upd->size) {
        // resize
        rd->upd->size *= UPDATE_RESIZE_COEF;
        rd->upd->arr = my_realloc(rd->upd->arr, sizeof(upd_elem_t) * (rd->upd->size));
    }
    rd->upd->arr[new] = data;

    vmap_add(rd->vm, old);
}

static void rdata_ref_first(rdata_t *rd)
{
    rd->ref->cur =  rd->ref->first;
}

static void rdata_ref_next(rdata_t *rd)
{
    rd->ref->cur = (rd->ref->cur)? rd->ref->cur = rd->ref->cur->next : NULL;
}

/**
 * Returns the refined variable for the given data
 */
static vars_t refine_var_check(vars_t var, symexp_list_t *data, rdata_t *rd)
{
    if (rd->upd->arr[var] == NULL) {
        rd->upd->arr[var] = data;
        return var;
    }

    if ((data == SYMEXP_NULL && rd->upd->arr[var] == SYMEXP_NULL) ||
        (data != SYMEXP_NULL && symexp_cmp(data, rd->upd->arr[var]))) {
        return var;
    }

    // check if the same update already doesn't exist
    rdata_ref_first(rd);
    while(rd->ref->cur) {
        if ((rd->ref->cur->old == var) && 
            ((data == SYMEXP_NULL && rd->upd->arr[rd->ref->cur->new] == SYMEXP_NULL) ||
             (data != SYMEXP_NULL && symexp_cmp(data, rd->upd->arr[rd->ref->cur->new]))
            )) {
            return rd->ref->cur->new;
        }
        rdata_ref_next(rd);
    }

    vars_t new = rd->vm->next_var; // next_var is incremented during rdata_add when adding into vmap
    rdata_add(rd, var, new, data);
    return new;
}

TASK_DECL_3(MTBDD, mtbdd_symb_refine, MTBDD*, MTBDD*, size_t);
TASK_IMPL_3(MTBDD, mtbdd_symb_refine, MTBDD*, p_map, MTBDD*, p_val, size_t, rd_raw)
{
    MTBDD map = *p_map; // ptr needed because of 'mtbdd_applyp'
    MTBDD val = *p_val;
    rdata_t *rd = (rdata_t*) rd_raw; // 'mtbdd_applyp' accepts only size_t parameter

    if (mtbdd_isleaf(map) && mtbdd_isleaf(val)) {
        sl_map_t *mdata = (sl_map_t*) mtbdd_getvalue(map);
        vars_t new_a, new_b, new_c, new_d;

        if (val == mtbdd_false) {
            new_a = refine_var_check(mdata->va, SYMEXP_NULL, rd);
            new_b = refine_var_check(mdata->vb, SYMEXP_NULL, rd);
            new_c = refine_var_check(mdata->vc, SYMEXP_NULL, rd);
            new_d = refine_var_check(mdata->vd, SYMEXP_NULL, rd);
        }
        else {
            sl_val_t *vdata = (sl_val_t*) mtbdd_getvalue(val);
            new_a = refine_var_check(mdata->va, vdata->a, rd);
            new_b = refine_var_check(mdata->vb, vdata->b, rd);
            new_c = refine_var_check(mdata->vc, vdata->c, rd);
            new_d = refine_var_check(mdata->vd, vdata->d, rd);
        }

        if (new_a == mdata->va && new_b == mdata->vb && new_c == mdata->vc && new_d == mdata->vd) {
            return map;
        }

        // new symbolic var needed
        sl_map_t new_data;
        new_data.va = new_a;
        new_data.vb = new_b;
        new_data.vc = new_c;
        new_data.vd = new_d;

        MTBDD res = mtbdd_makeleaf(ltype_symb_map_id, (uint64_t) &new_data);
        return res;
    }

    return mtbdd_invalid; // Recurse deeper
}
/**
 * Computes refine on the symbolic MTBDD pair
 * 
 * @param p_map pointer to a symbolic map MTBDD
 * 
 * @param p_val pointer to a symbolic value MTBDD
 * 
 * @param rdata ptr to structure cointaining all the data needed for refine (update, refine and map data structures)
 * 
 * @param opid opid needed for the Sylvan's apply
 * 
 */
#define my_mtbdd_symb_refine(p_map, p_val, rdata) \
        mtbdd_applyp(p_map, p_val, (size_t)rdata, TASK(mtbdd_symb_refine), apply_mtbdd_symb_refine_id)

/**
 * Evaluates the given variable according to the rdata expression and the map, saves the value into new_map
 */
static void eval_var(size_t var, rdata_t *rdata, coef_t* map, coef_t* new_map)
{
    symexp_list_t *expr = (symexp_list_t*)rdata->upd->arr[var];
    mpz_set_ui(new_map[var], 0);

    if (expr != SYMEXP_NULL) {
        coef_t imm_res;
        mpz_init(imm_res);

        if (expr != NULL) {
            symexp_list_first(expr);
            while(expr->active) {
                mpz_set(imm_res, map[expr->active->data->var]);
                mpz_mul(imm_res, imm_res, expr->active->data->coef);
                mpz_add(new_map[var], new_map[var], imm_res);
                symexp_list_next(expr);
            }
        }
        mpz_clear(imm_res);
    }
}

// ========================================

void init_sylvan_symb()
{
    init_my_leaf_symb_val();
    init_my_leaf_symb_map();
    apply_mtbdd_symb_refine_id = cache_next_opid();
}

void symb_init(MTBDD *circ, mtbdd_symb_t *symbc)
{
    size_t msize = 4 * (mtbdd_leafcount(*circ)); // multiplied because one var is needed for every coefficient
                                                 // !doesn't count F ... needs to be allocated manually
    vmap_init(&(symbc->vm), msize);
    symbc->is_reduced = true;  // initially tries to reduce symb. leaves into F
    symbc->is_refined = false;

    symbc->map = my_mtbdd_to_symb_map(*circ, symbc->vm);
    mtbdd_protect(&(symbc->map));
    symbc->val = my_mtbdd_map_to_symb_val(symbc->map, symbc->vm->map, symbc->is_reduced);
    mtbdd_protect(&(symbc->val));

    mpz_init(cs_k);
}

/**
 * Returns true if no errors will occur during evaluation (for value MTBDDs with reduced 0 leaves)
 */
static bool can_be_reduced(mtbdd_symb_t *symbc, rdata_t *rdata)
{
    bool is_correct = true;
    bool is_zero[rdata->vm->next_var];
    for (int i = 0; i < rdata->vm->next_var; i++) {
        is_zero[i] = false;
    }

    // The whole leaf behaves the same way, so checking every 4th variable is sufficient
    for (int i = 0; i < rdata->vm->next_var; i += 4) {
        // If leaf is initially 0:
        if (!mpz_sgn(rdata->vm->map[i]) && !mpz_sgn(rdata->vm->map[i+1]) 
            && !mpz_sgn(rdata->vm->map[i+2]) && !mpz_sgn(rdata->vm->map[i+3])) {
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
            //TODO: verify if checking first var instead of inclusion is correct
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

bool symb_refine(mtbdd_symb_t *symbc, rdata_t *rdata)
{
    MTBDD refined = my_mtbdd_symb_refine(symbc->map, symbc->val, rdata);
    mtbdd_protect(&refined);
    bool is_finished = (rdata->ref->first == NULL);

    // Check if the MTBDD can truly be reduced (= all 0 leafs remain unchanged)
    if (!symbc->is_refined && symbc->is_reduced) {
        is_finished = can_be_reduced(symbc, rdata) && is_finished; // In this order so can_be_reduced() is always called
        symbc->is_refined = true; // Reduce errors would already appear, so don't check again
                                  // (can be in this if because first refine is always reduced)
    }

    if (!is_finished) {
        // Reset symbolic simulation
        cs_k_reset();
        symbc->map = refined;
        symbc->val = my_mtbdd_map_to_symb_val(refined, symbc->vm->map, symbc->is_reduced);
    }

    mtbdd_unprotect(&refined);
    return is_finished;
}

void symb_eval(MTBDD *circ,  mtbdd_symb_t *symbc, uint64_t iters, rdata_t *rdata)
{
    coef_t *new_map = my_malloc(sizeof(coef_t) * symbc->vm->msize);
    for (int i = 0; i < symbc->vm->msize; i++) {
        mpz_init(new_map[i]);
    }
    coef_t *temp_map;
    for (uint64_t i = 0; i < iters; i++) {
        // update new_map
        for (int i = 0; i < symbc->vm->next_var; i++) {
            eval_var(i, rdata, symbc->vm->map, new_map);
        }

        // swap maps
        temp_map = symbc->vm->map;
        symbc->vm->map = new_map;
        new_map = temp_map;
    }

    *circ = my_mtbdd_from_symb(symbc->map, symbc->vm->map);

    mpz_mul_ui(cs_k, cs_k, (unsigned long)iters);
    mpz_add(c_k, c_k, cs_k);

    // dealloc aux variable
    for (int i = 0; i < symbc->vm->msize; i++) {
        mpz_clear(new_map[i]);
    }
    free(new_map);

    // Symbolic clean up
    vmap_delete(symbc->vm);
    mtbdd_unprotect(&(symbc->map));
    mtbdd_unprotect(&(symbc->val));
    mpz_clear(cs_k);
    sylvan_gc(); // Clears both the operation cache and the node cache, needed as some expressions may reappear again
}

void print_update(const char *filename, upd_list_t *upd)
{
    FILE *out = fopen(filename, "w");
    if (out == NULL) {
        error_exit("Cannot open the update output file.\n");
    }

    for (int i = 0; i < upd->size; i++) {
        symexp_list_t *expr = (symexp_list_t*)upd->arr[i];
        if (expr == NULL) { // stop when unused variables are reached
            break;
        }
        fprintf(out, "v[%d] = %s\n", i, symexp_to_str(expr));
    }

    fclose(out);
}

/* end of "symb_utils.c" */