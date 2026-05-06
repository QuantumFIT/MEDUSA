#include <string.h>
#include <math.h>
#include "mtbdd.h"
#include "mtbdd_out.h"
#include "hash.h"
#include "error.h"
#include "interface.h"


// Custom leaf implementation is taken from: https://github.com/MichalHe/sylvan-custom-leaf-example

/// coefficient k common for all MTBDD leaf values, uzed in MPZ representation
coef_t c_k;


/// Max. size of string written as leaf value in output file
#define MAX_LEAF_STR_LEN 250

/// Max. number of digits written in the .dot output file of a single number
#define MAX_NUM_LEN 50


qBDD my_mtbdd_t_xt_i(qBDD t, size_t xt) {
    return unary_apply_guarded(t, t_xt_create_i, xt);
}

qBDD my_mtbdd_b_xt_mul_i(qBDD t, size_t xt) {
    qBDD b_xt = qBDD_protect(newqBDD(xt, qBDD_false(), qBDD_true()));
    qBDD res = binary_apply_guarded(t, b_xt, mtbdd_b_xt_mul_i);
    return res;
}

qBDD my_mtbdd_b_xt_comp_mul_i(qBDD t, size_t xt) {
    qBDD b_xt_comp = qBDD_protect(newqBDD(xt, qBDD_true(), qBDD_false()));
    qBDD res = binary_apply_guarded(t, b_xt_comp, mtbdd_b_xt_mul_i);
    return res;
}

qBDD my_mtbdd_t_xt_comp_i(qBDD t, size_t xt) {
    return unary_apply_guarded(t, t_xt_comp_create_i, xt);
}

qBDD mtbdd_b_xt_mul_i(qBDD t, qBDD b) {
    if (qBDD_isFalse(t) || qBDD_isFalse(b)) {
        validateApplyResult();
        return qBDD_false();
    }

    if (qBDD_isTerminal(b) && qBDD_isTerminal(t)) 
    {
        validateApplyResult();
        return (t);
    }

    invalidateApplyResult();
    return t; // any
}


long get_coef(uint32_t start, uint32_t end, uint32_t target, char *curr_state)
{
    long skip;
    if (end > target && start < target) {
        skip = 1 << (end - target - 1);
        for (uint32_t i = start; i < target; i++) {
            if (curr_state[i] == NOT_MEASURED_CHAR) {
                skip *= 2;
            }
        }
    }
    else if (end <= target) {
        // It is probable that some skipped qubits were measured
        skip = 1;
        for (uint32_t i = start; i < target; i++) {
            if (curr_state[i] == NOT_MEASURED_CHAR) {
                skip *= 2;
            }
        }
    }
    else {
        skip = 1 << (end - start - 1); // -1 as the first skip is done by having both skip_low, skip_high
    }
    return skip;
}

prob_t interface_prob_sum(qBDD t, uint32_t xt, char* curr_state, int n) {
     // we must immediately convert to float else the skip coefficient will be also squared
    prob_t res = 0;

    // terminal cases
    if (qBDD_isFalse(t)) {
        return res;
    }
    if (qBDD_isTerminal(t)) {
        res = qBDD_calculateProb(t);
        return res;
    }

    // else node
    uint32_t var_a = qBDD_getVar(t);

    qBDD high = qBDD_getHigh(t);
    uint32_t var_high;
    if (qBDD_isTerminal(high)) {
        var_high = n;
    }
    else {
        var_high = qBDD_getVar(high);
    }
    long skip_high = get_coef(var_a, var_high, xt, curr_state);

    qBDD low = qBDD_getLow(t);
    uint32_t var_low;
    if (qBDD_isTerminal(low)) {
        var_low = n;
    }
    else {
        var_low = qBDD_getVar(low);
    }
    long skip_low = get_coef(var_a, var_low, xt, curr_state);

    // recursion
    if (var_a == xt || curr_state[var_a] == '1') {
        res = interface_prob_sum(high, xt, curr_state, n);
        res *= skip_high;
    }
    else if (curr_state[var_a] == '0') {
        res = interface_prob_sum(low, xt, curr_state, n);
        res *= skip_low;
    }
    else {
        
        prob_t res_low = interface_prob_sum(low, xt, curr_state, n);
        res_low *= skip_low;

        prob_t res_high = interface_prob_sum(high, xt, curr_state, n);
        res_high *= skip_high;

        res = res_low + res_high;
    }

    return res;
}

static prob_t prob_sum_recurse(qBDD t, uint32_t var, int n, uint64_t path_count)
{
    if (qBDD_isFalse(t)) return 0.0;

    if (qBDD_isTerminal(t)) {
        prob_t p = qBDD_calculateProb(t);
        // variables from 'var' to 'n-1' are all skipped = 2^(n-var) equal paths
        uint64_t remaining = (var < n) ? (1ULL << (n - var)) : 1ULL;
        return p * (prob_t)path_count * (prob_t)remaining;
    }

    uint32_t var_a = qBDD_getVar(t);

    // Variables skipped between 'var' and 'var_a' p each doubles the path count
    uint64_t multiplier = 1ULL << (var_a - var);

    qBDD high = qBDD_getHigh(t);
    uint32_t next_high = qBDD_isTerminal(high) ? (uint32_t)n : qBDD_getVar(high);
    prob_t res_high = prob_sum_recurse(high, var_a + 1, n, path_count * multiplier);

    qBDD low = qBDD_getLow(t);
    uint32_t next_low = qBDD_isTerminal(low) ? (uint32_t)n : qBDD_getVar(low);
    prob_t res_low  = prob_sum_recurse(low,  var_a + 1, n, path_count * multiplier);

    return res_high + res_low;
}

/**
 * Calculates the total probability represented by the qBDD, i.e. the sum of probabilities of all paths leading to 1.
 *
 * @param t The qBDD representing the quantum state
 * @param n Total number of qubits
 * @return  Total probability
 */
prob_t qBDD_total_prob(qBDD t, int n)
{
    return prob_sum_recurse(t, 0, n, 1ULL);
}

/* end of "mtbdd.c" */