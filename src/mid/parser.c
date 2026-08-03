/*
 * parser.c — Parser com pipes e redirecionamento (NEW-20)
 */

#include "petrush/parser.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define INITIAL_CAP 8

static int is_quote(char c)
{
    return c == '"' || c == '\'';
}

/* Operadores reconhecidos (unquoted): |  <  >  >> */
typedef enum {
    TOK_WORD = 0,
    TOK_PIPE,
    TOK_LT,
    TOK_GT,
    TOK_GTGT
} tok_kind_t;

typedef struct {
    tok_kind_t kind;
    char *text; /* owned for TOK_WORD; NULL for operators */
} token_t;

static void free_tokens(token_t *toks, int n)
{
    if (!toks) return;
    for (int i = 0; i < n; i++) {
        free(toks[i].text);
    }
    free(toks);
}

static void cmd_clear(petrush_cmd_t *cmd)
{
    if (!cmd) return;
    if (cmd->argv) {
        for (int i = 0; i < cmd->argc; i++) free(cmd->argv[i]);
        free(cmd->argv);
    }
    free(cmd->redir_in);
    free(cmd->redir_out);
    memset(cmd, 0, sizeof(*cmd));
}

void petrush_cmd_free(petrush_cmd_t *cmd)
{
    cmd_clear(cmd);
}

void petrush_pipeline_free(petrush_pipeline_t *pl)
{
    if (!pl || !pl->cmds) {
        if (pl) {
            pl->cmds = NULL;
            pl->ncmds = 0;
        }
        return;
    }
    for (int i = 0; i < pl->ncmds; i++) {
        cmd_clear(&pl->cmds[i]);
    }
    free(pl->cmds);
    pl->cmds = NULL;
    pl->ncmds = 0;
}

/*
 * Tokeniza input em words + operadores.
 * Retorna 0 e preenche *out_toks / *out_n; -1 em OOM/erro.
 */
static int tokenize(const char *input, token_t **out_toks, int *out_n)
{
    token_t *toks = malloc(sizeof(token_t) * INITIAL_CAP);
    if (!toks) return -1;
    int n = 0;
    size_t cap = INITIAL_CAP;
    const char *p = input;

    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;

        tok_kind_t kind = TOK_WORD;
        char *text = NULL;

        if (!is_quote(*p)) {
            if (*p == '|') {
                kind = TOK_PIPE;
                p++;
            } else if (*p == '<') {
                kind = TOK_LT;
                p++;
            } else if (*p == '>') {
                if (p[1] == '>') {
                    kind = TOK_GTGT;
                    p += 2;
                } else {
                    kind = TOK_GT;
                    p++;
                }
            }
        }

        if (kind != TOK_WORD) {
            /* operator token */
        } else {
            char quote = 0;
            if (is_quote(*p)) {
                quote = *p;
                p++;
            }
            const char *start = p;
            while (*p) {
                if (quote) {
                    if (*p == quote) {
                        p++;
                        break;
                    }
                } else {
                    if (isspace((unsigned char)*p)) break;
                    /* operador quebra palavra */
                    if (*p == '|' || *p == '<' || *p == '>') break;
                }
                p++;
            }
            size_t token_len = (size_t)(p - start);
            if (quote && token_len > 0 && start[token_len - 1] == quote) {
                token_len--;
            }
            text = malloc(token_len + 1);
            if (!text) {
                free_tokens(toks, n);
                return -1;
            }
            memcpy(text, start, token_len);
            text[token_len] = '\0';
        }

        if (n >= (int)cap) {
            cap *= 2;
            token_t *nt = realloc(toks, sizeof(token_t) * cap);
            if (!nt) {
                free(text);
                free_tokens(toks, n);
                return -1;
            }
            toks = nt;
        }
        toks[n].kind = kind;
        toks[n].text = text;
        n++;
    }

    *out_toks = toks;
    *out_n = n;
    return 0;
}

static int push_arg(petrush_cmd_t *cmd, char *word, size_t *argv_cap)
{
    if (cmd->argc >= (int)*argv_cap) {
        size_t nc = (*argv_cap) * 2;
        char **na = realloc(cmd->argv, sizeof(char *) * nc);
        if (!na) return -1;
        cmd->argv = na;
        *argv_cap = nc;
    }
    cmd->argv[cmd->argc++] = word;
    return 0;
}

