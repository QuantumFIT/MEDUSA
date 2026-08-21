/**
 * @file test_harness.h
 * Minimal assert-based test harness for MEDUSA / MoToBuddy API tests.
 * Tracks TEST_SECTION results and prints a colorful summary table at the end.
 */
#ifndef TEST_HARNESS_H
#define TEST_HARNESS_H

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <unistd.h>

/* MoToBuddy: silence default "Garbage collection #N: ..." printf in tests. */
#include "bdd.h"

#ifndef TEST_MAX_SECTIONS
#define TEST_MAX_SECTIONS 128
#endif

#ifndef TEST_SECTION_NAME_LEN
#define TEST_SECTION_NAME_LEN 96
#endif

/** Call after initPackage() so bdd_init's default GBC handler is cleared. */
static inline void test_silence_gbc(void) {
    bdd_gbc_hook(NULL);
}

static int g_tests_run = 0;
static int g_tests_failed = 0;

typedef struct {
    char name[TEST_SECTION_NAME_LEN];
    int asserts;
    int failed;
} test_section_t;

static test_section_t g_sections[TEST_MAX_SECTIONS];
static int g_nsections = 0;
static int g_sec_run_base = 0;
static int g_sec_fail_base = 0;
static int g_sec_open = 0;

static inline int test_use_color(void) {
    static int cached = -1;
    if (cached < 0)
        cached = isatty(STDOUT_FILENO) ? 1 : 0;
    return cached;
}

static inline const char *C_RESET(void)  { return test_use_color() ? "\033[0m" : ""; }
static inline const char *C_BOLD(void)   { return test_use_color() ? "\033[1m" : ""; }
static inline const char *C_GREEN(void)  { return test_use_color() ? "\033[32m" : ""; }
static inline const char *C_RED(void)    { return test_use_color() ? "\033[31m" : ""; }
static inline const char *C_DIM(void)    { return test_use_color() ? "\033[2m" : ""; }
static inline const char *C_CYAN(void)   { return test_use_color() ? "\033[36m" : ""; }

static inline void test_section_close(void) {
    if (!g_sec_open || g_nsections <= 0)
        return;
    test_section_t *s = &g_sections[g_nsections - 1];
    s->asserts = g_tests_run - g_sec_run_base;
    s->failed = g_tests_failed - g_sec_fail_base;
    g_sec_open = 0;
}

static inline void test_section_begin(const char *name) {
    test_section_close();
    if (g_nsections >= TEST_MAX_SECTIONS) {
        fprintf(stderr, "WARN: TEST_MAX_SECTIONS (%d) exceeded; truncating summary\n",
                TEST_MAX_SECTIONS);
        g_sec_run_base = g_tests_run;
        g_sec_fail_base = g_tests_failed;
        fprintf(stdout, "  [%s]\n", name);
        return;
    }
    test_section_t *s = &g_sections[g_nsections++];
    snprintf(s->name, sizeof(s->name), "%s", name);
    s->asserts = 0;
    s->failed = 0;
    g_sec_run_base = g_tests_run;
    g_sec_fail_base = g_tests_failed;
    g_sec_open = 1;
    fprintf(stdout, "  [%s]\n", name);
}

#define TEST_ASSERT(cond) do { \
    g_tests_run++; \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        g_tests_failed++; \
    } \
} while (0)

#define TEST_ASSERT_MSG(cond, msg) do { \
    g_tests_run++; \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d: %s — %s\n", __FILE__, __LINE__, #cond, (msg)); \
        g_tests_failed++; \
    } \
} while (0)

#define TEST_ASSERT_NEAR(a, b, eps) do { \
    g_tests_run++; \
    double _a = (double)(a), _b = (double)(b), _e = (double)(eps); \
    if (fabs(_a - _b) > _e) { \
        fprintf(stderr, "FAIL %s:%d: |%g - %g| > %g\n", __FILE__, __LINE__, _a, _b, _e); \
        g_tests_failed++; \
    } \
} while (0)

