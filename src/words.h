#ifndef CMONKEY_WORDS_H
#define CMONKEY_WORDS_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    WORD_LIST_EASY = 0,
    WORD_LIST_COMMON = 1,
    WORD_LIST_FULL = 2
} WordListKind;

typedef struct {
    const char **easy_words;
    size_t easy_count;
    bool easy_owned;

    const char **common_words;
    size_t common_count;
    bool common_owned;

    const char **full_words;
    size_t full_count;
    bool full_owned;

    const char **quotes;
    size_t quote_count;
    bool quotes_owned;

    const char **custom_lines;
    size_t custom_line_count;
    bool custom_lines_owned;

    const char **custom_files;
    size_t custom_file_count;
    bool custom_files_owned;
} WordsDb;

/** Initialize words/quotes from local files, with built-in fallback lists. */
int words_init(WordsDb *db);

/** Release any dynamically allocated lists loaded at startup. */
void words_free(WordsDb *db);

/** Convert list name into enum kind, defaulting to common. */
WordListKind words_list_from_name(const char *name);

/** Return canonical list name for a list kind. */
const char *words_list_name(WordListKind kind);

/** Build a target string; common mode uses weighted difficulty generation. */
int words_build_target(const WordsDb *db, WordListKind list_kind, int word_count, char *out, size_t out_size);

/** Select one random quote into out. */
int words_random_quote(const WordsDb *db, char *out, size_t out_size);

/** Build a target string from quotes filtered by difficulty. */
int words_build_quote_target(const WordsDb *db, WordListKind list_kind, int word_count, char *out, size_t out_size);

/** Build a target string from lines loaded from .txt files in data/custom. */
int words_build_custom_target(const WordsDb *db, int word_count, char *out, size_t out_size);

/** Return number of discovered custom .txt files. */
size_t words_custom_file_count(const WordsDb *db);

/** Return custom file name at index, or NULL when out of range. */
const char *words_custom_file_name(const WordsDb *db, size_t index);

#endif
