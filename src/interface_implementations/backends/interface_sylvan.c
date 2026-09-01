/**
 * @file interface_sylvan.c
 * Sylvan implementation of the Medusa qBDD API (optional backend).
 *
 * MoToBuddy is the preferred default. This file is compiled only for
 * `make sylvan_doubles` / `sylvan_gmp` (C path, USE_CXX=0).
 *
 * Gate application follows VeriFIT/MEDUSA mtbdd_apply_gate / mtbdd_apply_cgate:
 * missing xt (and control) nodes are treated as (var, dd, dd) before the C
 * interface_gate_* callback. Apply uses the leaf guarded-result protocol
 * (validateApplyResult / invalidateApplyResult) instead of TASK_IMPL +
 * mtbdd_invalid, so leaf files stay Buddy-shaped.
 */

#include "interface_sylvan.h"
#include "error.h"
#include "medusa_debug.h"
#include "medusa_mem_track.h"

#include <sylvan_int.h>

#include <limits.h>
#include <stdint.h>
#include <string.h>

mtbdd_terminal_type lt_classic, lt_symb_map, lt_symb_val;

static int g_varnum;
static int g_package_live;
static uint64_t g_opid_op, g_opid_op_param, g_opid_op_g;

/* Nested Lace RUN() deadlocks; apply/operation from a gate callback must CALL. */
static _Thread_local int g_in_task;
static _Thread_local int g_apply_valid = 1;
static _Thread_local int g_op_valid = 1;

static _Thread_local LEAF_TYPE (*g_leaf_bin)(LEAF_TYPE, LEAF_TYPE);
static _Thread_local LEAF_TYPE (*g_leaf_binp)(LEAF_TYPE, LEAF_TYPE, size_t);
static _Thread_local LEAF_TYPE (*g_leaf_una)(LEAF_TYPE);
static _Thread_local LEAF_TYPE (*g_leaf_unap)(LEAF_TYPE, size_t);
static _Thread_local qBDD (*g_bin_g)(qBDD, qBDD);
static _Thread_local qBDD (*g_bin_gp)(qBDD, qBDD, size_t);

#define OPID_MAX 128
static struct {
    uint64_t fn;
    uint64_t id;
} g_opids[OPID_MAX];
static int g_nopids;

static uint64_t opid_of(uint64_t fn)
{
    int i;
    for (i = 0; i < g_nopids; i++) {
        if (g_opids[i].fn == fn) {
            return g_opids[i].id;
        }
    }
    if (g_nopids >= OPID_MAX) {
        error_exit("too many Sylvan operation ids\n");
    }
    g_opids[g_nopids].fn = fn;
    g_opids[g_nopids].id = cache_next_opid();
    return g_opids[g_nopids++].id;
}

#ifdef MEDUSA_DEBUG
int medusa_dbg_bdd_ref(int bdd)
{
    (void)bdd;
    return -1;
}
#endif

/* -------------------------------------------------------------------------- */
/* Custom leaves: payload is LEAF_TYPE* (wrapper + pImpl). create() must deep
 * copy like original MEDUSA / Sylvan GMP — the unique table owns the copy and
 * destroy() always frees it. Sharing pImpl with the caller UAF's on intern
 * (duplicate probe) and on GC. */
/* -------------------------------------------------------------------------- */

static void syl_leaf_create(uint64_t *p)
{
    LEAF_TYPE *orig = (LEAF_TYPE *)(uintptr_t)(*p);
    LEAF_TYPE *copy = (LEAF_TYPE *)malloc(sizeof(LEAF_TYPE));
    if (copy == NULL) {
        error_exit("Bad memory allocation.\n");
    }
    *copy = clonePimpl(orig ? *orig : (LEAF_TYPE){ .pImpl = NULL });
    *p = (uint64_t)(uintptr_t)copy;
}

static void syl_leaf_create_map(uint64_t *p)
{
    typedef struct { uint64_t vre, vim; } map_payload_t;
    LEAF_TYPE *orig = (LEAF_TYPE *)(uintptr_t)(*p);
    LEAF_TYPE *copy = (LEAF_TYPE *)malloc(sizeof(LEAF_TYPE));
    if (copy == NULL) {
        error_exit("Bad memory allocation.\n");
    }
    copy->pImpl = NULL;
    if (orig && orig->pImpl) {
        map_payload_t *m = (map_payload_t *)malloc(sizeof(map_payload_t));
        if (m == NULL) {
            error_exit("Bad memory allocation.\n");
        }
        *m = *(map_payload_t *)orig->pImpl;
        copy->pImpl = (void *)m;
    }
    *p = (uint64_t)(uintptr_t)copy;
}

static void syl_leaf_create_val(uint64_t *p)
{
    typedef struct { void *re, *im; } val_payload_t;
    LEAF_TYPE *orig = (LEAF_TYPE *)(uintptr_t)(*p);
    LEAF_TYPE *copy = (LEAF_TYPE *)malloc(sizeof(LEAF_TYPE));
    if (copy == NULL) {
        error_exit("Bad memory allocation.\n");
    }
    copy->pImpl = NULL;
    if (orig && orig->pImpl) {
        val_payload_t *v = (val_payload_t *)malloc(sizeof(val_payload_t));
        if (v == NULL) {
            error_exit("Bad memory allocation.\n");
        }
        *v = *(val_payload_t *)orig->pImpl;
        copy->pImpl = (void *)v;
    }
    *p = (uint64_t)(uintptr_t)copy;
}