static int finalize_argv(petrush_cmd_t *cmd)
{
    if (!cmd->argv) {
        /* estágio vazio — válido só se for erro depois */
        return 0;
    }
    char **final_argv = realloc(cmd->argv, sizeof(char *) * ((size_t)cmd->argc + 1));
    if (!final_argv) return -1;
    cmd->argv = final_argv;
    cmd->argv[cmd->argc] = NULL;
    return 0;
}

/*
 * Constrói um estágio a partir de tokens [begin, end).
 */
static int build_stage(token_t *toks, int begin, int end, petrush_cmd_t *cmd)
{
    memset(cmd, 0, sizeof(*cmd));
    size_t argv_cap = INITIAL_CAP;
    cmd->argv = malloc(sizeof(char *) * argv_cap);
    if (!cmd->argv) return -1;

    for (int i = begin; i < end; i++) {
        tok_kind_t k = toks[i].kind;
        if (k == TOK_WORD) {
            char *dup = toks[i].text;
            toks[i].text = NULL; /* ownership transfer */
            if (push_arg(cmd, dup, &argv_cap) != 0) {
                free(dup);
                cmd_clear(cmd);
                return -1;
            }
            continue;
        }
        if (k == TOK_PIPE) {
            /* não deveria aparecer dentro de um estágio */
            cmd_clear(cmd);
            return -1;
        }
        /* redir: próximo token deve ser WORD */
        if (i + 1 >= end || toks[i + 1].kind != TOK_WORD) {
            cmd_clear(cmd);
            return -1;
        }
        char *path = toks[i + 1].text;
        toks[i + 1].text = NULL;
        i++; /* consume path */

        if (k == TOK_LT) {
            free(cmd->redir_in);
            cmd->redir_in = path;
        } else if (k == TOK_GT || k == TOK_GTGT) {
            free(cmd->redir_out);
            cmd->redir_out = path;
            cmd->redir_append = (k == TOK_GTGT) ? 1 : 0;
        } else {
            free(path);
            cmd_clear(cmd);
            return -1;
        }
    }

    if (cmd->argc == 0) {
        cmd_clear(cmd);
        return -1; /* redirecionamento sem comando */
    }
    if (finalize_argv(cmd) != 0) {
        cmd_clear(cmd);
        return -1;
    }
    return 0;
}

int petrush_parse_pipeline(const char *input, petrush_pipeline_t *out)
/* NOLINTNEXTLINE(readability-function-cognitive-complexity) */
{
    if (!input || !out) return -1;
    memset(out, 0, sizeof(*out));

    size_t len = strlen(input);
    if (len == 0) {
        return 0;
    }

    token_t *toks = NULL;
    int ntoks = 0;
    if (tokenize(input, &toks, &ntoks) != 0) return -1;

    if (ntoks == 0) {
        free_tokens(toks, ntoks);
        return 0;
    }

    /* Contar estágios (pipes) */
    int ncmds = 1;
    for (int i = 0; i < ntoks; i++) {
        if (toks[i].kind == TOK_PIPE) ncmds++;
    }

    petrush_cmd_t *cmds = calloc((size_t)ncmds, sizeof(petrush_cmd_t));
    if (!cmds) {
        free_tokens(toks, ntoks);
        return -1;
    }

    int stage = 0;
    int begin = 0;
    for (int i = 0; i <= ntoks; i++) {
        int is_end = (i == ntoks);
        int is_pipe = (!is_end && toks[i].kind == TOK_PIPE);
        if (!is_end && !is_pipe) continue;

        if (begin >= i && is_pipe) {
            /* pipe vazio "a | | b" */
            for (int j = 0; j < stage; j++) cmd_clear(&cmds[j]);
            free(cmds);
            free_tokens(toks, ntoks);
            return -1;
        }

        if (build_stage(toks, begin, i, &cmds[stage]) != 0) {
            for (int j = 0; j < stage; j++) cmd_clear(&cmds[j]);
            free(cmds);
            free_tokens(toks, ntoks);
            return -1;
        }
        stage++;
        begin = i + 1;
        if (is_pipe && begin >= ntoks) {
            /* trailing pipe */
            for (int j = 0; j < stage; j++) cmd_clear(&cmds[j]);
            free(cmds);
            free_tokens(toks, ntoks);
            return -1;
        }
    }

    free_tokens(toks, ntoks);
    out->cmds = cmds;
    out->ncmds = ncmds;
    return 0;
}

