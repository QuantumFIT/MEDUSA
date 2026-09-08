/**
 * @file norm_track.h
 * @brief Per-gate norm deviation tracking for FP backend analysis.
 * Only active for floating-point backends (DOUBLES, MPFR).
 */
#ifndef NORM_TRACK_H
#define NORM_TRACK_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined(LEAF_BACKEND_DOUBLES) || defined(LEAF_BACKEND_MPFR)

#include <stdint.h>
#include "interface.h"

int  norm_track_init(const char *csv_path);
void norm_track_close(void);
void norm_track_record(const char *gate_name, qBDD circ, int n);

extern int g_norm_track_enabled;
extern int g_num_qubits;

#define NORM_TRACK_RECORD(gate_name, circ, n) \
    do { if (g_norm_track_enabled) norm_track_record((gate_name), (circ), (n)); } while (0)

#else /* GMP or unknown backend -- stub everything out */

#define NORM_TRACK_RECORD(gate_name, circ, n) do {} while (0)

static inline int  norm_track_init(const char *csv_path) { (void)csv_path; return 0; }
static inline void norm_track_close(void) {}
static inline void norm_track_record(const char *gate_name, qBDD circ, int n) {
    (void)gate_name; (void)circ; (void)n;
}

extern int g_norm_track_enabled;
extern int g_num_qubits;

#endif /* LEAF_BACKEND_DOUBLES || LEAF_BACKEND_MPFR */

#ifdef __cplusplus
}
#endif

#endif /* NORM_TRACK_H */