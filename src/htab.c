#include <string.h>
#include <stdbool.h>
#include <gmp.h>
#include <assert.h>
#include "htab.h"
#include "hash.h"
#include "error.h"
#include "interface.h"

/// Max allowed average list length
#define AVG_LEN_MAX 2
/// Table resize coefficient
#define RESIZE_COEF 2
/// Obtains a properly casted key from the symbolic item ptr
#define htab_s_get_key(item_p) ((htab_s_key_t)(item_p->data.key))
/// Obtains a properly casted key from the measure item ptr
#define htab_m_get_key(item_p) ((htab_m_key_t)(item_p->data.key))

htab_t* htab_init(size_t n)
{
    htab_t *t = my_malloc(sizeof(htab_t));

    t->arr_ptr = my_malloc(sizeof(htab_item_t*) * n);
    memset(t->arr_ptr, 0, sizeof(htab_item_t*) * n);

    t->size = 0;
    t->arr_size = n;

    return t;
}

/**
 * Symbolic table item dealloc
 */
static void htab_s_del_item(htab_item_t *i)
{
    htab_s_key_t key = htab_s_get_key(i);
    symexp_list_del(key);
    free(i);
}

/**
 * Measure table item dealloc
 */
static void htab_m_del_item(htab_item_t *i)
{
    free(i->data.key);
    free(i);
}

/**
 * Deletes all table items
 */
static void htab_clear(htab_t *t, void (*del_item_func)(htab_item_t*))
{
    htab_item_t *curr;

    for (size_t i = 0; i < t->arr_size; i++) {
        while (t->arr_ptr[i] != NULL) {
            curr = t->arr_ptr[i];
            t->arr_ptr[i] = t->arr_ptr[i]->next;
            del_item_func(curr);
        }
    }
    t->size = 0;
}

/**
 * Deletes the whole table
 */
static void htab_free(htab_t *t, void (*del_item_func)(htab_item_t*))
{
    if (t->arr_ptr != NULL) {
        htab_clear(t, del_item_func);
        free(t->arr_ptr);
    }
    free(t);
}

void htab_s_clear(htab_t *t)
{
    htab_clear(t, htab_s_del_item);
}

void htab_s_free(htab_t *t)
{
    htab_free(t, htab_s_del_item);
}

void htab_m_clear(htab_t *t)
{
    htab_clear(t, htab_m_del_item);
}

void htab_m_free(htab_t *t)
{
    htab_clear(t, htab_m_del_item);
}

/**
 * The hash function used when operating with symbolic hash table items
 */
static size_t htab_s_hash_func(htab_key_t key_raw)
{
    htab_s_key_t key = (htab_s_key_t) key_raw;
    size_t val = 0;
    symexp_list_first(key);
    while (key->active) {
        val = MY_HASH_COMB(val, key->active->data->var);    // var is also uint64_t, so no need to cast
        val = MY_HASH_COMB_GMP(val, key->active->data->coef);
        symexp_list_next(key);
    }
    return val;
}

/**
 * The hash function used when operating with measure hash table items
 */
static size_t htab_m_hash_func(htab_key_t key_raw) {
    return my_str_hash((htab_m_key_t) key_raw);
}

/**
 * Changes the size of the table's array and moves items from the original lists (size must be >0)
 */
static void htab_resize(htab_t *t, size_t newn, size_t (*hash_func)(htab_key_t))
{
    assert(newn != 0);

    // alloc and init new array
    htab_item_t **new_arr_ptr;
    new_arr_ptr = my_malloc(newn * sizeof(htab_item_t*));
    memset(new_arr_ptr, 0, newn * sizeof(htab_item_t*));

    // index change
    htab_item_t *curr;
    htab_item_t *temp; // aux ptr to the elem in the new arr
    size_t new_index; // aux var for the new index of the current elem
    
    for (size_t i = 0; i < t->arr_size; i++) {
        curr = t->arr_ptr[i];

        while (curr != NULL) {
            new_index = hash_func(curr->data.key) % newn;

            // insert into new array
            if (new_arr_ptr[new_index] == NULL) {
                new_arr_ptr[new_index] = curr;
                curr = curr->next; //preserve the original list
                new_arr_ptr[new_index]->next = NULL;
            }
            else {
                temp = new_arr_ptr[new_index];
                while (temp->next != NULL) {
                    temp = temp->next;
                }
                temp->next = curr;
                curr = curr->next; // preserve the original list
                temp->next->next = NULL;
            }
        }
    }

    free(t->arr_ptr);
    t->arr_ptr = new_arr_ptr;
    t->arr_size = newn;
    return;
}

