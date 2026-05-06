#include "gates.h"
#include "interface.h"
#include "norm_track.h"
#include "mtbddop.h"
#include <string.h>
prob_t measure(qBDD *a, uint32_t xt, char *curr_state, int n)
{
    uint32_t var_t;
    if (qBDD_isTerminal(*a)) {
        var_t = n;
    }
    else {
        var_t = qBDD_getVar(*a);
    }
    long skip_coef;
    if (var_t == 0) {
        skip_coef = 1;
    }
    else {
        skip_coef = get_coef(0, var_t, xt, curr_state);
    }
    prob_t prob = interface_prob_sum(*a, xt, curr_state, n);
    prob *= skip_coef;
    
    return prob;
}

qBDD interface_gate_x(size_t xt, qBDD low, qBDD high) {
    return newqBDD(xt, high, low);
}

qBDD interface_gate_y(size_t xt, qBDD low, qBDD high) {
    qBDD nhigh = unary_apply(high, invertLeaf);
    qBDD_protect(nhigh);
    qBDD updated = newqBDD(xt, nhigh, low);
    qBDD_protect(updated);
    qBDD_unprotect(nhigh);
    qBDD res = unary_apply(updated, rotateCoef2);
    qBDD_unprotect(updated);
    return res;
}

qBDD interface_gate_z(size_t xt, qBDD low, qBDD high) {
    qBDD nhigh = unary_apply(high, invertLeaf);
    qBDD_protect(nhigh);
    qBDD res = newqBDD(xt, low, nhigh);
    qBDD_unprotect(nhigh);
    return res;
}

qBDD interface_gate_s(size_t xt, qBDD low, qBDD high) {
    qBDD newH = unary_apply(high, rotateCoef2);
    qBDD_protect(newH);
    qBDD res = newqBDD(xt, low, newH);
    qBDD_unprotect(newH);
    return res;
}

qBDD interface_gate_t(size_t xt, qBDD low, qBDD high) {
    qBDD newH = unary_apply(high, rotateCoef1S);
    qBDD_protect(newH);
    qBDD res = newqBDD(xt, low, newH);
    qBDD_unprotect(newH);
    return res;
}

qBDD interface_gate_h(size_t xt, qBDD low, qBDD high) {
    if (low == high) {
        qBDD nlow = unary_apply(low , times2LeafS);
        qBDD_protect(nlow);
        qBDD res = newqBDD(xt, nlow, qBDD_false());
        qBDD_unprotect(nlow);
        return res;
    } else {
        qBDD nh = qBDD_protect(binary_apply(low, high, subLeafS));
        qBDD nl = qBDD_protect(binary_apply(low, high, addLeafS));
        qBDD new_bdd = newqBDD(xt, nl, nh);
        qBDD_unprotect(nh);
        qBDD_unprotect(nl);

        return new_bdd;
    }
}

qBDD interface_gate_rx_pihalf(size_t xt, qBDD low, qBDD high) {
    qBDD rot_low, rot_high;

    rot_low = unary_apply(low, rotateCoef2);
    qBDD_protect(rot_low);
    rot_high = unary_apply(high, rotateCoef2);
    qBDD_protect(rot_high);
    qBDD newLow = binary_apply(low, rot_high, subLeafS);
    qBDD_protect(newLow);
    qBDD newHigh = binary_apply(high, rot_low, subLeafS);
    qBDD_protect(newHigh);
    qBDD_unprotect(rot_high);
    qBDD_unprotect(rot_low);
    qBDD res = newqBDD(xt, newLow, newHigh);
    qBDD_unprotect(newLow);
    qBDD_unprotect(newHigh);
    return res;
}

qBDD interface_gate_ry_pihalf(size_t xt, qBDD low, qBDD high) {
    qBDD newLow = binary_apply(low, high, subLeafS);
    qBDD_protect(newLow);
    qBDD newHigh = binary_apply(low, high, addLeafS);
    qBDD_protect(newHigh);
    qBDD res = newqBDD(xt, newLow, newHigh);
    qBDD_unprotect(newLow);
    qBDD_unprotect(newHigh);
    return res;
}

