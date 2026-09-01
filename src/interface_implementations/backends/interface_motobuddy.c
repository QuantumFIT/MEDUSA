/**
 * @file interface_motobuddy.c
 * Backend-specific implementation of interface for MoToBuddy.
 */

#include "interface_motobuddy.h"
#include "medusa_mem_track.h"
#include "medusa_debug.h"
#include "error.h"
#include <limits.h>
#include <string.h>
#include "kernel.h" /* bddnodes[], bdd_gbc, mtbdd_operator_reset */

#ifdef MEDUSA_DEBUG
int medusa_dbg_bdd_ref(int bdd)
{
    if (bdd < 2 || bdd >= bddnodesize) return -1;
    return (int)bddnodes[bdd].refcou;
}
#endif

/*
 * Package lifecycle
 */

void freePackage(){
    MEDUSA_DBG(.cat = MEDUSA_DBG_LIFECYCLE, .evt = "freePackage", .where = "freePackage");
    setLeafPrintProb(false);
    clearInvSqrtCoeffNormal();
    clearInvSqrtCoeffSymb();
    bdd_done();
}

int qBDD_leafcount(qBDD a) {
    return mtbdd_leaf_count(a);
}

void deleteCircuit(qBDD* circ) {
    if (!circ || *circ == bdd_false()) return;
    MEDUSA_DBG(.cat = MEDUSA_DBG_LIFECYCLE, .evt = "deleteCircuit", .where = "deleteCircuit",
               .use_bdd = 1, .bdd = (int)*circ, .ref = medusa_dbg_bdd_ref((int)*circ),
               .is_false = 0, .leaves = qBDD_leafcount(*circ));
    qBDD_unprotect(*circ);
    *circ = bdd_false();
}

void forceGC() {
#ifdef MEDUSA_DEBUG
    /* Caller should pass circ via MEDUSA_DBG at call site; here log table pressure. */
    MEDUSA_DBG(.cat = MEDUSA_DBG_GC, .evt = "forceGC_enter", .where = "forceGC",
               .note = "bdd_gbc");
#endif
    bdd_gbc();
#ifdef MEDUSA_DEBUG
    MEDUSA_DBG(.cat = MEDUSA_DBG_GC, .evt = "forceGC_leave", .where = "forceGC");
#endif
}

void clearOpCache(void) {
    /* Refine (and similar) mutate rdata as a side effect of apply. MoToBuddy's
     * guarded_param cache keys include (int)param, so reused/truncated rdata
     * pointers (and recycled BDD ids after GC) can skip refine_var_check. */
    mtbdd_operator_reset();
}

void q_fprintdot(FILE *out, qBDD a) {
    buddy_mtbdd_fprintdot(out, a);
}

/* 
 * Terminal type registry
*/

mtbdd_terminal_type lt_classic, lt_symb_map, lt_symb_val;


size_t qBDD_symbolicMapLType() {
    return lt_symb_map;
}

size_t qBDD_symbolicValLType() {
    return lt_symb_val;
}

size_t qBDD_classicLType() {
    return lt_classic;
}

size_t qBDD_getTerminalType(qBDD terminal) {
    return mtbdd_get_terminal_type(terminal);
}

size_t qBDD_level(qBDD node) {
    return LEVEL(node);
}
/* 
 * Reference counting
 */

qBDD qBDD_protect(qBDD toProtect) {
    qBDD r = bdd_addref(toProtect);
    /* Noisy: enable with MEDUSA_DEBUG=protect */
    MEDUSA_DBG(.cat = MEDUSA_DBG_PROTECT, .evt = "protect", .where = "qBDD_protect",
               .use_bdd = 1, .bdd = (int)toProtect,
               .ref = medusa_dbg_bdd_ref((int)toProtect),
               .is_false = qBDD_isFalse(toProtect), .leaves = 0);
    return r;
}

