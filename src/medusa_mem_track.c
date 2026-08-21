/**
 * @file medusa_mem_track.c
 */
#include "medusa_mem_track.h"

static size_t g_pimpl_allocs;
static size_t g_pimpl_frees;
static size_t g_wrap_allocs;

void medusa_mem_reset(void) {
    g_pimpl_allocs = 0;
    g_pimpl_frees = 0;
    g_wrap_allocs = 0;
}

void medusa_mem_note_pimpl_alloc(void) { g_pimpl_allocs++; }
void medusa_mem_note_pimpl_free(void)  { g_pimpl_frees++; }
void medusa_mem_note_wrap_alloc(void)  { g_wrap_allocs++; }

void medusa_mem_get(size_t *pimpl_allocs, size_t *pimpl_frees, size_t *wrap_allocs) {
    if (pimpl_allocs) *pimpl_allocs = g_pimpl_allocs;
    if (pimpl_frees)  *pimpl_frees  = g_pimpl_frees;
    if (wrap_allocs)  *wrap_allocs  = g_wrap_allocs;
}
