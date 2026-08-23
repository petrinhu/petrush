/*
 * expand.c — tilde and environment variable expansion
 */

#include "petrush/expand.h"
#include "petrush/env.h"
#ifdef PETRUSH_HAVE_ASM
#include "petrush/asm.h"
#endif

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

/* expand_brace chama expand_word no word de :- / :+ */
char *expand_word(const char *word);

static int ensure_cap(char **out, size_t *cap, size_t need)
{
    while (need >= *cap) {
        size_t nc = *cap * 2;
        char *n = realloc(*out, nc);
        if (!n) return -1;
        *out = n;
        *cap = nc;
    }
    return 0;
}

static int append_bytes(char **out, size_t *o, size_t *cap,
                        const char *src, size_t len)
{
    if (ensure_cap(out, cap, *o + len + 1) != 0) return -1;
    memcpy(*out + *o, src, len);
    *o += len;
    return 0;
}

static int append_char(char **out, size_t *o, size_t *cap, char c)
{
    if (ensure_cap(out, cap, *o + 2) != 0) return -1;
    (*out)[(*o)++] = c;
    return 0;
}

/*
 * FEAT-PARAM: ${VAR}, ${VAR:-word}, ${VAR:+word}, ${#VAR}.
 * Sem nameref/${!}/replace/# % strip. Forma nao suportada = literal.
 * Retorna 0 e avanca *pp apos '}'. -1 = OOM. 1 = tratar '$' como literal.
 */
