#include "config.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *CONFIG_DIR_1 = ".config";
static const char *CONFIG_DIR_2 = "cmonkey";
static const char *CONFIG_FILE = "config";

static char *trim(char *s)
{
    while (*s != '\0' && isspace((unsigned char)*s)) {
        s++;
    }
    if (*s == '\0') {
        return s;
    }

    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }
    return s;
}

static int parse_bool(const char *v, bool *out)
{
    if (strcmp(v, "true") == 0 || strcmp(v, "1") == 0 || strcmp(v, "yes") == 0) {
        *out = true;
        return 0;
    }
    if (strcmp(v, "false") == 0 || strcmp(v, "0") == 0 || strcmp(v, "no") == 0) {
        *out = false;
        return 0;
    }
    return -1;
}

static int build_paths(char *dir_path, size_t dir_path_size, char *file_path, size_t file_path_size)
{
    const char *home = getenv("HOME");
    if (home == NULL || home[0] == '\0') {
        return -1;
    }

    if (snprintf(dir_path, dir_path_size, "%s/%s", home, CONFIG_DIR_1) >= (int)dir_path_size) {
        return -1;
    }
    if (mkdir(dir_path, 0755) < 0 && errno != EEXIST) {
        return -1;
    }

    if (snprintf(dir_path, dir_path_size, "%s/%s/%s", home, CONFIG_DIR_1, CONFIG_DIR_2) >= (int)dir_path_size) {
        return -1;
    }
    if (mkdir(dir_path, 0755) < 0 && errno != EEXIST) {
        return -1;
    }

    if (snprintf(file_path, file_path_size, "%s/%s", dir_path, CONFIG_FILE) >= (int)file_path_size) {
        return -1;
    }
    return 0;
}

static int write_default_config(const char *path, const Config *cfg)
{
    FILE *fp = fopen(path, "w");
    if (fp == NULL) {
        return -1;
    }
    fprintf(fp,
            "# ~/.config/cmonkey/config\n\n"
            "default_time = %d\n"
            "default_words = %d\n"
            "word_list = %s\n"
            "color = %s\n",
            cfg->default_time,
            cfg->default_words,
            cfg->word_list,
            cfg->color ? "true" : "false");
    fclose(fp);
    return 0;
}

void config_set_defaults(Config *cfg)
{
    cfg->default_time = 60;
    cfg->default_words = 25;
    strcpy(cfg->word_list, "common");
    cfg->color = true;
}

int config_load(Config *cfg)
{
    config_set_defaults(cfg);

    char dir_path[512];
    char file_path[1024];
    if (build_paths(dir_path, sizeof(dir_path), file_path, sizeof(file_path)) != 0) {
        return -1;
    }

    if (access(file_path, F_OK) != 0) {
        if (write_default_config(file_path, cfg) != 0) {
            return -1;
        }
        return 0;
    }

    FILE *fp = fopen(file_path, "r");
    if (fp == NULL) {
        return -1;
    }

    char line[512];
    while (fgets(line, sizeof(line), fp) != NULL) {
        char *comment = strchr(line, '#');
        if (comment != NULL) {
            *comment = '\0';
        }

        char *eq = strchr(line, '=');
        if (eq == NULL) {
            continue;
        }

        *eq = '\0';
        char *key = trim(line);
        char *value = trim(eq + 1);
        if (key[0] == '\0' || value[0] == '\0') {
            continue;
        }

        if (strcmp(key, "default_time") == 0) {
            const int v = atoi(value);
            if (v > 0) {
                cfg->default_time = v;
            }
        } else if (strcmp(key, "default_words") == 0) {
            const int v = atoi(value);
            if (v > 0) {
                cfg->default_words = v;
            }
        } else if (strcmp(key, "word_list") == 0) {
            if (strcmp(value, "easy") == 0 || strcmp(value, "common") == 0 || strcmp(value, "full") == 0) {
                strncpy(cfg->word_list, value, sizeof(cfg->word_list) - 1);
                cfg->word_list[sizeof(cfg->word_list) - 1] = '\0';
            }
        } else if (strcmp(key, "color") == 0) {
            bool parsed = cfg->color;
            if (parse_bool(value, &parsed) == 0) {
                cfg->color = parsed;
            }
        }
    }

    fclose(fp);
    return 0;
}
