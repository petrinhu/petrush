/*
 * expand.c — tilde and environment variable expansion + OSH-1/2 posicionais
 * + OSH-9 cmdsubst via hook DIP
 * + OSH-11 $((expr)) arith (sem arith.c; regra de 3)
 * + OSH-14 here-doc unquoted body expand (sem tilde/glob/split)
 * + OSH-16 $? / $- / shellopt e|u|x (sem set.c)
 * + OSH-17 set -u nounset (take + stderr)
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
#include <stdint.h>
#include <inttypes.h>
#include <dirent.h>

/* OSH-1: posicionais em struct (nao environ). */
static char *g_pos_arg0;
static char **g_pos_args;
static int g_pos_nargs;

/* OSH-9: hook DIP (NULL = '$' literal). */
static petrush_cmdsubst_hook_t g_cmdsubst_hook;

/* OSH-11: div0 na ultima expansao arith. */
static int g_arith_error;

/* OSH-17: expansao de parametro unset com set -u. */
static int g_nounset_error;

/* OSH-16: set -e/-u/-x bits + $?; C (noclobber) always-on em $-. */
static int g_opt_e;
static int g_opt_u;
static int g_opt_x;
static int g_last_status;
static char g_flags_buf[8];

void petrush_set_cmdsubst_hook(petrush_cmdsubst_hook_t hook)
{
    g_cmdsubst_hook = hook;
}

int petrush_take_arith_error(void)
{
    int e = g_arith_error;
    g_arith_error = 0;
    return e;
}

int petrush_take_nounset_error(void)
{
    int e = g_nounset_error;
    g_nounset_error = 0;
    return e;
}

static void nounset_fail(const char *name)
{
    fprintf(stderr, "petrush: %s: parameter not set\n", name ? name : "");
    g_nounset_error = 1;
}

/* NOLINTNEXTLINE(bugprone-easily-swappable-parameters) */
int petrush_shellopt_set(char flag, int on)
{
    int v = on ? 1 : 0;
    switch (flag) {
    case 'e':
        g_opt_e = v;
        return 0;
    case 'u':
        g_opt_u = v;
        return 0;
    case 'x':
        g_opt_x = v;
        return 0;
    default:
        return -1;
    }
}

int petrush_shellopt_get(char flag)
{
    switch (flag) {
    case 'e':
        return g_opt_e;
    case 'u':
        return g_opt_u;
    case 'x':
        return g_opt_x;
    default:
        return 0;
    }
}

const char *petrush_shellopt_flags(void)
{
    size_t i = 0;
    g_flags_buf[i++] = 'C';
    if (g_opt_e) {
        g_flags_buf[i++] = 'e';
    }
    if (g_opt_u) {
        g_flags_buf[i++] = 'u';
    }
    if (g_opt_x) {
        g_flags_buf[i++] = 'x';
    }
    g_flags_buf[i] = '\0';
    return g_flags_buf;
}

void petrush_last_status_set(int status)
{
    g_last_status = status;
}

int petrush_last_status(void)
{
    return g_last_status;
}

void petrush_shellopt_reset_for_tests(void)
{
    g_opt_e = 0;
    g_opt_u = 0;
    g_opt_x = 0;
    g_last_status = 0;
    g_nounset_error = 0;
}

void petrush_positional_clear(void)
{
    free(g_pos_arg0);
    g_pos_arg0 = NULL;
    if (g_pos_args) {
        for (int i = 0; i < g_pos_nargs; i++) {
            free(g_pos_args[i]);
        }
        free(g_pos_args);
        g_pos_args = NULL;
    }
    g_pos_nargs = 0;
}

