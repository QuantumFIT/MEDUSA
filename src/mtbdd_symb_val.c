#include "mtbdd_symb_val.h"
#include "interface.h"

qBDD my_mtbdd_map_to_symb_val_normal_i(qBDD t, size_t map) {
    return unary_apply_guarded(t, mtbdd_map_to_symb_val_i, map);
}

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

qBDD my_mtbdd_from_symb_i(qBDD t, size_t raw_map) {
    return unary_apply_guarded(t, mtbdd_from_symb_i, raw_map);
}

qBDD my_mtbdd_symb_plus_i(qBDD a, qBDD b) {
    return binary_apply(a, b, mtbdd_symb_plus_i);
}

qBDD my_mtbdd_symb_minus_i(qBDD a, qBDD b) {
    return binary_apply(a, b, mtbdd_symb_minus_i);
}

qBDD my_mtbdd_symb_times_c_i(qBDD t, size_t c_raw) {
    return unary_apply_param(t, mtbdd_symb_times_c_i, c_raw);
}

qBDD my_mtbdd_symb_neg_i(qBDD t) {
    return unary_apply(t, mtbdd_symb_neg_i);
}

qBDD my_mtbdd_symb_coef_rot1_i(qBDD t) {
    return unary_apply(t, mtbdd_symb_coef_rot1_i);
}

qBDD my_mtbdd_symb_coef_rot1_i_inv(qBDD t) {
    return unary_apply(t, mtbdd_symb_coef_rot1_i_inv);
}

qBDD my_mtbdd_symb_coef_rot2_i(qBDD t) {
    return unary_apply(t, mtbdd_symb_coef_rot2_i);
}

/* end of "mtbdd_symb_val.c" */
