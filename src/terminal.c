#include "terminal.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

static struct termios g_original_termios;
static bool g_raw_mode_enabled = false;

static const char *ANSI_RESET = "\033[0m";
static const char *ANSI_BOLD = "\033[1m";
static const char *ANSI_DIM = "\033[2m";
static const char *ANSI_RED = "\033[31m";
static const char *ANSI_WHITE = "\033[97m";
static const char *ANSI_GREEN = "\033[32m";
static const char *CLEAR_SCREEN = "\033[2J\033[H";
static const char *CURSOR_HIDE = "\033[?25l";
static const char *CURSOR_SHOW = "\033[?25h";
static const int TARGET_ROW_WIDTH = 66;
static const int TARGET_VISIBLE_ROWS = 3;

typedef struct {
    int start;
    int end;
} RowSpan;

static const char *color(bool use_color, const char *code)
{
    return use_color ? code : "";
}

static void render_logo(bool use_color)
{
    printf("%s%s", color(use_color, ANSI_BOLD), color(use_color, ANSI_WHITE));
    printf("                            _\n");
    printf("  ___ _ __ ___   ___  _ __ | | _____ _   _\n");
    printf(" / __| '_ ` _ \\ / _ \\| '_ \\| |/ / _ \\ | | |\n");
    printf("| (__| | | | | | (_) | | | |   <  __/ |_| |\n");
    printf(" \\___|_| |_| |_|\\___/|_| |_|_|\\_\\___|\\__, |\n");
    printf("                                       |__/\n");
    printf("%s\n", color(use_color, ANSI_RESET));
}

static int build_row_layout(const TestSession *session, RowSpan rows[MAX_TEST_CHARS])
{
    if (session->target_len <= 0) {
        rows[0].start = 0;
        rows[0].end = 0;
        return 1;
    }

    int row_count = 0;
    int start = 0;

    while (start < session->target_len && row_count < MAX_TEST_CHARS) {
        int end = start;
        int width = 0;
        int last_space = -1;

        while (end < session->target_len) {
            const char ch = session->target[end];
            if (ch == ' ') {
                last_space = end;
            }

            end++;
            width++;

            if (width >= TARGET_ROW_WIDTH) {
                if (end < session->target_len && last_space >= start) {
                    end = last_space + 1;
                }
                break;
            }
        }

        if (end <= start) {
            end = start + 1;
        }

        rows[row_count].start = start;
        rows[row_count].end = end;
        row_count++;
        start = end;
    }

    if (row_count <= 0) {
        rows[0].start = 0;
        rows[0].end = session->target_len;
        return 1;
    }

    return row_count;
}

static int find_cursor_row(const RowSpan *rows, int row_count, int cursor, int target_len)
{
    if (row_count <= 0) {
        return 0;
    }
    if (cursor >= target_len) {
        return row_count - 1;
    }

    for (int i = 0; i < row_count; i++) {
        if (cursor < rows[i].end) {
            return i;
        }
    }

    return row_count - 1;
}

static void draw_progress_bar(double progress, int width, bool use_color)
{
    if (progress < 0.0) {
        progress = 0.0;
    }
    if (progress > 1.0) {
        progress = 1.0;
    }

    int filled = (int)(progress * (double)width);
    if (filled > width) {
        filled = width;
    }

    fputs(color(use_color, ANSI_GREEN), stdout);
    putchar('[');
    for (int i = 0; i < width; i++) {
        putchar(i < filled ? '#' : '-');
    }
    putchar(']');
    fputs(color(use_color, ANSI_RESET), stdout);
}

static void handle_exit_signal(int signo)
{
    terminal_restore();
    _exit(128 + signo);
}

void terminal_init(void)
{
    signal(SIGINT, handle_exit_signal);
    signal(SIGTERM, handle_exit_signal);
}

int terminal_enable_raw_mode(void)
{
    if (g_raw_mode_enabled) {
        return 0;
    }

    if (tcgetattr(STDIN_FILENO, &g_original_termios) != 0) {
        return -1;
    }

    struct termios raw = g_original_termios;
    raw.c_iflag &= (tcflag_t) ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_cflag |= (tcflag_t) CS8;
    raw.c_lflag &= (tcflag_t) ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0) {
        return -1;
    }

    g_raw_mode_enabled = true;
    fputs(CURSOR_HIDE, stdout);
    fflush(stdout);
    return 0;
}