qBDD qBDD_unprotect(qBDD toUnprotect) {
    int ref_before = medusa_dbg_bdd_ref((int)toUnprotect);
    qBDD r = bdd_delref(toUnprotect);
    /* Noisy: enable with MEDUSA_DEBUG=protect (or protect,gc,...) */
    MEDUSA_DBG(.cat = MEDUSA_DBG_PROTECT, .evt = "unprotect", .where = "qBDD_unprotect",
               .use_bdd = 1, .bdd = (int)toUnprotect,
               .ref = medusa_dbg_bdd_ref((int)toUnprotect),
               .is_false = qBDD_isFalse(toUnprotect), .leaves = 0,
               .note = (ref_before == 1) ? "ref_was_1" : NULL);
    return r;
}


/* 
 * Node classification
 */

int qBDD_isFalse(qBDD toCheck) {
    return toCheck == bdd_false();
}

int qBDD_isTerminal(qBDD toCheck) {
    return ISCONST(toCheck) || ISTERMINAL(toCheck);
}

int qBDD_isInternal(qBDD toCheck) {
    return !ISTERMINAL(toCheck) && !ISCONST(toCheck);
}


/* 
 * Node construction
 */

qBDD qBDD_false() {
    return bdd_false();
}

qBDD qBDD_true() {
    return bdd_true();
}

qBDD newqBDD(unsigned int target, qBDD lhs, qBDD rhs) {
    /* Children must stay live if makenode triggers GC (CUSTOM terminals are refcou=0). */
    PUSHREF(lhs);
    PUSHREF(rhs);
    qBDD res = bdd_makenode(target, READREF(2), READREF(1));
    POPREF(2);
    return res;
}

qBDD qBDD_maketerminal(size_t type, void* valuep) {
    return mtbdd_maketerminal(valuep, type);
}

qBDD cube(int value, int width, qBDD *variables, qBDD leaf1, qBDD leaf0) {
    return mtbdd_cube2(0x0, width, variables, leaf1, leaf0);
}


/*
 * Node traversal
 */

qBDD qBDD_getHigh(qBDD a) {
    return HIGH(a);
}

qBDD qBDD_getLow(qBDD a) {
    return LOW(a);
}

size_t qBDD_getVar(qBDD a) {
    return bdd_var(a);
}

LEAF_TYPE qBDD_getTerminalValue(qBDD a) {
    void* val = mtbdd_getTerminalValue(a);
    if (val == NULL) {
        /* false / freelist / collected leaf — do not dereference */
        printf("Warning: qBDD_getTerminalValue called on node %d with NULL value\n", a);
        LEAF_TYPE empty = { .pImpl = NULL };
        return empty;
    }
    return *(LEAF_TYPE*)val;
}


/*
 * MTBDD operations
 */

LEAF_TYPE (*applyOperationToConvert)(LEAF_TYPE, LEAF_TYPE);
LEAF_TYPE (*applyOperationToConvertUnary)(LEAF_TYPE);
LEAF_TYPE (*applyOperationToConvertUnaryParam)(LEAF_TYPE, size_t);
LEAF_TYPE (*applyParamOperationToConvert)(LEAF_TYPE, LEAF_TYPE, size_t);

/** Own the apply result in a heap wrapper. Never return with an orphaned pImpl. */
static void *wrap_leaf_result(LEAF_TYPE result)
{
    if (result.pImpl == NULL) {
        return NULL;
    }
    LEAF_TYPE *result_ptr = (LEAF_TYPE *)malloc(sizeof(LEAF_TYPE));
    if (result_ptr == NULL) {
        freePimpl(&result);
        error_exit("Bad memory allocation.\n");
    }
    medusa_mem_note_wrap_alloc();
    result_ptr->pImpl = result.pImpl;
    return result_ptr;
}

void *convertedApplyOperation(void *a, void *b) {
    LEAF_TYPE *leaf_a = (LEAF_TYPE*)a;
    LEAF_TYPE *leaf_b = (LEAF_TYPE*)b;
    LEAF_TYPE leaf_a_val;
    LEAF_TYPE leaf_b_val;

    leaf_a_val.pImpl = NULL;
    leaf_b_val.pImpl = NULL;

    if(leaf_a == NULL) {
        leaf_a_val.pImpl = NULL;
    } else {
        leaf_a_val = *leaf_a;
    }
    if(leaf_b == NULL) {
        leaf_b_val.pImpl = NULL;
    } else {
        leaf_b_val = *leaf_b;
    }

    LEAF_TYPE result = applyOperationToConvert(leaf_a_val, leaf_b_val);
    return wrap_leaf_result(result);
}

