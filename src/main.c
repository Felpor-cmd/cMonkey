#include "config.h"
#include "engine.h"
#include "input.h"
#include "metrics.h"
#include "terminal.h"
#include "words.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define CMONKEY_VERSION "0.1.0"

typedef struct {
    RunMode mode;
    bool mode_set;
    int time_seconds;
    int word_count;
    bool no_color;
    bool show_help;
    bool show_version;
    char list_name[16];
    bool has_list_name;
} CliOptions;

typedef enum {
    RESULT_RETRY_SAME = 0,
    RESULT_RETRY_NEW = 1,
    RESULT_MAIN_MENU = 2,
    RESULT_QUIT = 3
} ResultAction;

typedef enum {
    MAIN_MENU_START = 0,
    MAIN_MENU_QUIT = 1
} MainMenuAction;

typedef struct {
    int time_seconds;
    WordListKind list_kind;
} RunSetup;

typedef enum {
    RUN_SETUP_START = 0,
    RUN_SETUP_BACK = 1,
    RUN_SETUP_QUIT = 2
} RunSetupAction;

static const int RUN_SETUP_TIMES[] = { 15, 30, 60, 120 };
static const WordListKind RUN_SETUP_LISTS[] = { WORD_LIST_EASY, WORD_LIST_COMMON, WORD_LIST_FULL };

static void print_usage(void)
{
    puts("Usage: cmonkey [OPTIONS]");
    puts("");
    puts("Options:");
    puts("  -t <seconds>      Time mode — type for N seconds (default: 60)");
    puts("  -w <words>        Word mode — type exactly N words");
    puts("  -q                Quote mode — type a random passage");
    puts("  --list <name>     Word list: easy, common (40/30/20/10 mix), full");
    puts("  --no-color        Disable ANSI color output");
    puts("  -h, --help        Show this help message");
    puts("  -v, --version     Print version and exit");
}

static int parse_positive_int(const char *s, int *out)
{
    char *end = NULL;
    errno = 0;
    long v = strtol(s, &end, 10);
    if (errno != 0 || end == s || *end != '\0' || v <= 0 || v > 100000) {
        return -1;
    }
    *out = (int)v;
    return 0;
}

static bool is_valid_list_name(const char *name)
{
    return strcmp(name, "easy") == 0 || strcmp(name, "common") == 0 || strcmp(name, "full") == 0;
}

static int parse_args(int argc, char **argv, CliOptions *opts)
{
    memset(opts, 0, sizeof(*opts));
    opts->mode = RUN_MODE_TIME;

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];

        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            opts->show_help = true;
            continue;
        }
        if (strcmp(arg, "-v") == 0 || strcmp(arg, "--version") == 0) {
            opts->show_version = true;
            continue;
        }
        if (strcmp(arg, "--no-color") == 0) {
            opts->no_color = true;
            continue;
        }
        if (strcmp(arg, "-q") == 0) {
            opts->mode = RUN_MODE_QUOTE;
            opts->mode_set = true;
            continue;
        }
        if (strcmp(arg, "-t") == 0) {
            if (i + 1 >= argc || parse_positive_int(argv[++i], &opts->time_seconds) != 0) {
                fprintf(stderr, "Invalid value for -t\n");
                return -1;
            }
            opts->mode = RUN_MODE_TIME;
            opts->mode_set = true;
            continue;
        }
        if (strcmp(arg, "-w") == 0) {
            if (i + 1 >= argc || parse_positive_int(argv[++i], &opts->word_count) != 0) {
                fprintf(stderr, "Invalid value for -w\n");
                return -1;
            }
            opts->mode = RUN_MODE_WORD;
            opts->mode_set = true;
            continue;
        }
        if (strcmp(arg, "--list") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Missing value for --list\n");
                return -1;
            }
            strncpy(opts->list_name, argv[++i], sizeof(opts->list_name) - 1);
            opts->list_name[sizeof(opts->list_name) - 1] = '\0';
            opts->has_list_name = true;
            continue;
        }
        if (strncmp(arg, "--list=", 7) == 0) {
            strncpy(opts->list_name, arg + 7, sizeof(opts->list_name) - 1);
            opts->list_name[sizeof(opts->list_name) - 1] = '\0';
            opts->has_list_name = true;
            continue;
        }

        fprintf(stderr, "Unknown option: %s\n", arg);
        return -1;
    }

    if (opts->has_list_name && !is_valid_list_name(opts->list_name)) {
        fprintf(stderr, "Invalid --list value. Expected: easy, common, full\n");
        return -1;
    }

    return 0;
}

static long long now_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000LL + (long long)(ts.tv_nsec / 1000LL);
}

