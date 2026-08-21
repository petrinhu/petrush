/*
 * expand.c — tilde and environment variable expansion
 */

#include "petrush/expand.h"
#include "petrush/env.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

static int is_name_char(char c)
{
    return isalnum((unsigned char)c) || c == '_';
}

char *expand_word(const char *word)
{
    if (!word) return NULL;

    /* UX-12: ~ and ~/ */
    if (word[0] == '~' && (word[1] == '\0' || word[1] == '/')) {
        const char *home = petrush_getenv("HOME");
        if (!home) home = "";
        size_t need = strlen(home) + strlen(word); /* home + rest without ~ */
        char *out = malloc(need + 1);
        if (!out) return NULL;
        if (word[1] == '\0') {
            snprintf(out, need + 1, "%s", home);
        } else {
            snprintf(out, need + 1, "%s%s", home, word + 1);
        }
        return out;
    }

    /* UX-13: $VAR and ${VAR} anywhere in word */
    size_t cap = strlen(word) * 4 + 64;
    if (cap < 256) cap = 256;
    char *out = malloc(cap);
    if (!out) return NULL;
    size_t o = 0;
    const char *p = word;

    while (*p) {
        if (*p == '$' && p[1]) {
            const char *name = NULL;
            size_t nlen = 0;
            if (p[1] == '{') {
                const char *q = p + 2;
                while (*q && *q != '}') q++;
                if (*q == '}') {
                    name = p + 2;
                    nlen = (size_t)(q - name);
                    p = q + 1;
                } else {
                    /* literal $ */
                    if (o + 1 >= cap) {
                        cap *= 2;
                        char *n = realloc(out, cap);
                        if (!n) { free(out); return NULL; }
                        out = n;
                    }
                    out[o++] = *p++;
                    continue;
                }
            } else if (is_name_char(p[1])) {
                name = p + 1;
                const char *q = name;
                while (is_name_char(*q)) q++;
                nlen = (size_t)(q - name);
                p = q;
            } else {
                if (o + 1 >= cap) {
                    cap *= 2;
                    char *n = realloc(out, cap);
                    if (!n) { free(out); return NULL; }
                    out = n;
                }
                out[o++] = *p++;
                continue;
            }

            char nbuf[256];
            if (nlen >= sizeof(nbuf)) nlen = sizeof(nbuf) - 1;
            memcpy(nbuf, name, nlen);
            nbuf[nlen] = '\0';
            const char *val = petrush_getenv(nbuf);
            if (!val) val = "";
            size_t vlen = strlen(val);
            while (o + vlen + 1 >= cap) {
                cap *= 2;
                char *n = realloc(out, cap);
                if (!n) { free(out); return NULL; }
                out = n;
            }
            memcpy(out + o, val, vlen);
            o += vlen;
            continue;
        }

        if (o + 1 >= cap) {
            cap *= 2;
            char *n = realloc(out, cap);
            if (!n) { free(out); return NULL; }
            out = n;
        }
        out[o++] = *p++;
    }
    out[o] = '\0';
    return out;
}

void expand_cmd_argv(petrush_cmd_t *cmd)
{
    if (!cmd || !cmd->argv) return;
    for (int i = 0; i < cmd->argc; i++) {
        if (!cmd->argv[i]) continue;
        char *e = expand_word(cmd->argv[i]);
        if (e) {
            free(cmd->argv[i]);
            cmd->argv[i] = e;
        }
    }
    /* also expand redir paths */
    if (cmd->redir_in) {
        char *e = expand_word(cmd->redir_in);
        if (e) { free(cmd->redir_in); cmd->redir_in = e; }
    }
    if (cmd->redir_out) {
        char *e = expand_word(cmd->redir_out);
        if (e) { free(cmd->redir_out); cmd->redir_out = e; }
    }
    if (cmd->redir_err) {
        char *e = expand_word(cmd->redir_err);
        if (e) { free(cmd->redir_err); cmd->redir_err = e; }
    }
}
