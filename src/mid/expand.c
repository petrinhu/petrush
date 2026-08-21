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
#include <dirent.h>

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

/* UX-18: matcher próprio (* = sequência, ? = um byte; [ literal). */
static int match_pat(const char *pat, const char *str)
{
    while (*pat && *str) {
        if (*pat == '*') {
            pat++;
            if (!*pat) return 1;
            while (*str) {
                if (match_pat(pat, str)) return 1;
                str++;
            }
            return 0;
        }
        if (*pat == '?') {
            pat++;
            str++;
            continue;
        }
        if (*pat != *str) return 0;
        pat++;
        str++;
    }
    while (*pat == '*') pat++;
    return *pat == '\0' && *str == '\0';
}

static int pattern_has_meta(const char *s)
{
    for (; *s; s++) {
        if (*s == '*' || *s == '?') return 1;
    }
    return 0;
}

static int cmp_strptr(const void *a, const void *b)
{
    const char *sa = *(const char *const *)a;
    const char *sb = *(const char *const *)b;
    return strcmp(sa, sb);
}

static char **glob_literal(const char *pattern, int *n)
{
    char **out = malloc(sizeof(char *));
    if (!out) {
        *n = -1;
        return NULL;
    }
    out[0] = strdup(pattern);
    if (!out[0]) {
        free(out);
        *n = -1;
        return NULL;
    }
    *n = 1;
    return out;
}

static void free_matches(char **m, int n)
{
    if (!m) return;
    for (int i = 0; i < n; i++) free(m[i]);
    free(m);
}

char **glob_word(const char *pattern, int *n)
{
    if (!pattern || !n) return NULL;
    *n = 0;

    /* ** fora de escopo: literal */
    if (strstr(pattern, "**") != NULL) {
        return glob_literal(pattern, n);
    }

    if (!pattern_has_meta(pattern)) {
        return glob_literal(pattern, n);
    }

    const char *slash = strrchr(pattern, '/');
    char dirbuf[PATH_MAX];
    const char *dir;
    const char *base;

    if (slash) {
        size_t dlen = (size_t)(slash - pattern);
        if (dlen == 0) {
            dir = "/";
        } else {
            if (dlen >= sizeof(dirbuf)) return glob_literal(pattern, n);
            memcpy(dirbuf, pattern, dlen);
            dirbuf[dlen] = '\0';
            dir = dirbuf;
        }
        base = slash + 1;
        /* meta no componente de diretório → literal */
        if (pattern_has_meta(dir)) {
            return glob_literal(pattern, n);
        }
        if (!pattern_has_meta(base)) {
            return glob_literal(pattern, n);
        }
    } else {
        dir = ".";
        base = pattern;
    }

    DIR *d = opendir(dir);
    if (!d) {
        return glob_literal(pattern, n);
    }

    int allow_dot = (base[0] == '.');
    char **matches = NULL;
    int count = 0;
    int cap = 0;
    int overflow = 0;

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        const char *name = ent->d_name;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
        if (name[0] == '.' && !allow_dot) continue;
        if (!match_pat(base, name)) continue;

        if (count >= PETRUSH_GLOB_MAX) {
            overflow = 1;
            break;
        }
        if (count >= cap) {
            int nc = cap ? cap * 2 : 8;
            char **nm = realloc(matches, sizeof(char *) * (size_t)nc);
            if (!nm) {
                overflow = 1;
                break;
            }
            matches = nm;
            cap = nc;
        }

        char path[PATH_MAX];
        int wr;
        if (slash) {
            if (strcmp(dir, "/") == 0) {
                wr = snprintf(path, sizeof(path), "/%s", name);
            } else {
                wr = snprintf(path, sizeof(path), "%s/%s", dir, name);
            }
        } else {
            wr = snprintf(path, sizeof(path), "%s", name);
        }
        if (wr < 0 || (size_t)wr >= sizeof(path)) {
            overflow = 1;
            break;
        }
        matches[count] = strdup(path);
        if (!matches[count]) {
            overflow = 1;
            break;
        }
        count++;
    }
    closedir(d);

    if (overflow) {
        free_matches(matches, count);
        *n = -1;
        return NULL;
    }

    if (count == 0) {
        free(matches);
        return glob_literal(pattern, n);
    }

    qsort(matches, (size_t)count, sizeof(char *), cmp_strptr);
    *n = count;
    return matches;
}

static int argv_is_quoted(const petrush_cmd_t *cmd, int i)
{
    if (!cmd->argv_quoted) return 0;
    return cmd->argv_quoted[i] != 0;
}

void expand_cmd_argv(petrush_cmd_t *cmd)
{
    if (!cmd || !cmd->argv) return;

    /* 1) ~ / $VAR em cada argv */
    for (int i = 0; i < cmd->argc; i++) {
        if (!cmd->argv[i]) continue;
        char *e = expand_word(cmd->argv[i]);
        if (e) {
            free(cmd->argv[i]);
            cmd->argv[i] = e;
        }
    }

    /* 2) glob * ? só em unquoted (depois de expand_word) */
    for (int i = 0; i < cmd->argc; ) {
        if (!cmd->argv[i] || argv_is_quoted(cmd, i) ||
            !pattern_has_meta(cmd->argv[i])) {
            i++;
            continue;
        }

        int gn = 0;
        char **g = glob_word(cmd->argv[i], &gn);
        if (!g) {
            /* fail closed: mantém o padrão */
            i++;
            continue;
        }

        if (gn == 1 && strcmp(g[0], cmd->argv[i]) == 0) {
            /* sem match real — já é o padrão */
            free_matches(g, gn);
            i++;
            continue;
        }

        /* splice: substitui 1 argv por N */
        int old_argc = cmd->argc;
        int new_argc = old_argc - 1 + gn;
        char **na = realloc(cmd->argv, sizeof(char *) * ((size_t)new_argc + 1));
        if (!na) {
            free_matches(g, gn);
            return;
        }
        cmd->argv = na;

        int *nq = NULL;
        if (cmd->argv_quoted) {
            nq = realloc(cmd->argv_quoted, sizeof(int) * (size_t)new_argc);
            if (!nq) {
                free_matches(g, gn);
                return;
            }
            cmd->argv_quoted = nq;
        } else {
            nq = calloc((size_t)new_argc, sizeof(int));
            if (!nq) {
                free_matches(g, gn);
                return;
            }
            cmd->argv_quoted = nq;
        }

        /* deslocar cauda para a direita/esquerda */
        if (gn != 1) {
            memmove(&cmd->argv[i + gn], &cmd->argv[i + 1],
                    sizeof(char *) * (size_t)(old_argc - i - 1));
            memmove(&cmd->argv_quoted[i + gn], &cmd->argv_quoted[i + 1],
                    sizeof(int) * (size_t)(old_argc - i - 1));
        }

        free(cmd->argv[i]);
        for (int j = 0; j < gn; j++) {
            cmd->argv[i + j] = g[j];
            /* matches nascem quoted=1: pipeline chama expand 2x */
            cmd->argv_quoted[i + j] = 1;
        }
        free(g);
        cmd->argc = new_argc;
        cmd->argv[cmd->argc] = NULL;
        i += gn;
    }

    /* redirs: só ~/$VAR — sem glob */
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
