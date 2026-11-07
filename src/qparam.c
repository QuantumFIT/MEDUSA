#include "qparam.h"
#include "error.h"

qparam_list_t* qparam_list_create()
{
    qparam_list_t *res = my_malloc(sizeof(qparam_list_t));
    res->first = NULL;
    res->active = NULL;
    return res;
}

void qparam_list_first(qparam_list_t *l)
{
    if (l) {
        l->active =  l->first;
    }
}

void qparam_list_next(qparam_list_t *l)
{
    if (l) {
        l->active = (l->active)? l->active->next : NULL;
    }
}

void qparam_list_insert_first(qparam_list_t *l, uint32_t q_index)
{
    if(l) {
        qparam_t *new_el = my_malloc(sizeof(qparam_t));
        new_el->q_index = q_index;
        new_el->next = l->first;
        l->first = new_el;
    }
}

void qparam_list_del(qparam_list_t *l)
{
    if (l) {
        qparam_t *tmp;
        qparam_list_first(l);
        while (l->active) {
            tmp = l->active;
            qparam_list_next(l);
            // Dealloc:
            free(tmp);
        }
        free(l);
    }
}

/* end of "qparam.c" */