static void syl_leaf_destroy_classic(uint64_t v)
{
    void *p = (void *)(uintptr_t)v;
    freePimpl(p);
    free(p);
}

static void syl_leaf_destroy_map(uint64_t v)
{
    void *p = (void *)(uintptr_t)v;
    terminal_symb_map_free(p);
    free(p);
}

static void syl_leaf_destroy_val(uint64_t v)
{
    void *p = (void *)(uintptr_t)v;
    terminal_symb_val_free(p);
    free(p);
}

static int syl_leaf_equals_classic(uint64_t a, uint64_t b)
{
    return terminal_compare_generic((void *)(uintptr_t)a, (void *)(uintptr_t)b);
}

static int syl_leaf_equals_map(uint64_t a, uint64_t b)
{
    return terminal_symb_map_compare_generic((void *)(uintptr_t)a, (void *)(uintptr_t)b);
}

static int syl_leaf_equals_val(uint64_t a, uint64_t b)
{
    return terminal_symb_val_compare_generic((void *)(uintptr_t)a, (void *)(uintptr_t)b);
}

static uint64_t syl_leaf_hash_classic(uint64_t v, uint64_t seed)
{
    unsigned h = terminal_hash_generic((void *)(uintptr_t)v);
    return seed ^ (uint64_t)h ^ ((uint64_t)h << 32);
}

static uint64_t syl_leaf_hash_map(uint64_t v, uint64_t seed)
{
    unsigned h = terminal_symb_map_hash_generic((void *)(uintptr_t)v);
    return seed ^ (uint64_t)h ^ ((uint64_t)h << 32);
}

static uint64_t syl_leaf_hash_val(uint64_t v, uint64_t seed)
{
    unsigned h = terminal_symb_val_hash_generic((void *)(uintptr_t)v);
    return seed ^ (uint64_t)h ^ ((uint64_t)h << 32);
}

static char *syl_leaf_to_str(int complemented, uint64_t v, char *buf, size_t buflen)
{
    (void)complemented;
    return terminal_to_str_generic((void *)(uintptr_t)v, buf, buflen);
}

static char *syl_leaf_to_str_mark(int complemented, uint64_t v, char *buf, size_t buflen)
{
    (void)complemented;
    if (v == 0) {
        strncpy(buf, "NULL", buflen - 1);
        buf[buflen - 1] = '\0';
        return buf;
    }
    strncpy(buf, "T", buflen - 1);
    buf[buflen - 1] = '\0';
    return buf;
}

static int is_bool_leaf(MTBDD t)
{
    return t == mtbdd_false || t == mtbdd_true;
}

static int is_custom_leaf(MTBDD t)
{
    return mtbdd_isleaf(t) && !is_bool_leaf(t);
}

static LEAF_TYPE leaf_from_dd(MTBDD t)
{
    LEAF_TYPE empty = { .pImpl = NULL };
    if (!is_custom_leaf(t)) {
        return empty;
    }
    LEAF_TYPE *p = (LEAF_TYPE *)(uintptr_t)mtbdd_getvalue(t);
    if (p == NULL) {
        return empty;
    }
    return *p;
}

static uint32_t type_from_dd(MTBDD t, uint32_t fallback)
{
    if (is_custom_leaf(t)) {
        return mtbdd_gettype(t);
    }
    return fallback;
}

static void drop_caller_leaf(uint32_t type, void *valuep)
{
    if (valuep == NULL) {
        return;
    }
    if (type == lt_classic) {
        freePimpl(valuep);
    } else if (type == lt_symb_map) {
        terminal_symb_map_free(valuep);
    } else if (type == lt_symb_val) {
        terminal_symb_val_free(valuep);
    }
    free(valuep);
}

static MTBDD make_result_leaf(LEAF_TYPE result, uint32_t type)
{
    if (result.pImpl == NULL) {
        return mtbdd_false;
    }
    LEAF_TYPE *wrap = (LEAF_TYPE *)malloc(sizeof(LEAF_TYPE));
    if (wrap == NULL) {
        freePimpl(&result);
        error_exit("Bad memory allocation.\n");
    }
    wrap->pImpl = result.pImpl;
    MTBDD r = mtbdd_makeleaf(type, (uint64_t)(uintptr_t)wrap);
    /* Unique table owns a deep copy; drop the apply-local payload. */
    drop_caller_leaf(type, wrap);
    return r;
}

static MTBDD makenode_keep(uint32_t var, MTBDD low, MTBDD high)
{
    return mtbdd_makenode(var, low, high);
}

/* -------------------------------------------------------------------------- */
/* Package lifecycle
 * -------------------------------------------------------------------------- */

void init_terminal_symb_val_i(void)
{
    lt_symb_val = sylvan_mt_create_type();
    sylvan_mt_set_create(lt_symb_val, syl_leaf_create_val);
    sylvan_mt_set_destroy(lt_symb_val, syl_leaf_destroy_val);
    sylvan_mt_set_equals(lt_symb_val, syl_leaf_equals_val);
    sylvan_mt_set_hash(lt_symb_val, syl_leaf_hash_val);
    sylvan_mt_set_to_str(lt_symb_val, syl_leaf_to_str_mark);
}

