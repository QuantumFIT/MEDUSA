#include <string.h>
#include "symb_utils.h"
#include "mtbdd_symb_val.h"
#include "sylvan_int.h" // for cache_next_opid()
#include "error.h"
#include "fmpz.h"       // fmpz includes need to be after gmp includes
#include "fmpz_vec.h"
#include "fmpz_mat.h"

/// Opid for mtbdd_symb_refine (needed for mtbdd_applyp)
static uint64_t apply_mtbdd_symb_refine_id;

/// Coefficient for resizing refine data's update array
#define UPDATE_RESIZE_COEF 2

//TODO: Change to FLINT memory limit, also make it adjustable as a program parameter
/// Max allowed matrix power in evaluation
#define POW_LIMIT (UWORD(1) << 25)

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

// =====================
// Evaluation internal:
// =====================

/// State of calculation of matrix power using repeated squaring
/// Necessary to finish evaluation when limit is hit during repeated squaring
typedef struct pow_state {
    /// Matrix set to the value of the result (aliasing with the input matrix is not allowed)
    fmpz_mat_t mtx_res;
    /// Current max square power of input matrix
    fmpz_mat_t mtx_p;
    /// Remaining exponent to be calculated
    ulong rem_exp;
} pow_state_t;

/**
 * Initializes FMPZ matrix according to rdata symexp values for all variables
 */
static void init_upd_matrix(fmpz_mat_t mtx, vars_t nvars, rdata_t *rdata)
{
    // 
    symexp_list_t *expr;

    fmpz_mat_zero(mtx);
    for (vars_t i = 0; i < nvars; i++) {
        // Convert symexp for var i to nonzero matrix row indices
        expr = (symexp_list_t*)rdata->upd->arr[i];

        if (expr != SYMEXP_NULL && expr != NULL) {
            symexp_list_first(expr);
            while(expr->active) {
                fmpz_set_mpz(fmpz_mat_entry(mtx, i, expr->active->data->var), expr->active->data->coef);
                symexp_list_next(expr);
            }
        }
    }
}

/**
 * Sets values of the vector to the current variable values according to map
 */
static void init_state_vector(fmpz* state, vars_t nvars, coef_t* map)
{
    for (vars_t i = 0; i < nvars; i++)
        fmpz_set_mpz(&(state[i]), map[i]);
}

/**
 * Sets values of map to the current variable values according to the vector
 */
static void update_map_from_vec(fmpz* state, vars_t nvars, coef_t* map)
{
    for (vars_t i = 0; i < nvars; i++)
        fmpz_get_mpz(map[i], &(state[i]));
}

/**
 * TODO: Get current FLINT memory usage during matrix power.
 * Calculate as memory currently used by A, B, P + estimate P^2 < limit.
 * Either use exact values (system calls, not every iteration) or estimate.
 * For estimate, good initial value for each matrix is probably:
 *      number of elements of A * largest element of A
 * using fmpz_bits(). This approach would need to additionally track B and P
 * size estimates in the custom_mat_pow().
 */
// static size_t flint_mem_check()
// {
// }

/**
 * Computes power of a square matrix A. Utilizes repeated squaring while the matrix entries
 * are not too large.
 * 
 * The size limit is necessary to avoid OOM by keeping a single copy. Result
 * is stored in the context, in case limit is encountered, the computation must be finished off
 * with linear multiplication using the intermediate result and the current max power. This is signalized
 * by nonzero remaining exponent in the updated context. The context must be initialized and cleaned
 * outside of this function.
 * 
 * Based on the iterative pseudocode described on Wikipedia:
 * https://en.wikipedia.org/wiki/Exponentiation_by_squaring
 * 
 * @param ctx initialized context for repeated squaring
 * (contains the result, saving the whole context necessary for linear multiplication switch)
 * 
 * @param A base matrix (must be square)
 * 
 * @param exp power exponent
 *
 */
