/**
 * @file interface_motobuddy.h
 * Backend-specific setup for MoToBuddy (preferred / default MEDUSA product).
 */

#ifndef INTERFACE_MOTOBUDDY_H
#define INTERFACE_MOTOBUDDY_H

#include "bdd.h"

#ifndef QBDD_TYPE_DEFINED
#define QBDD_TYPE_DEFINED
typedef BDD qBDD;
#endif

#include "mtbdd.h"
#include "medusa_debug.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Hot wrappers, inlined. mtbdd.h already pulls in kernel.h (bddnodes[], LEVEL,
 * LOW/HIGH, PUSHREF/READREF/POPREF) for every translation unit, so these can be
 * defined here and vanish into their callers in mtbdd.c, gates.c and the leaf
 * code. Out of line, qBDD_isFalse alone was 2.3 % of all instructions on
 * LP-QuantumCounting/08_04_05_0 (issue #19). interface_bdd_core.h skips the
 * prototypes for these when QBDD_HOT_OPS_INLINED is defined.
 */
#define QBDD_HOT_OPS_INLINED

/* BDDZERO/BDDONE rather than bdd_false()/bdd_true(): under USE_CXX those two
 * resolve to bdd.h's C++ wrappers returning a `bdd` object, not a BDD index. */
static inline int qBDD_isFalse(qBDD toCheck) {
    return toCheck == BDDZERO;
}

static inline int qBDD_isTerminal(qBDD toCheck) {
    return ISCONST(toCheck) || ISTERMINAL(toCheck);
}

static inline int qBDD_isInternal(qBDD toCheck) {
    return !ISTERMINAL(toCheck) && !ISCONST(toCheck);
}

static inline qBDD qBDD_false(void) {
    return BDDZERO;
}

static inline qBDD qBDD_true(void) {
    return BDDONE;
}

static inline qBDD qBDD_getHigh(qBDD a) {
    return HIGH(a);
}

static inline qBDD qBDD_getLow(qBDD a) {
    return LOW(a);
}

static inline size_t qBDD_level(qBDD node) {
    return LEVEL(node);
}

static inline qBDD newqBDD(unsigned int target, qBDD lhs, qBDD rhs) {
    /* Children must stay live if makenode triggers GC (CUSTOM terminals are refcou=0). */
    PUSHREF(lhs);
    PUSHREF(rhs);
    qBDD res = bdd_makenode(target, READREF(2), READREF(1));
    POPREF(2);
    return res;
}

static inline qBDD qBDD_protect(qBDD toProtect) {
    qBDD r = bdd_addref(toProtect);
    /* Noisy: enable with MEDUSA_DEBUG=protect */
    MEDUSA_DBG(.cat = MEDUSA_DBG_PROTECT, .evt = "protect", .where = "qBDD_protect",
               .use_bdd = 1, .bdd = (int)toProtect,
               .ref = medusa_dbg_bdd_ref((int)toProtect),
               .is_false = qBDD_isFalse(toProtect), .leaves = 0);
    return r;
}

static inline qBDD qBDD_unprotect(qBDD toUnprotect) {
    int ref_before = medusa_dbg_bdd_ref((int)toUnprotect);
    qBDD r = bdd_delref(toUnprotect);
    /* Noisy: enable with MEDUSA_DEBUG=protect (or protect,gc,...) */
    MEDUSA_DBG(.cat = MEDUSA_DBG_PROTECT, .evt = "unprotect", .where = "qBDD_unprotect",
               .use_bdd = 1, .bdd = (int)toUnprotect,
               .ref = medusa_dbg_bdd_ref((int)toUnprotect),
               .is_false = qBDD_isFalse(toUnprotect), .leaves = 0,
               .note = (ref_before == 1) ? "ref_was_1" : NULL);
    (void)ref_before;
    return r;
}

#ifdef __cplusplus
}
#endif

#include "interface_bdd_core.h"
#include "interface_leaf.h"
#include "interface_gate_ops.h"
#include "interface_prob.h"
#include "interface_norm.h"
#include "interface_symb.h"
#include "interface_lifecycle.h"

extern LEAF_TYPE clonePimpl(LEAF_TYPE src);

#endif /* INTERFACE_MOTOBUDDY_H */