/**
 * @file medusa_mem_track.h
 * Lightweight counters for pImpl / apply-wrapper allocations (leak tests).
 * note_* are static inline on extern counters so hot leaf paths pay no call (#33).
 */
#ifndef MEDUSA_MEM_TRACK_H
#define MEDUSA_MEM_TRACK_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

extern size_t medusa_g_pimpl_allocs;
extern size_t medusa_g_pimpl_frees;
extern size_t medusa_g_wrap_allocs;

void medusa_mem_reset(void);
void medusa_mem_get(size_t *pimpl_allocs, size_t *pimpl_frees, size_t *wrap_allocs);

static inline void medusa_mem_note_pimpl_alloc(void) { medusa_g_pimpl_allocs++; }
static inline void medusa_mem_note_pimpl_free(void)  { medusa_g_pimpl_frees++; }
static inline void medusa_mem_note_wrap_alloc(void)  { medusa_g_wrap_allocs++; }

/** Live pImpl payloads not yet freed (stored terminals + temps). */
static inline size_t medusa_mem_pimpl_live(void) {
    return medusa_g_pimpl_allocs - medusa_g_pimpl_frees;
}

#ifdef __cplusplus
}
#endif

#endif /* MEDUSA_MEM_TRACK_H */