qBDD interface_gate_rx(size_t xt, qBDD low, qBDD high, size_t param) {
    qBDD newLow  = qBDD_protect(binary_apply_param(low, high, rx_low_leaf,  param));
    qBDD newHigh = qBDD_protect(binary_apply_param(low, high, rx_high_leaf, param));
    qBDD res = newqBDD(xt, newLow, newHigh);
    qBDD_unprotect(newLow);
    qBDD_unprotect(newHigh);
    return res;
}

qBDD interface_gate_ry(size_t xt, qBDD low, qBDD high, size_t param) {
    qBDD newLow  = qBDD_protect(binary_apply_param(low, high, ry_low_leaf,  param));
    qBDD newHigh = qBDD_protect(binary_apply_param(low, high, ry_high_leaf, param));
    qBDD res = newqBDD(xt, newLow, newHigh);
    qBDD_unprotect(newLow);
    qBDD_unprotect(newHigh);
    return res;
}

qBDD interface_gate_rz(size_t xt, qBDD low, qBDD high, size_t param) {
    qBDD newLow  = qBDD_protect(unary_apply_param(low,  rz_low_leaf,  param));
    qBDD newHigh = qBDD_protect(unary_apply_param(high, rz_high_leaf, param));
    qBDD res = newqBDD(xt, newLow, newHigh);
    qBDD_unprotect(newLow);
    qBDD_unprotect(newHigh);
    return res;
}

void gate_x(qBDD *p_t, uint32_t xt)
{
    // Check if xt is a missing root not needed -> swap is identical with the original MTBDD
    if (xt >= qBDD_level(*p_t)) {
        return;
    }
    qBDD res;
#ifndef __cplusplus
    size_t newxt = (size_t) xt;
    res = bdd_operation(*p_t, &newxt, 0, interface_gate_x);

#else
    auto instruction = mtbdd_with_traverse_to(xt, mtbdd_make_swap());
    res = instruction(*p_t);
#endif
    qBDD_protect(res);
    qBDD_unprotect(*p_t);
    *p_t = res;

    if (g_norm_track_enabled) {
        char label[32];
        snprintf(label, sizeof(label), "X(%u)", xt);
        norm_track_record(label, *p_t, g_num_qubits);
    }
}

void gate_y(qBDD *p_t, uint32_t xt)
{
    size_t newxt = (size_t) xt;
    qBDD res = qBDD_protect(bdd_operation(*p_t, &newxt, 0, interface_gate_y));
    qBDD_unprotect(*p_t);
    *p_t = res;

    if (g_norm_track_enabled) {
        char label[32];
        snprintf(label, sizeof(label), "Y(%u)", xt);
        norm_track_record(label, *p_t, g_num_qubits);
    }
}

void gate_z(qBDD *p_t, uint32_t xt)
{
    size_t newxt = (size_t) xt;
    qBDD res = qBDD_protect(bdd_operation(*p_t, &newxt, 0, interface_gate_z));
    qBDD_unprotect(*p_t);
    *p_t = res;

    if (g_norm_track_enabled) {
        char label[32];
        snprintf(label, sizeof(label), "Z(%u)", xt);
        norm_track_record(label, *p_t, g_num_qubits);
    }
}

void gate_rz(qBDD *p_t, uint32_t xt, double theta)
{
    size_t newxt = (size_t) xt;
    size_t param;
    memcpy(&param, &theta, sizeof(double));
    qBDD res = qBDD_protect(bdd_operation_param(*p_t, &newxt, 0, interface_gate_rz, param));
    qBDD_unprotect(*p_t);
    *p_t = res;

    if (g_norm_track_enabled) {
        char label[48];
        snprintf(label, sizeof(label), "Rz(%u,%.6f)", xt, theta);
        norm_track_record(label, *p_t, g_num_qubits);
    }
}

