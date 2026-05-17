#include "engine.h"

#include <stddef.h>
#include <string.h>

static void record_word_time(TestSession *session, long long now_us)
{
    if (session->word_time_count >= MAX_WORD_TIMES) {
        return;
    }
    session->word_times_us[session->word_time_count++] = now_us;
}

void engine_init_session(TestSession *session, const char *target, RunMode mode, int mode_limit)
{
    memset(session, 0, sizeof(*session));
    session->mode = mode;
    session->mode_limit = mode_limit;
    session->state = STATE_IDLE;

    if (target == NULL) {
        session->target[0] = '\0';
        session->target_len = 0;
        return;
    }

    strncpy(session->target, target, MAX_TEST_CHARS - 1);
    session->target[MAX_TEST_CHARS - 1] = '\0';
    session->target_len = (int)strlen(session->target);
}

void engine_start(TestSession *session, long long now_us)
{
    if (session->state != STATE_IDLE) {
        return;
    }
    session->state = STATE_RUNNING;
    session->start_us = now_us;
}

void engine_handle_char(TestSession *session, char c, long long now_us)
{
    if (session->state == STATE_FINISHED) {
        return;
    }
    if (session->state == STATE_IDLE) {
        engine_start(session, now_us);
    }
    if (session->cursor >= session->target_len) {
        return;
    }

    const int idx = session->cursor;
    const char expected = session->target[idx];

    session->typed[idx] = c;
    session->cursor++;
    session->total_keystrokes++;

    if (c == expected) {
        session->correct_chars++;
    } else {
        session->errors++;
    }

    if (expected == ' ' || session->cursor == session->target_len) {
        record_word_time(session, now_us);
    }

    if (session->mode != RUN_MODE_TIME && session->cursor >= session->target_len) {
        engine_finish(session, now_us, false);
    }
}

void engine_handle_backspace(TestSession *session)
{
    if (session->state == STATE_FINISHED || session->cursor <= 0) {
        return;
    }

    session->cursor--;

    const int idx = session->cursor;
    const char expected = session->target[idx];
    const char typed = session->typed[idx];

    if (typed == expected) {
        if (session->correct_chars > 0) {
            session->correct_chars--;
        }
    }

    if ((expected == ' ' || (idx + 1) == session->target_len) && session->word_time_count > 0) {
        session->word_time_count--;
    }

    session->typed[idx] = '\0';
}

void engine_finish(TestSession *session, long long now_us, bool aborted)
{
    if (session->state == STATE_FINISHED) {
        return;
    }
    if (session->start_us == 0) {
        session->start_us = now_us;
    }
    session->state = STATE_FINISHED;
    session->end_us = now_us;
    if (session->end_us < session->start_us) {
        session->end_us = session->start_us;
    }
    session->aborted = aborted;
}

long long engine_elapsed_us(const TestSession *session, long long now_us)
{
    if (session->start_us == 0) {
        return 0;
    }
    if (session->state == STATE_FINISHED) {
        return session->end_us - session->start_us;
    }
    if (now_us < session->start_us) {
        return 0;
    }
    return now_us - session->start_us;
}

bool engine_time_expired(const TestSession *session, long long now_us)
{
    if (session->mode != RUN_MODE_TIME || session->mode_limit <= 0) {
        return false;
    }
    if (session->state != STATE_RUNNING) {
        return false;
    }
    const long long limit_us = (long long)session->mode_limit * 1000000LL;
    return engine_elapsed_us(session, now_us) >= limit_us;
}

double engine_progress(const TestSession *session, long long now_us)
{
    if (session->mode == RUN_MODE_TIME) {
        if (session->mode_limit <= 0 || session->start_us == 0) {
            return 0.0;
        }
        const double limit_us = (double)session->mode_limit * 1000000.0;
        double p = (double)engine_elapsed_us(session, now_us) / limit_us;
        if (p < 0.0) {
            p = 0.0;
        }
        if (p > 1.0) {
            p = 1.0;
        }
        return p;
    }

    if (session->target_len <= 0) {
        return 0.0;
    }

    double p = (double)session->cursor / (double)session->target_len;
    if (p < 0.0) {
        p = 0.0;
    }
    if (p > 1.0) {
        p = 1.0;
    }
    return p;
}