void init_terminal_symb_map_i(void)
{
    lt_symb_map = sylvan_mt_create_type();
    sylvan_mt_set_create(lt_symb_map, syl_leaf_create_map);
    sylvan_mt_set_destroy(lt_symb_map, syl_leaf_destroy_map);
    sylvan_mt_set_equals(lt_symb_map, syl_leaf_equals_map);
    sylvan_mt_set_hash(lt_symb_map, syl_leaf_hash_map);
    sylvan_mt_set_to_str(lt_symb_map, syl_leaf_to_str_mark);
}

void initPackage(unsigned cacheSize, unsigned nodeSize, unsigned varNum)
{
    (void)cacheSize;
    (void)nodeSize;

    if (varNum == 0) {
        varNum = 1;
    }
    if (varNum > (unsigned)INT_MAX) {
        varNum = (unsigned)INT_MAX;
    }

    lace_start(1, 0);
    /* Same default as original MEDUSA: 2 GB unique table + cache. */
    sylvan_set_limits(2000LL * 1024 * 1024, 3, 5);
    sylvan_init_package();
    sylvan_init_mtbdd();
    g_package_live = 1;
    g_varnum = (int)varNum;

    lt_classic = sylvan_mt_create_type();
    sylvan_mt_set_create(lt_classic, syl_leaf_create);
    sylvan_mt_set_destroy(lt_classic, syl_leaf_destroy_classic);
    sylvan_mt_set_equals(lt_classic, syl_leaf_equals_classic);
    sylvan_mt_set_hash(lt_classic, syl_leaf_hash_classic);
    sylvan_mt_set_to_str(lt_classic, syl_leaf_to_str);
    init_terminal_symb_map_i();
    init_terminal_symb_val_i();

    g_nopids = 0;
    g_opid_op = cache_next_opid();
    g_opid_op_param = cache_next_opid();
    g_opid_op_g = cache_next_opid();

    MEDUSA_DBG(.cat = MEDUSA_DBG_LIFECYCLE, .evt = "initPackage", .where = "initPackage",
               .use_n = 1, .n_qubits = (int)varNum,
               .note = "lace_start+sylvan_init");
}

void freePackage(void)
{
    MEDUSA_DBG(.cat = MEDUSA_DBG_LIFECYCLE, .evt = "freePackage", .where = "freePackage");
    setLeafPrintProb(false);
    clearInvSqrtCoeffNormal();
    clearInvSqrtCoeffSymb();
    if (g_package_live) {
        sylvan_quit();
        lace_stop();
        g_package_live = 0;
    }
}

int qBDD_leafcount(qBDD a)
{
    return (int)mtbdd_leafcount(a);
}

void deleteCircuit(qBDD *circ)
{
    if (!circ || *circ == mtbdd_false) {
        return;
    }
    MEDUSA_DBG(.cat = MEDUSA_DBG_LIFECYCLE, .evt = "deleteCircuit", .where = "deleteCircuit",
               .use_bdd = 1, .bdd = (int)(uintptr_t)*circ,
               .is_false = 0, .leaves = qBDD_leafcount(*circ));
    qBDD_unprotect(*circ);
    *circ = mtbdd_false;
}

void forceGC(void)
{
    MEDUSA_DBG(.cat = MEDUSA_DBG_GC, .evt = "forceGC_enter", .where = "forceGC",
               .note = "sylvan_gc");
    sylvan_gc();
    MEDUSA_DBG(.cat = MEDUSA_DBG_GC, .evt = "forceGC_leave", .where = "forceGC");
}

void clearOpCache(void)
{
    cache_clear();
}

void q_fprintdot(FILE *out, qBDD a)
{
    mtbdd_fprintdot(out, a);
}

size_t qBDD_symbolicMapLType(void) { return lt_symb_map; }
size_t qBDD_symbolicValLType(void) { return lt_symb_val; }
size_t qBDD_classicLType(void) { return lt_classic; }

size_t qBDD_getTerminalType(qBDD terminal)
{
    if (!mtbdd_isleaf(terminal) || is_bool_leaf(terminal)) {
        return 0;
    }
    return mtbdd_gettype(terminal);
}

size_t qBDD_level(qBDD node)
{
    if (mtbdd_isleaf(node)) {
        return (size_t)g_varnum;
    }
    return mtbdd_getvar(node);
}

/* -------------------------------------------------------------------------- */
/* Reference counting — Sylvan values table (Buddy addref/delref analogue)
 * -------------------------------------------------------------------------- */

qBDD qBDD_protect(qBDD toProtect)
{
    MTBDD r = mtbdd_ref(toProtect);
    MEDUSA_DBG(.cat = MEDUSA_DBG_PROTECT, .evt = "protect", .where = "qBDD_protect",
               .use_bdd = 1, .bdd = (int)(uintptr_t)toProtect,
               .is_false = qBDD_isFalse(toProtect), .leaves = 0);
    return r;
}

qBDD qBDD_unprotect(qBDD toUnprotect)
{
    mtbdd_deref(toUnprotect);
    MEDUSA_DBG(.cat = MEDUSA_DBG_PROTECT, .evt = "unprotect", .where = "qBDD_unprotect",
               .use_bdd = 1, .bdd = (int)(uintptr_t)toUnprotect,
               .is_false = qBDD_isFalse(toUnprotect), .leaves = 0);
    return toUnprotect;
}