void gate_s(qBDD *p_t, uint32_t xt)
{
    size_t newxt = (size_t) xt;

    qBDD res = qBDD_protect(bdd_operation(*p_t, &newxt, 0, interface_gate_s));

    qBDD_unprotect(*p_t);
    *p_t = res;

    if (g_norm_track_enabled) {
        char label[32];
        snprintf(label, sizeof(label), "S(%u)", xt);
        norm_track_record(label, *p_t, g_num_qubits);
    }
}

void gate_t(qBDD *p_t, uint32_t xt)
{   
    size_t newxt = (size_t) xt;
    qBDD res = qBDD_protect(bdd_operation(*p_t, &newxt, 0, interface_gate_t));
    qBDD_unprotect(*p_t);
    *p_t = res;

    if (g_norm_track_enabled) {
        char label[32];
        snprintf(label, sizeof(label), "T(%u)", xt);
        norm_track_record(label, *p_t, g_num_qubits);
    }
}

void gate_h(qBDD *p_t, uint32_t xt)
{   
    size_t newxt = (size_t) xt;
    qBDD res = qBDD_protect(bdd_operation(*p_t, &newxt, 0, interface_gate_h));
    incInvSqrtCoeff();
    qBDD_unprotect(*p_t);
    *p_t = res;

    if (g_norm_track_enabled) {
        char label[32];
        snprintf(label, sizeof(label), "H(%u)", xt);
        norm_track_record(label, *p_t, g_num_qubits);
    }
}

void gate_rx_pihalf(qBDD *p_t, uint32_t xt)
{
    size_t newxt = (size_t) xt;
    qBDD res = qBDD_protect(bdd_operation(*p_t, &newxt, 0, interface_gate_rx_pihalf));
    incInvSqrtCoeff();
    qBDD_unprotect(*p_t);
    *p_t = res;

    if (g_norm_track_enabled) {
        char label[32];
        snprintf(label, sizeof(label), "Rx_pihalf(%u)", xt);
        norm_track_record(label, *p_t, g_num_qubits);
    }
}

void gate_rx(qBDD *p_t, uint32_t xt, double theta)
{
    size_t newxt = (size_t) xt;
    size_t param;
    memcpy(&param, &theta, sizeof(double));
    qBDD res = qBDD_protect(bdd_operation_param(*p_t, &newxt, 0, interface_gate_rx, param));
    qBDD_unprotect(*p_t);
    *p_t = res;

    if (g_norm_track_enabled) {
        char label[48];
        snprintf(label, sizeof(label), "Rx(%u,%.6f)", xt, theta);
        norm_track_record(label, *p_t, g_num_qubits);
    }
}

void gate_ry(qBDD *p_t, uint32_t xt, double theta)
{
    size_t newxt = (size_t) xt;
    size_t param;
    memcpy(&param, &theta, sizeof(double));
    qBDD res = qBDD_protect(bdd_operation_param(*p_t, &newxt, 0, interface_gate_ry, param));
    qBDD_unprotect(*p_t);
    *p_t = res;

    if (g_norm_track_enabled) {
        char label[48];
        snprintf(label, sizeof(label), "Ry(%u,%.6f)", xt, theta);
        norm_track_record(label, *p_t, g_num_qubits);
    }
}
qBDD interface_gate_tdg(size_t xt, qBDD low, qBDD high) {
    qBDD newH = unary_apply(high, rotateCoef1S_inv);
    qBDD_protect(newH);
    qBDD res = newqBDD(xt, low, newH);
    qBDD_unprotect(newH);
    return res;
}

void gate_tdg(qBDD *p_t, uint32_t xt)
{
    size_t newxt = (size_t) xt;
    qBDD res = qBDD_protect(bdd_operation(*p_t, &newxt, 0, interface_gate_tdg));
    qBDD_unprotect(*p_t);
    *p_t = res;

    if (g_norm_track_enabled) {
        char label[32];
        snprintf(label, sizeof(label), "Tdg(%u)", xt);
        norm_track_record(label, *p_t, g_num_qubits);
    }
}


