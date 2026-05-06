#include <string.h>
#include <gmp.h>
#include "symexp.h"
#include "htab.h"
#include "error.h"
#include "interface.h"
/// Global symbolic hash table
static htab_t *symexp_table;


void symexp_htab_init(size_t size)
{
    symexp_table = htab_init(size);
}

void symexp_htab_clear()
{
    htab_s_clear(symexp_table);
}

void symexp_htab_delete()
{
    htab_s_free(symexp_table);
}

static symexp_list_t* symexp_htab_add(symexp_list_t *l)
{
    symexp_list_t *res = htab_s_lookup_add(symexp_table, l);
    if (res != l) { // Already is in htab
        symexp_list_del(l);
    }
    return res;
}

symexp_list_t* symexp_init(vars_t v)
{
    symexp_list_t *res = symexp_list_create();
    
    symexp_val_t first_val;
    first_val.var = v;
    mpz_init_set_ui(first_val.coef, 1);
    symexp_list_insert_first(res, &first_val);
    mpz_clear(first_val.coef);

    return symexp_htab_add(res);
}

symexp_list_t* symexp_mul_c(symexp_list_t *a, unsigned long c)
{
    symexp_list_t *res = symexp_list_mkcpy(a);
    symexp_list_mul_c(res, c);
    return symexp_htab_add(res);
}

symexp_list_t* symexp_op(symexp_list_t *a, symexp_list_t *b, symexp_op_t op)
{
    // Case: NULL + NULL, NULL - NULL
    if (a == NULL && b == NULL) {
        return NULL;
    }
    // Case: a + NULL, a - NULL
    else if (b == NULL) {
        return a;
    }
    // Case: NULL + b
    else if (a == NULL && 
            (op == SYMEXP_ADD)) {
        return b;
    }
    
    symexp_list_t *res;
    // Case: NULL - b
    if (a == NULL && 
        (op == SYMEXP_SUB)) {
        res = symexp_list_mkcpy(b);
        symexp_list_neg(res);
    }
    else {
        res = symexp_list_mkcpy(b);
        if (op == SYMEXP_SUB) {
            symexp_list_neg(res);
        }
        symexp_list_first(res);
        symexp_list_first(a);
        symexp_el_t *res_prev = NULL;
        while(a->active != NULL) {
            // if reached the end of res
            if (res->active == NULL) {
                // insert all the remaining elements at once
                res->active = res_prev;
                while(a->active != NULL) {
                    symexp_list_insert_after(res, a->active->data);
                    symexp_list_next(res); // Moves to the just added element
                    symexp_list_next(a); // Moves to next element
                }
                break;
            }
            // else if need to insert here
            else if (a->active->data->var < res->active->data->var) {
                if (res_prev == NULL) { // inserting before first element of res
                    symexp_list_insert_first(res, a->active->data);
                    // res active stays the same
                    symexp_list_next(a);
                    res_prev = res->first;
                }
                else { // regular insert before the current element
                    res->active = res_prev;
                    symexp_list_insert_after(res, a->active->data);
                    symexp_list_next(res); // Moves to the just added element
                    res_prev = res->active;
                    symexp_list_next(res); // Moves to a new element in res
                    symexp_list_next(a);
                }
            }
            // else if the same variable
            else if (a->active->data->var == res->active->data->var) {
                mpz_add(res->active->data->coef, res->active->data->coef, a->active->data->coef); // 119
                //add_generic(res->active->data->coef, res->active->data->coef, a->active->data->coef);
                // Check if result isn't 0 
                if (mpz_sgn(res->active->data->coef) == 0) {
                    if (res_prev == NULL) {
                        symexp_list_remove_first(res);
                        symexp_list_first(res); // move to new element, prev stays the same
                    }
                    else {
                        res->active = res_prev;
                        symexp_list_remove_after(res);
                        symexp_list_next(res); // move to new element, prev stays the same
                    }
                    symexp_list_next(a);
                }
                else {
                    res_prev = res->active;
                    symexp_list_next(res);
                    symexp_list_next(a);
                }
            }
            // else no insert now
            else {
                res_prev = res->active;
                symexp_list_next(res);
            }
        }
    }

    // Check if cannot be reduced to NULL
    if (res->first == NULL) {
        symexp_list_del(res);
        return NULL;
    }

    return symexp_htab_add(res);
}

bool symexp_cmp(symexp_list_t *a, symexp_list_t *b)
{
    return (a == b)? true : false;
}

char* symexp_to_str(symexp_list_t *l)
{
    char buf[MAX_ST_TO_STR_LEN] = {0};
    int chars_written;
    int buflen = 0;

    if (l == NULL) {
        return "";
    }
    else if (l == SYMEXP_NULL) {
        return "0";
    }
    else {
        symexp_list_first(l);
        while (l->active != NULL) {
            // Append string
            chars_written = gmp_snprintf(buf + buflen, MAX_ST_TO_STR_LEN, "%+Zd[%ld]", l->active->data->coef, l->active->data->var);
            // Was string truncated?
            if (chars_written >= MAX_ST_TO_STR_LEN) {
                error_exit("Allocated string length for leaf value output has not been sufficient.\n");
            }
            else if (chars_written < 0) {
                error_exit("An encoding error has occured when producing leaf value output.\n");
            }
            buflen += chars_written;
            symexp_list_next(l);
        }
    }

    char *new_buf = (char*)my_malloc((buflen + 1) * sizeof(char));
    memcpy(new_buf, buf, buflen * sizeof(char));
    new_buf[buflen] = '\0';
    return new_buf;
}

bool symexp_is_first_var_marked(symexp_list_t *l, bool *is_zero)
{
    bool var_marked = false;
    symexp_list_first(l);
    if (l->active && l->active->data) { 
        if (is_zero[l->active->data->var]) {
            var_marked = true;
        }
    }
    return var_marked;
}

/* end of "symexp.c" */