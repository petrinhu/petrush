/*
 * complete.c — linenoise completion + history autosuggest
 */

#include "petrush/complete.h"
#include "petrush/dispatcher.h"
#include "petrush/env.h"
#include "petrush/highlight.h"
#include "petrush/ui_port.h"
#include "linenoise.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <ctype.h>

/* linenoise highlight hook → Front colorize (UX-21). */
static char *highlight_cb(const char *buf, size_t len)
{
    return petrush_hl_colorize(buf, len);
}

static void completion_cb(const char *buf, linenoiseCompletions *lc)
{
    if (!buf) return;

    /* última palavra (após espaço) */
    const char *word = buf;
    const char *sp = strrchr(buf, ' ');
    if (sp) word = sp + 1;

    size_t wlen = strlen(word);

    /* builtins (só se primeira palavra) */
    if (word == buf || (sp && sp == buf - 1)) {
        /* first word */
    }
    int first = (word == buf);
    if (first) {
        int n = petrush_builtin_count();
        for (int i = 0; i < n; i++) {
            const char *name = petrush_builtin_name(i);
            if (name && strncmp(name, word, wlen) == 0) {
                linenoiseAddCompletion(lc, name);
            }
        }
        /* PATH executables */
        const char *path_env = petrush_getenv("PATH");
        if (!path_env) path_env = "/bin:/usr/bin";
        char *pc = strdup(path_env);
        if (pc) {
            for (char *dir = strtok(pc, ":"); dir; dir = strtok(NULL, ":")) {
                DIR *d = opendir(dir);
                if (!d) continue;
                struct dirent *ent;
                while ((ent = readdir(d)) != NULL) {
                    if (strncmp(ent->d_name, word, wlen) != 0) continue;
                    char full[PATH_MAX];
                    if ((size_t)snprintf(full, sizeof(full), "%s/%s", dir, ent->d_name) >= sizeof(full)) {
                        continue;
                    }
                    if (access(full, X_OK) == 0) {
                        linenoiseAddCompletion(lc, ent->d_name);
                    }
                }
                closedir(d);
            }
            free(pc);
        }
    }

    /* file completion for current word as path prefix */
    {
        char dirpath[PATH_MAX];
        const char *base = word;
        const char *slash = strrchr(word, '/');
        if (slash) {
            size_t dlen = (size_t)(slash - word);
            if (dlen >= sizeof(dirpath)) return;
            memcpy(dirpath, word, dlen);
            dirpath[dlen] = '\0';
            if (dlen == 0) {
                snprintf(dirpath, sizeof(dirpath), "/");
            }
            base = slash + 1;
        } else {
            snprintf(dirpath, sizeof(dirpath), ".");
        }
        size_t blen = strlen(base);
        DIR *d = opendir(dirpath);
        if (!d) return;
        struct dirent *ent;
        while ((ent = readdir(d)) != NULL) {
            if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
            if (strncmp(ent->d_name, base, blen) != 0) continue;
            char candidate[PATH_MAX];
            int cn;
            if (slash) {
                cn = snprintf(candidate, sizeof(candidate), "%s/%s", dirpath, ent->d_name);
            } else {
                cn = snprintf(candidate, sizeof(candidate), "%s", ent->d_name);
            }
            if (cn < 0 || (size_t)cn >= sizeof(candidate)) continue;
            /* full line prefix + candidate */
            char full_line[PATH_MAX];
            size_t prefix_len = (size_t)(word - buf);
            if (prefix_len >= sizeof(full_line)) continue;
            if (prefix_len + (size_t)cn + 1 > sizeof(full_line)) continue;
            memcpy(full_line, buf, prefix_len);
            memcpy(full_line + prefix_len, candidate, (size_t)cn + 1);
            linenoiseAddCompletion(lc, full_line);
        }
        closedir(d);
    }
}

int petrush_complete_count(const char *buf)
{
    linenoiseCompletions lc = {0, NULL};
    completion_cb(buf ? buf : "", &lc);
    int n = (int)lc.len;
    for (size_t i = 0; i < lc.len; i++) free(lc.cvec[i]);
    free(lc.cvec);
    return n;
}

char *petrush_history_hint(const char *buf)
{
    if (!buf || !*buf) return NULL;
    size_t blen = strlen(buf);
    int n = linenoiseHistoryLen();
    /* newest first */
    for (int i = n - 1; i >= 0; i--) {
        const char *h = linenoiseHistoryGet(i);
        if (!h || !*h) continue;
        if (strlen(h) <= blen) continue;
        if (strncmp(h, buf, blen) == 0) {
            return strdup(h + blen); /* only the ghost suffix */
        }
    }
    return NULL;
}

static char *hints_cb(const char *buf, int *color, int *bold)
{
    char *hint = petrush_history_hint(buf);
    if (hint) {
        *color = 35; /* magenta-ish ghost */
        *bold = 0;
    }
    return hint;
}

static void free_hints_cb(void *ptr)
{
    free(ptr);
}

void petrush_setup_linenoise_ux(void)
{
    static const petrush_ui_port_t linenoise_port = {
        .clear_screen = linenoiseClearScreen,
        .history_len = linenoiseHistoryLen,
        .history_get = linenoiseHistoryGet,
    };

    linenoiseSetCompletionCallback(completion_cb);
    linenoiseSetHintsCallback(hints_cb);
    linenoiseSetFreeHintsCallback(free_hints_cb);
    /* NO_COLOR (qualquer valor) desliga highlight; sem flag nova. */
    if (!getenv("NO_COLOR"))
        linenoiseSetHighlightCallback(highlight_cb);

    /* ARCH-03: Mid usa a porta; adapter linenoise só aqui. */
    petrush_ui_port_bind(&linenoise_port);
}