static ResultAction wait_for_results_action(void)
{
    while (true) {
        const InputEvent ev = input_read_event();

        if (ev.type == INPUT_ENTER) {
            return RESULT_RETRY_NEW;
        }
        if (ev.type == INPUT_ESCAPE || ev.type == INPUT_CTRL_C) {
            return RESULT_QUIT;
        }
        if (ev.type == INPUT_CHAR) {
            if (ev.ch == 'r' || ev.ch == 'R') {
                return RESULT_RETRY_SAME;
            }
            if (ev.ch == 'm' || ev.ch == 'M') {
                return RESULT_MAIN_MENU;
            }
            if (ev.ch == 'q' || ev.ch == 'Q') {
                return RESULT_QUIT;
            }
        }

        struct timespec req = { .tv_sec = 0, .tv_nsec = 10 * 1000 * 1000 };
        nanosleep(&req, NULL);
    }
}

static MainMenuAction wait_for_main_menu_action(bool use_color)
{
    while (true) {
        terminal_render_main_menu(use_color);
        const InputEvent ev = input_read_event();

        if (ev.type == INPUT_ENTER) {
            return MAIN_MENU_START;
        }
        if (ev.type == INPUT_ESCAPE || ev.type == INPUT_CTRL_C) {
            return MAIN_MENU_QUIT;
        }
        if (ev.type == INPUT_CHAR) {
            if (ev.ch == '1' || ev.ch == 's' || ev.ch == 'S') {
                return MAIN_MENU_START;
            }
            if (ev.ch == 'q' || ev.ch == 'Q') {
                return MAIN_MENU_QUIT;
            }
        }

        struct timespec req = { .tv_sec = 0, .tv_nsec = 10 * 1000 * 1000 };
        nanosleep(&req, NULL);
    }
}

static size_t find_time_option_index(int seconds)
{
    for (size_t i = 0; i < sizeof(RUN_SETUP_TIMES) / sizeof(RUN_SETUP_TIMES[0]); i++) {
        if (RUN_SETUP_TIMES[i] == seconds) {
            return i;
        }
    }
    return 2;
}

static size_t find_list_option_index(WordListKind list_kind)
{
    for (size_t i = 0; i < sizeof(RUN_SETUP_LISTS) / sizeof(RUN_SETUP_LISTS[0]); i++) {
        if (RUN_SETUP_LISTS[i] == list_kind) {
            return i;
        }
    }
    return 1;
}

static RunSetupAction wait_for_run_setup(bool use_color, RunSetup *setup)
{
    size_t time_idx = find_time_option_index(setup->time_seconds);
    size_t list_idx = find_list_option_index(setup->list_kind);
    bool focus_time = true;

    while (true) {
        terminal_render_run_setup(use_color, RUN_SETUP_TIMES[time_idx], words_list_name(RUN_SETUP_LISTS[list_idx]), focus_time);
        const InputEvent ev = input_read_event();

        if (ev.type == INPUT_CTRL_C) {
            return RUN_SETUP_QUIT;
        }
        if (ev.type == INPUT_ESCAPE) {
            return RUN_SETUP_BACK;
        }
        if (ev.type == INPUT_ENTER) {
            setup->time_seconds = RUN_SETUP_TIMES[time_idx];
            setup->list_kind = RUN_SETUP_LISTS[list_idx];
            return RUN_SETUP_START;
        }
        if (ev.type == INPUT_ARROW_LEFT || ev.type == INPUT_ARROW_RIGHT) {
            focus_time = !focus_time;
        } else if (ev.type == INPUT_ARROW_UP) {
            if (focus_time) {
                time_idx = (time_idx + (sizeof(RUN_SETUP_TIMES) / sizeof(RUN_SETUP_TIMES[0])) - 1) %
                           (sizeof(RUN_SETUP_TIMES) / sizeof(RUN_SETUP_TIMES[0]));
            } else {
                list_idx = (list_idx + (sizeof(RUN_SETUP_LISTS) / sizeof(RUN_SETUP_LISTS[0])) - 1) %
                           (sizeof(RUN_SETUP_LISTS) / sizeof(RUN_SETUP_LISTS[0]));
            }
        } else if (ev.type == INPUT_ARROW_DOWN) {
            if (focus_time) {
                time_idx = (time_idx + 1) % (sizeof(RUN_SETUP_TIMES) / sizeof(RUN_SETUP_TIMES[0]));
            } else {
                list_idx = (list_idx + 1) % (sizeof(RUN_SETUP_LISTS) / sizeof(RUN_SETUP_LISTS[0]));
            }
        }

        struct timespec req = { .tv_sec = 0, .tv_nsec = 10 * 1000 * 1000 };
        nanosleep(&req, NULL);
    }
}

static bool wait_for_main_menu_and_setup(bool use_color, RunSetup *setup)
{
    while (true) {
        const MainMenuAction action = wait_for_main_menu_action(use_color);
        if (action == MAIN_MENU_QUIT) {
            return false;
        }

        const RunSetupAction setup_action = wait_for_run_setup(use_color, setup);
        if (setup_action == RUN_SETUP_START) {
            return true;
        }
        if (setup_action == RUN_SETUP_QUIT) {
            return false;
        }
    }
}

