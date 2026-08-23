/*
 * hist_expand.c — !! / !n / !$ / !^ history expansion (minimal)
 * FEAT-BANG: !$ = last arg; !^ = first arg (word 1). Sem !str, sem :h/:t.
 */

#include "petrush/hist_expand.h"
#include "petrush/ui_port.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Último evento real do history (pula vazio e "!!"). */
static const char *hist_last_event(void)
{
    int n = petrush_history_len();
    if (n <= 0) return NULL;
    for (int i = n - 1; i >= 0; i--) {
        const char *h = petrush_history_get(i);
        if (h && h[0] && strcmp(h, "!!") != 0) {
            return h;
        }
    }
    return NULL;
}

/*
 * Extrai word designator de uma linha de history.
 * '$' = última palavra; '^' = word 1 (primeiro arg). Sem word 1 → NULL.
 */
static char *hist_word_designator(const char *event, char which)
{
    const char *starts[64];
    size_t lens[64];
    int nwords = 0;
    const char *p = event;

    while (*p && nwords < 64) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        starts[nwords] = p;
        while (*p && !isspace((unsigned char)*p)) p++;
        lens[nwords] = (size_t)(p - starts[nwords]);
        nwords++;
    }
    if (nwords == 0) return NULL;

    int idx;
    if (which == '$') {
        idx = nwords - 1;
    } else if (which == '^') {
        if (nwords < 2) return NULL;
        idx = 1;
    } else {
        return NULL;
    }

    char *out = malloc(lens[idx] + 1);
    if (!out) return NULL;
    memcpy(out, starts[idx], lens[idx]);
    out[lens[idx]] = '\0';
    return out;
}

char *hist_expand_line(const char *line)
{
    if (!line) return NULL;

    /* trim leading space for match only */
    while (*line && isspace((unsigned char)*line)) line++;

    if (strcmp(line, "!!") == 0) {
        const char *h = hist_last_event();
        return h ? strdup(h) : NULL;
    }

    /* FEAT-BANG: !$ / !^ sobre o último evento (!!) */
    if (strcmp(line, "!$") == 0) {
        const char *h = hist_last_event();
        return h ? hist_word_designator(h, '$') : NULL;
    }
    if (strcmp(line, "!^") == 0) {
        const char *h = hist_last_event();
        return h ? hist_word_designator(h, '^') : NULL;
    }

    if (line[0] == '!' && isdigit((unsigned char)line[1])) {
        char *end = NULL;
        long num = strtol(line + 1, &end, 10);
        if (end && *end == '\0' && num >= 1) {
            int idx = (int)num - 1; /* 1-based oldest */
            const char *h = petrush_history_get(idx);
            if (h) return strdup(h);
        }
    }

    return NULL;
}
