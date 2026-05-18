#ifndef CMONKEY_TERMINAL_H
#define CMONKEY_TERMINAL_H

#include <stdbool.h>
#include <stddef.h>

#include "engine.h"
#include "metrics.h"

/** Install signal handlers that restore terminal state before exiting. */
void terminal_init(void);

/** Enable raw terminal mode with non-blocking reads. */
int terminal_enable_raw_mode(void);

/** Restore the original terminal mode and show cursor. */
void terminal_restore(void);

/** Clear the screen and move cursor to top-left. */
void terminal_clear_screen(void);

/** Render the live typing test screen. */
void terminal_render_test(const TestSession *session, const Metrics *metrics, long long now_us, bool use_color);

/** Render the post-run results screen. */
void terminal_render_results(const TestSession *session, const Metrics *metrics, const char *difficulty_name, bool use_color);

/** Render the startup main menu. */
void terminal_render_main_menu(bool use_color);

/** Render a run setup screen with time and difficulty selectors. */
void terminal_render_run_setup(bool use_color, const char *title, int selected_time_seconds, const char *selected_list_name, bool focus_time);

/** Render custom-mode setup with time selector and discovered file list. */
void terminal_render_custom_setup(bool use_color, int selected_time_seconds, const char *const *file_names, size_t file_count);

#endif