static int expand_brace(const char **pp, char **out, size_t *o, size_t *cap)
{
    const char *p = *pp; /* aponta para '{' */
    const char *body = p + 1;
    const char *end = body;
    while (*end && *end != '}') end++;
    if (*end != '}') return 1; /* unclosed: literal $ */

    const char *after = end + 1;
    const char *cur = body;

    /* ${#VAR} */
    if (*cur == '#') {
        cur++;
        if (!is_name_char(*cur)) return 1;
        const char *name = cur;
        while (is_name_char(*cur)) cur++;
        if (cur != end) return 1; /* ${#VAR...extra} fora de escopo */

        char nbuf[256];
        size_t nlen = (size_t)(cur - name);
        if (nlen >= sizeof(nbuf)) nlen = sizeof(nbuf) - 1;
        memcpy(nbuf, name, nlen);
        nbuf[nlen] = '\0';

        const char *val = petrush_getenv(nbuf);
        size_t vlen = val ? strlen(val) : 0;
        char lenbuf[32];
        int nw = snprintf(lenbuf, sizeof(lenbuf), "%zu", vlen);
        if (nw < 0) return -1;
        if (append_bytes(out, o, cap, lenbuf, (size_t)nw) != 0) return -1;
        *pp = after;
        return 0;
    }

    if (!is_name_char(*cur)) return 1;
    const char *name = cur;
    while (is_name_char(*cur)) cur++;
    size_t nlen = (size_t)(cur - name);
    if (nlen == 0) return 1;

    char nbuf[256];
    if (nlen >= sizeof(nbuf)) nlen = sizeof(nbuf) - 1;
    memcpy(nbuf, name, nlen);
    nbuf[nlen] = '\0';

    const char *val = petrush_getenv(nbuf);
    int null_or_unset = (val == NULL || val[0] == '\0');

    if (cur == end) {
        /* ${VAR} */
        if (!val) val = "";
        if (append_bytes(out, o, cap, val, strlen(val)) != 0) return -1;
        *pp = after;
        return 0;
    }

    /* ${VAR:-word} / ${VAR:+word} */
    if (cur[0] == ':' && (cur[1] == '-' || cur[1] == '+') &&
        cur + 2 <= end) {
        char op = cur[1];
        const char *word = cur + 2;
        size_t wlen = (size_t)(end - word);
        char *wbuf = NULL;
        char *wexp = NULL;
        const char *use = NULL;
        size_t ulen = 0;

        if (op == '-') {
            if (null_or_unset) {
                wbuf = malloc(wlen + 1);
                if (!wbuf) return -1;
                memcpy(wbuf, word, wlen);
                wbuf[wlen] = '\0';
                wexp = expand_word(wbuf);
                free(wbuf);
                if (!wexp) return -1;
                use = wexp;
                ulen = strlen(wexp);
            } else {
                use = val;
                ulen = strlen(val);
            }
        } else { /* '+' */
            if (!null_or_unset) {
                wbuf = malloc(wlen + 1);
                if (!wbuf) return -1;
                memcpy(wbuf, word, wlen);
                wbuf[wlen] = '\0';
                wexp = expand_word(wbuf);
                free(wbuf);
                if (!wexp) return -1;
                use = wexp;
                ulen = strlen(wexp);
            } else {
                use = "";
                ulen = 0;
            }
        }

        int rc = append_bytes(out, o, cap, use, ulen);
        free(wexp);
        if (rc != 0) return -1;
        *pp = after;
        return 0;
    }

    /* operador nao suportado (${!}, ${VAR#}, ${VAR%}, ${VAR/}...) */
    return 1;
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

    /* UX-13 + FEAT-PARAM: $VAR / ${VAR} / ${VAR:-} / ${VAR:+} / ${#VAR} */
    size_t cap = strlen(word) * 4 + 64;
    if (cap < 256) cap = 256;
    char *out = malloc(cap);
    if (!out) return NULL;
    size_t o = 0;
    const char *p = word;

    while (*p) {
        if (*p == '$' && p[1]) {
            if (p[1] == '{') {
                const char *brace = p + 1;
                int br = expand_brace(&brace, &out, &o, &cap);
                if (br == -1) { free(out); return NULL; }
                if (br == 0) {
                    p = brace;
                    continue;
                }
                /* unsupported / unclosed: emit literal '$' */
                if (append_char(&out, &o, &cap, *p) != 0) {
                    free(out);
                    return NULL;
                }
                p++;
                continue;
            }
            if (is_name_char(p[1])) {
                const char *name = p + 1;
                const char *q = name;
                while (is_name_char(*q)) q++;
                size_t nlen = (size_t)(q - name);
                char nbuf[256];
                if (nlen >= sizeof(nbuf)) nlen = sizeof(nbuf) - 1;
                memcpy(nbuf, name, nlen);
                nbuf[nlen] = '\0';
                const char *val = petrush_getenv(nbuf);
                if (!val) val = "";
                if (append_bytes(&out, &o, &cap, val, strlen(val)) != 0) {
                    free(out);
                    return NULL;
                }
                p = q;
                continue;
            }
            if (append_char(&out, &o, &cap, *p) != 0) {
                free(out);
                return NULL;
            }
            p++;
            continue;
        }

        if (append_char(&out, &o, &cap, *p) != 0) {
            free(out);
            return NULL;
        }
        p++;
    }
    out[o] = '\0';
    return out;
}

/* UX-18 / ASM-GLOB: * = sequência, ? = um byte; [ literal (sem classes []). */
static int match_pat(const char *pat, const char *str)
{
#ifdef PETRUSH_HAVE_ASM
    return petrush_glob_match(pat, str);
#else
    /* Fallback iterativo (mesmo contrato) quando PETRUSH_ASM=OFF. */
    const char *p = pat;
    const char *s = str;
    const char *star_p = NULL;
    const char *star_s = NULL;

    while (*s) {
        if (*p == '*') {
            star_p = ++p;
            star_s = s;
            while (*p == '*') {
                star_p = ++p;
            }
            if (!*p) return 1;
        } else if (*p == '?' || (*p && *p == *s)) {
            p++;
            s++;
        } else if (star_p) {
            p = star_p;
            s = ++star_s;
        } else {
            return 0;
        }
    }
    while (*p == '*') p++;
    return *p == '\0';
#endif
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
