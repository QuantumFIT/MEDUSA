/**
 * @file medusa_mem_track.h
 * Lightweight counters for pImpl / apply-wrapper allocations (leak tests).
 */
#ifndef MEDUSA_MEM_TRACK_H
#define MEDUSA_MEM_TRACK_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void medusa_mem_reset(void);
void medusa_mem_note_pimpl_alloc(void);
void medusa_mem_note_pimpl_free(void);
void medusa_mem_note_wrap_alloc(void);

void medusa_mem_get(size_t *pimpl_allocs, size_t *pimpl_frees, size_t *wrap_allocs);

/** Live pImpl payloads not yet freed (stored terminals + temps). */
static inline size_t medusa_mem_pimpl_live(void) {
    size_t a = 0, f = 0, w = 0;
    medusa_mem_get(&a, &f, &w);
    return a - f;
}

#ifdef __cplusplus
}
#endif

#endif /* MEDUSA_MEM_TRACK_H */
