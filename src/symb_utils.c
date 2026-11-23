#include <string.h>
#include "symb_utils.h"
#include "mtbdd_symb_val.h"
#include "sylvan_int.h" // for cache_next_opid()
#include "error.h"
#include "fmpz.h"       // fmpz includes need to be after gmp includes
#include "fmpz_vec.h"
#include "fmpz_mat.h"
#include "longlong.h"   // FLINT_BIT_COUNT

/// Opid for mtbdd_symb_refine (needed for mtbdd_applyp)
static uint64_t apply_mtbdd_symb_refine_id;

/// Coefficient for resizing refine data's update array
#define UPDATE_RESIZE_COEF 2

//TODO: Make the memory budget adjustable as a program parameter
/// Memory budget for repeated squaring in bytes (4 GiB)
#define POW_MEM_BUDGET ((size_t)4 * 1024 * 1024 * 1024)

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
///
/// Used for finishing evaluation when limit is hit during repeated squaring
/// and for checking if limit is hit
typedef struct pow_state {
    /// Matrix set to the value of A^pow (aliasing with the input matrix A is not allowed)
    fmpz_mat_t mtx_res;
    /// Scratch matrix for squaring
    fmpz_mat_t mtx_p;
    /// Power of the current result of repeated squaring
    ulong pow;
    /// Bytes used by the base matrix A (constant during the power)
    size_t a_bytes;
    /// Memory budget for the computation in bytes (soft limit)
    size_t budget;
} pow_state_t;

/**
 * Returns the number of bytes used by an fmpz entry (on top of the base word) with the given bit size.
 * (0 if the value fits into the word, otherwise the mpz struct and its limbs)
 */
static inline size_t mpz_bytes(flint_bitcnt_t bits)
{
    if (bits <= SMALL_FMPZ_BITCOUNT_MAX)
        return 0;
    // header + ceil(bits/64) * limbs, instead of ceil() uses (bits + 63) / 64 to round up
    return sizeof(__mpz_struct) + ((bits + FLINT_BITS - 1) / FLINT_BITS) * sizeof(mp_limb_t);
}

/**
 * Returns an estimated byte size of an n x n matrix with nnz nonzero entries of the given bit size.
 */
static inline size_t mat_bytes_est(slong n, slong nnz, flint_bitcnt_t bits)
{
    return (size_t) n * n * sizeof(fmpz) + (size_t) nnz * mpz_bytes(bits);
}

/**
 * Initializes the context for repeated squaring of A, including the memory budget.
 * Matrices mtx_res and mtx_p must be cleared by the caller.
 */
static void pow_state_init(pow_state_t *ctx, const fmpz_mat_t A)
{
    slong n = fmpz_mat_nrows(A);

    fmpz_mat_init(ctx->mtx_res, n, n);
    fmpz_mat_init(ctx->mtx_p, n, n);
    ctx->pow = 0;

    // Memory used by A
    ctx->a_bytes = n * n * sizeof(fmpz); // Zero entry size
    for (slong i = 0; i < n; i++) {
        for (slong j = 0; j < n; j++)
            ctx->a_bytes += mpz_bytes(fmpz_bits(fmpz_mat_entry(A, i, j))); // Nonzero entry size
    }

    ctx->budget = POW_MEM_BUDGET;
}

/**
 * Initializes FMPZ matrix according to rdata symexp values for all variables
 */
static void init_upd_matrix(fmpz_mat_t mtx, vars_t nvars, rdata_t *rdata)
{
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
 * Decides whether repeated squaring may continue to the next power.
 *
 * The next step computes mtx_res^2 (and possibly mtx_res^2 * A), so the entries roughly
 * double in size. This can lead to OOM for big integers.
 * This is a soft limit (see also the remainder in rs_evaluate()).
 *
 * Estimates the peak memory of the next step from the current mtx_res (R). Entries of
 * P = R^2 have at most 2 * max_bits(R) + ceil(log2(max row nnz of R)) bits (a product adds
 * the bit sizes, a sum of m terms adds at most ceil(log2 m) carry bits). R' = P * A is
 * estimated as the size of P, since A's small entries add only a few bits.
 * Peak memory:
 *  - next bit 0:  A + R + P  + temporaries of the squaring
 *  - next bit 1:  A + P + R' + temporaries of the multiplication (R' overwrites R)
 *
 * @param ctx current state of repeated squaring computation
 *
 * @param next_pow power that mtx_res would hold after the next step
 *
 * @return true if the next step may be performed
 */
static bool pow_can_continue(const pow_state_t *ctx, ulong next_pow)
{
    slong n = fmpz_mat_nrows(ctx->mtx_res);
    slong nnz = 0;
    slong max_row_nnz = 0;
    flint_bitcnt_t max_bits = 0;
    size_t r_bytes = (size_t) n * n * sizeof(fmpz);

    // Scan R instead of precomputing the transitive closure of A: near the limit the
    // nonzero pattern of A^pow has stopped changing, so nnz(R^2) ~ nnz(R)
    for (slong i = 0; i < n; i++) {
        slong row_nnz = 0;
        for (slong j = 0; j < n; j++) {
            const fmpz *e = fmpz_mat_entry(ctx->mtx_res, i, j);
            if (fmpz_is_zero(e))
                continue;

            flint_bitcnt_t b = fmpz_bits(e);
            row_nnz++;
            if (b > max_bits) max_bits = b;
            r_bytes += mpz_bytes(b);
        }
        nnz += row_nnz;
        if (row_nnz > max_row_nnz) max_row_nnz = row_nnz;
    }

    // Predict max size of P's entry
    flint_bitcnt_t carry_bits = (max_row_nnz <= 1) ? 0 : FLINT_BIT_COUNT(max_row_nnz - 1);
    flint_bitcnt_t p_bits = 2 * max_bits + carry_bits;
    size_t p_bytes = mat_bytes_est(n, nnz, p_bits);

    // Temporaries: for large entries FLINT multiplies via FFT, which transforms all n^2
    // entries (zeros included), so it needs ~n^2 entries of P's size regardless of sparsity
    // Note: for small entries, this overestimates the real RSS (however, these are not near the limit)
    size_t tmp_bytes = (size_t) n * n * mpz_bytes(p_bits);

    size_t peak;
    if (next_pow & UWORD(1)) {
        // R' equals approx. P (A is small)
        peak = ctx->a_bytes + 2 * p_bytes + tmp_bytes;
    }
    else {
        peak = ctx->a_bytes + r_bytes + p_bytes + tmp_bytes;
    }

    return peak <= ctx->budget;
}