int petrush_positional_set(const char *arg0, int nargs, char *const *args)
{
    petrush_positional_clear();
    if (nargs < 0) {
        nargs = 0;
    }
    if (arg0) {
        g_pos_arg0 = strdup(arg0);
        if (!g_pos_arg0) {
            return -1;
        }
    }
    if (nargs > 0) {
        g_pos_args = calloc((size_t)nargs, sizeof(char *));
        if (!g_pos_args) {
            petrush_positional_clear();
            return -1;
        }
        for (int i = 0; i < nargs; i++) {
            const char *src = (args && args[i]) ? args[i] : "";
            g_pos_args[i] = strdup(src);
            if (!g_pos_args[i]) {
                petrush_positional_clear();
                return -1;
            }
        }
    }
    g_pos_nargs = nargs;
    return 0;
}

const char *petrush_positional_get(unsigned n)
{
    if (n == 0) {
        return g_pos_arg0 ? g_pos_arg0 : "";
    }
    if ((int)n > g_pos_nargs) {
        return NULL;
    }
    return g_pos_args[n - 1];
}

unsigned petrush_positional_count(void)
{
    return (unsigned)g_pos_nargs;
}

int petrush_positional_shift(unsigned n)
{
    if (n == 0) {
        return 0;
    }
    if ((int)n > g_pos_nargs) {
        return -1;
    }
    /* Libera os n primeiros $1..; desliza o resto; $0 intacto. */
    for (int i = 0; i < (int)n; i++) {
        free(g_pos_args[i]);
        g_pos_args[i] = NULL;
    }
    int remain = g_pos_nargs - (int)n;
    if (remain > 0) {
        memmove(g_pos_args, g_pos_args + (int)n, (size_t)remain * sizeof(char *));
        for (int i = remain; i < g_pos_nargs; i++) {
            g_pos_args[i] = NULL;
        }
    }
    if (remain == 0) {
        free(g_pos_args);
        g_pos_args = NULL;
    }
    g_pos_nargs = remain;
    return 0;
}

static int is_name_char(char c)
{
    return isalnum((unsigned char)c) || c == '_';
}

static char ifs_join_char(void)
{
    const char *ifs = petrush_getenv("IFS");
    if (ifs && ifs[0] != '\0') {
        return ifs[0];
    }
    return ' ';
}

/* Junta $1..$# com IFS[0] (default espaco). Caller free. */
static char *positional_join(void)
{
    unsigned n = petrush_positional_count();
    char sep = ifs_join_char();
    size_t total = 1; /* NUL */
    for (unsigned i = 1; i <= n; i++) {
        const char *v = petrush_positional_get(i);
        if (!v) {
            v = "";
        }
        total += strlen(v);
        if (i < n) {
            total += 1;
        }
    }
    char *out = malloc(total);
    if (!out) {
        return NULL;
    }
    size_t o = 0;
    for (unsigned i = 1; i <= n; i++) {
        const char *v = petrush_positional_get(i);
        if (!v) {
            v = "";
        }
        size_t len = strlen(v);
        memcpy(out + o, v, len);
        o += len;
        if (i < n) {
            out[o++] = sep;
        }
    }
    out[o] = '\0';
    return out;
}

/* expand_brace chama expand_word no word de :- / :+ */
/* NOLINTNEXTLINE(readability-redundant-declaration) */
char *expand_word(const char *word);

/* ---- OSH-11 arith eval (int64; + - * / % ( ) unary; sem bitwise) ---- */

static int parse_i64_local(const char *s, size_t len, int64_t *out)
{
#ifdef PETRUSH_HAVE_ASM
    return petrush_parse_i64(s, len, out);
#else
    if (!s || !out || len == 0) {
        return -1;
    }
    size_t i = 0;
    int neg = 0;
    if (s[i] == '+' || s[i] == '-') {
        neg = (s[i] == '-');
        i++;
        if (i >= len) {
            return -1;
        }
    }
    if (i >= len || !isdigit((unsigned char)s[i])) {
        return -1;
    }
    uint64_t acc = 0;
    const uint64_t lim = (uint64_t)INT64_MAX + (neg ? 1U : 0U);
    for (; i < len; i++) {
        if (!isdigit((unsigned char)s[i])) {
            return -1;
        }
        unsigned d = (unsigned)(s[i] - '0');
        if (acc > (lim - d) / 10U) {
            return -1;
        }
        acc = (acc * 10U) + d;
    }
    if (neg) {
        *out = (acc == (uint64_t)INT64_MAX + 1U)
                   ? INT64_MIN
                   : -(int64_t)acc;
    } else {
        *out = (int64_t)acc;
    }
    return 0;
#endif
}