void *convertedApplyOperationParam(void *a, void *b, size_t param) {
    LEAF_TYPE *leaf_a = (LEAF_TYPE*)a;
    LEAF_TYPE *leaf_b = (LEAF_TYPE*)b;
    LEAF_TYPE leaf_a_val;
    LEAF_TYPE leaf_b_val;

    leaf_a_val.pImpl = NULL;
    leaf_b_val.pImpl = NULL;

    if (leaf_a == NULL) {
        leaf_a_val.pImpl = NULL;
    } else {
        leaf_a_val = *leaf_a;
    }
    if (leaf_b == NULL) {
        leaf_b_val.pImpl = NULL;
    } else {
        leaf_b_val = *leaf_b;
    }

    LEAF_TYPE result = applyParamOperationToConvert(leaf_a_val, leaf_b_val, param);
    return wrap_leaf_result(result);
}

void *convertedApplyOperationUnary(void *a){
    LEAF_TYPE first;
    if (a == NULL) {first.pImpl=NULL;}
    else {first = *(LEAF_TYPE*)a;}

    LEAF_TYPE result = applyOperationToConvertUnary(first);
    return wrap_leaf_result(result);
}

void *convertedApplyOperationUnaryParam(void *a, size_t arg) {
    LEAF_TYPE first;
    if (a == NULL) {first.pImpl=NULL;}
    else {first = *(LEAF_TYPE*)a;}

    LEAF_TYPE result = applyOperationToConvertUnaryParam(first, arg);
    return wrap_leaf_result(result);
}


/* Binary -- as caching is used with pointers in MoToBuddy, each operations needs it's own pointer */
void *convertedApplyOperationAdd   (void *a, void *b) { return convertedApplyOperation(a, b); }
void *convertedApplyOperationSub   (void *a, void *b) { return convertedApplyOperation(a, b); }
void *convertedApplyOperationMask  (void *a, void *b) { return convertedApplyOperation(a, b); }
void *convertedApplyOperationPlusI (void *a, void *b) { return convertedApplyOperation(a, b); }
void *convertedApplyOperationMinusI(void *a, void *b) { return convertedApplyOperation(a, b); }
void *convertedApplyOperationAddS  (void *a, void *b) { return convertedApplyOperation(a, b); }
void *convertedApplyOperationSubS  (void *a, void *b) { return convertedApplyOperation(a, b); }
void *convertedApplyOperationMulS  (void *a, void *b) { return convertedApplyOperation(a, b); }
void *convertedApplyOperationDivS  (void *a, void *b) { return convertedApplyOperation(a, b); }

/* Unary -- as caching is used with pointers in MoToBuddy, each operations needs it's own pointer */
void *convertedApplyOperationInv   (void *a) { return convertedApplyOperationUnary(a); }
void *convertedApplyOperationRot1  (void *a) { return convertedApplyOperationUnary(a); }
void *convertedApplyOperationRot2  (void *a) { return convertedApplyOperationUnary(a); }
void *convertedApplyOperationTimes2(void *a) { return convertedApplyOperationUnary(a); }
void *convertedApplyOperationNegI  (void *a) { return convertedApplyOperationUnary(a); }
void *convertedApplyOperationNegIMul (void *a) { return convertedApplyOperationUnary(a); }
void *convertedApplyOperationRot1I (void *a) { return convertedApplyOperationUnary(a); }
void *convertedApplyOperationRot1IInv(void *a) { return convertedApplyOperationUnary(a); }
void *convertedApplyOperationRot2I (void *a) { return convertedApplyOperationUnary(a); }
void *convertedApplyOperationRot1S   (void *a) { return convertedApplyOperationUnary(a); }
void *convertedApplyOperationRot1SInv(void *a) { return convertedApplyOperationUnary(a); }
void *convertedApplyOperationRot2S   (void *a) { return convertedApplyOperationUnary(a); }
void *convertedApplyOperationTimes2S (void *a) { return convertedApplyOperationUnary(a); }
void *convertedApplyOperationSqrt2   (void *a) { return convertedApplyOperationUnary(a); }