/**
 * Computes power of a square matrix A using repeated squaring while the matrix entries
 * are not too large.
 * 
 * Processes exponent bits from MSB to LSB. This direction avoids big matrix * big matrix
 * multiplications when '1' bit is encounntered in exponent.
 * 
 * The size limit is necessary to avoid OOM. Result is stored in the context.
 * If stopped early (limit is encountered), ctx->pow < exp and the computation must be
 * finished off with multiplication using the updated context.
 * The context must be initialized and cleaned outside of this function.
 * 
 * Based on fmpz_mat_pow from FLINT library by Fredrik Johansson.
 * 
 * @param ctx initialized computational context for repeated squaring (necessary for early stop)
 * 
 * @param A base matrix (must be square)
 * 
 * @param exp power exponent
 *
 */
static void custom_mat_pow(pow_state_t *ctx, const fmpz_mat_t A, ulong exp)
{
    if (exp == 0) {
        fmpz_mat_one(ctx->mtx_res);
        ctx->pow = 0;
        return;
    }

    ulong next_pow;
    
    // MSB of exp is always 1: start with A^1
    fmpz_mat_set(ctx->mtx_res, A);
    ctx->pow = 1;

    // Repeated squaring MSB -> LSB: square, then multiply by A if the bit is set
    for (slong i = FLINT_BIT_COUNT(exp) - 2; i >= 0; i--) {
        // Loop starts from i = index (-1) of second MSB (-1)

        next_pow = exp >> i;
        // If over limit, stop and finish multiplication linearly
        if (!pow_can_continue(ctx, next_pow)) break;
        
        fmpz_mat_sqr(ctx->mtx_p, ctx->mtx_res);
        if (next_pow & UWORD(1)) {
            fmpz_mat_mul(ctx->mtx_res, ctx->mtx_p, A);
        }
        else {
            // Square into P and then pointer swap, since squaring into mtx_res
            // would allocate a hidden temporary on every call
            fmpz_mat_swap(ctx->mtx_res, ctx->mtx_p);
        }
        ctx->pow = next_pow;
    }
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
 * @param iters number of loop iterations
 */
static void rs_evaluate(fmpz *res, fmpz *state, fmpz_mat_t mtx_upd, ulong iters)
{
    vars_t nvars = fmpz_mat_nrows(mtx_upd);
    // Create initial context for matrix power computation
    pow_state_t pow_ctx;
    pow_state_init(&pow_ctx, mtx_upd);

    // Repeated squaring
    custom_mat_pow(&pow_ctx, mtx_upd, iters);
    // Scratch matrix for squaring not needed for linear part
    fmpz_mat_clear(pow_ctx.mtx_p);

    // Check if finished within budget
    if (pow_ctx.pow == iters) {
        fmpz_mat_mul_fmpz_vec(res, pow_ctx.mtx_res, state, nvars);
        fmpz_mat_clear(pow_ctx.mtx_res);
        return;
    }

    // Not finished: iters = pow * n_mult + n_rem, n_rem < pow
    ulong n_mult = iters / pow_ctx.pow;
    ulong n_rem = iters % pow_ctx.pow;
    fmpz *tmp_res = _fmpz_vec_init(nvars);

    // res' = mtx_res^n_mult * state (powers of A commute)
    _fmpz_vec_set(res, state, nvars);
    for (; n_mult > 0; n_mult--) {
        fmpz_mat_mul_fmpz_vec(tmp_res, pow_ctx.mtx_res, res, nvars);
        _fmpz_vec_swap(res, tmp_res, nvars);
    }
    // mtx_res is not needed anymore
    fmpz_mat_clear(pow_ctx.mtx_res);

    // Remainder: res = A^n_rem * res'
    // fmpz_mat_pow() has no limit and can peak up to ~1.33x the budget approved in custom_mat_pow()
    // (its last step may multiply by A while mtx_res's did not). However, the result vectors are
    // not covered by the budget either (soft limit).
    if (n_rem > 0) {
        fmpz_mat_t mtx_rem;
        fmpz_mat_init(mtx_rem, nvars, nvars);
        fmpz_mat_pow(mtx_rem, mtx_upd, n_rem);

        fmpz_mat_mul_fmpz_vec(tmp_res, mtx_rem, res, nvars);
        _fmpz_vec_swap(res, tmp_res, nvars);

        fmpz_mat_clear(mtx_rem);
    }

    _fmpz_vec_clear(tmp_res, nvars);
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
    rs_evaluate(res, state, mtx_upd, iters);

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