typedef struct {
    const char *p;
    const char *end;
    int err; /* 0 ok; 1 div0; 2 syntax */
} arith_ctx_t;

static void arith_skip_ws(arith_ctx_t *c)
{
    while (c->p < c->end &&
           (*c->p == ' ' || *c->p == '\t' || *c->p == '\n' || *c->p == '\r')) {
        c->p++;
    }
}

static int arith_parse_expr(arith_ctx_t *c, int64_t *out);

static int arith_value_from_text(const char *s, size_t len, int64_t *out)
{
    if (!s || len == 0) {
        *out = 0;
        return 0;
    }
    if (parse_i64_local(s, len, out) != 0) {
        *out = 0;
    }
    return 0;
}

/* Recursive descent: primary may call expr for parentheses. */
/* NOLINTNEXTLINE(misc-no-recursion) */
static int arith_parse_primary(arith_ctx_t *c, int64_t *out)
{
    arith_skip_ws(c);
    if (c->p >= c->end) {
        c->err = 2;
        *out = 0;
        return -1;
    }
    if (*c->p == '(') {
        c->p++;
        if (arith_parse_expr(c, out) != 0) {
            return -1;
        }
        arith_skip_ws(c);
        if (c->p >= c->end || *c->p != ')') {
            c->err = 2;
            return -1;
        }
        c->p++;
        return 0;
    }
    if (*c->p == '$') {
        c->p++;
        if (c->p < c->end && isdigit((unsigned char)*c->p)) {
            unsigned idx = (unsigned)(*c->p - '0');
            c->p++;
            const char *val = petrush_positional_get(idx);
            return arith_value_from_text(val, val ? strlen(val) : 0, out);
        }
        if (c->p < c->end && is_name_char(*c->p) &&
            !isdigit((unsigned char)*c->p)) {
            const char *name = c->p;
            while (c->p < c->end && is_name_char(*c->p)) {
                c->p++;
            }
            size_t nlen = (size_t)(c->p - name);
            char nbuf[256];
            if (nlen >= sizeof(nbuf)) {
                nlen = sizeof(nbuf) - 1;
            }
            memcpy(nbuf, name, nlen);
            nbuf[nlen] = '\0';
            const char *val = petrush_getenv(nbuf);
            return arith_value_from_text(val, val ? strlen(val) : 0, out);
        }
        c->err = 2;
        *out = 0;
        return -1;
    }
    if (is_name_char(*c->p) && !isdigit((unsigned char)*c->p)) {
        const char *name = c->p;
        while (c->p < c->end && is_name_char(*c->p)) {
            c->p++;
        }
        size_t nlen = (size_t)(c->p - name);
        char nbuf[256];
        if (nlen >= sizeof(nbuf)) {
            nlen = sizeof(nbuf) - 1;
        }
        memcpy(nbuf, name, nlen);
        nbuf[nlen] = '\0';
        const char *val = petrush_getenv(nbuf);
        return arith_value_from_text(val, val ? strlen(val) : 0, out);
    }
    if (isdigit((unsigned char)*c->p)) {
        const char *start = c->p;
        while (c->p < c->end && isdigit((unsigned char)*c->p)) {
            c->p++;
        }
        return arith_value_from_text(start, (size_t)(c->p - start), out);
    }
    c->err = 2;
    *out = 0;
    return -1;
}