void *convertedApplyOperationParamRxLow (void *a, void *b, size_t param) { return convertedApplyOperationParam(a, b, param); }
void *convertedApplyOperationParamRxHigh(void *a, void *b, size_t param) { return convertedApplyOperationParam(a, b, param); }
void *convertedApplyOperationParamRyLow (void *a, void *b, size_t param) { return convertedApplyOperationParam(a, b, param); }
void *convertedApplyOperationParamRyHigh(void *a, void *b, size_t param) { return convertedApplyOperationParam(a, b, param); }

void *convertedApplyOperationRzLow (void *a, size_t param) { return convertedApplyOperationUnaryParam(a, param); }
void *convertedApplyOperationRzHigh(void *a, size_t param) { return convertedApplyOperationUnaryParam(a, param); }



void *convertedApplyOperationMulPhase(void *a, size_t arg) { return convertedApplyOperationUnaryParam(a, arg); }

qBDD bdd_operation(qBDD operand, size_t *targets, size_t controlNum,
                   qBDD (*op)(size_t, qBDD, qBDD)) {
    return mtbdd_operation(operand, targets, controlNum, op);
}

qBDD bdd_operation_param(qBDD operand, size_t *targets, size_t controlNum,
                   qBDD (*op)(size_t, qBDD, qBDD, size_t), size_t param) {
    return mtbdd_operation_param(operand, targets, controlNum, op, param);
}

qBDD bdd_operation_guarded(qBDD operand, size_t *targets, size_t controlNum,
                            qBDD (*op)(size_t, qBDD)) {
    return mtbdd_operation_guarded(operand, targets, controlNum, op);
}

qBDD binary_apply(qBDD l, qBDD r, LEAF_TYPE (*op)(LEAF_TYPE, LEAF_TYPE)) {
    applyOperationToConvert = op;
    if      (op == addLeaf)            return mtbdd_apply(l, r, convertedApplyOperationAdd);
    else if (op == subLeaf)            return mtbdd_apply(l, r, convertedApplyOperationSub);
    else if (op == addLeafS)           return mtbdd_apply(l, r, convertedApplyOperationAddS);
    else if (op == subLeafS)           return mtbdd_apply(l, r, convertedApplyOperationSubS);
    else if (op == mulLeafS)           return mtbdd_apply(l, r, convertedApplyOperationMulS);
    else if (op == divLeafS)           return mtbdd_apply(l, r, convertedApplyOperationDivS);
    else if (op == mtbdd_symb_minus_i) return mtbdd_apply(l, r, convertedApplyOperationMinusI);
    else if (op == mtbdd_symb_plus_i)  return mtbdd_apply(l, r, convertedApplyOperationPlusI);
    else                               return mtbdd_apply(l, r, convertedApplyOperation);
}

qBDD binary_apply_guarded(qBDD l, qBDD r, qBDD (*op)(qBDD, qBDD)) {
    return mtbdd_apply_guarded(l, r, op);
}

qBDD binary_apply_guarded_param(qBDD l, qBDD r, qBDD(*op)(qBDD, qBDD, size_t), size_t param) {
    return mtbdd_apply_guarded_param(l, r, op, param);
}

qBDD binary_apply_param(qBDD l, qBDD r, LEAF_TYPE(*op)(LEAF_TYPE, LEAF_TYPE, size_t), size_t param) {
    applyParamOperationToConvert = op;
    if      (op == rx_low_leaf)  return mtbdd_apply_param(l, r, convertedApplyOperationParamRxLow,  param);
    else if (op == rx_high_leaf) return mtbdd_apply_param(l, r, convertedApplyOperationParamRxHigh, param);
    else if (op == ry_low_leaf)  return mtbdd_apply_param(l, r, convertedApplyOperationParamRyLow,  param);
    else if (op == ry_high_leaf) return mtbdd_apply_param(l, r, convertedApplyOperationParamRyHigh, param);
    else                         return mtbdd_apply_param(l, r, convertedApplyOperationParam,        param);
}

