#ifndef CMONKEY_METRICS_H
#define CMONKEY_METRICS_H

#include "engine.h"

typedef struct {
    double wpm_net;
    double wpm_raw;
    double accuracy;
    int correct_chars;
    int error_count;
    double consistency;
    double elapsed_s;
} Metrics;

/** Compute metrics using the session's finished end timestamp. */
void metrics_compute(const TestSession *session, Metrics *out);

/** Compute metrics as-of now while a session is still running. */
void metrics_compute_live(const TestSession *session, long long now_us, Metrics *out);

#endif