/**
 * Returns whether two symbolic keys are identical
 */
static bool htab_s_key_cmp(htab_s_key_t a, htab_s_key_t b)
{
    bool res = true;

    symexp_list_first(a);
    symexp_list_first(b);

    while(a->active && b->active) {
        if ((a->active->data->var != b->active->data->var)
             || mpz_cmp(a->active->data->coef, b->active->data->coef)) {
                res = false;
                break;
        }
        symexp_list_next(a);
        symexp_list_next(b);
    }
    if (a->active != b->active) {    // Check if both NULL
        res = false;
    }

    return res;
}

htab_s_key_t htab_s_lookup_add(htab_t *t, htab_s_key_t key)
{
    htab_item_t *item = t->arr_ptr[htab_s_hash_func((htab_key_t)key) % t->arr_size];

    // Try to find the item
    while (item != NULL) {
        if (htab_s_key_cmp(htab_s_get_key(item), key)) {
            item->data.value++; // Value counts the number of occurances, currently only for debug purposes
            return htab_s_get_key(item);
        }
        item = item->next;
    }

    // Item not found -> init new item
    item = my_malloc(sizeof(htab_item_t));
    item->data.key = key;
    item->data.value = 1;
    item->next = NULL;
    // Insert new item into the table
    size_t new_index = htab_s_hash_func((htab_key_t)key) % t->arr_size;
    if (t->arr_ptr[new_index] == NULL) {
        t->arr_ptr[new_index] = item;
    }
    else {
        htab_item_t *temp = t->arr_ptr[new_index];
        while (temp->next != NULL) {
            temp = temp->next;
        }
        temp->next = item;
    }
    // Update table size
    t->size++;
    if (((t->size + 0.0)/ t->arr_size) > AVG_LEN_MAX) { // +0.0 because of non-integer division
        htab_resize(t, t->size * RESIZE_COEF, htab_s_hash_func);
    }

    return htab_s_get_key(item);
}

void htab_s_lookup_remove(htab_t *t, htab_s_key_t key)
{
    size_t item_index = htab_s_hash_func((htab_key_t)key) % t->arr_size;
    htab_item_t *item = t->arr_ptr[item_index];
    htab_item_t *prev = NULL;

    // Find the item
    while (item != NULL) {
        if (htab_s_get_key(item) == key) {
            item->data.value--;
            if (!item->data.value) {
                if (prev) {
                    prev->next = item->next;
                }
                else {
                    t->arr_ptr[item_index] = item->next;
                }
                htab_s_del_item(item);
            }
            break;
        }
        prev = item;
        item = item->next;
    }
}

void htab_m_lookup_add(htab_t *t, htab_m_key_t key)
{
    htab_item_t *item = t->arr_ptr[htab_m_hash_func(key) % t->arr_size];

    // find the item
    while (item != NULL) {
        if (strcmp(item->data.key,key) == 0) {
            item->data.value++;
            return;
        }
        item = item->next;
    }

    item = my_malloc(sizeof(htab_item_t));
    // item init
    htab_m_key_t key_temp = my_malloc(sizeof(char) * (strlen(key) + 1));
    item->data.key = (htab_key_t)(strcpy(key_temp, key));
    item->data.value = 1;
    item->next = NULL;

    // insert new item into the table
    size_t new_index = htab_m_hash_func(key) % t->arr_size;
    if (t->arr_ptr[new_index] == NULL) {
        t->arr_ptr[new_index] = item;
    }
    else {
        htab_item_t *temp = t->arr_ptr[new_index];
        while (temp->next != NULL) {
            temp = temp->next;
        }
        temp->next = item;
    }

    t->size++;
    if (((t->size + 0.0) / t->arr_size) > AVG_LEN_MAX) { // +0.0 because of non-integer division
        htab_resize(t, t->size * RESIZE_COEF, htab_m_hash_func);
    }
}

void htab_m_print_all(htab_t *t, FILE *output)
{
    fprintf(output, "Sampled results:\n");

    htab_item_t *curr;
    for (size_t i = 0; i < t->arr_size; i++) {
        curr = t->arr_ptr[i];

        while (curr != NULL) {
            fprintf(output,"    \'");
            // key stored as LSBF
            for(int i=(strlen(curr->data.key)-1); i >= 0 ;i--) {
                putc(htab_m_get_key(curr)[i], output);
            }
            fprintf(output,"\'    %d\n", curr->data.value);
            curr = curr->next;
        }
    }
    return;
}

/* end of "htab.c" */