void gate_ry_pihalf(qBDD *p_t, uint32_t xt)
{
    size_t newxt = (size_t) xt;
    qBDD res = qBDD_protect(bdd_operation(*p_t, &newxt, 0, interface_gate_ry_pihalf));
    incInvSqrtCoeff();
    qBDD_unprotect(*p_t);
    *p_t = res;

    if (g_norm_track_enabled) {
        char label[32];
        snprintf(label, sizeof(label), "Ry_pihalf(%u)", xt);
        norm_track_record(label, *p_t, g_num_qubits);
    }

}

void gate_cnot(qBDD *p_t, uint32_t xt, uint32_t xc)
{
    qBDD t = *p_t;
    qBDD_protect(t);
    qBDD res;

#ifndef __cplusplus
    res = my_mtbdd_b_xt_comp_mul_i(t, xc); // Bxc_c * T
    qBDD_protect(res);

    qBDD t_xt = my_mtbdd_t_xt_i(t, xt); // Txt
    qBDD_protect(t_xt);
    qBDD bracket_left = my_mtbdd_b_xt_comp_mul_i(t_xt, xt); // Bxt_c * Txt
    qBDD_protect(bracket_left);
    qBDD_unprotect(t_xt);

    qBDD t_xt_comp = my_mtbdd_t_xt_comp_i(t, xt); // Txt_c
    qBDD_protect(t_xt_comp);
    qBDD_unprotect(t);
    qBDD bracket_right =  my_mtbdd_b_xt_mul_i(t_xt_comp, xt); // Bxt * Txt_c
    qBDD_protect(bracket_right);
    qBDD_unprotect(t_xt_comp);

    qBDD inter_res = binary_apply(bracket_left, bracket_right, addLeaf); // (Bxt_c * Txt) + (Bxt * Txt_c)
    qBDD_protect(inter_res);
    qBDD_unprotect(bracket_left);
    qBDD_unprotect(bracket_right);
    qBDD inter_res2 = my_mtbdd_b_xt_mul_i(inter_res, xc); // Bxc * (Bxt_c * Txt + Bxt * Txt_c)
    qBDD_unprotect(inter_res);
    qBDD_protect(inter_res2);
    qBDD res2 = qBDD_protect(binary_apply(res, inter_res2, addLeaf)); // (Bxc_c * T) + (Bxc * (Bxt_c * Txt + Bxt * Txt_c))
    qBDD_unprotect(inter_res2);
    qBDD_unprotect(*p_t);
    *p_t = res2;

#else
    SwapParam high_swap_param = {
        .put_up = +[](BDD node) -> BDD {
            return HIGH(node);
        },
        .put_in = +[](BDD node, BDD received) -> BDD {
            return bdd_makenode(LEVEL(node), LOW(node), received);
        }
    };

    if (xt < xc) {
        /* t is above c in the tree (lower level index = closer to root).
            At level t the LOW and HIGH subtrees must be kept in lockstep
            as we descend to c, then swap their HIGH children there.     */
        auto swap_action = mtbdd_make_swap(high_swap_param, high_swap_param);
        auto do_lockstep = mtbdd_with_lockstep_to(xc, swap_action);
        auto cx = mtbdd_with_traverse_to(xt,
            [=](BDD node) -> BDD {
                auto [new_L, new_R] = do_lockstep(LOW(node), HIGH(node));
                return bdd_makenode(LEVEL(node), new_L, new_R);
            }
        );
        res = cx(t);
    } else {
        /* c is above t: follow only the HIGH branch at c (control = 1),
            then descend independently to t and do a simple subtree swap. */
        auto cx = mtbdd_with_traverse_to(
            xc,
            mtbdd_with_traverse_to(xt, mtbdd_make_swap()),
            Branch::LR, Branch::R
        );
        res = cx(t);
    }
    qBDD_unprotect(*p_t);
    qBDD_protect(res);
    *p_t = res;
#endif

    if (g_norm_track_enabled) {
        char label[32];
        snprintf(label, sizeof(label), "CNOT(%u,%u)", xt, xc);
        norm_track_record(label, *p_t, g_num_qubits);
    }

}

