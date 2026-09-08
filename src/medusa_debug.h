/**
 * @file medusa_debug.h
 * @brief Compile-time gated diagnostics for Medusa (GC / protect / symb flakes).
 *
 * Enable
 * ------
 *   make buddy_doubles_f64 MEDUSA_DEBUG=1
 *   # or: CFLAGS+=-DMEDUSA_DEBUG
 *
 * Runtime filter (only when compiled in)
 * --------------------------------------
 *   MEDUSA_DEBUG=1                 all categories
 *   MEDUSA_DEBUG=gc,symb,norm      subset (comma-separated)
 *   MEDUSA_DEBUG_FILE=path         output file (default: stderr)
 *
 * Output format: JSON Lines (one object per line), UTF-8.
 * Example:
 *   {"seq":12,"cat":"gc","evt":"forceGC_before","where":"symb_eval",
 *    "bdd":7156,"ref":1,"is_false":0,"leaves":1,"total":1.0,"n":15,"loop":3}
 *
 * Categories (edit medusa_debug.c defaults / env list to tune noise):
 *   lifecycle  package init/teardown, deleteCircuit
 *   protect    qBDD_protect / unprotect (+ refcount when available)
 *   gc         forceGC enter/leave + circ snapshot
 *   symb       symb_init / refine / eval
 *   gate       per-gate (noisy; off unless listed)
 *   norm       total_prob checkpoints
 *   leaf       terminal freefun (noisy; off unless listed)
 *
 * Without -DMEDUSA_DEBUG, all MEDUSA_DBG(*) macros compile to nothing.
 */

#ifndef MEDUSA_DEBUG_H
#define MEDUSA_DEBUG_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    MEDUSA_DBG_LIFECYCLE = 1u << 0,
    MEDUSA_DBG_PROTECT   = 1u << 1,
    MEDUSA_DBG_GC        = 1u << 2,
    MEDUSA_DBG_SYMB      = 1u << 3,
    MEDUSA_DBG_GATE      = 1u << 4,
    MEDUSA_DBG_NORM      = 1u << 5,
    MEDUSA_DBG_LEAF      = 1u << 6,
    MEDUSA_DBG_ALL       = 0xffffffffu
};

/** Optional fields for one log line. Zero-initialize; set only what you need. */
typedef struct medusa_dbg_event {
    unsigned cat;           /**< MEDUSA_DBG_* bit */
    const char *evt;        /**< short event id, e.g. "forceGC_before" */
    const char *where;      /**< function / site name */

    int use_bdd;
    int bdd;                /**< qBDD node id */
    int ref;                /**< Buddy refcou, or -1 if unknown */
    int is_false;
    int leaves;

    int use_total;
    double total;           /**< qBDD_total_prob snapshot */

    int use_n;
    int n_qubits;

    int use_iters;
    uint64_t iters;

    int use_loop;
    int loop_idx;

    int use_mem;
    size_t pimpl_live;
    size_t wrap_allocs;

    const char *gate;       /**< optional gate label */
    const char *note;       /**< free-form; keep short, no quotes */
} medusa_dbg_event_t;

#ifdef MEDUSA_DEBUG

void medusa_dbg_init(void);
int  medusa_dbg_enabled(void);
int  medusa_dbg_cat_on(unsigned cat);
void medusa_dbg_log(const medusa_dbg_event_t *ev);

/** Buddy node refcou peek (implemented in interface_motobuddy.c). -1 if N/A. */
int medusa_dbg_bdd_ref(int bdd);

#define MEDUSA_DBG(...)                                                         \
    do {                                                                        \
        if (medusa_dbg_enabled()) {                                             \
            medusa_dbg_event_t _medusa_dbg_ev_ = { __VA_ARGS__ };               \
            if (medusa_dbg_cat_on(_medusa_dbg_ev_.cat))                          \
                medusa_dbg_log(&_medusa_dbg_ev_);                                \
        }                                                                       \
    } while (0)

#else /* !MEDUSA_DEBUG */

static inline void medusa_dbg_init(void) {}
static inline int  medusa_dbg_enabled(void) { return 0; }
static inline int  medusa_dbg_cat_on(unsigned cat) { (void)cat; return 0; }
static inline void medusa_dbg_log(const medusa_dbg_event_t *ev) { (void)ev; }
static inline int  medusa_dbg_bdd_ref(int bdd) { (void)bdd; return -1; }

#define MEDUSA_DBG(...) ((void)0)

#endif /* MEDUSA_DEBUG */

#ifdef __cplusplus
}
#endif

#endif /* MEDUSA_DEBUG_H */
