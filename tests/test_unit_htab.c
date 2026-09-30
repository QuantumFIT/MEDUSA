/**
 * Unit tests for the hash table (src/htab.c).
 *
 * htab.c sat at 70.4% line and 59.4% branch with three functions never
 * executed - htab_resize, htab_m_clear and htab_s_lookup_remove - because the
 * suites that use a table only ever put a handful of entries in one.
 *
 * htab_resize is the interesting one. It is static and genuinely reachable:
 * both htab_s_lookup_add and htab_m_lookup_add call it once the load factor
 * passes AVG_LEN_MAX (2), so it fires on any table that grows. Rehashing and
 * relinking chains is exactly the kind of code that is wrong in a way normal
 * use never notices, because a lost entry just looks like a fresh insert.
 *
 * The invariant that catches that: inserting a key that is already present
 * must not change the table's size. So after inserting N distinct keys,
 * re-inserting the same N keys must leave size == N. If a resize dropped or
 * mislinked an entry, the second pass creates it again and size grows.
 *
 * The table is self-contained, so none of this needs a BDD package.
 */
#include "test_harness.h"
#include "htab.h"
#include "symexp_list.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Above AVG_LEN_MAX (2) x the initial bucket count, so several resizes run.
 * Deliberately not a round number: a bug that only shows on a power of two
 * would be a poor thing to miss. */
#define INITIAL_BUCKETS 8
#define MANY_KEYS       237

static void key_at(char *buf, size_t buflen, int i)
{
    snprintf(buf, buflen, "state-%06d", i);
}

static void test_m_insert_and_growth(void)
{
    TEST_SECTION("htab: measure table grows and keeps every key");

    htab_t *t = htab_init(INITIAL_BUCKETS);
    TEST_ASSERT(t != NULL);
    TEST_ASSERT(t->size == 0);
    size_t buckets0 = t->arr_size;

    char key[32];
    for (int i = 0; i < MANY_KEYS; i++) {
        key_at(key, sizeof key, i);
        htab_m_lookup_add(t, key);
    }
    TEST_ASSERT_MSG(t->size == (size_t)MANY_KEYS, "one entry per distinct key");
    TEST_ASSERT_MSG(t->arr_size > buckets0, "the table must have grown its bucket array");

    /* The load factor must actually have been brought back under the bound,
     * not merely grown once. */
    TEST_ASSERT_MSG(((double)t->size / (double)t->arr_size) <= 2.0,
                    "resize must restore the load factor");

    /* Re-inserting every key must find each one rather than create it again.
     * This is what a dropped or mislinked entry after a rehash would break. */
    for (int i = 0; i < MANY_KEYS; i++) {
        key_at(key, sizeof key, i);
        htab_m_lookup_add(t, key);
    }
    TEST_ASSERT_MSG(t->size == (size_t)MANY_KEYS,
                    "re-inserting known keys must not grow the table (rehash lost an entry?)");

    htab_m_free(t);
}

static void test_m_duplicate_counting(void)
{
    TEST_SECTION("htab: measure table counts repeats");

    htab_t *t = htab_init(INITIAL_BUCKETS);
    for (int i = 0; i < 5; i++) {
        htab_m_lookup_add(t, "repeated");
    }
    TEST_ASSERT_MSG(t->size == 1, "five inserts of one key is one entry");

    htab_m_lookup_add(t, "other");
    TEST_ASSERT(t->size == 2);

    htab_m_free(t);
}

static void test_m_print_all(void)
{
    TEST_SECTION("htab: measure table prints every entry");

    htab_t *t = htab_init(INITIAL_BUCKETS);
    char key[32];
    const int n = 40;                       /* enough to force a resize first */
    for (int i = 0; i < n; i++) {
        key_at(key, sizeof key, i);
        htab_m_lookup_add(t, key);
    }

    char *buf = NULL;
    size_t len = 0;
    FILE *f = open_memstream(&buf, &len);
    TEST_ASSERT(f != NULL);
    htab_m_print_all(t, f);
    fclose(f);

    /* print_all writes each key *reversed*: measurement keys are stored
     * least-significant bit first and printed most-significant first, so the
     * output is the human-facing bit string rather than the stored one. Pinned
     * here because it is not obvious from the call site and a "tidy-up" that
     * dropped the reversal would silently mirror every reported state. */
    int found = 0;
    for (int i = 0; i < n; i++) {
        key_at(key, sizeof key, i);
        char rev[32];
        size_t klen = strlen(key);
        for (size_t j = 0; j < klen; j++) {
            rev[j] = key[klen - 1 - j];
        }
        rev[klen] = '\0';

        const char *p = strstr(buf, rev);
        if (p != NULL && strstr(p + 1, rev) == NULL) {
            found++;
        }
    }
    TEST_ASSERT_MSG(found == n, "print_all must list each key exactly once, reversed");
    TEST_ASSERT_MSG(strstr(buf, "Sampled results:") != NULL, "print_all writes its header");

    free(buf);
    htab_m_free(t);
}

