#include "metrics.h"

#include <math.h>
#include <string.h>

static double clamp(double x, double lo, double hi)
{
    if (x < lo) {
        return lo;
    }
    if (x > hi) {
        return hi;
    }
    return x;
}

static double compute_consistency(const TestSession *session)
{
    if (session->word_time_count <= 0 || session->start_us == 0) {
        return 0.0;
    }

    double wpms[MAX_WORD_TIMES];
    int count = 0;
    long long prev = session->start_us;

    for (int i = 0; i < session->word_time_count; i++) {
        const long long t = session->word_times_us[i];
        if (t <= prev) {
            continue;
        }
        const double dt = (double)(t - prev) / 1000000.0;
        if (dt <= 0.0) {
            continue;
        }
        wpms[count++] = 60.0 / dt;
        prev = t;
    }

    if (count == 0) {
        return 0.0;
    }
    if (count == 1) {
        return 100.0;
    }

    double mean = 0.0;
    for (int i = 0; i < count; i++) {
        mean += wpms[i];
    }
    mean /= (double)count;
    if (mean <= 0.0) {
        return 0.0;
    }

    double variance = 0.0;
    for (int i = 0; i < count; i++) {
        const double d = wpms[i] - mean;
        variance += d * d;
    }
    variance /= (double)count;

    const double stddev = sqrt(variance);
    const double cv = stddev / mean;
    return clamp((1.0 - cv) * 100.0, 0.0, 100.0);
}

static void metrics_compute_with_end(const TestSession *session, long long end_us, Metrics *out)
{
    memset(out, 0, sizeof(*out));

    out->correct_chars = session->correct_chars;
    out->error_count = session->errors;
    out->consistency = compute_consistency(session);

    if (session->start_us == 0 || end_us <= session->start_us) {
        return;
    }

    out->elapsed_s = (double)(end_us - session->start_us) / 1000000.0;
    if (out->elapsed_s <= 0.0) {
        return;
    }

    out->wpm_net = ((double)session->correct_chars / 5.0) / out->elapsed_s * 60.0;
    out->wpm_raw = ((double)session->total_keystrokes / 5.0) / out->elapsed_s * 60.0;

    if (session->total_keystrokes > 0) {
        out->accuracy = ((double)session->correct_chars / (double)session->total_keystrokes) * 100.0;
    }
    out->accuracy = clamp(out->accuracy, 0.0, 100.0);
}

void metrics_compute(const TestSession *session, Metrics *out)
{
    metrics_compute_with_end(session, session->end_us, out);
}

void metrics_compute_live(const TestSession *session, long long now_us, Metrics *out)
{
    if (session->state == STATE_FINISHED) {
        metrics_compute_with_end(session, session->end_us, out);
        return;
    }
    metrics_compute_with_end(session, now_us, out);
}