qBDD unary_apply(qBDD l, LEAF_TYPE (*op)(LEAF_TYPE)) {
    applyOperationToConvertUnary = op;
    if      (op == rotateCoef1)            return mtbdd_apply_unary(l, convertedApplyOperationRot1);
    else if (op == rotateCoef2)            return mtbdd_apply_unary(l, convertedApplyOperationRot2);
    else if (op == rotateCoef1S)           return mtbdd_apply_unary(l, convertedApplyOperationRot1S);
    else if (op == rotateCoef1S_inv)       return mtbdd_apply_unary(l, convertedApplyOperationRot1SInv);
    else if (op == rotateCoef2S)           return mtbdd_apply_unary(l, convertedApplyOperationRot2S);
    else if (op == times2Leaf)             return mtbdd_apply_unary(l, convertedApplyOperationTimes2);
    else if (op == times2LeafS)            return mtbdd_apply_unary(l, convertedApplyOperationTimes2S);
    else if (op == invertLeaf)             return mtbdd_apply_unary(l, convertedApplyOperationInv);
    else if (op == negI_mul)                return mtbdd_apply_unary(l, convertedApplyOperationNegIMul);
    else if (op == mtbdd_symb_neg_i)       return mtbdd_apply_unary(l, convertedApplyOperationNegI);
    else if (op == mtbdd_symb_coef_rot1_i) return mtbdd_apply_unary(l, convertedApplyOperationRot1I);
    else if (op == mtbdd_symb_coef_rot1_i_inv) return mtbdd_apply_unary(l, convertedApplyOperationRot1IInv);
    else if (op == mtbdd_symb_coef_rot2_i) return mtbdd_apply_unary(l, convertedApplyOperationRot2I);
    else                                   return mtbdd_apply_unary(l, convertedApplyOperationUnary);
}
qBDD unary_apply_guarded(qBDD l, qBDD(*op)(qBDD, size_t), size_t arg) {
    return mtbdd_apply_unary_guarded(l, (BDD(*)(BDD, void*))op, arg);
}

qBDD unary_apply_param(qBDD l, LEAF_TYPE(*op)(LEAF_TYPE, size_t), size_t arg) {
    applyOperationToConvertUnaryParam = op;
    if      (op == mulPhaseLeaf)  return mtbdd_apply_unary_param(l, convertedApplyOperationMulPhase, arg);
    else if (op == rz_low_leaf)   return mtbdd_apply_unary_param(l, convertedApplyOperationRzLow,   arg);
    else if (op == rz_high_leaf)  return mtbdd_apply_unary_param(l, convertedApplyOperationRzHigh,  arg);
    else                          return mtbdd_apply_unary_param(l, (void*(*)(void*, size_t))op, arg);
}
/*
 * Operation result flags
 */

void validateOperationResult() {
    FLAG_VALID_OPERATION();
}

void invalidateOperationResult() {
    FLAG_INVALID_OPERATION();
}

void validateApplyResult() {
    FLAG_VALID_APPLY();
}

void invalidateApplyResult() {
    FLAG_INVALID_APPLY();
}

/**
 * Terminal handlers
 */

CUSTOM_COMPARE_DECLARE(terminal_symb_val_compare)
CUSTOM_COMPARE_DEFINE_START(terminal_symb_val_compare, l_a, l_b)
    return terminal_symb_val_compare_generic(l_a, l_b);
CUSTOM_COMPARE_DEFINE_END

CUSTOM_HASH_DECLARE(terminal_symb_val_hash)
CUSTOM_HASH_DEFINE_START(terminal_symb_val_hash, l_a)
    return terminal_symb_val_hash_generic(l_a);
CUSTOM_HASH_DEFINE_END

CUSTOM_HASH_DECLARE(terminal_symb_map_hash)
CUSTOM_HASH_DEFINE_START(terminal_symb_map_hash, l_a)
    return terminal_symb_map_hash_generic(l_a);
CUSTOM_HASH_DEFINE_END

CUSTOM_COMPARE_DECLARE(terminal_symb_map_compare)
CUSTOM_COMPARE_DEFINE_START(terminal_symb_map_compare, l_a, l_b)
    return terminal_symb_map_compare_generic(l_a, l_b);
CUSTOM_COMPARE_DEFINE_END

CUSTOM_COMPARE_DECLARE(terminal_compare);
CUSTOM_COMPARE_DEFINE_START(terminal_compare, a, b)
    return terminal_compare_generic(a,b);
CUSTOM_COMPARE_DEFINE_END

