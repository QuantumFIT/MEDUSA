/**
 * @file norm_track.c
 * @brief Per-gate norm deviation tracking for FP backend analysis.
 */
#include "norm_track.h"

#if defined(LEAF_BACKEND_DOUBLES) || defined(LEAF_BACKEND_MPFR)

#include "leaf_primitive_double.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>

int g_norm_track_enabled = 0;
int g_num_qubits = 0;
leaf_scalar_t max_dev = 0.0;

static FILE *s_csv = NULL;
static uint64_t s_gate_idx = 0;

/* Internal helpers  */

/** Write the comment-header block (epsilon, backend). */
static void write_header() {
#if defined(LEAF_TYPE_FLOAT)
    fprintf(s_csv, "# epsilon:%.10e\n",  (double)LEAF_ABS_EPS);
#elif defined(LEAF_TYPE_DOUBLE)
    fprintf(s_csv, "# epsilon:%.20e\n",  (double)LEAF_ABS_EPS);
#elif defined(LEAF_TYPE_LONG_DOUBLE)
    fprintf(s_csv, "# epsilon:%.24Le\n", (long double)LEAF_ABS_EPS);
#elif defined(LEAF_TYPE_QUAD)
    fprintf(s_csv, "# epsilon:%.34e\n",  (double)LEAF_ABS_EPS);
#else
    fprintf(s_csv, "# epsilon:unknown\n");
#endif
    fprintf(s_csv, "# backend:f%ld\n", sizeof(leaf_scalar_t) * 8);
    fprintf(s_csv, "gate_idx;gate_name;total_prob;norm_dev;within_epsilon\n");

}


int norm_track_init(const char *csv_path) {
    s_csv = fopen(csv_path, "w");
    if (!s_csv) {
        perror("norm_track_init: fopen");
        return -1;
    }

    s_gate_idx = 0;
    g_norm_track_enabled = 1;

    write_header();
    return 0;
}

void norm_track_close(void) {
    if (!s_csv) return;   // guard first

    char buf_dev[128];    // f128 needs ~40 chars, 128 is safe
    buf_dev[sizeof(buf_dev) - 1] = '\0';  // force null termination

    leaf_primitive_t dev_primitive = { max_dev };
    to_str_generic(dev_primitive, buf_dev, sizeof(buf_dev));

    buf_dev[sizeof(buf_dev) - 1] = '\0';

    fprintf(s_csv, "# max_norm_dev:%s\n", buf_dev);
    fflush(s_csv);
    fclose(s_csv);
    s_csv = NULL;

    g_norm_track_enabled = 0;
}
void norm_track_record(const char *gate_name, qBDD circ, int n) {
    if (!s_csv) return;

    prob_t total = qBDD_total_prob(circ, n);
    leaf_primitive_t total_primitive = { total };
    leaf_scalar_t dev = LEAF_ABS(total - LEAF_ONE);
    leaf_primitive_t dev_primitive = { dev };
    leaf_primitive_t one_primitive = { LEAF_ONE };

    if (dev > max_dev) {
        max_dev = dev;
    }

    int within = cmp_generic(total_primitive, one_primitive) == 0; // total == 1.0 within epsilon

    char buf_total[64], buf_dev[64];
    to_str_generic(total_primitive, buf_total, sizeof(buf_total));
    to_str_generic(dev_primitive, buf_dev, sizeof(buf_dev));
    fprintf(s_csv, "%lu;%s;%s;%s;%d\n",
            s_gate_idx++,
            gate_name,
            buf_total,
            buf_dev,
            within);
}

#else /* GMP or unknown — provide the global definitions only */

int g_norm_track_enabled = 0;
int g_num_qubits = 0;

#endif /* LEAF_BACKEND_DOUBLES || LEAF_BACKEND_MPFR */

/* EOF norm_track.c */