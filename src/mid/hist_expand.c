/*
 * hist_expand.c — !! and !n history expansion (minimal)
 */

#include "petrush/hist_expand.h"
#include "linenoise.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

char *hist_expand_line(const char *line)
{
    if (!line) return NULL;

    /* trim leading space for match only */
    while (*line && isspace((unsigned char)*line)) line++;

    if (strcmp(line, "!!") == 0) {
        int n = linenoiseHistoryLen();
        if (n <= 0) return NULL;
        /* last real entry — skip empty trailing if any */
        for (int i = n - 1; i >= 0; i--) {
            const char *h = linenoiseHistoryGet(i);
            if (h && h[0] && strcmp(h, "!!") != 0) {
                return strdup(h);
            }
        }
        return NULL;
    }

    if (line[0] == '!' && isdigit((unsigned char)line[1])) {
        char *end = NULL;
        long num = strtol(line + 1, &end, 10);
        if (end && *end == '\0' && num >= 1) {
            int idx = (int)num - 1; /* 1-based oldest */
            const char *h = linenoiseHistoryGet(idx);
            if (h) return strdup(h);
        }
    }

    return NULL;
}