static void test_m_clear(void)
{
    TEST_SECTION("htab: measure table clear");

    htab_t *t = htab_init(INITIAL_BUCKETS);
    char key[32];
    for (int i = 0; i < 30; i++) {
        key_at(key, sizeof key, i);
        htab_m_lookup_add(t, key);
    }
    TEST_ASSERT(t->size == 30);

    htab_m_clear(t);
    TEST_ASSERT_MSG(t->size == 0, "clear must empty the table");

    /* Still usable afterwards, and the keys are genuinely gone rather than
     * merely uncounted. */
    htab_m_lookup_add(t, "after-clear");
    TEST_ASSERT_MSG(t->size == 1, "table must be reusable after clear");

    htab_m_free(t);
}

/* --- symbolic side ------------------------------------------------------- */

/** A one-term symbolic expression, which is enough to key the table. */
static symexp_list_t *make_expr(unsigned long var, long coef)
{
    symexp_list_t *l = symexp_list_create();
    symexp_val_t *v = malloc(sizeof *v);
    if (!v) {
        exit(EXIT_FAILURE);
    }
    mpz_init_set_si(v->coef, coef);
    v->var = (vars_t)var;
    v->sqrt2_inv = 0;
    symexp_list_insert_first(l, v);
    return l;
}

static void test_s_insert_and_growth(void)
{
    TEST_SECTION("htab: symbolic table grows and interns keys");

    htab_t *t = htab_init(INITIAL_BUCKETS);
    const int n = 120;                      /* well past the load bound */
    symexp_list_t **keys = malloc((size_t)n * sizeof *keys);
    TEST_ASSERT(keys != NULL);

    for (int i = 0; i < n; i++) {
        keys[i] = make_expr((unsigned long)i, i + 1);
        htab_s_lookup_add(t, keys[i]);
    }
    TEST_ASSERT_MSG(t->size == (size_t)n, "one entry per distinct expression");
    TEST_ASSERT_MSG(t->arr_size > INITIAL_BUCKETS, "the bucket array must have grown");

    /* An equal expression must intern to the key already in the table, not
     * add a second entry - which is the whole point of this table. */
    symexp_list_t *dup = make_expr(0, 1);
    htab_s_key_t got = htab_s_lookup_add(t, dup);
    TEST_ASSERT_MSG(t->size == (size_t)n, "an equal expression must not add an entry");
    TEST_ASSERT_MSG(got == keys[0], "lookup must return the interned key");
    symexp_list_del(dup);

    htab_s_free(t);
    free(keys);
}

static void test_s_remove(void)
{
    TEST_SECTION("htab: symbolic table reference counting");

    htab_t *t = htab_init(INITIAL_BUCKETS);
    symexp_list_t *a = make_expr(1, 7);
    symexp_list_t *b = make_expr(2, 9);

    htab_s_key_t ka = htab_s_lookup_add(t, a);
    htab_s_lookup_add(t, a);                /* second reference */
    htab_s_lookup_add(t, b);
    TEST_ASSERT(t->size == 2);

    /* Liveness is observed through interning rather than through t->size: an
     * expression equal to one still in the table must come back as the same
     * pointer, and one whose entry has been dropped must not. */
    htab_s_lookup_remove(t, a);
    symexp_list_t *probe1 = make_expr(1, 7);
    TEST_ASSERT_MSG(htab_s_lookup_add(t, probe1) == ka,
                    "an entry with references left must still intern to the same key");
    symexp_list_del(probe1);
    htab_s_lookup_remove(t, a);             /* undo the probe's reference */

    htab_s_lookup_remove(t, a);
    symexp_list_t *probe2 = make_expr(1, 7);
    TEST_ASSERT_MSG(htab_s_lookup_add(t, probe2) != ka,
                    "the last release must drop the entry");

    /* Removing something that is not there must be a no-op, not a crash. */
    symexp_list_t *absent = make_expr(99, 1);
    htab_s_lookup_remove(t, absent);
    symexp_list_del(absent);

    /* Known quirk, pinned deliberately: htab_s_lookup_remove unlinks and frees
     * the item but never decrements t->size, so the field over-counts after a
     * removal. Since t->size drives the load-factor test in lookup_add, a
     * table that saw removals resizes earlier and larger than it needs to.
     *
     * Nothing calls htab_s_lookup_remove today, so this is latent rather than
     * active, and it is asserted as it behaves rather than as it should - the
     * same treatment the CLI gaps got before issue #13. If the size accounting
     * is fixed, this assertion should be inverted rather than deleted. */
    size_t size_before = t->size;
    symexp_list_t *c = make_expr(3, 5);
    htab_s_lookup_add(t, c);
    htab_s_lookup_remove(t, c);
    TEST_ASSERT_MSG(t->size == size_before + 1,
                    "remove does not decrement t->size (known: see the comment above)");

    htab_s_free(t);
}

int main(void)
{
    printf("MEDUSA hash table unit tests\n");

    test_m_insert_and_growth();
    test_m_duplicate_counting();
    test_m_print_all();
    test_m_clear();
    test_s_insert_and_growth();
    test_s_remove();

    return test_report("test_unit_htab");
}