void gate_cz(qBDD *p_t, uint32_t xt, uint32_t xc)
{
    size_t newxt = (size_t) xt;
    size_t newxc = (size_t) xc;
    size_t ctrls[2] = {newxc, newxt};
    qBDD res = qBDD_protect(bdd_operation(*p_t, ctrls, 1, interface_gate_z));
    qBDD_unprotect(*p_t);
    *p_t = res;

    if (g_norm_track_enabled) {
        char label[32];
        snprintf(label, sizeof(label), "CZ(%u,%u)", xt, xc);
        norm_track_record(label, *p_t, g_num_qubits);
    }
}

void gate_toffoli(qBDD *p_t, uint32_t xt, uint32_t xc1, uint32_t xc2) {
    
    qBDD t = *p_t;
    qBDD_protect(t);
    qBDD res;
#ifndef __cplusplus
    res = my_mtbdd_b_xt_comp_mul_i(t, xc1); // Bxc_c * T
    qBDD_protect(res);
    qBDD t_xt = my_mtbdd_t_xt_i(t, xt);
    qBDD_protect(t_xt);
    qBDD bracket_left = my_mtbdd_b_xt_comp_mul_i(t_xt, xt); // Bxt_c * Txt
    qBDD_protect(bracket_left);
    qBDD_unprotect(t_xt);
    qBDD t_xt_comp = my_mtbdd_t_xt_comp_i(t, xt);
    qBDD_protect(t_xt_comp);
    qBDD bracket_right = my_mtbdd_b_xt_mul_i(t_xt_comp, xt); // Bxt * Txt_c
    qBDD_protect(bracket_right);
    qBDD_unprotect(t_xt_comp);
    qBDD inter_res = binary_apply(bracket_left, bracket_right, addLeaf); // (Bxt_c * Txt) + (Bxt * Txt_c)
    qBDD_protect(inter_res);
    qBDD_unprotect(bracket_left);
    qBDD_unprotect(bracket_right);
    qBDD bracket_right2 = my_mtbdd_b_xt_mul_i(inter_res, xc2); // Bxc' * (Bxt_c * Txt + Bxt * Txt_c)
    qBDD_protect(bracket_right2);
    qBDD_unprotect(inter_res);
    qBDD bracket_left2 = my_mtbdd_b_xt_comp_mul_i(t, xc2); // Bxc'_c * T
    qBDD_protect(bracket_left2);
    qBDD_unprotect(t);
    qBDD inter_res2 = binary_apply(bracket_left2, bracket_right2, addLeaf); // (Bxc'_c * T) + (Bxc' * (Bxt_c * Txt + Bxt * Txt_c))
    qBDD_protect(inter_res2);
    qBDD_unprotect(bracket_left2);
    qBDD_unprotect(bracket_right2);
    qBDD inter_res3 = my_mtbdd_b_xt_mul_i(inter_res2, xc1); // Bxc * (Bxc'_c * T + Bxc' * (Bxt_c * Txt + Bxt * Txt_c))
    qBDD_protect(inter_res3);
    qBDD_unprotect(inter_res2);
    qBDD res2 = binary_apply(res, inter_res3, addLeaf); // (Bxc_c * T) + (Bxc * (Bxc'_c * T + Bxc' * (Bxt_c * Txt + Bxt * Txt_c)))
    qBDD_protect(res2);
    qBDD_unprotect(res);
    qBDD_unprotect(inter_res3);
    *p_t = res2;
    
#else
    SwapParam high_swap_param = {
        .put_up = +[](BDD node) -> BDD {
            return HIGH(node);
        },
        .put_in = +[](BDD node, BDD received) -> BDD {
            return bdd_makenode(LEVEL(node), LOW(node), received);
        }
    };

    int c1 = (int) xc1;
    int c2 = (int) xc2;
    int t0 = (int) xt;

    if (c1 > c2) { int tmp = c1; c1 = c2; c2 = tmp; }

    /* Auto-select implementation variant by relative level ordering */

    if (t0 < c1) {
        /* t0 < c1 < c2
            Target is above both controls. Traverse to t0, then use two
            nested locksteps - outer down to c1, inner down to c2 - each
            filtering to the HIGH branch only before descending further.  */
        auto swap_action    = mtbdd_make_swap(high_swap_param, high_swap_param);
        auto inner_lockstep = mtbdd_with_lockstep_to(c2, swap_action,
                                                        Branch::LR, Branch::R,
                                                        Branch::LR, Branch::R);
        auto outer_lockstep = mtbdd_with_lockstep_to(c1, inner_lockstep,
                                                        Branch::LR, Branch::R,
                                                        Branch::LR, Branch::R);
        auto ccx = mtbdd_with_traverse_to(
            t0,
            [=](BDD node) -> BDD {
                auto [new_L, new_R] = outer_lockstep(LOW(node), HIGH(node));
                return bdd_makenode(LEVEL(node), new_L, new_R);
            }
        );
        res = ccx(t);

    } else if (c1 < t0 && t0 < c2) {
        /* c1 < t0 < c2
            First control is above the target, second control is below.
            Traverse to c1 filtering HIGH only, then to t0 where LOW and
            HIGH are lockstepped down to c2, swapping HIGH children there. */
        auto swap_action2 = mtbdd_make_swap(high_swap_param, high_swap_param);
        auto at_t = [=](BDD node) -> BDD {
            auto do_lockstep = mtbdd_with_lockstep_to(c2, swap_action2,
                                                        Branch::LR, Branch::R,
                                                        Branch::LR, Branch::R);
            auto [new_L, new_R] = do_lockstep(LOW(node), HIGH(node));
            return bdd_makenode(LEVEL(node), new_L, new_R);
        };
        auto ccx = mtbdd_with_traverse_to(
            c1,
            mtbdd_with_traverse_to(t0, at_t),
            Branch::LR, Branch::R
        );
        res = ccx(t);

    } else {
        /* c1 < c2 < t0
            Both controls are above the target. Traverse to c1 (HIGH),
            then to c2 (HIGH), then to t0 for a straightforward subtree swap.
            No lockstep needed - each subtree is independent at this point. */
        auto ccx = mtbdd_with_traverse_to(
            c1,
            mtbdd_with_traverse_to(
                c2,
                mtbdd_with_traverse_to(t0, mtbdd_make_swap()),
                Branch::LR, Branch::R
            ),
            Branch::LR, Branch::R
        );
        res = ccx(t);
    }


#endif
    qBDD_unprotect(*p_t);
    qBDD_protect(res);
    *p_t = res;

    if (g_norm_track_enabled) {
        char label[32];
        snprintf(label, sizeof(label), "Toffoli(%u,%u,%u)", xt, xc1, xc2);
        norm_track_record(label, *p_t, g_num_qubits);
    }
}

