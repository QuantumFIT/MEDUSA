#include <stdio.h>
#include <assert.h>
#include "mtbdd_out.h"
#include "error.h"

/// Global variable for the map (needed as custom arguments cannot be added to Sylvan's mtbdd_fprintdot())
static lnum_map_t out_map;

/// Step for array reallocation when the array size isn't sufficient when adding
#define REALLOC_STEP 5

void lnum_map_init(size_t size)
{
    assert(out_map.msize == 0); // Check if not already initialized
    out_map.msize = size;
    out_map.map = my_malloc(sizeof(coef_t*) * size);
    out_map.shifts = my_malloc(sizeof(mp_bitcnt_t) * size);
    out_map.next_var = 0;
}

uint64_t lnum_map_add(coef_t *n_ptr, mp_bitcnt_t shift)
{
    if (out_map.next_var >= out_map.msize) {
        out_map.map = my_realloc(out_map.map, sizeof(coef_t*) * (out_map.msize + REALLOC_STEP));
        out_map.shifts = my_realloc(out_map.shifts, sizeof(mp_bitcnt_t) * (out_map.msize + REALLOC_STEP));
        out_map.msize += REALLOC_STEP;
    }

    out_map.map[out_map.next_var] = n_ptr;
    out_map.shifts[out_map.next_var] = shift;

    return out_map.next_var++; // Returns the current variable that has been just assigned to the number
}

bool lnum_map_is_empty()
{
    return (out_map.next_var == 0);
}

 // REDO
void lnum_map_print(FILE *f)
{
    mpz_t temp;
    mpz_init(temp);
    for (size_t i = 0; i < out_map.next_var; i++) {
        fprintf(f,VAR_NAME_FMT " = ", i);
        mpz_fdiv_q_2exp(temp, *(out_map.map[i]), out_map.shifts[i]);
        if (mpz_out_str(f, 10, temp) == 0) {
            error_exit("An error occurred when writing the value of " VAR_NAME_FMT " to the output file.\n", i);
        }
        fprintf(f,"\n\n");
    }
    mpz_clear(temp);
}

void lnum_map_clear()
{
    free(out_map.map);
    free(out_map.shifts);
    out_map.msize = 0;
    out_map.next_var = 0;
}

/* end of "mtbdd_out.c" */