int qBDD_isFalse(qBDD toCheck) { return toCheck == mtbdd_false; }

int qBDD_isTerminal(qBDD toCheck) { return mtbdd_isleaf(toCheck); }

int qBDD_isInternal(qBDD toCheck) { return mtbdd_isnode(toCheck); }

qBDD qBDD_false(void) { return mtbdd_false; }
qBDD qBDD_true(void) { return mtbdd_true; }

qBDD qBDD_maketerminal(size_t type, void *valuep)
{
    MTBDD r = mtbdd_makeleaf((uint32_t)type, (uint64_t)(uintptr_t)valuep);
    /* create() interned a deep copy; Buddy-shaped callers never free valuep. */
    drop_caller_leaf((uint32_t)type, valuep);
    return r;
}

qBDD cube(int value, int width, qBDD *variables, qBDD leaf1, qBDD leaf0)
{
    return mtbdd_cube2(value, width, variables, leaf1, leaf0);
}

qBDD qBDD_getHigh(qBDD a) { return mtbdd_gethigh(a); }
qBDD qBDD_getLow(qBDD a) { return mtbdd_getlow(a); }

size_t qBDD_getVar(qBDD a)
{
    if (mtbdd_isleaf(a)) {
        return (size_t)g_varnum;
    }
    return mtbdd_getvar(a);
}

LEAF_TYPE qBDD_getTerminalValue(qBDD a)
{
    if (!is_custom_leaf(a)) {
        LEAF_TYPE empty = { .pImpl = NULL };
        return empty;
    }
    void *val = (void *)(uintptr_t)mtbdd_getvalue(a);
    if (val == NULL) {
        printf("Warning: qBDD_getTerminalValue called on node with NULL value\n");
        LEAF_TYPE empty = { .pImpl = NULL };
        return empty;
    }
    return *(LEAF_TYPE *)val;
}

/* -------------------------------------------------------------------------- */
/* Apply — official Sylvan mtbdd_apply / uapply / applyp
 *
 * cache_get3(opid, dd, ...) requires opid from cache_next_opid() (high 24
 * bits). A function pointer OR'd into the node index collides and can hang.
 * Nested RUN from a worker deadlocks; CALL when already in a Lace task.
 * -------------------------------------------------------------------------- */

TASK_DECL_3(MTBDD, syl_leaf_binp, MTBDD *, MTBDD *, size_t);
TASK_DECL_3(MTBDD, syl_leaf_binpp, MTBDD *, MTBDD *, size_t);
TASK_DECL_2(MTBDD, syl_leaf_una, MTBDD, size_t);
TASK_DECL_2(MTBDD, syl_leaf_unap, MTBDD, size_t);
TASK_DECL_3(MTBDD, syl_bin_g, MTBDD *, MTBDD *, size_t);
TASK_DECL_3(MTBDD, syl_bin_gp, MTBDD *, MTBDD *, size_t);

/* Set TLS on the Lace worker, then apply. Function pointers stored on main
 * are invisible to the worker (Grover Toffoli hang / NULL callback). */
TASK_DECL_5(MTBDD, syl_bin_entry, MTBDD, MTBDD, uint64_t, uint64_t, size_t);
TASK_IMPL_5(MTBDD, syl_bin_entry, MTBDD, a, MTBDD, b, uint64_t, opfn, uint64_t, kind, size_t, param)
{
    g_in_task = 1;
    mtbdd_applyp_op cop;
    uint64_t opid = opid_of(opfn);
    if (kind == 0) {
        g_leaf_bin = (LEAF_TYPE (*)(LEAF_TYPE, LEAF_TYPE))(uintptr_t)opfn;
        cop = TASK(syl_leaf_binp);
    } else if (kind == 1) {
        g_leaf_binp = (LEAF_TYPE (*)(LEAF_TYPE, LEAF_TYPE, size_t))(uintptr_t)opfn;
        cop = TASK(syl_leaf_binpp);
    } else if (kind == 2) {
        g_bin_g = (qBDD (*)(qBDD, qBDD))(uintptr_t)opfn;
        cop = TASK(syl_bin_g);
    } else {
        g_bin_gp = (qBDD (*)(qBDD, qBDD, size_t))(uintptr_t)opfn;
        cop = TASK(syl_bin_gp);
    }
    return CALL(mtbdd_applyp, a, b, param, cop, opid);
}

TASK_DECL_4(MTBDD, syl_una_entry, MTBDD, uint64_t, uint64_t, size_t);
TASK_IMPL_4(MTBDD, syl_una_entry, MTBDD, t, uint64_t, opfn, uint64_t, kind, size_t, param)
{
    g_in_task = 1;
    if (kind == 0) {
        g_leaf_una = (LEAF_TYPE (*)(LEAF_TYPE))(uintptr_t)opfn;
        return CALL(mtbdd_uapply, t, TASK(syl_leaf_una), opid_of(opfn));
    }
    g_leaf_unap = (LEAF_TYPE (*)(LEAF_TYPE, size_t))(uintptr_t)opfn;
    return CALL(mtbdd_uapply, t, TASK(syl_leaf_unap), param);
}

