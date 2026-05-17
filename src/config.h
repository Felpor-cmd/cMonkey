#ifndef CMONKEY_CONFIG_H
#define CMONKEY_CONFIG_H

#include <stdbool.h>

typedef struct {
    int default_time;
    int default_words;
    char word_list[16];
    bool color;
} Config;

/** Populate a config struct with built-in defaults. */
void config_set_defaults(Config *cfg);

/** Load config from ~/.config/cmonkey/config and create it if missing. */
int config_load(Config *cfg);

#endif
