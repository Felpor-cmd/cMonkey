#include "words.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *FALLBACK_EASY[] = {
    "cat", "dog", "sun", "moon", "book", "tree", "star", "bird", "home", "milk",
    "rain", "wind", "road", "stone", "apple", "grape", "chair", "table", "light", "cloud",
    "river", "laugh", "green", "quiet", "small", "music", "water", "happy", "paper", "night"
};

static const char *FALLBACK_COMMON[] = {
    "about", "after", "again", "almost", "always", "answer", "around", "because", "before", "between",
    "build", "change", "common", "create", "during", "early", "enough", "family", "friend", "future",
    "great", "group", "happen", "health", "important", "inside", "learn", "little", "local", "market",
    "number", "office", "people", "planet", "power", "public", "record", "result", "school", "second",
    "simple", "system", "things", "travel", "under", "value", "world", "yellow", "window", "writing"
};

static const char *FALLBACK_FULL[] = {
    "abstraction", "algorithm", "allocation", "analysis", "architecture", "asynchronous", "bandwidth", "calibration",
    "characteristic", "compatibility", "compression", "concurrency", "configuration", "consistency", "declaration", "deterministic",
    "efficiency", "encapsulation", "exception", "execution", "expression", "foundation", "generation", "implementation",
    "instrumentation", "integration", "interface", "optimization", "performance", "persistence", "polymorphism", "predictable",
    "refactoring", "resilience", "scalability", "simulation", "specification", "synchronization", "throughput", "validation"
};

static const char *FALLBACK_QUOTES[] = {
    "the quick brown fox jumps over the lazy dog",
    "small steps every day become massive progress over time",
    "consistency beats intensity when you are building a skill",
    "practice does not make perfect practice makes progress",
    "clarity comes from action not from overthinking"
};

static void free_owned_list(const char **list, size_t count, bool owned)
{
    if (!owned || list == NULL) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        free((void *)list[i]);
    }
    free((void *)list);
}

static void strip_newline(char *line)
{
    const size_t len = strlen(line);
    if (len == 0) {
        return;
    }
    size_t end = len;
    while (end > 0) {
        const char c = line[end - 1];
        if (c != '\n' && c != '\r') {
            break;
        }
        line[end - 1] = '\0';
        end--;
    }
}

static int load_lines_file(const char *path, const char ***out_list, size_t *out_count)
{
    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        return -1;
    }

    char **list = NULL;
    size_t count = 0;
    size_t cap = 0;
    char *line = NULL;
    size_t line_cap = 0;
    ssize_t n = 0;

    while ((n = getline(&line, &line_cap, fp)) != -1) {
        (void)n;
        strip_newline(line);
        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }

        if (count == cap) {
            size_t next_cap = cap == 0 ? 64 : cap * 2;
            char **tmp = realloc(list, next_cap * sizeof(*tmp));
            if (tmp == NULL) {
                free(line);
                fclose(fp);
                free_owned_list((const char **)list, count, true);
                return -1;
            }
            list = tmp;
            cap = next_cap;
        }

        char *copy = strdup(line);
        if (copy == NULL) {
            free(line);
            fclose(fp);
            free_owned_list((const char **)list, count, true);
            return -1;
        }
        list[count++] = copy;
    }

    free(line);
    fclose(fp);

    if (count == 0) {
        free(list);
        return -1;
    }

    *out_list = (const char **)list;
    *out_count = count;
    return 0;
}

static int load_from_data_candidates(const char *filename, const char ***out_list, size_t *out_count)
{
    const char *prefixes[] = { "data", "./data", "/usr/local/share/cmonkey" };
    char path[512];

    for (size_t i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); i++) {
        if (snprintf(path, sizeof(path), "%s/%s", prefixes[i], filename) >= (int)sizeof(path)) {
            continue;
        }
        if (load_lines_file(path, out_list, out_count) == 0) {
            return 0;
        }
    }
    return -1;
}