static void custom_mat_pow(pow_state_t *ctx, const fmpz_mat_t A, ulong exp)
{
    // Current power
    ulong p_pow = 1;
    fmpz_mat_set(ctx->mtx_p, A);

    // Set result for exp == 0
    fmpz_mat_one(ctx->mtx_res);

    // Repeated squaring LSB -> MSB
    while (exp > 0) {
        if (exp & UWORD(1)) fmpz_mat_mul(ctx->mtx_res, ctx->mtx_res, ctx->mtx_p);

        // If over limit, stop and finish multiplication linearly
        if ((p_pow << 1) > POW_LIMIT) break;

        // Skip squaring for MSB: won't be used
        if(exp > 1) {
            fmpz_mat_sqr(ctx->mtx_p, ctx->mtx_p);
            p_pow <<= 1;
        }
        exp >>= 1;
    }
    ctx->rem_exp = exp;
}

/**
 * Get result state vector with final variable (amplitude) values.
 * 
 * @param res final state vector
 * 
 * @param state initial state vector
 * 
 * @param mtx_upd single loop iteration matrix
 * 
 * @param nvars number of variables (amplitudes)
 * 
 * @param iters number of loop iterations
 */
static void rs_evaluate(fmpz *res, fmpz *state, fmpz_mat_t mtx_upd, vars_t nvars, ulong iters)
{
    // Create initial context for matrix power
    pow_state_t pow_ctx;
    fmpz_mat_init(pow_ctx.mtx_res, nvars, nvars);
    fmpz_mat_init(pow_ctx.mtx_p, nvars, nvars);

    // Repeated squaring
    custom_mat_pow(&pow_ctx, mtx_upd, iters);

    fmpz_mat_mul_fmpz_vec(res, pow_ctx.mtx_res, state, nvars);
    // If not finished, proceed with linear multiplication for remaining powers
    if (pow_ctx.rem_exp > 0) {
        // Get number of necessary multiplications by max achieved square power
        ulong n_mult = 0;
        pow_ctx.rem_exp >>= 1; // If LSB is 1, power is already contained in res
        ulong current_pow = 2; // Initially 1, also shifted once with rem_exp

        while (pow_ctx.rem_exp > 0) {
            if (pow_ctx.rem_exp & UWORD(1)) n_mult += current_pow;
            current_pow <<= 1;
            pow_ctx.rem_exp >>= 1;
        }

        // Get P^n_mult * res
        fmpz* tmp_res = _fmpz_vec_init(nvars);
        for (; n_mult > 0; n_mult--) {
            fmpz_mat_mul_fmpz_vec(tmp_res, pow_ctx.mtx_p, res, nvars);

            // Swap
            fmpz *swap = res;
            res = tmp_res;
            tmp_res = swap;
        }
    }

    // Clean up pow context
    fmpz_mat_clear(pow_ctx.mtx_res);
    fmpz_mat_clear(pow_ctx.mtx_p);
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
    vars_t nvars = symbc->vm->next_var;

    // Init single loop iteration matrix
    fmpz_mat_t mtx_upd;
    fmpz_mat_init(mtx_upd, nvars, nvars);
    init_upd_matrix(mtx_upd, nvars, rdata);
    
    // Init current and final state vector
    fmpz* state = _fmpz_vec_init(nvars);
    fmpz* res = _fmpz_vec_init(nvars);
    init_state_vector(state, nvars, symbc->vm->map);

    // Repeated squaring to get final vector
    rs_evaluate(res, state, mtx_upd, nvars, iters);

    // Update mtbdd
    update_map_from_vec(res, nvars, symbc->vm->map);
    *circ = my_mtbdd_from_symb(symbc->map, symbc->vm->map);

    // Update k
    mpz_mul_ui(cs_k, cs_k, (unsigned long)iters);
    mpz_add(c_k, c_k, cs_k);

    // Fmpz clean up
    fmpz_mat_clear(mtx_upd);
    _fmpz_vec_clear(state, nvars);
    _fmpz_vec_clear(res, nvars);

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