#define TEST_SECTION(name) test_section_begin(name)

static inline void test_print_hline(int name_w, int status_w, int asserts_w) {
    putchar('+');
    for (int i = 0; i < name_w + 2; i++) putchar('-');
    putchar('+');
    for (int i = 0; i < status_w + 2; i++) putchar('-');
    putchar('+');
    for (int i = 0; i < asserts_w + 2; i++) putchar('-');
    putchar('+');
    putchar('\n');
}

static inline void test_print_summary_table(const char *suite) {
    test_section_close();

    const int status_w = 6;
    const int asserts_w = 10;
    int name_w = 24;
    for (int i = 0; i < g_nsections; i++) {
        int n = (int)strlen(g_sections[i].name);
        if (n > name_w) name_w = n;
    }
    if ((int)strlen(suite) + 8 > name_w)
        name_w = (int)strlen(suite) + 8;
    if (name_w > 72) name_w = 72;

    int passed = 0, failed_sec = 0;
    for (int i = 0; i < g_nsections; i++) {
        if (g_sections[i].failed == 0)
            passed++;
        else
            failed_sec++;
    }

    printf("\n%s%s══ Test summary: %s ══%s\n",
           C_BOLD(), C_CYAN(), suite, C_RESET());
    test_print_hline(name_w, status_w, asserts_w);
    printf("| %s%-*s%s | %s%-*s%s | %s%-*s%s |\n",
           C_BOLD(), name_w, "Test", C_RESET(),
           C_BOLD(), status_w, "Status", C_RESET(),
           C_BOLD(), asserts_w, "Asserts", C_RESET());
    test_print_hline(name_w, status_w, asserts_w);

    if (g_nsections == 0) {
        char abuf[16];
        snprintf(abuf, sizeof(abuf), "%d/%d",
                 g_tests_run - g_tests_failed, g_tests_run);
        printf("| %-*s | %s%-*s%s | %-*s |\n",
               name_w, "(no TEST_SECTION)",
               g_tests_failed ? C_RED() : C_GREEN(),
               status_w, g_tests_failed ? "FAIL" : "PASS",
               C_RESET(),
               asserts_w, abuf);
    } else {
        for (int i = 0; i < g_nsections; i++) {
            test_section_t *s = &g_sections[i];
            int ok = (s->failed == 0);
            const char *col = ok ? C_GREEN() : C_RED();
            const char *st = ok ? "PASS" : "FAIL";
            char aname[TEST_SECTION_NAME_LEN];
            char abuf[16];
            if ((int)strlen(s->name) > name_w && name_w > 3)
                snprintf(aname, sizeof(aname), "%.*s...", name_w - 3, s->name);
            else
                snprintf(aname, sizeof(aname), "%s", s->name);
            snprintf(abuf, sizeof(abuf), "%d/%d",
                     s->asserts - s->failed, s->asserts);
            printf("| %-*s | %s%-*s%s | %-*s |\n",
                   name_w, aname,
                   col, status_w, st, C_RESET(),
                   asserts_w, abuf);
        }
    }

    test_print_hline(name_w, status_w, asserts_w);

    if (g_tests_failed == 0) {
        printf("%s%sOK%s %s: %d assertions",
               C_BOLD(), C_GREEN(), C_RESET(), suite, g_tests_run);
        if (g_nsections > 0)
            printf("  %s(%d/%d sections passed)%s",
                   C_DIM(), passed, g_nsections, C_RESET());
        printf("\n");
    } else {
        printf("%s%sFAILED%s %s: %d/%d assertions failed",
               C_BOLD(), C_RED(), C_RESET(), suite, g_tests_failed, g_tests_run);
        if (g_nsections > 0)
            printf("  %s(%d section(s) failed)%s",
                   C_DIM(), failed_sec, C_RESET());
        printf("\n");
    }
}

static inline int test_report(const char *suite) {
    test_print_summary_table(suite);
    return g_tests_failed == 0 ? 0 : 1;
}

#endif /* TEST_HARNESS_H */
