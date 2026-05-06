#include <stdlib.h>
#include <gmp.h>
#include "symexp_list.h"
#include "error.h"
#include <stdio.h>
#include "interface.h"

symexp_list_t* symexp_list_create()
{
    symexp_list_t *res = my_malloc(sizeof(symexp_list_t));
    res->active = NULL;
    res->first = NULL;
    return res;
}

void symexp_list_first(symexp_list_t *l)
{
    if (l) {
        l->active =  l->first;
    }
}

void symexp_list_next(symexp_list_t *l)
{
    if (l) {
        l->active = (l->active)? l->active = l->active->next : NULL;
    }
}

void symexp_list_insert_first(symexp_list_t *l, symexp_val_t *val)
{
    if(l) {
        symexp_el_t *new_el = my_malloc(sizeof(symexp_el_t));
        new_el->data = my_malloc(sizeof(symexp_val_t));
        new_el->data->var = val->var;
        //init_set_generic(new_el->data->coef, val->coef);
        mpz_init_set(new_el->data->coef, val->coef);  // REDO
        new_el->next = l->first;
        l->first = new_el;
    }
}

void symexp_list_insert_after(symexp_list_t *l, symexp_val_t *val)
{
    if(l && l->active){
        symexp_el_t *new_el = my_malloc(sizeof(symexp_el_t));
        new_el->data = my_malloc(sizeof(symexp_val_t));
        new_el->data->var = val->var;
        //init_set_generic(new_el->data->coef, val->coef);
        mpz_init_set(new_el->data->coef, val->coef);  // REDO
        new_el->next = l->active->next;
        l->active->next = new_el;
    }
}

void symexp_list_remove_first(symexp_list_t *l)
{
    if(l && l->first){
        symexp_el_t *new_first = l->first->next;
        // Check if first isn't active
        if (l->first == l->active) {
            l->active = NULL;
        }
        //clear_generic(l->first->data->coef);
        mpz_clear(l->first->data->coef);
        free(l->first->data);
        free(l->first);
        l->first = new_first;
    }
}

void symexp_list_remove_after(symexp_list_t *l)
{
    if(l && l->active){
        symexp_el_t *el_before = l->active;
        symexp_list_next(l); // now on element that should be deleted
        el_before->next = l->active->next;
        //clear_generic(l->active->data->coef);
        mpz_clear(l->active->data->coef);  // REDO
        free(l->active->data);
        free(l->active);
        l->active = el_before;
    }
}

void symexp_list_neg(symexp_list_t *l)
{
    if(l){
        symexp_list_first(l);
        while (l->active) {
            //neg_generic(l->active->data->coef, l->active->data->coef);
            mpz_neg(l->active->data->coef, l->active->data->coef);
            symexp_list_next(l);
        }
    }
}

// void symexp_list_mul_sqrt2inv(symexp_list_t *l)
// {
//     if(l){
//         symexp_list_first(l);
//         while (l->active) {
//             mul_sqrt2inv_generic(l->active->data->coef, l->active->data->coef);
//             symexp_list_next(l);
//         }
//     }

// }

void symexp_list_mul_c(symexp_list_t *l, unsigned long c)
{
    if(l){
        symexp_list_first(l);
        while (l->active) {
            //mul_ui_generic(l->active->data->coef, l->active->data->coef, c);
            mpz_mul_ui(l->active->data->coef, l->active->data->coef, c);  // REDO
            symexp_list_next(l);
        }
    }
}

void symexp_list_del(symexp_list_t *l)
{
    if (l) {
        symexp_el_t *tmp;
        symexp_list_first(l);
        while (l->active) {
            // Doesn't use other remove functions because now we can skip rewiring
            tmp = l->active;
            symexp_list_next(l);
            // Dealloc:
            //clear_generic(tmp->data->coef);
            mpz_clear(tmp->data->coef);
            free(tmp->data);
            free(tmp);
        }
        free(l);
    }
}

symexp_list_t* symexp_list_mkcpy(symexp_list_t *l)
{
    symexp_list_t *cpy = NULL;
    if (l && l->first) {    // Not empty
        cpy = my_malloc(sizeof(symexp_list_t));
        cpy->active = NULL;
        cpy->first = NULL;

        symexp_list_first(l);
        symexp_el_t *tmp;
        while (l->active != NULL) {
            tmp = my_malloc(sizeof(symexp_el_t));
            tmp->data = my_malloc(sizeof(symexp_val_t));
            tmp->data->var = l->active->data->var; // here invalid read
            tmp->next = NULL;
            //init_set_generic(tmp->data->coef, l->active->data->coef);
            mpz_init_set(tmp->data->coef, l->active->data->coef);  // REDO

            if (cpy->first == NULL) {    // Add the first element
                cpy->first = tmp;
                symexp_list_first(cpy);
            }
            else {
                cpy->active->next = tmp;
                symexp_list_next(cpy);
            }
            symexp_list_next(l);
        }
    }
    cpy->active = NULL;
    return cpy;
}

/* end of "symexp.c" */