void terminal_restore(void)
{
    if (g_raw_mode_enabled) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_original_termios);
        g_raw_mode_enabled = false;
    }
    fputs(ANSI_RESET, stdout);
    fputs(CURSOR_SHOW, stdout);
    fflush(stdout);
}

void terminal_clear_screen(void)
{
    fputs(CLEAR_SCREEN, stdout);
}

void terminal_render_test(const TestSession *session, const Metrics *metrics, long long now_us, bool use_color)
{
    terminal_clear_screen();
    fputs(CURSOR_HIDE, stdout);

    render_logo(use_color);
    RowSpan rows[MAX_TEST_CHARS];
    const int row_count = build_row_layout(session, rows);
    const int cursor_row = find_cursor_row(rows, row_count, session->cursor, session->target_len);

    int first_visible_row = 0;
    if (cursor_row >= 2) {
        first_visible_row = cursor_row - 1;
    }
    int last_visible_row = first_visible_row + TARGET_VISIBLE_ROWS;
    if (last_visible_row > row_count) {
        last_visible_row = row_count;
    }
    if ((last_visible_row - first_visible_row) < TARGET_VISIBLE_ROWS && first_visible_row > 0) {
        first_visible_row = last_visible_row - TARGET_VISIBLE_ROWS;
        if (first_visible_row < 0) {
            first_visible_row = 0;
        }
    }

    printf(" Target\n");
    for (int row = first_visible_row; row < last_visible_row; row++) {
        printf(" ");
        for (int i = rows[row].start; i < rows[row].end; i++) {
            const char t = session->target[i];

            if (i < session->cursor) {
                if (session->typed[i] == t) {
                    fputs(color(use_color, ANSI_WHITE), stdout);
                } else {
                    fputs(color(use_color, ANSI_RED), stdout);
                }
                putchar(t);
                fputs(color(use_color, ANSI_RESET), stdout);
            } else {
                fputs(color(use_color, ANSI_DIM), stdout);
                putchar(t);
                fputs(color(use_color, ANSI_RESET), stdout);
            }
        }
        printf("\n");
    }

    printf("\n > ");
    const int input_start = rows[cursor_row].start;
    for (int i = input_start; i < session->cursor && i < session->target_len; i++) {
        if (session->typed[i] == session->target[i]) {
            fputs(color(use_color, ANSI_WHITE), stdout);
        } else {
            fputs(color(use_color, ANSI_RED), stdout);
        }
        putchar(session->typed[i]);
        fputs(color(use_color, ANSI_RESET), stdout);
    }
    putchar('_');
    printf("\n\n");

    const long long elapsed_us = engine_elapsed_us(session, now_us);
    const int elapsed_s = (int)(elapsed_us / 1000000LL);
    const int mm = elapsed_s / 60;
    const int ss = elapsed_s % 60;
    const double progress = engine_progress(session, now_us);

    if (session->mode == RUN_MODE_TIME) {
        printf(" Time: %02d:%02d  WPM: %.0f  ACC: %.1f%%  ", mm, ss, metrics->wpm_net, metrics->accuracy);
        draw_progress_bar(progress, 24, use_color);
        printf("  %ds", session->mode_limit);
    } else {
        printf(" Progress: %3.0f%%  WPM: %.0f  ACC: %.1f%%  ", progress * 100.0, metrics->wpm_net, metrics->accuracy);
        draw_progress_bar(progress, 24, use_color);
    }

    if (session->aborted) {
        printf("\n Aborted");
    }
    printf("\n [Esc] abort  [Ctrl+C] quit\n");
    fflush(stdout);
}

void terminal_render_results(const TestSession *session, const Metrics *metrics, const char *difficulty_name, bool use_color)
{
    (void)session;
    terminal_clear_screen();
    fputs(CURSOR_HIDE, stdout);

    printf(" %s-- Results --%s\n\n", color(use_color, ANSI_BOLD), color(use_color, ANSI_RESET));
    printf(" WPM (net)       %.0f\n", metrics->wpm_net);
    printf(" WPM (raw)       %.0f\n", metrics->wpm_raw);
    printf(" Accuracy        %.1f %%\n", metrics->accuracy);
    printf(" Correct chars   %d\n", metrics->correct_chars);
    printf(" Errors          %d\n", metrics->error_count);
    printf(" Difficulty      %s\n", difficulty_name != NULL ? difficulty_name : "unknown");
    printf(" Consistency     %.1f %%\n", metrics->consistency);
    printf(" Time            %.1f s\n", metrics->elapsed_s);
    printf("\n [Enter] new game   [r] retry same   [m] main menu   [q] quit\n");
    fflush(stdout);
}

