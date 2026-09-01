#include "mtbdd_symb_map.h"
#include "error.h"
#include "interface.h"
#include "mtbdd.h"

/// Realloc step for vmap
#define REALLOC_COEF 2

void vmap_init(vmap_t **vm, size_t size)
{
    *vm = my_malloc(sizeof(vmap_t));
    (*vm)->msize = size;
    (*vm)->map = my_malloc(sizeof(coef_t) * size);
    (*vm)->next_var = 0;
    (*vm)->mappings = NULL;
}

void vmap_add(vmap_t *vm, vars_t old)
{
    if (vm->next_var >= vm->msize) {
        int old_msize = vm->msize;
        vm->msize *= REALLOC_COEF;
        vm->map = my_realloc(vm->map, sizeof(coef_t) * vm->msize);

        for (int i = old_msize; i < vm->msize; i++) {
            init_generic(vm->map[i]);
        }
    }

    init_set_generic(vm->map[vm->next_var], vm->map[old]);
    vm->next_var++;
}


void vmap_clear(vmap_t *vm)
{
    for (size_t i = 0; i < vm->next_var; i++) {
        clear_generic(vm->map[i]);
    }
    free(vm->map);
    vm->msize = 0;
}

void vmap_delete(vmap_t *vm)
{
    mapping_entry_t *e = vm->mappings;
    while (e != NULL) {
        mapping_entry_t *next = e->next;
        free(e);
        e = next;
    }
    vm->mappings = NULL;
    vmap_clear(vm);
    free(vm);
}

qBDD vmap_lookup(vmap_t *m, qBDD a) {
    for (mapping_entry_t *e = m->mappings; e; e = e->next) {
        if (e->original == a) return e->mapped;
    }
    return 0;
}

void vmap_insert(vmap_t *m, qBDD orig, qBDD mapped) {
    mapping_entry_t *e = my_malloc(sizeof(mapping_entry_t));
    e->original = orig;
    e->mapped = mapped;
    e->next = m->mappings;
    m->mappings = e;
}

qBDD my_mtbdd_to_symb_map_i(qBDD t, size_t m) {
    return unary_apply_guarded(t, mtbdd_to_symb_map_i, m);
}

/* end of "mtbdd_symb_map.c" */
