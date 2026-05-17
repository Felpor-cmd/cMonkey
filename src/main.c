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
    RESULT_QUIT = 2
} ResultAction;

typedef enum {
    MAIN_MENU_START = 0,
    MAIN_MENU_QUIT = 1
} MainMenuAction;

static void print_usage(void)
{
    puts("Usage: cmonkey [OPTIONS]");
    puts("");
    puts("Options:");
    puts("  -t <seconds>      Time mode — type for N seconds (default: 60)");
    puts("  -w <words>        Word mode — type exactly N words");
    puts("  -q                Quote mode — type a random passage");
    puts("  --list <name>     Word list to use: easy, common, full (default: common)");
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
            return RESULT_RETRY_SAME;
        }
        if (ev.type == INPUT_ESCAPE || ev.type == INPUT_CTRL_C) {
            return RESULT_QUIT;
        }
        if (ev.type == INPUT_CHAR) {
            if (ev.ch == 'r' || ev.ch == 'R') {
                return RESULT_RETRY_NEW;
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

    const int time_seconds = (opts.time_seconds > 0) ? opts.time_seconds : cfg.default_time;
    const int word_count = (opts.word_count > 0) ? opts.word_count : cfg.default_words;
    const bool use_color = opts.no_color ? false : cfg.color;
    const char *list_name = opts.has_list_name ? opts.list_name : cfg.word_list;
    const WordListKind list_kind = words_list_from_name(list_name);
    const RunMode mode = opts.mode;

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

    if (use_main_menu) {
        const MainMenuAction action = wait_for_main_menu_action(use_color);
        if (action == MAIN_MENU_QUIT) {
            terminal_restore();
            words_free(&words_db);
            return 0;
        }
    }

    while (running) {
        if (regenerate) {
            if (build_target(target, sizeof(target), &words_db, mode, list_kind, time_seconds, word_count) != 0) {
                terminal_restore();
                fprintf(stderr, "Failed to build target text\n");
                words_free(&words_db);
                return 1;
            }
        }

        const int mode_limit = (mode == RUN_MODE_TIME) ? time_seconds : word_count;
        TestSession session;
        engine_init_session(&session, target, mode, mode_limit);

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
        terminal_render_results(&session, &final_metrics, use_color);

        const ResultAction action = wait_for_results_action();
        if (action == RESULT_QUIT) {
            running = false;
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