static MTBDD syl_run_bin(MTBDD a, MTBDD b, uint64_t opfn, uint64_t kind, size_t param)
{
    if (g_in_task) {
        LACE_VARS;
        return CALL(syl_bin_entry, a, b, opfn, kind, param);
    }
    return RUN(syl_bin_entry, a, b, opfn, kind, param);
}

static MTBDD syl_run_una(MTBDD t, uint64_t opfn, uint64_t kind, size_t param)
{
    if (g_in_task) {
        LACE_VARS;
        return CALL(syl_una_entry, t, opfn, kind, param);
    }
    return RUN(syl_una_entry, t, opfn, kind, param);
}

TASK_IMPL_3(MTBDD, syl_leaf_binp, MTBDD *, pa, MTBDD *, pb, size_t, p)
{
    (void)p;
    MTBDD a = *pa, b = *pb;
    if (a == mtbdd_false && b == mtbdd_false) {
        return mtbdd_false;
    }
    if (mtbdd_isleaf(a) && mtbdd_isleaf(b)) {
        uint32_t type = type_from_dd(a, type_from_dd(b, lt_classic));
        return make_result_leaf(g_leaf_bin(leaf_from_dd(a), leaf_from_dd(b)), type);
    }
    return mtbdd_invalid;
}

TASK_IMPL_3(MTBDD, syl_leaf_binpp, MTBDD *, pa, MTBDD *, pb, size_t, p)
{
    MTBDD a = *pa, b = *pb;
    if (mtbdd_isleaf(a) && mtbdd_isleaf(b)) {
        uint32_t type = type_from_dd(a, type_from_dd(b, lt_classic));
        return make_result_leaf(g_leaf_binp(leaf_from_dd(a), leaf_from_dd(b), p), type);
    }
    return mtbdd_invalid;
}

TASK_IMPL_2(MTBDD, syl_leaf_una, MTBDD, t, size_t, unused)
{
    (void)unused;
    if (mtbdd_isleaf(t)) {
        uint32_t type = type_from_dd(t, lt_classic);
        return make_result_leaf(g_leaf_una(leaf_from_dd(t)), type);
    }
    return mtbdd_invalid;
}

TASK_IMPL_2(MTBDD, syl_leaf_unap, MTBDD, t, size_t, arg)
{
    if (mtbdd_isleaf(t)) {
        uint32_t type = type_from_dd(t, lt_classic);
        return make_result_leaf(g_leaf_unap(leaf_from_dd(t), arg), type);
    }
    return mtbdd_invalid;
}

TASK_IMPL_3(MTBDD, syl_bin_g, MTBDD *, pa, MTBDD *, pb, size_t, p)
{
    (void)p;
    g_apply_valid = 1;
    MTBDD r = g_bin_g(*pa, *pb);
    return g_apply_valid ? r : mtbdd_invalid;
}

TASK_IMPL_3(MTBDD, syl_bin_gp, MTBDD *, pa, MTBDD *, pb, size_t, p)
{
    g_apply_valid = 1;
    MTBDD r = g_bin_gp(*pa, *pb, p);
    return g_apply_valid ? r : mtbdd_invalid;
}

TASK_DECL_3(MTBDD, syl_task_node, uint32_t, MTBDD, MTBDD);
TASK_IMPL_3(MTBDD, syl_task_node, uint32_t, var, MTBDD, low, MTBDD, high)
{
    g_in_task++;
    mtbdd_refs_push(low);
    mtbdd_refs_push(high);
    MTBDD r = makenode_keep(var, low, high);
    mtbdd_refs_pop(2);
    g_in_task--;
    return r;
}

qBDD newqBDD(unsigned int target, qBDD lhs, qBDD rhs)
{
    if (g_in_task) {
        mtbdd_refs_push(lhs);
        mtbdd_refs_push(rhs);
        qBDD res = makenode_keep(target, lhs, rhs);
        mtbdd_refs_pop(2);
        return res;
    }
    return RUN(syl_task_node, (uint32_t)target, lhs, rhs);
}

static MTBDD cube_build(int assignment, int width, qBDD *variables, MTBDD leaf1, MTBDD leaf0)
{
    MTBDD acc = leaf1;
    mtbdd_refs_push(acc);
    mtbdd_refs_push(leaf0);
    for (int i = width - 1; i >= 0; i--) {
        uint32_t v = (uint32_t)i;
        if (variables && mtbdd_isnode(variables[i])) {
            v = mtbdd_getvar(variables[i]);
        }
        int bit = 0;
        if (i < 31) {
            bit = (assignment >> i) & 1;
        }
        MTBDD next = bit ? makenode_keep(v, leaf0, acc) : makenode_keep(v, acc, leaf0);
        mtbdd_refs_pop(1);
        acc = mtbdd_refs_push(next);
    }
    mtbdd_refs_pop(2);
    return acc;
}

TASK_DECL_5(MTBDD, syl_task_cube, int, int, uint64_t, MTBDD, MTBDD);
TASK_IMPL_5(MTBDD, syl_task_cube, int, assignment, int, width, uint64_t, vars_raw,
            MTBDD, leaf1, MTBDD, leaf0)
{
    g_in_task++;
    MTBDD acc = cube_build(assignment, width, (qBDD *)(uintptr_t)vars_raw, leaf1, leaf0);
    g_in_task--;
    return acc;
}