/* NOLINTNEXTLINE(misc-no-recursion) */
static int arith_parse_unary(arith_ctx_t *c, int64_t *out)
{
    arith_skip_ws(c);
    if (c->p < c->end && (*c->p == '+' || *c->p == '-')) {
        char op = *c->p;
        c->p++;
        int64_t v = 0;
        if (arith_parse_unary(c, &v) != 0) {
            return -1;
        }
        if (op == '-') {
            if (v == INT64_MIN) {
                *out = INT64_MIN;
            } else {
                *out = -v;
            }
        } else {
            *out = v;
        }
        return 0;
    }
    return arith_parse_primary(c, out);
}

/* NOLINTNEXTLINE(misc-no-recursion) */
static int arith_parse_term(arith_ctx_t *c, int64_t *out)
{
    int64_t left = 0;
    if (arith_parse_unary(c, &left) != 0) {
        return -1;
    }
    for (;;) {
        arith_skip_ws(c);
        if (c->p >= c->end) {
            break;
        }
        char op = *c->p;
        if (op != '*' && op != '/' && op != '%') {
            break;
        }
        c->p++;
        int64_t right = 0;
        if (arith_parse_unary(c, &right) != 0) {
            return -1;
        }
        if (op == '*') {
            left *= right;
        } else if (right == 0) {
            c->err = 1;
            *out = 0;
            return -1;
        } else if (op == '/') {
            left /= right;
        } else {
            left %= right;
        }
    }
    *out = left;
    return 0;
}

/* NOLINTNEXTLINE(misc-no-recursion) */
static int arith_parse_expr(arith_ctx_t *c, int64_t *out)
{
    int64_t left = 0;
    if (arith_parse_term(c, &left) != 0) {
        return -1;
    }
    for (;;) {
        arith_skip_ws(c);
        if (c->p >= c->end) {
            break;
        }
        char op = *c->p;
        if (op != '+' && op != '-') {
            break;
        }
        c->p++;
        int64_t right = 0;
        if (arith_parse_term(c, &right) != 0) {
            return -1;
        }
        if (op == '+') {
            left += right;
        } else {
            left -= right;
        }
    }
    *out = left;
    return 0;
}

/* 0 ok; 1 div0; 2 syntax. *out sempre escrito. */
static int eval_arith(const char *inner, size_t len, int64_t *out)
{
    *out = 0;
    if (!inner) {
        return 2;
    }
    arith_ctx_t c = { .p = inner, .end = inner + len, .err = 0 };
    arith_skip_ws(&c);
    if (c.p >= c.end) {
        return 0; /* $(()) -> 0 */
    }
    if (arith_parse_expr(&c, out) != 0) {
        return c.err ? c.err : 2;
    }
    arith_skip_ws(&c);
    if (c.p < c.end) {
        *out = 0;
        return 2;
    }
    return 0;
}

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
/* NOLINTNEXTLINE(readability-function-cognitive-complexity, misc-no-recursion) */
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
        if (g_opt_u && !val) {
            nounset_fail(nbuf);
        }
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
        /* ${VAR} — OSH-17: unset + -u → erro; set-vazia ok */
        if (!val) {
            if (g_opt_u) {
                nounset_fail(nbuf);
            }
            val = "";
        }
        if (append_bytes(out, o, cap, val, strlen(val)) != 0) return -1;
        *pp = after;
        return 0;
    }

    /* ${VAR:-word} / ${VAR:+word} — operadores isentam -u */
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

/*
 * UX-13 + FEAT-PARAM + OSH-1/9/11: $VAR / ${} / $n / $( ) / $(( )).
 * heredoc_escapes=1 (OSH-14): \ escapa $, `, \ e newline; sem tilde (caller).
 */