static int build_target(char *target,
                        size_t target_size,
                        const WordsDb *words_db,
                        RunMode mode,
                        WordListKind list_kind,
                        int time_seconds,
                        int word_count)
{
    if (mode == RUN_MODE_QUOTE) {
        return words_random_quote(words_db, target, target_size);
    }

    int count = word_count;
    if (mode == RUN_MODE_TIME) {
        count = time_seconds * 4;
        if (count < 60) {
            count = 60;
        }
        if (count > 1000) {
            count = 1000;
        }
    }
    return words_build_target(words_db, list_kind, count, target, target_size);
}

int main(int argc, char **argv)
{
    Config cfg;
    if (config_load(&cfg) != 0) {
        fprintf(stderr, "Failed to load config from ~/.config/cmonkey/config\n");
        return 1;
    }

    CliOptions opts;
    if (parse_args(argc, argv, &opts) != 0) {
        print_usage();
        return 2;
    }

    if (opts.show_help) {
        print_usage();
        return 0;
    }
    if (opts.show_version) {
        printf("cmonkey %s\n", CMONKEY_VERSION);
        return 0;
    }

    int current_time_seconds = (opts.time_seconds > 0) ? opts.time_seconds : cfg.default_time;
    const int word_count = (opts.word_count > 0) ? opts.word_count : cfg.default_words;
    const bool use_color = opts.no_color ? false : cfg.color;
    const char *list_name = opts.has_list_name ? opts.list_name : cfg.word_list;
    WordListKind current_list_kind = words_list_from_name(list_name);
    RunMode current_mode = opts.mode;

    srand((unsigned int)time(NULL));

    WordsDb words_db;
    if (words_init(&words_db) != 0) {
        fprintf(stderr, "Failed to initialize word lists\n");
        return 1;
    }

    terminal_init();
    if (terminal_enable_raw_mode() != 0) {
        fprintf(stderr, "Failed to enable raw terminal mode\n");
        words_free(&words_db);
        return 1;
    }

    bool running = true;
    bool regenerate = true;
    char target[MAX_TEST_CHARS];
    memset(target, 0, sizeof(target));
    const bool use_main_menu = !opts.mode_set;
    RunSetup run_setup = {
        .time_seconds = current_time_seconds,
        .list_kind = current_list_kind
    };

    if (use_main_menu) {
        if (!wait_for_main_menu_and_setup(use_color, &run_setup)) {
            terminal_restore();
            words_free(&words_db);
            return 0;
        }
        current_time_seconds = run_setup.time_seconds;
        current_list_kind = run_setup.list_kind;
        current_mode = RUN_MODE_TIME;
    }

    while (running) {
        if (regenerate) {
            if (build_target(target, sizeof(target), &words_db, current_mode, current_list_kind, current_time_seconds, word_count) != 0) {
                terminal_restore();
                fprintf(stderr, "Failed to build target text\n");
                words_free(&words_db);
                return 1;
            }
        }

        const int mode_limit = (current_mode == RUN_MODE_TIME) ? current_time_seconds : word_count;
        TestSession session;
        engine_init_session(&session, target, current_mode, mode_limit);

        bool interrupted = false;

        while (session.state != STATE_FINISHED) {
            const long long t = now_us();
            if (engine_time_expired(&session, t)) {
                engine_finish(&session, t, false);
            }

            const InputEvent ev = input_read_event();
            if (ev.type == INPUT_CTRL_C) {
                interrupted = true;
                break;
            }
            if (ev.type == INPUT_ESCAPE) {
                engine_finish(&session, t, true);
            } else if (ev.type == INPUT_BACKSPACE) {
                engine_handle_backspace(&session);
            } else if (ev.type == INPUT_CHAR) {
                engine_handle_char(&session, ev.ch, t);
            }

            Metrics live;
            metrics_compute_live(&session, t, &live);
            terminal_render_test(&session, &live, t, use_color);
            struct timespec req = { .tv_sec = 0, .tv_nsec = 10 * 1000 * 1000 };
            nanosleep(&req, NULL);
        }

        if (interrupted) {
            break;
        }

        Metrics final_metrics;
        metrics_compute(&session, &final_metrics);
        terminal_render_results(&session, &final_metrics, words_list_name(current_list_kind), use_color);

        const ResultAction action = wait_for_results_action();
        if (action == RESULT_QUIT) {
            running = false;
        } else if (action == RESULT_MAIN_MENU) {
            run_setup.time_seconds = current_time_seconds;
            run_setup.list_kind = current_list_kind;
            if (!wait_for_main_menu_and_setup(use_color, &run_setup)) {
                running = false;
            } else {
                current_time_seconds = run_setup.time_seconds;
                current_list_kind = run_setup.list_kind;
                current_mode = RUN_MODE_TIME;
                regenerate = true;
            }
        } else if (action == RESULT_RETRY_NEW) {
            regenerate = true;
        } else {
            regenerate = false;
        }
    }

    terminal_restore();
    words_free(&words_db);
    return 0;
}