qBDD binary_apply(qBDD l, qBDD r, LEAF_TYPE (*op)(LEAF_TYPE, LEAF_TYPE))
{
    return syl_run_bin(l, r, (uint64_t)(uintptr_t)op, 0, 0);
}

qBDD binary_apply_param(qBDD l, qBDD r, LEAF_TYPE (*op)(LEAF_TYPE, LEAF_TYPE, size_t), size_t param)
{
    return syl_run_bin(l, r, (uint64_t)(uintptr_t)op, 1, param);
}

qBDD unary_apply(qBDD l, LEAF_TYPE (*op)(LEAF_TYPE))
{
    return syl_run_una(l, (uint64_t)(uintptr_t)op, 0, 0);
}

TASK_DECL_3(MTBDD, syl_unap_app, MTBDD, uint64_t, size_t);
TASK_IMPL_3(MTBDD, syl_unap_app, MTBDD, t, uint64_t, opfn, size_t, arg)
{
    g_in_task = 1;
    uint64_t opid = opid_of(opfn);
    MTBDD result;
    sylvan_gc_test();
    if (cache_get3(opid, t, opfn, arg, &result)) {
        return result;
    }
    if (mtbdd_isleaf(t)) {
        LEAF_TYPE (*op)(LEAF_TYPE, size_t) =
            (LEAF_TYPE (*)(LEAF_TYPE, size_t))(uintptr_t)opfn;
        uint32_t type = type_from_dd(t, lt_classic);
        result = make_result_leaf(op(leaf_from_dd(t), arg), type);
        cache_put3(opid, t, opfn, arg, result);
        return result;
    }
    MTBDD low = mtbdd_refs_push(CALL(syl_unap_app, mtbdd_getlow(t), opfn, arg));
    MTBDD high = mtbdd_refs_push(CALL(syl_unap_app, mtbdd_gethigh(t), opfn, arg));
    result = makenode_keep(mtbdd_getvar(t), low, high);
    mtbdd_refs_pop(2);
    cache_put3(opid, t, opfn, arg, result);
    return result;
}

qBDD unary_apply_param(qBDD l, LEAF_TYPE (*op)(LEAF_TYPE, size_t), size_t arg)
{
    uint64_t opfn = (uint64_t)(uintptr_t)op;
    if (g_in_task) {
        LACE_VARS;
        return CALL(syl_unap_app, l, opfn, arg);
    }
    return RUN(syl_unap_app, l, opfn, arg);
}

qBDD binary_apply_guarded(qBDD l, qBDD r, qBDD (*op)(qBDD, qBDD))
{
    return syl_run_bin(l, r, (uint64_t)(uintptr_t)op, 2, 0);
}

qBDD binary_apply_guarded_param(qBDD l, qBDD r, qBDD (*op)(qBDD, qBDD, size_t), size_t param)
{
    return syl_run_bin(l, r, (uint64_t)(uintptr_t)op, 3, param);
}

TASK_DECL_3(MTBDD, syl_una_g_app, MTBDD, uint64_t, size_t);
TASK_IMPL_3(MTBDD, syl_una_g_app, MTBDD, t, uint64_t, opfn, size_t, arg)
{
    g_in_task = 1;
    uint64_t opid = opid_of(opfn);
    MTBDD result;
    qBDD (*op)(qBDD, size_t) = (qBDD (*)(qBDD, size_t))(uintptr_t)opfn;
    sylvan_gc_test();
    if (cache_get3(opid, t, opfn, arg, &result)) {
        return result;
    }
    g_apply_valid = 1;
    result = op(t, arg);
    if (g_apply_valid) {
        cache_put3(opid, t, opfn, arg, result);
        return result;
    }
    if (mtbdd_isleaf(t)) {
        return result;
    }
    MTBDD low = mtbdd_refs_push(CALL(syl_una_g_app, mtbdd_getlow(t), opfn, arg));
    MTBDD high = mtbdd_refs_push(CALL(syl_una_g_app, mtbdd_gethigh(t), opfn, arg));
    result = makenode_keep(mtbdd_getvar(t), low, high);
    mtbdd_refs_pop(2);
    cache_put3(opid, t, opfn, arg, result);
    return result;
}

qBDD unary_apply_guarded(qBDD l, qBDD (*op)(qBDD, size_t), size_t arg)
{
    uint64_t opfn = (uint64_t)(uintptr_t)op;
    if (g_in_task) {
        LACE_VARS;
        return CALL(syl_una_g_app, l, opfn, arg);
    }
    return RUN(syl_una_g_app, l, opfn, arg);
}

/* -------------------------------------------------------------------------- */
/* bdd_operation — original mtbdd_apply_gate / apply_cgate
 *
 * targets[0 .. controlNum-1] = controls, targets[controlNum] = target.
 * Missing nodes are applied as op(var, dd, dd) (or control: makenode with
 * identity low / recursed high).
 * -------------------------------------------------------------------------- */

typedef qBDD (*gate_op_t)(size_t, qBDD, qBDD);
typedef qBDD (*gate_op_param_t)(size_t, qBDD, qBDD, size_t);
typedef qBDD (*gate_op_g_t)(size_t, qBDD);