/* NOLINTNEXTLINE(readability-function-cognitive-complexity, misc-no-recursion) */
static char *expand_params_inner(const char *word, int heredoc_escapes)
{
    size_t cap = (strlen(word) * 4) + 64;
    if (cap < 256) {
        cap = 256;
    }
    char *out = malloc(cap);
    if (!out) {
        return NULL;
    }
    size_t o = 0;
    const char *p = word;

    while (*p) {
        if (heredoc_escapes && *p == '\\') {
            char nxt = p[1];
            if (nxt == '$' || nxt == '`' || nxt == '\\') {
                if (append_char(&out, &o, &cap, nxt) != 0) {
                    free(out);
                    return NULL;
                }
                p += 2;
                continue;
            }
            if (nxt == '\n') {
                p += 2; /* line continuation */
                continue;
            }
            /* outros \X: literais (backslash + char) */
            if (append_char(&out, &o, &cap, '\\') != 0) {
                free(out);
                return NULL;
            }
            if (nxt) {
                if (append_char(&out, &o, &cap, nxt) != 0) {
                    free(out);
                    return NULL;
                }
                p += 2;
            } else {
                p++;
            }
            continue;
        }

        if (*p == '$' && p[1]) {
            if (p[1] == '{') {
                const char *brace = p + 1;
                int br = expand_brace(&brace, &out, &o, &cap);
                if (br == -1) {
                    free(out);
                    return NULL;
                }
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
            /* OSH-1: $# $@ $* $0-$9 (um digito; $10 = $1 + "0") */
            /* OSH-16: $? $- */
            if (p[1] == '#') {
                char nbuf[32];
                int nw = snprintf(nbuf, sizeof(nbuf), "%u",
                                  petrush_positional_count());
                if (nw < 0 ||
                    append_bytes(&out, &o, &cap, nbuf, (size_t)nw) != 0) {
                    free(out);
                    return NULL;
                }
                p += 2;
                continue;
            }
            if (p[1] == '?') {
                char nbuf[32];
                int nw = snprintf(nbuf, sizeof(nbuf), "%d",
                                  petrush_last_status());
                if (nw < 0 ||
                    append_bytes(&out, &o, &cap, nbuf, (size_t)nw) != 0) {
                    free(out);
                    return NULL;
                }
                p += 2;
                continue;
            }
            if (p[1] == '-') {
                const char *fl = petrush_shellopt_flags();
                if (append_bytes(&out, &o, &cap, fl, strlen(fl)) != 0) {
                    free(out);
                    return NULL;
                }
                p += 2;
                continue;
            }
            if (p[1] == '@' || p[1] == '*') {
                /* embutido: junta como $* (splice multi-word so em expand_cmd) */
                char *joined = positional_join();
                if (!joined) {
                    free(out);
                    return NULL;
                }
                int rc = append_bytes(&out, &o, &cap, joined, strlen(joined));
                free(joined);
                if (rc != 0) {
                    free(out);
                    return NULL;
                }
                p += 2;
                continue;
            }
            if (isdigit((unsigned char)p[1])) {
                unsigned idx = (unsigned)(p[1] - '0');
                const char *val = petrush_positional_get(idx);
                if (!val) {
                    /* $0 nunca NULL; $1..$9 unset → -u erro (exceto $@ $*) */
                    if (g_opt_u && idx >= 1) {
                        char pbuf[4];
                        (void)snprintf(pbuf, sizeof(pbuf), "$%u", idx);
                        nounset_fail(pbuf);
                    }
                    val = "";
                }
                if (append_bytes(&out, &o, &cap, val, strlen(val)) != 0) {
                    free(out);
                    return NULL;
                }
                p += 2;
                continue;
            }
            if (is_name_char(p[1])) {
                const char *name = p + 1;
                const char *q = name;
                while (is_name_char(*q)) {
                    q++;
                }
                size_t nlen = (size_t)(q - name);
                char nbuf[256];
                if (nlen >= sizeof(nbuf)) {
                    nlen = sizeof(nbuf) - 1;
                }
                memcpy(nbuf, name, nlen);
                nbuf[nlen] = '\0';
                const char *val = petrush_getenv(nbuf);
                if (!val) {
                    if (g_opt_u) {
                        nounset_fail(nbuf);
                    }
                    val = "";
                }
                if (append_bytes(&out, &o, &cap, val, strlen(val)) != 0) {
                    free(out);
                    return NULL;
                }
                p = q;
                continue;
            }
            /* OSH-11: $((expr)); OSH-9: $(cmd) via hook */
            if (p[1] == '(') {
                size_t asp = petrush_arith_span(p);
                if (asp > 0) {
                    size_t inner_len = asp >= 5 ? asp - 5 : 0;
                    int64_t val = 0;
                    int ar = eval_arith(p + 3, inner_len, &val);
                    if (ar == 1) {
                        fprintf(stderr, "petrush: arithmetic: division by 0\n");
                        g_arith_error = 1;
                        val = 0;
                    } else if (ar != 0) {
                        val = 0;
                    }
                    char nbuf[32];
                    int nw = snprintf(nbuf, sizeof(nbuf), "%" PRId64, val);
                    if (nw < 0 ||
                        append_bytes(&out, &o, &cap, nbuf, (size_t)nw) != 0) {
                        free(out);
                        return NULL;
                    }
                    p += asp;
                    continue;
                }
                size_t sp = petrush_cmdsubst_span(p);
                if (sp == 0) {
                    /* unclosed $(...: '$' literal */
                    if (append_char(&out, &o, &cap, *p) != 0) {
                        free(out);
                        return NULL;
                    }
                    p++;
                    continue;
                }
                if (!g_cmdsubst_hook) {
                    if (append_char(&out, &o, &cap, *p) != 0) {
                        free(out);
                        return NULL;
                    }
                    p++;
                    continue;
                }
                size_t inner_len = sp - 3; /* sem "$(", ")" */
                char *inner = malloc(inner_len + 1);
                if (!inner) {
                    free(out);
                    return NULL;
                }
                memcpy(inner, p + 2, inner_len);
                inner[inner_len] = '\0';
                char *captured = g_cmdsubst_hook(inner);
                free(inner);
                if (!captured) {
                    p += sp;
                    continue;
                }
                size_t clen = strlen(captured);
                while (clen > 0 && captured[clen - 1] == '\n') {
                    captured[--clen] = '\0';
                }
                int arc = append_bytes(&out, &o, &cap, captured, clen);
                free(captured);
                if (arc != 0) {
                    free(out);
                    return NULL;
                }
                p += sp;
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

/* OSH-14: corpo here-doc unquoted — $ / $( ) / $(( )); sem tilde/glob/split. */
char *expand_heredoc_body(const char *body)
{
    if (!body) {
        return NULL;
    }
    return expand_params_inner(body, 1);
}

/* NOLINTNEXTLINE(misc-no-recursion) */
char *expand_word(const char *word)
{
    if (!word) {
        return NULL;
    }

    /* UX-12: ~ and ~/ */
    if (word[0] == '~' && (word[1] == '\0' || word[1] == '/')) {
        const char *home = petrush_getenv("HOME");
        if (!home) {
            home = "";
        }
        size_t need = strlen(home) + strlen(word); /* home + rest without ~ */
        char *out = malloc(need + 1);
        if (!out) {
            return NULL;
        }
        if (word[1] == '\0') {
            snprintf(out, need + 1, "%s", home);
        } else {
            snprintf(out, need + 1, "%s%s", home, word + 1);
        }
        return out;
    }

    return expand_params_inner(word, 0);
}

/* UX-18 / ASM-GLOB: * = sequência, ? = um byte; [ literal (sem classes []). */
/* NOLINTNEXTLINE(bugprone-easily-swappable-parameters) */
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

/* NOLINTNEXTLINE(bugprone-easily-swappable-parameters) */
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

/* NOLINTNEXTLINE(readability-function-cognitive-complexity) */
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

/*
 * OSH-1: substitui argv[i] que e exatamente $@ / $*.
 * "$@" / $@ / $* unquoted → N palavras (ou remove se N=0).
 * "$*" → 1 palavra (IFS[0]).
 * Avanca *pi para o proximo slot a processar. -1 = OOM.
 */
static int expand_positional_token(petrush_cmd_t *cmd, int *pi)
{
    int i = *pi;
    const char *tok = cmd->argv[i];
    int is_at = (strcmp(tok, "$@") == 0);
    int is_star = (strcmp(tok, "$*") == 0);
    if (!is_at && !is_star) {
        return 0;
    }

    int quoted = argv_is_quoted(cmd, i);
    unsigned n = petrush_positional_count();

    if (is_star && quoted) {
        char *joined = positional_join();
        if (!joined) {
            return -1;
        }
        free(cmd->argv[i]);
        cmd->argv[i] = joined;
        *pi = i + 1;
        return 1;
    }

    /* $@ (quoted ou nao) e $* unquoted: N palavras literais */
    int old_argc = cmd->argc;
    int new_argc = old_argc - 1 + (int)n;
    char **na = realloc(cmd->argv, sizeof(char *) * ((size_t)new_argc + 1));
    if (!na) {
        return -1;
    }
    cmd->argv = na;

    int *nq = NULL;
    if (cmd->argv_quoted) {
        nq = realloc(cmd->argv_quoted, sizeof(int) * (size_t)(new_argc > 0 ? new_argc : 1));
        if (!nq) {
            return -1;
        }
        cmd->argv_quoted = nq;
    } else {
        nq = calloc((size_t)(new_argc > 0 ? new_argc : 1), sizeof(int));
        if (!nq) {
            return -1;
        }
        cmd->argv_quoted = nq;
    }

    free(cmd->argv[i]);
    if ((int)n != 1) {
        memmove(&cmd->argv[i + (int)n], &cmd->argv[i + 1],
                sizeof(char *) * (size_t)(old_argc - i - 1));
        /* new_argc ja redimensionou argv_quoted; analyzer nao rastreia. */
        /* NOLINTNEXTLINE(clang-analyzer-security.ArrayBound) */
        memmove(&cmd->argv_quoted[i + (int)n], &cmd->argv_quoted[i + 1],
                sizeof(int) * (size_t)(old_argc - i - 1));
    }
    for (unsigned j = 0; j < n; j++) {
        const char *v = petrush_positional_get(j + 1);
        if (!v) {
            v = "";
        }
        cmd->argv[i + (int)j] = strdup(v);
        if (!cmd->argv[i + (int)j]) {
            return -1;
        }
        /* literais finais: nao re-glob / nao re-expand */
        cmd->argv_quoted[i + (int)j] = 1;
    }
    cmd->argc = new_argc;
    cmd->argv[cmd->argc] = NULL;
    *pi = i + (int)n;
    return 1;
}

void expand_cmd_argv(petrush_cmd_t *cmd)
{
    if (!cmd || !cmd->argv) return;

    /* 1) ~ / $VAR / $n / $#; splice $@ / $* (sem re-expand dos spliced) */
    for (int i = 0; i < cmd->argc; ) {
        if (!cmd->argv[i]) {
            i++;
            continue;
        }
        int pr = expand_positional_token(cmd, &i);
        if (pr < 0) {
            return;
        }
        if (pr > 0) {
            continue; /* i ja avancou; literais prontos */
        }
        char *e = expand_word(cmd->argv[i]);
        if (e) {
            free(cmd->argv[i]);
            cmd->argv[i] = e;
        }
        i++;
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

    /* OSH-14: here-doc unquoted — expand no dispatch (ambiente do momento).
     * Quoted permanece literal. Clone do AST fica cru; mutamos a copia de trabalho. */
    if (cmd->here_body && !cmd->here_quoted) {
        char *e = expand_heredoc_body(cmd->here_body);
        if (e) {
            free(cmd->here_body);
            cmd->here_body = e;
        }
    }
}
