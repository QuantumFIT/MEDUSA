/**
 * @file medusa_debug.c
 * JSON-Lines debug sink. Active only when built with -DMEDUSA_DEBUG.
 */

#include "medusa_debug.h"

#ifdef MEDUSA_DEBUG

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static FILE *g_out;
static unsigned g_mask;
static int g_ready;
static uint64_t g_seq;

static unsigned parse_cat(const char *tok)
{
    if (!tok || !*tok) return 0;
    if (strcmp(tok, "1") == 0 || strcmp(tok, "all") == 0) return MEDUSA_DBG_ALL;
    if (strcmp(tok, "lifecycle") == 0) return MEDUSA_DBG_LIFECYCLE;
    if (strcmp(tok, "protect") == 0)   return MEDUSA_DBG_PROTECT;
    if (strcmp(tok, "gc") == 0)        return MEDUSA_DBG_GC;
    if (strcmp(tok, "symb") == 0)      return MEDUSA_DBG_SYMB;
    if (strcmp(tok, "gate") == 0)      return MEDUSA_DBG_GATE;
    if (strcmp(tok, "norm") == 0)      return MEDUSA_DBG_NORM;
    if (strcmp(tok, "leaf") == 0)      return MEDUSA_DBG_LEAF;
    return 0;
}

static const char *cat_name(unsigned cat)
{
    if (cat & MEDUSA_DBG_LIFECYCLE) return "lifecycle";
    if (cat & MEDUSA_DBG_PROTECT)   return "protect";
    if (cat & MEDUSA_DBG_GC)        return "gc";
    if (cat & MEDUSA_DBG_SYMB)      return "symb";
    if (cat & MEDUSA_DBG_GATE)      return "gate";
    if (cat & MEDUSA_DBG_NORM)      return "norm";
    if (cat & MEDUSA_DBG_LEAF)      return "leaf";
    return "other";
}

/* Default when MEDUSA_DEBUG=1: skip noisy gate/leaf/protect unless asked. */
static unsigned default_mask(void)
{
    return MEDUSA_DBG_LIFECYCLE | MEDUSA_DBG_GC | MEDUSA_DBG_SYMB | MEDUSA_DBG_NORM;
}

void medusa_dbg_init(void)
{
    if (g_ready) return;

    const char *env = getenv("MEDUSA_DEBUG");
    if (!env || !*env) {
        /* Compiled in but env unset: still enable defaults (opt-in via build). */
        g_mask = default_mask();
    } else if (strcmp(env, "0") == 0 || strcmp(env, "off") == 0) {
        g_mask = 0;
    } else if (strcmp(env, "1") == 0 || strcmp(env, "all") == 0) {
        g_mask = default_mask();
    } else {
        g_mask = 0;
        char buf[256];
        strncpy(buf, env, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        for (char *tok = strtok(buf, ","); tok; tok = strtok(NULL, ",")) {
            while (*tok == ' ') tok++;
            g_mask |= parse_cat(tok);
        }
        if (g_mask == 0)
            g_mask = default_mask();
    }

    const char *path = getenv("MEDUSA_DEBUG_FILE");
    if (path && *path) {
        g_out = fopen(path, "w");
        if (!g_out)
            g_out = stderr;
    } else {
        g_out = stderr;
    }

    g_ready = 1;
    g_seq = 0;

    if (g_mask) {
        fprintf(g_out,
                "{\"seq\":0,\"cat\":\"lifecycle\",\"evt\":\"dbg_init\",\"where\":\"medusa_dbg_init\","
                "\"note\":\"mask=0x%x file=%s\"}\n",
                g_mask, (path && *path) ? path : "stderr");
        fflush(g_out);
    }
}

int medusa_dbg_enabled(void)
{
    if (!g_ready) medusa_dbg_init();
    return g_mask != 0;
}

int medusa_dbg_cat_on(unsigned cat)
{
    if (!g_ready) medusa_dbg_init();
    return (g_mask & cat) != 0;
}

static void json_escape_note(FILE *f, const char *s)
{
    if (!s) {
        fputs("null", f);
        return;
    }
    fputc('"', f);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if (*p == '"' || *p == '\\') {
            fputc('\\', f);
            fputc(*p, f);
        } else if (*p < 0x20) {
            fprintf(f, "\\u%04x", *p);
        } else {
            fputc(*p, f);
        }
    }
    fputc('"', f);
}

void medusa_dbg_log(const medusa_dbg_event_t *ev)
{
    if (!ev || !ev->evt) return;
    if (!g_ready) medusa_dbg_init();
    if (!(g_mask & ev->cat)) return;

    g_seq++;
    fprintf(g_out, "{\"seq\":%llu,\"cat\":\"%s\",\"evt\":\"%s\",\"where\":\"%s\"",
            (unsigned long long)g_seq,
            cat_name(ev->cat),
            ev->evt,
            ev->where ? ev->where : "");

    if (ev->use_bdd) {
        fprintf(g_out, ",\"bdd\":%d,\"ref\":%d,\"is_false\":%d,\"leaves\":%d",
                ev->bdd, ev->ref, ev->is_false, ev->leaves);
    }
    if (ev->use_total)
        fprintf(g_out, ",\"total\":%.17g", ev->total);
    if (ev->use_n)
        fprintf(g_out, ",\"n\":%d", ev->n_qubits);
    if (ev->use_iters)
        fprintf(g_out, ",\"iters\":%llu", (unsigned long long)ev->iters);
    if (ev->use_loop)
        fprintf(g_out, ",\"loop\":%d", ev->loop_idx);
    if (ev->use_mem)
        fprintf(g_out, ",\"pimpl_live\":%zu,\"wrap_allocs\":%zu",
                ev->pimpl_live, ev->wrap_allocs);
    if (ev->gate)
        fprintf(g_out, ",\"gate\":\"%s\"", ev->gate);
    if (ev->note) {
        fputs(",\"note\":", g_out);
        json_escape_note(g_out, ev->note);
    }
    fputs("}\n", g_out);
    fflush(g_out);
}

#else /* !MEDUSA_DEBUG */

/* Translation unit kept so the object always links; API is inline in the header. */

#endif /* MEDUSA_DEBUG */
