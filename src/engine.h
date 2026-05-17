#ifndef CMONKEY_ENGINE_H
#define CMONKEY_ENGINE_H

#include <stdbool.h>

#define MAX_TEST_CHARS 8192
#define MAX_WORD_TIMES 2048

typedef enum {
    STATE_IDLE = 0,
    STATE_RUNNING = 1,
    STATE_FINISHED = 2
} TestState;

typedef enum {
    RUN_MODE_TIME = 0,
    RUN_MODE_WORD = 1,
    RUN_MODE_QUOTE = 2
} RunMode;

typedef struct {
    char target[MAX_TEST_CHARS];
    int target_len;
    char typed[MAX_TEST_CHARS];
    int cursor;
    int errors;
    int total_keystrokes;
    int correct_chars;
    long long start_us;
    long long end_us;
    long long word_times_us[MAX_WORD_TIMES];
    int word_time_count;
    RunMode mode;
    int mode_limit;
    TestState state;
    bool aborted;
} TestSession;

/** Initialize a session with target text and mode configuration. */
void engine_init_session(TestSession *session, const char *target, RunMode mode, int mode_limit);

/** Mark the session as running and set start time if not already set. */
void engine_start(TestSession *session, long long now_us);

/** Handle one typed character and apply correctness/error accounting. */
void engine_handle_char(TestSession *session, char c, long long now_us);

/** Handle backspace and reverse per-position state where applicable. */
void engine_handle_backspace(TestSession *session);

/** Finish the session and store end time. */
void engine_finish(TestSession *session, long long now_us, bool aborted);

/** Return true when timed mode has reached its limit. */
bool engine_time_expired(const TestSession *session, long long now_us);

/** Return elapsed microseconds from first keystroke until end or now. */
long long engine_elapsed_us(const TestSession *session, long long now_us);

/** Return progress in [0, 1] for the current mode. */
double engine_progress(const TestSession *session, long long now_us);

#endif