CUSTOM_HASH_DECLARE(terminal_hash);
CUSTOM_HASH_DEFINE_START(terminal_hash, q)
    return terminal_hash_generic(q);
CUSTOM_HASH_DEFINE_END
char* terminal_to_str_val(void* ldata_raw, char *buddy_buf, size_t buddy_bufsize) {
    if (ldata_raw == NULL) {
        strncpy(buddy_buf, "NULL", buddy_bufsize - 1);
    } else {
        strncpy(buddy_buf, "T", buddy_bufsize - 1);
    }
    buddy_buf[buddy_bufsize - 1] = '\0';
    return buddy_buf;
}
void init_terminal_symb_val_i() {
    lt_symb_val = mtbdd_new_terminal_type();
    mtbdd_register_compare_function(lt_symb_val, terminal_symb_val_compare);
    mtbdd_register_free_function(lt_symb_val, terminal_symb_val_free);
    mtbdd_register_to_str_function(lt_symb_val, terminal_to_str_val);
    mtbdd_register_hash_function(lt_symb_val, terminal_symb_val_hash);
}

char* terminal_to_str_map(void* ldata_raw, char *buddy_buf, size_t buddy_bufsize) {
    if (ldata_raw == NULL) {
        strncpy(buddy_buf, "NULL", buddy_bufsize - 1);
    } else {
        strncpy(buddy_buf, "T", buddy_bufsize - 1);
    }
    buddy_buf[buddy_bufsize - 1] = '\0';
    return buddy_buf;
}



void init_terminal_symb_map_i() {
    lt_symb_map = mtbdd_new_terminal_type();
    mtbdd_register_compare_function(lt_symb_map, terminal_symb_map_compare);
    mtbdd_register_free_function(lt_symb_map, terminal_symb_map_free);
    mtbdd_register_to_str_function(lt_symb_map, terminal_to_str_map);
    mtbdd_register_hash_function(lt_symb_map, terminal_symb_map_hash);
}

void initPackage(unsigned cacheSize, unsigned nodeSize, unsigned varNum) {
    /* 0 means "use BuDDy defaults" so existing initPackage(0,0,0) callers keep working. */
    if (cacheSize == 0) {
        cacheSize = 10000;
    }
    if (nodeSize == 0) {
        nodeSize = 10000;
    }
    if (varNum == 0) {
        varNum = 1;
    }
    if (cacheSize > (unsigned)INT_MAX) {
        cacheSize = (unsigned)INT_MAX;
    }
    if (nodeSize > (unsigned)INT_MAX) {
        nodeSize = (unsigned)INT_MAX;
    }
    if (varNum > (unsigned)INT_MAX) {
        varNum = (unsigned)INT_MAX;
    }

    mtbdd = 1;
    /* BuDDy: bdd_init(nodetable size, operator-cache size). */
    if (bdd_init((int)nodeSize, (int)cacheSize) < 0) {
        error_exit("bdd_init failed (nodeSize=%u, cacheSize=%u).\n", nodeSize, cacheSize);
    }
    if (bdd_setvarnum((int)varNum) < 0) {
        error_exit("bdd_setvarnum failed (varNum=%u).\n", varNum);
    }
    MEDUSA_DBG(.cat = MEDUSA_DBG_LIFECYCLE, .evt = "initPackage", .where = "initPackage",
               .use_n = 1, .n_qubits = (int)varNum,
               .note = "bdd_init(nodeSize,cacheSize)");
    lt_classic = mtbdd_new_terminal_type();
    mtbdd_register_compare_function(lt_classic, terminal_compare);
    /* freefun must only release LEAF_TYPE.pImpl (see freePimpl). Do not free the
     * outer LEAF_TYPE* — MoToBuddy free()s that after freefun.
     * Revert: patches/revert-classic-freepimpl.patch */
    mtbdd_register_free_function(lt_classic, freePimpl);
    mtbdd_register_hash_function(lt_classic, terminal_hash);
    mtbdd_register_to_str_function(lt_classic, terminal_to_str_generic);
    init_terminal_symb_map_i();
    init_terminal_symb_val_i();
    SETDOMAIN(CUSTOM);
}

/* EOF interface_motobuddy.c */