static const char **select_list(const WordsDb *db, WordListKind kind, size_t *count)
{
    if (kind == WORD_LIST_EASY) {
        *count = db->easy_count;
        return db->easy_words;
    }
    if (kind == WORD_LIST_FULL) {
        *count = db->full_count;
        return db->full_words;
    }
    *count = db->common_count;
    return db->common_words;
}

int words_init(WordsDb *db)
{
    memset(db, 0, sizeof(*db));

    if (load_from_data_candidates("words_1k.txt", &db->easy_words, &db->easy_count) == 0) {
        db->easy_owned = true;
    } else {
        db->easy_words = FALLBACK_EASY;
        db->easy_count = sizeof(FALLBACK_EASY) / sizeof(FALLBACK_EASY[0]);
    }

    if (load_from_data_candidates("words_10k.txt", &db->common_words, &db->common_count) == 0) {
        db->common_owned = true;
    } else {
        db->common_words = FALLBACK_COMMON;
        db->common_count = sizeof(FALLBACK_COMMON) / sizeof(FALLBACK_COMMON[0]);
    }

    if (load_from_data_candidates("words_10k.txt", &db->full_words, &db->full_count) == 0) {
        db->full_owned = true;
    } else {
        db->full_words = FALLBACK_FULL;
        db->full_count = sizeof(FALLBACK_FULL) / sizeof(FALLBACK_FULL[0]);
    }

    if (load_from_data_candidates("quotes.txt", &db->quotes, &db->quote_count) == 0) {
        db->quotes_owned = true;
    } else {
        db->quotes = FALLBACK_QUOTES;
        db->quote_count = sizeof(FALLBACK_QUOTES) / sizeof(FALLBACK_QUOTES[0]);
    }

    return 0;
}

void words_free(WordsDb *db)
{
    free_owned_list(db->easy_words, db->easy_count, db->easy_owned);
    free_owned_list(db->common_words, db->common_count, db->common_owned);
    free_owned_list(db->full_words, db->full_count, db->full_owned);
    free_owned_list(db->quotes, db->quote_count, db->quotes_owned);
    memset(db, 0, sizeof(*db));
}

WordListKind words_list_from_name(const char *name)
{
    if (name != NULL && strcmp(name, "easy") == 0) {
        return WORD_LIST_EASY;
    }
    if (name != NULL && strcmp(name, "full") == 0) {
        return WORD_LIST_FULL;
    }
    return WORD_LIST_COMMON;
}

const char *words_list_name(WordListKind kind)
{
    if (kind == WORD_LIST_EASY) {
        return "easy";
    }
    if (kind == WORD_LIST_FULL) {
        return "full";
    }
    return "common";
}

int words_build_target(const WordsDb *db, WordListKind list_kind, int word_count, char *out, size_t out_size)
{
    if (out == NULL || out_size == 0 || word_count <= 0) {
        return -1;
    }

    size_t list_count = 0;
    const char **list = select_list(db, list_kind, &list_count);
    if (list == NULL || list_count == 0) {
        return -1;
    }

    size_t pos = 0;
    for (int i = 0; i < word_count; i++) {
        const char *word = list[rand() % list_count];
        const size_t len = strlen(word);
        const bool needs_space = (i + 1) < word_count;

        if (pos + len + (needs_space ? 1 : 0) + 1 > out_size) {
            return -1;
        }

        memcpy(out + pos, word, len);
        pos += len;

        if (needs_space) {
            out[pos++] = ' ';
        }
    }

    out[pos] = '\0';
    return 0;
}

int words_random_quote(const WordsDb *db, char *out, size_t out_size)
{
    if (db->quotes == NULL || db->quote_count == 0 || out == NULL || out_size == 0) {
        return -1;
    }

    const char *quote = db->quotes[rand() % db->quote_count];
    if (strlen(quote) + 1 > out_size) {
        return -1;
    }
    strcpy(out, quote);
    return 0;
}