int petrush_parse(const char *input, petrush_cmd_t *out)
{
    if (!out) return -1;
    memset(out, 0, sizeof(*out));

    petrush_pipeline_t pl = {0};
    if (petrush_parse_pipeline(input, &pl) != 0) return -1;

    if (pl.ncmds == 0) {
        petrush_pipeline_free(&pl);
        return 0;
    }
    if (pl.ncmds != 1) {
        petrush_pipeline_free(&pl);
        return -1;
    }

    /* steal stage 0 */
    *out = pl.cmds[0];
    free(pl.cmds);
    pl.cmds = NULL;
    pl.ncmds = 0;
    return 0;
}

void petrush_list_free(petrush_list_t *list)
{
    if (!list || !list->items) {
        if (list) {
            list->items = NULL;
            list->nitems = 0;
        }
        return;
    }
    for (int i = 0; i < list->nitems; i++) {
        petrush_pipeline_free(&list->items[i].pl);
    }
    free(list->items);
    list->items = NULL;
    list->nitems = 0;
}

/* Encontra próximo && ou || fora de aspas. Retorna 1 se achou. */
static int find_list_connector(const char *s, size_t from, size_t *out_pos, petrush_run_cond_t *out_cond)
{
    char quote = 0;
    for (size_t i = from; s[i]; i++) {
        char c = s[i];
        if (quote) {
            if (c == quote) quote = 0;
            continue;
        }
        if (c == '\'' || c == '"') {
            quote = c;
            continue;
        }
        if (c == '&' && s[i + 1] == '&') {
            *out_pos = i;
            *out_cond = PETRUSH_COND_AND;
            return 1;
        }
        if (c == '|' && s[i + 1] == '|') {
            *out_pos = i;
            *out_cond = PETRUSH_COND_OR;
            return 1;
        }
    }
    return 0;
}

int petrush_parse_list(const char *input, petrush_list_t *out)
{
    if (!input || !out) return -1;
    memset(out, 0, sizeof(*out));

    /* contagem de segmentos */
    int nitems = 1;
    size_t pos = 0;
    petrush_run_cond_t dummy;
    size_t cpos;
    while (find_list_connector(input, pos, &cpos, &dummy)) {
        nitems++;
        pos = cpos + 2;
    }

    petrush_list_item_t *items = calloc((size_t)nitems, sizeof(*items));
    if (!items) return -1;

    size_t start = 0;
    petrush_run_cond_t next_cond = PETRUSH_COND_ALWAYS;
    int idx = 0;
    pos = 0;
    while (idx < nitems) {
        size_t conn_pos = 0;
        petrush_run_cond_t conn = PETRUSH_COND_ALWAYS;
        int has = find_list_connector(input, pos, &conn_pos, &conn);

        size_t end = has ? conn_pos : strlen(input);
        /* extrair substring [start, end) */
        while (start < end && isspace((unsigned char)input[start])) start++;
        size_t e = end;
        while (e > start && isspace((unsigned char)input[e - 1])) e--;
        size_t seglen = e - start;
        char *seg = malloc(seglen + 1);
        if (!seg) {
            for (int j = 0; j < idx; j++) petrush_pipeline_free(&items[j].pl);
            free(items);
            return -1;
        }
        memcpy(seg, input + start, seglen);
        seg[seglen] = '\0';

        items[idx].cond = next_cond;
        if (petrush_parse_pipeline(seg, &items[idx].pl) != 0) {
            free(seg);
            for (int j = 0; j < idx; j++) petrush_pipeline_free(&items[j].pl);
            free(items);
            return -1;
        }
        free(seg);
        idx++;

        if (!has) break;
        next_cond = conn;
        start = conn_pos + 2;
        pos = start;
    }

    out->items = items;
    out->nitems = nitems;
    return 0;
}