void gate_mcx(qBDD *p_t, qparam_list_t *qparams) {
// Assumes indices in qparams are in the reverse order of the input file! (target qubit first, qc1 last)
    qBDD t = *p_t;
    qBDD_protect(t);

#ifndef __cplusplus

    qBDD res;

    qparam_list_first(qparams);

    qBDD t_xt = my_mtbdd_t_xt_i(t, qparams->active->q_index);
    qBDD_protect(t_xt);
    qBDD bracket_left = my_mtbdd_b_xt_comp_mul_i(t_xt, qparams->active->q_index);
    qBDD_protect(bracket_left);
    qBDD_unprotect(t_xt);

    qBDD t_xt_comp = my_mtbdd_t_xt_comp_i(t, qparams->active->q_index);
    qBDD_protect(t_xt_comp);
    qBDD bracket_right = my_mtbdd_b_xt_mul_i(t_xt_comp, qparams->active->q_index);
    qBDD_protect(bracket_right);
    qBDD_unprotect(t_xt_comp);

    res = binary_apply(bracket_left, bracket_right, addLeaf);
    qBDD_protect(res);

    qparam_list_next(qparams);

    while (qparams->active) {
        qBDD bracket_right1 = my_mtbdd_b_xt_mul_i(res, qparams->active->q_index);
        qBDD_protect(bracket_right1);
        qBDD bracket_left1 = my_mtbdd_b_xt_comp_mul_i(t, qparams->active->q_index);
        qBDD_protect(bracket_left1);
        qBDD_unprotect(res);
        res = binary_apply(bracket_left1, bracket_right1, addLeaf);
        qBDD_protect(res);
        qBDD_unprotect(bracket_left1);
        qBDD_unprotect(bracket_right1);
        qparam_list_next(qparams);
    }
    qBDD_unprotect(t);
    qBDD_unprotect(bracket_left);
    qBDD_unprotect(bracket_right);

#else
    // --- Extract target + controls ---
    qparam_list_first(qparams);
    int target = qparams->active->q_index;
    qparam_list_next(qparams);

    std::vector<int> controls;
    while (qparams->active) {
        controls.push_back(qparams->active->q_index);
        qparam_list_next(qparams);
    }
    std::sort(controls.begin(), controls.end());

    std::vector<int> below_t, above_t;
    for (int c : controls) {
        if      (c < target) below_t.push_back(c);
        else if (c > target) above_t.push_back(c);
    }



    NodeOp at_t;
    if (above_t.empty()) {
        at_t = mtbdd_make_swap();
    } else {
        SwapParam high_swap_param = {
            .put_up = +[](BDD node) -> BDD {
                return HIGH(node);
            },
            .put_in = +[](BDD node, BDD received) -> BDD {
                PUSHREF(LOW(node));
                PUSHREF(received);
                qBDD updated = bdd_makenode(LEVEL(node), READREF(2), READREF(1));
                POPREF(2);
                return updated;
            }
        };
        auto swap_action = mtbdd_make_swap(high_swap_param, high_swap_param);

        BinaryNodeOp do_lockstep;
        int last_c_above = above_t.back(); above_t.pop_back();
        do_lockstep = mtbdd_with_lockstep_to(last_c_above, swap_action,
                                                            Branch::LR, Branch::R,
                                                            Branch::LR, Branch::R);

        for (int i = (int)above_t.size() - 1; i >= 0; --i) {
            int c = above_t[i];
            do_lockstep = mtbdd_with_lockstep_to(c, do_lockstep,
                                                            Branch::LR, Branch::R,
                                                            Branch::LR, Branch::R);
        }

        
        at_t = [=](BDD node) -> BDD {
            auto [new_L, new_R] = do_lockstep(LOW(node), HIGH(node));
            PUSHREF(new_L);
            PUSHREF(new_R);
            BDD res = bdd_makenode(LEVEL(node), READREF(2), READREF(1));
            POPREF(2);
            return res;
        };
    

    }

    qBDD res;

    if (below_t.empty()) {
        // No controls below target: apply at_t directly at target level
        auto mcx = mtbdd_with_traverse_to(target, at_t);
        res = mcx(t);
    } else {
        int last_c_below = below_t.back(); below_t.pop_back();
        auto mcx = mtbdd_with_traverse_to(last_c_below, at_t);

        for (int i = (int)below_t.size() - 1; i >= 0; --i) {
            int c = below_t[i];
            mcx = mtbdd_with_traverse_to(c, mcx, Branch::LR, Branch::R);
        }

        res = mcx(t);
    }


#endif

    qBDD_unprotect(*p_t);
    qBDD_protect(res);
    *p_t = res;

    if (g_norm_track_enabled) {
        char label[64];
        int offset = snprintf(label, sizeof(label), "MCX(%u", qparams->first->q_index);
        qparam_t *iter = qparams->first->next;
        while (iter) {
            offset += snprintf(label + offset, sizeof(label) - offset, ",%u", iter->q_index);
            iter = iter->next;
        }
        snprintf(label + offset, sizeof(label) - offset, ")");
        norm_track_record(label, *p_t, g_num_qubits);
    }
}
/* end of "gates.c" */