static uint64_t targets_key(size_t *targets, size_t controlNum, size_t cidx)
{
    uint64_t k = ((uint64_t)cidx << 32) ^ (uint64_t)controlNum;
    size_t i;
    for (i = 0; i <= controlNum; i++) {
        k = k * 0x9e3779b97f4a7c15ULL + (uint64_t)targets[i];
    }
    return k;
}

TASK_DECL_5(MTBDD, syl_op, MTBDD, uint64_t, uint64_t, size_t, size_t);
TASK_IMPL_5(MTBDD, syl_op, MTBDD, dd, uint64_t, opfn, uint64_t, targets_raw,
            size_t, ctrl_flags, size_t, param)
{
    g_in_task = 1;
    size_t *targets = (size_t *)(uintptr_t)targets_raw;
    size_t controlNum = ctrl_flags & 0xffffu;
    size_t cidx = (ctrl_flags >> 16) & 0xffffu;
    int has_param = (int)((ctrl_flags >> 32) & 1u);
    uint64_t opid = has_param ? g_opid_op_param : g_opid_op;
    uint64_t tkey = targets_key(targets, controlNum, cidx);
    MTBDD result;

    sylvan_gc_test();
    if (cache_get3(opid, dd, opfn, tkey, &result)) {
        return result;
    }

    if (dd == mtbdd_false) {
        return mtbdd_false;
    }

    uint32_t want = (cidx < controlNum)
                        ? (uint32_t)targets[cidx]
                        : (uint32_t)targets[controlNum];

    int skipped = mtbdd_isleaf(dd) || want < mtbdd_getvar(dd);
    size_t next_flags;

    if (skipped) {
        if (cidx < controlNum) {
            next_flags = controlNum | ((cidx + 1) << 16) | ((size_t)has_param << 32);
            MTBDD high = mtbdd_refs_push(CALL(syl_op, dd, opfn, targets_raw, next_flags, param));
            result = makenode_keep(want, dd, high);
            mtbdd_refs_pop(1);
            cache_put3(opid, dd, opfn, tkey, result);
            return result;
        }
        if (has_param) {
            gate_op_param_t op = (gate_op_param_t)(uintptr_t)opfn;
            result = op((size_t)want, dd, dd, param);
        } else {
            gate_op_t op = (gate_op_t)(uintptr_t)opfn;
            result = op((size_t)want, dd, dd);
        }
        cache_put3(opid, dd, opfn, tkey, result);
        return result;
    }

    uint32_t var = mtbdd_getvar(dd);
    if (cidx < controlNum && var == (uint32_t)targets[cidx]) {
        next_flags = controlNum | ((cidx + 1) << 16) | ((size_t)has_param << 32);
        MTBDD high = mtbdd_refs_push(CALL(syl_op, mtbdd_gethigh(dd), opfn, targets_raw,
                                          next_flags, param));
        result = makenode_keep(var, mtbdd_getlow(dd), high);
        mtbdd_refs_pop(1);
        cache_put3(opid, dd, opfn, tkey, result);
        return result;
    }
    if (cidx >= controlNum && var == (uint32_t)targets[controlNum]) {
        if (has_param) {
            gate_op_param_t op = (gate_op_param_t)(uintptr_t)opfn;
            result = op((size_t)var, mtbdd_getlow(dd), mtbdd_gethigh(dd), param);
        } else {
            gate_op_t op = (gate_op_t)(uintptr_t)opfn;
            result = op((size_t)var, mtbdd_getlow(dd), mtbdd_gethigh(dd));
        }
        cache_put3(opid, dd, opfn, tkey, result);
        return result;
    }

    next_flags = ctrl_flags;
    MTBDD low = mtbdd_refs_push(CALL(syl_op, mtbdd_getlow(dd), opfn, targets_raw,
                                     next_flags, param));
    MTBDD high = mtbdd_refs_push(CALL(syl_op, mtbdd_gethigh(dd), opfn, targets_raw,
                                      next_flags, param));
    result = makenode_keep(var, low, high);
    mtbdd_refs_pop(2);
    cache_put3(opid, dd, opfn, tkey, result);
    return result;
}