void terminal_render_main_menu(bool use_color)
{
    terminal_clear_screen();
    fputs(CURSOR_HIDE, stdout);

    render_logo(use_color);
    printf(" %sMain Menu%s\n\n", color(use_color, ANSI_BOLD), color(use_color, ANSI_RESET));
    printf(" [1] Regular run\n");
    printf(" [2] Quotes\n");
    printf(" [3] Custom\n");
    printf(" [q] Quit\n");
    printf("\n Select an option and press Enter.\n");
    fflush(stdout);
}

static void render_setup_option(const char *label, bool selected, bool focused, bool use_color, int width)
{
    if (selected && focused) {
        printf("%s> %-*s%s", color(use_color, ANSI_GREEN), width - 2, label, color(use_color, ANSI_RESET));
        return;
    }
    if (selected) {
        printf("%s* %-*s%s", color(use_color, ANSI_WHITE), width - 2, label, color(use_color, ANSI_RESET));
        return;
    }
    printf("%s  %-*s%s", color(use_color, ANSI_DIM), width - 2, label, color(use_color, ANSI_RESET));
}

void terminal_render_run_setup(bool use_color, const char *title, int selected_time_seconds, const char *selected_list_name, bool focus_time)
{
    static const int time_options[] = { 15, 30, 60, 120 };
    static const char *list_options[] = { "easy", "common", "full" };
    const int max_rows = 4;

    terminal_clear_screen();
    fputs(CURSOR_HIDE, stdout);
    render_logo(use_color);

    printf(" %s%s%s\n\n",
           color(use_color, ANSI_BOLD),
           (title != NULL) ? title : "Run Setup",
           color(use_color, ANSI_RESET));
    printf(" %sTime%s                %sDifficulty%s\n",
           color(use_color, ANSI_BOLD), color(use_color, ANSI_RESET),
           color(use_color, ANSI_BOLD), color(use_color, ANSI_RESET));

    for (int row = 0; row < max_rows; row++) {
        char time_label[16] = "";
        const char *list_label = "";
        bool time_selected = false;
        bool list_selected = false;

        if (row < (int)(sizeof(time_options) / sizeof(time_options[0]))) {
            snprintf(time_label, sizeof(time_label), "%ds", time_options[row]);
            time_selected = (time_options[row] == selected_time_seconds);
        }
        if (row < (int)(sizeof(list_options) / sizeof(list_options[0]))) {
            list_label = list_options[row];
            list_selected = (selected_list_name != NULL && strcmp(list_label, selected_list_name) == 0);
        }

        printf(" ");
        render_setup_option(time_label, time_selected, focus_time && time_selected, use_color, 20);
        printf(" ");
        render_setup_option(list_label, list_selected, !focus_time && list_selected, use_color, 20);
        printf("\n");
    }

    printf("\n Left/Right: switch field  Up/Down: change option\n");
    printf(" Enter: start run  Esc: back to main menu\n");
    fflush(stdout);
}

void terminal_render_custom_setup(bool use_color, int selected_time_seconds, const char *const *file_names, size_t file_count)
{
    static const int time_options[] = { 15, 30, 60, 120 };

    terminal_clear_screen();
    fputs(CURSOR_HIDE, stdout);
    render_logo(use_color);

    printf(" %sCustom Setup%s\n\n", color(use_color, ANSI_BOLD), color(use_color, ANSI_RESET));
    printf(" %sTime%s\n", color(use_color, ANSI_BOLD), color(use_color, ANSI_RESET));

    for (size_t i = 0; i < sizeof(time_options) / sizeof(time_options[0]); i++) {
        char label[16];
        snprintf(label, sizeof(label), "%ds", time_options[i]);
        printf(" ");
        render_setup_option(label, time_options[i] == selected_time_seconds, time_options[i] == selected_time_seconds, use_color, 20);
        printf("\n");
    }

    printf("\n %sdata/custom files%s\n", color(use_color, ANSI_BOLD), color(use_color, ANSI_RESET));
    if (file_count == 0) {
        printf(" %s(no readable .txt files found)%s\n", color(use_color, ANSI_RED), color(use_color, ANSI_RESET));
    } else {
        for (size_t i = 0; i < file_count; i++) {
            printf(" - %s\n", file_names[i]);
        }
    }

    printf("\n Up/Down: change time\n");
    printf(" Enter: start run  Esc: back to main menu\n");
    fflush(stdout);
}
