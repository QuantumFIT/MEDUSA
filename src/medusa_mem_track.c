/**
 * @file medusa_mem_track.c
 */
#include "medusa_mem_track.h"

size_t medusa_g_pimpl_allocs;
size_t medusa_g_pimpl_frees;
size_t medusa_g_wrap_allocs;

void medusa_mem_reset(void) {
    medusa_g_pimpl_allocs = 0;
    medusa_g_pimpl_frees = 0;
    medusa_g_wrap_allocs = 0;
}

void medusa_mem_get(size_t *pimpl_allocs, size_t *pimpl_frees, size_t *wrap_allocs) {
    if (pimpl_allocs) *pimpl_allocs = medusa_g_pimpl_allocs;
    if (pimpl_frees)  *pimpl_frees  = medusa_g_pimpl_frees;
    if (wrap_allocs)  *wrap_allocs  = medusa_g_wrap_allocs;
}
