/*
 * alias.c — Tabela simples de aliases (Rule of 3, anti-OE)
 */

#include "petrush/alias.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define ALIAS_MAX 64
#define ALIAS_NAME_MAX 64
#define ALIAS_VAL_MAX 512

typedef struct {
    char name[ALIAS_NAME_MAX];
    char value[ALIAS_VAL_MAX];
    int used;
} alias_entry_t;

static alias_entry_t g_aliases[ALIAS_MAX];

static int valid_name(const char *name)
{
    if (!name || !*name) return 0;
    if (!isalpha((unsigned char)name[0]) && name[0] != '_') return 0;
    for (const char *p = name; *p; p++) {
        if (!isalnum((unsigned char)*p) && *p != '_' && *p != '-') return 0;
    }
    return 1;
}

void alias_clear_all(void)
{
    memset(g_aliases, 0, sizeof(g_aliases));
}

int alias_set(const char *name, const char *value)
{
    if (!valid_name(name) || !value) return -1;
    if (strlen(name) >= ALIAS_NAME_MAX) return -1;
    if (strlen(value) >= ALIAS_VAL_MAX) return -1;

    /* update existing */
    for (int i = 0; i < ALIAS_MAX; i++) {
        if (g_aliases[i].used && strcmp(g_aliases[i].name, name) == 0) {
            snprintf(g_aliases[i].value, sizeof(g_aliases[i].value), "%s", value);
            return 0;
        }
    }
    /* free slot */
    for (int i = 0; i < ALIAS_MAX; i++) {
        if (!g_aliases[i].used) {
            g_aliases[i].used = 1;
            snprintf(g_aliases[i].name, sizeof(g_aliases[i].name), "%s", name);
            snprintf(g_aliases[i].value, sizeof(g_aliases[i].value), "%s", value);
            return 0;
        }
    }
    return -1; /* table full */
}

int alias_unset(const char *name)
{
    if (!name) return -1;
    for (int i = 0; i < ALIAS_MAX; i++) {
        if (g_aliases[i].used && strcmp(g_aliases[i].name, name) == 0) {
            g_aliases[i].used = 0;
            g_aliases[i].name[0] = '\0';
            g_aliases[i].value[0] = '\0';
            return 0;
        }
    }
    return -1;
}

const char *alias_get(const char *name)
{
    if (!name) return NULL;
    for (int i = 0; i < ALIAS_MAX; i++) {
        if (g_aliases[i].used && strcmp(g_aliases[i].name, name) == 0) {
            return g_aliases[i].value;
        }
    }
    return NULL;
}

void alias_list_print(void)
{
    for (int i = 0; i < ALIAS_MAX; i++) {
        if (g_aliases[i].used) {
            printf("alias %s='%s'\n", g_aliases[i].name, g_aliases[i].value);
        }
    }
}

char *alias_expand_line(const char *line)
{
    if (!line) return NULL;

    /* skip leading space */
    while (*line && isspace((unsigned char)*line)) line++;

    /* extract first word */
    const char *start = line;
    const char *p = line;
    while (*p && !isspace((unsigned char)*p) && *p != '|' && *p != '<' &&
           *p != '>' && *p != '&') {
        p++;
    }
    size_t wlen = (size_t)(p - start);
    if (wlen == 0) {
        return strdup(line);
    }
    if (wlen >= ALIAS_NAME_MAX) {
        return strdup(line);
    }

    char word[ALIAS_NAME_MAX];
    memcpy(word, start, wlen);
    word[wlen] = '\0';

    const char *repl = alias_get(word);
    if (!repl) {
        return strdup(line);
    }

    /* rest of line after first word */
    const char *rest = p;
    size_t need = strlen(repl) + strlen(rest) + 1;
    char *out = malloc(need);
    if (!out) return NULL;
    snprintf(out, need, "%s%s", repl, rest);
    return out;
}
