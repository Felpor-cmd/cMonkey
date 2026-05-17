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

#endif