TASK_DECL_5(MTBDD, syl_op_g, MTBDD, uint64_t, uint64_t, size_t, size_t);
TASK_IMPL_5(MTBDD, syl_op_g, MTBDD, dd, uint64_t, opfn, uint64_t, targets_raw,
            size_t, ctrl_flags, size_t, unused)
{
    (void)unused;
    g_in_task = 1;
    size_t *targets = (size_t *)(uintptr_t)targets_raw;
    size_t controlNum = ctrl_flags & 0xffffu;
    size_t cidx = (ctrl_flags >> 16) & 0xffffu;
    uint64_t tkey = targets_key(targets, controlNum, cidx);
    MTBDD result;
    gate_op_g_t op = (gate_op_g_t)(uintptr_t)opfn;

    sylvan_gc_test();
    if (cache_get3(g_opid_op_g, dd, opfn, tkey, &result)) {
        return result;
    }

    if (dd == mtbdd_false) {
        return mtbdd_false;
    }

    uint32_t want = (cidx < controlNum)
                        ? (uint32_t)targets[cidx]
                        : (uint32_t)targets[controlNum];

    int skipped = mtbdd_isleaf(dd) || want < mtbdd_getvar(dd);
    MTBDD target_dd = dd;
    size_t next_flags;

    if (skipped) {
        g_op_valid = 1;
        result = op((size_t)want, dd);
        if (g_op_valid) {
            cache_put3(g_opid_op_g, dd, opfn, tkey, result);
            return result;
        }
        if (cidx < controlNum) {
            next_flags = controlNum | ((cidx + 1) << 16);
            MTBDD high = mtbdd_refs_push(CALL(syl_op_g, dd, opfn, targets_raw,
                                              next_flags, 0));
            result = makenode_keep(want, dd, high);
            mtbdd_refs_pop(1);
            cache_put3(g_opid_op_g, dd, opfn, tkey, result);
            return result;
        }
        cache_put3(g_opid_op_g, dd, opfn, tkey, dd);
        return dd;
    }

    uint32_t var = mtbdd_getvar(dd);
    if (cidx < controlNum && var == (uint32_t)targets[cidx]) {
        next_flags = controlNum | ((cidx + 1) << 16);
        MTBDD high = mtbdd_refs_push(CALL(syl_op_g, mtbdd_gethigh(dd), opfn, targets_raw,
                                          next_flags, 0));
        result = makenode_keep(var, mtbdd_getlow(dd), high);
        mtbdd_refs_pop(1);
        cache_put3(g_opid_op_g, dd, opfn, tkey, result);
        return result;
    }

    g_op_valid = 1;
    result = op((size_t)want, target_dd);
    if (g_op_valid) {
        cache_put3(g_opid_op_g, dd, opfn, tkey, result);
        return result;
    }

    next_flags = ctrl_flags;
    MTBDD low = mtbdd_refs_push(CALL(syl_op_g, mtbdd_getlow(dd), opfn, targets_raw,
                                     next_flags, 0));
    MTBDD high = mtbdd_refs_push(CALL(syl_op_g, mtbdd_gethigh(dd), opfn, targets_raw,
                                      next_flags, 0));
    result = makenode_keep(var, low, high);
    mtbdd_refs_pop(2);
    cache_put3(g_opid_op_g, dd, opfn, tkey, result);
    return result;
}

static MTBDD syl_run_op(MTBDD dd, uint64_t opfn, uint64_t tr, size_t flags, size_t param)
{
    if (g_in_task) {
        LACE_VARS;
        return CALL(syl_op, dd, opfn, tr, flags, param);
    }
    return RUN(syl_op, dd, opfn, tr, flags, param);
}

static MTBDD syl_run_op_g(MTBDD dd, uint64_t opfn, uint64_t tr, size_t flags)
{
    if (g_in_task) {
        LACE_VARS;
        return CALL(syl_op_g, dd, opfn, tr, flags, 0);
    }
    return RUN(syl_op_g, dd, opfn, tr, flags, 0);
}

qBDD bdd_operation(qBDD operand, size_t *targets, size_t controlNum,
                   qBDD (*op)(size_t, qBDD, qBDD))
{
    return syl_run_op(operand, (uint64_t)(uintptr_t)op, (uint64_t)(uintptr_t)targets,
                      controlNum, 0);
}

qBDD bdd_operation_param(qBDD operand, size_t *targets, size_t controlNum,
                         qBDD (*op)(size_t, qBDD, qBDD, size_t), size_t param)
{
    size_t flags = controlNum | ((size_t)1 << 32);
    return syl_run_op(operand, (uint64_t)(uintptr_t)op, (uint64_t)(uintptr_t)targets,
                      flags, param);
}

qBDD bdd_operation_guarded(qBDD operand, size_t *targets, size_t controlNum,
                           qBDD (*op)(size_t, qBDD))
{
    return syl_run_op_g(operand, (uint64_t)(uintptr_t)op, (uint64_t)(uintptr_t)targets,
                        controlNum);
}

void validateOperationResult(void) { g_op_valid = 1; }
void invalidateOperationResult(void) { g_op_valid = 0; }
void validateApplyResult(void) { g_apply_valid = 1; }
void invalidateApplyResult(void) { g_apply_valid = 0; }

/* -------------------------------------------------------------------------- */
/* Buddy shims used by leaves
 * -------------------------------------------------------------------------- */

int bdd_varnum(void) { return g_varnum; }

int bdd_setvarnum(int n)
{
    if (n > g_varnum) {
        g_varnum = n;
    }
    return 0;
}

qBDD bdd_ithvar(int i) { return mtbdd_ithvar((uint32_t)i); }

int bdd_error(int e)
{
    error_exit("bdd_error %d\n", e);
    return 0;
}

qBDD mtbdd_maketerminal(void *valuep, mtbdd_terminal_type type)
{
    return qBDD_maketerminal(type, valuep);
}

void *mtbdd_getTerminalValue(qBDD terminal)
{
    if (!is_custom_leaf(terminal)) {
        return NULL;
    }
    return (void *)(uintptr_t)mtbdd_getvalue(terminal);
}

qBDD mtbdd_cube2(int assignment, int width, qBDD *variables, qBDD leaf1, qBDD leaf0)
{
    if (g_in_task) {
        return cube_build(assignment, width, variables, leaf1, leaf0);
    }
    return RUN(syl_task_cube, assignment, width, (uint64_t)(uintptr_t)variables, leaf1, leaf0);
}

/* EOF interface_sylvan.c */
