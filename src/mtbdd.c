#include "mtbdd.h"
#include "interface.h"

qBDD my_mtbdd_t_xt_i(qBDD t, size_t xt) {
    return unary_apply_guarded(t, t_xt_create_i, xt);
}

qBDD my_mtbdd_b_xt_mul_i(qBDD t, size_t xt) {
    qBDD b_xt = qBDD_protect(newqBDD(xt, qBDD_false(), qBDD_true()));
    qBDD res = binary_apply_guarded(t, b_xt, mtbdd_b_xt_mul_i);
    qBDD_unprotect(b_xt);
    return res;
}

qBDD my_mtbdd_b_xt_comp_mul_i(qBDD t, size_t xt) {
    qBDD b_xt_comp = qBDD_protect(newqBDD(xt, qBDD_true(), qBDD_false()));
    qBDD res = binary_apply_guarded(t, b_xt_comp, mtbdd_b_xt_mul_i);
    qBDD_unprotect(b_xt_comp);
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


double get_coef(uint32_t first, uint32_t end, uint32_t target, const char *qubit_state)
{
    /* Each skipped variable leads to the same leaf whatever its value. Both
     * values count while it is unmeasured; only its outcome counts once it is
     * measured, and only 1 for the target. */
    double coef = 1.0;
    for (uint32_t i = first; i < end; i++) {
        if (i != target && qubit_state[i] == NOT_MEASURED_CHAR) {
            coef *= 2.0;
        }
    }
    return coef;
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
    /* var_a itself is not skipped: the branches below sum over its values. */
    double skip_high = get_coef(var_a + 1, var_high, xt, curr_state);

    qBDD low = qBDD_getLow(t);
    uint32_t var_low;
    if (qBDD_isTerminal(low)) {
        var_low = n;
    }
    else {
        var_low = qBDD_getVar(low);
    }
    double skip_low = get_coef(var_a + 1, var_low, xt, curr_state);

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
        uint64_t remaining = ((int)var < n) ? (1ULL << (n - var)) : 1ULL;
        return p * (prob_t)path_count * (prob_t)remaining;
    }

    uint32_t var_a = qBDD_getVar(t);

    // Variables skipped between 'var' and 'var_a' p each doubles the path count
    uint64_t multiplier = 1ULL << (var_a - var);

    qBDD high = qBDD_getHigh(t);
    prob_t res_high = prob_sum_recurse(high, var_a + 1, n, path_count * multiplier);

    qBDD low = qBDD_getLow(t);
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