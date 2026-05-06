/**
 * @file interface_motobuddy.c
 * Backend-specific implementation of interface for MoToBuddy.
 */

#include "interface_motobuddy.h"
#include <string.h>

/*
 * Package lifecycle
 */

void freePackage(){
    bdd_done();
}

int qBDD_leafcount(qBDD a) {
    return mtbdd_leaf_count(a);
}

void deleteCircuit(qBDD* circ) {
    return; /* stub */
}

void forceGC() {
    bdd_gbc();
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
    return bdd_addref(toProtect);
}

qBDD qBDD_unprotect(qBDD toUnprotect) {
    return bdd_delref(toUnprotect);
}


/* 
 * Node classification
 */

int qBDD_isFalse(qBDD toCheck) {
    return toCheck == bdd_false();
}

int qBDD_isTerminal(qBDD toCheck) {
   // printf("a%d\n", toCheck);
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
    return bdd_makenode(target, lhs, rhs);
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
        printf("Warning: qBDD_getTerminalValue called on a terminal %d with NULL value\n", a);
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
    
    // apply operation on LEAF_TYPE values to get a new LEAF_TYPE (by value)
    LEAF_TYPE result = applyOperationToConvert(leaf_a_val, leaf_b_val); // 169

    if (result.pImpl == NULL) {
        // no internal data, just copy pointer as NULL
        return NULL;
    }

    // allocate memory for the new LEAF_TYPE pointer to return
    LEAF_TYPE *result_ptr = (LEAF_TYPE*)malloc(sizeof(LEAF_TYPE));
    if (result_ptr == NULL) {
        fprintf(stderr, "ERROR: malloc failed in convertedApplyOperation\n");
        return NULL;
    }

    result_ptr->pImpl = result.pImpl;
    return result_ptr;
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

    if (result.pImpl == NULL) {
        return NULL;
    }

    LEAF_TYPE *result_ptr = (LEAF_TYPE*)malloc(sizeof(LEAF_TYPE));
    if (result_ptr == NULL) {
        fprintf(stderr, "ERROR: malloc failed in convertedApplyOperationParam\n");
        return NULL;
    }

    result_ptr->pImpl = result.pImpl;
    return result_ptr;
}

void *convertedApplyOperationUnary(void *a){
    LEAF_TYPE first;
    if (a == NULL) {first.pImpl=NULL;}
    else {first = *(LEAF_TYPE*)a;}

    LEAF_TYPE result = applyOperationToConvertUnary(first);
    if (result.pImpl == NULL) {
        return NULL;
    }
    LEAF_TYPE *result_ptr = (LEAF_TYPE*)malloc(sizeof(LEAF_TYPE));
    if(result_ptr == NULL){
        fprintf(stderr, "ERROR: malloc failed in convertedApplyOperationUnary\n");
        return NULL;
        // some err
    }

    // Assign the new_impl to result_ptr
    result_ptr->pImpl = result.pImpl;

    return result_ptr;
}

void *convertedApplyOperationUnaryParam(void *a, size_t arg) {
    LEAF_TYPE first;
    if (a == NULL) {first.pImpl=NULL;}
    else {first = *(LEAF_TYPE*)a;}

    LEAF_TYPE result = applyOperationToConvertUnaryParam(first, arg);
    if (result.pImpl == NULL) {
        return NULL;
    }
    LEAF_TYPE *result_ptr = (LEAF_TYPE*)malloc(sizeof(LEAF_TYPE));
    if(result_ptr == NULL){
        fprintf(stderr, "ERROR: malloc failed in convertedApplyOperationUnaryGuarded\n");
        return NULL;
        // some err
    }

    // Assign the new_impl to result_ptr
    result_ptr->pImpl = result.pImpl;

    return result_ptr;
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
void *convertedApplyOperationRot2I (void *a) { return convertedApplyOperationUnary(a); }
void *convertedApplyOperationRot1S   (void *a) { return convertedApplyOperationUnary(a); }
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
    else if (op == rotateCoef2S)           return mtbdd_apply_unary(l, convertedApplyOperationRot2S);
    else if (op == times2Leaf)             return mtbdd_apply_unary(l, convertedApplyOperationTimes2);
    else if (op == times2LeafS)            return mtbdd_apply_unary(l, convertedApplyOperationTimes2S);
    else if (op == invertLeaf)             return mtbdd_apply_unary(l, convertedApplyOperationInv);
    else if (op == negI_mul)                return mtbdd_apply_unary(l, convertedApplyOperationNegIMul);
    else if (op == mtbdd_symb_neg_i)       return mtbdd_apply_unary(l, convertedApplyOperationNegI);
    else if (op == mtbdd_symb_coef_rot1_i) return mtbdd_apply_unary(l, convertedApplyOperationRot1I);
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
    mtbdd_register_free_function(lt_symb_val, NULL); /* TODO ADD FREE */
    mtbdd_register_to_str_function(lt_symb_val, terminal_to_str_val); /* TODO ADD TO_STR */
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
    mtbdd_register_free_function(lt_symb_map, NULL); /* TODO ADD FREE */
    mtbdd_register_to_str_function(lt_symb_map, terminal_to_str_map); /* TODO ADD TO_STR */
    mtbdd_register_hash_function(lt_symb_map, terminal_symb_map_hash);
}

void initPackage(unsigned cacheSize, unsigned nodeSize, unsigned varNum) {
    mtbdd = 1;
    bdd_init(10000, 10000);
    bdd_setvarnum(1);
    lt_classic = mtbdd_new_terminal_type();
    mtbdd_register_compare_function(lt_classic, terminal_compare);
    //mtbdd_register_free_function(lt_classic, freePimpl); TODO
    mtbdd_register_hash_function(lt_classic, terminal_hash);
    mtbdd_register_to_str_function(lt_classic, terminal_to_str_generic);
    init_terminal_symb_map_i();
    init_terminal_symb_val_i();
    SETDOMAIN(CUSTOM);
}

/* EOF interface_motobuddy.c */
