/*
 * parser.c - Parser com pipes e redirecionamento (NEW-20 + UX-16)
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

/*
 * OSH-9: span de $(...) a partir de '$'. $(( nao e cmdsubst.
 * Nesting de paren; aspas protegem. 0 se nao casa ou unclosed.
 */
size_t petrush_cmdsubst_span(const char *p)
{
    if (!p || p[0] != '$' || p[1] != '(' || p[2] == '(') {
        return 0;
    }
    const char *s = p + 2;
    int depth = 1;
    char quote = 0;
    while (*s && depth > 0) {
        if (quote) {
            if (*s == quote) {
                quote = 0;
            }
            s++;
            continue;
        }
        if (*s == '\'' || *s == '"') {
            quote = *s;
            s++;
            continue;
        }
        if (*s == '(') {
            depth++;
            s++;
            continue;
        }
        if (*s == ')') {
            depth--;
            s++;
            continue;
        }
        s++;
    }
    if (depth != 0) {
        return 0;
    }
    return (size_t)(s - p);
}

/* Operadores unquoted: | < > >> 2> 2>> 2>&1 &> */
typedef enum {
    TOK_WORD = 0,
    TOK_PIPE,
    TOK_LT,
    TOK_GT,
    TOK_GTGT,
    TOK_ERRGT,     /* 2> */
    TOK_ERRGTGT,   /* 2>> */
    TOK_ERRTOOUT,  /* 2>&1 */
    TOK_AMPGT      /* &> */
} tok_kind_t;

typedef struct {
    tok_kind_t kind;
    char *text; /* owned for TOK_WORD; NULL for operators */
    int quoted; /* 1 se o token WORD começou com ' ou " */
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
    /* argv slots must be NULL or owned (calloc + push_arg); never free garbage */
    if (cmd->argv) {
        for (int i = 0; i < cmd->argc; i++) {
            free(cmd->argv[i]);
            cmd->argv[i] = NULL;
        }
        free(cmd->argv);
        cmd->argv = NULL;
    }
    free(cmd->argv_quoted);
    cmd->argv_quoted = NULL;
    free(cmd->redir_in);
    cmd->redir_in = NULL;
    free(cmd->redir_out);
    cmd->redir_out = NULL;
    free(cmd->redir_err);
    cmd->redir_err = NULL;
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

/* | < > >> &> - 1 se consumiu; 0 se não casa. */
static int try_consume_simple_op(const char **pp, tok_kind_t *kind)
{
    const char *p = *pp;
    if (*p == '|') {
        *kind = TOK_PIPE;
        *pp = p + 1;
        return 1;
    }
    if (*p == '<') {
        *kind = TOK_LT;
        *pp = p + 1;
        return 1;
    }
    if (*p == '&' && p[1] == '>') {
        *kind = TOK_AMPGT;
        *pp = p + 2;
        return 1;
    }
    if (*p == '>') {
        if (p[1] == '>') {
            *kind = TOK_GTGT;
            *pp = p + 2;
        } else {
            *kind = TOK_GT;
            *pp = p + 1;
        }
        return 1;
    }
    return 0;
}

/*
 * 2> 2>> 2>&1 - 1 consumiu; 0 não casa; -1 forma inválida (2>&12, 2>&2, 2>&).
 * 12> permanece WORD "12" + TOK_GT (só casa com '2' literal na ponta).
 */
static int try_consume_err_redir(const char **pp, tok_kind_t *kind)
{
    const char *p = *pp;
    if (!(*p == '2' && p[1] == '>')) {
        return 0;
    }
    if (p[2] == '>') {
        *kind = TOK_ERRGTGT;
        *pp = p + 3;
        return 1;
    }
    if (p[2] == '&') {
        if (p[3] == '1') {
            char after = p[4];
            if (after == '\0' || isspace((unsigned char)after) ||
                after == '|' || after == '<' || after == '>' ||
                after == '&') {
                *kind = TOK_ERRTOOUT;
                *pp = p + 4;
                return 1;
            }
            /* 2>&12 etc. - não abrir arquivo "&12" */
            return -1;
        }
        /* 2>&  / 2>&2 - inválido nesta fatia */
        return -1;
    }
    *kind = TOK_ERRGT;
    *pp = p + 2;
    return 1;
}

/* Aspas + break em operador. Owned text ou NULL (OOM). */
static char *scan_word(const char **pp, int *quoted_out)
{
    const char *p = *pp;
    char quote = 0;
    int quoted = 0;
    if (is_quote(*p)) {
        quote = *p;
        quoted = 1;
        p++;
    }
    const char *start = p;
    while (*p) {
        if (quote) {
            if (*p == quote) {
                p++;
                break;
            }
            p++;
            continue;
        }
        if (isspace((unsigned char)*p)) {
            break;
        }
        if (*p == '|' || *p == '<' || *p == '>') {
            break;
        }
        if (*p == '&' && p[1] == '>') {
            break;
        }
        /* OSH-9: nao quebrar em espaco/ops dentro de $(...) */
        if (*p == '$') {
            size_t sp = petrush_cmdsubst_span(p);
            if (sp > 0) {
                p += sp;
                continue;
            }
        }
        p++;
    }
    size_t token_len = (size_t)(p - start);
    if (quote && token_len > 0 && start[token_len - 1] == quote) {
        token_len--;
    }
    char *text = malloc(token_len + 1);
    if (!text) {
        *pp = p;
        *quoted_out = quoted;
        return NULL;
    }
    memcpy(text, start, token_len);
    text[token_len] = '\0';
    *pp = p;
    *quoted_out = quoted;
    return text;
}

/*
 * Tokeniza input em words + operadores.
 * Retorna 0 e preenche *out_toks / *out_n; -1 em OOM/erro.
 */
static int tokenize(const char *input, token_t **out_toks, int *out_n)
{
    token_t *toks = malloc(sizeof(token_t) * INITIAL_CAP);
    if (!toks) {
        return -1;
    }
    int n = 0;
    size_t cap = INITIAL_CAP;
    const char *p = input;

    while (*p) {
        while (*p && isspace((unsigned char)*p)) {
            p++;
        }
        if (!*p) {
            break;
        }

        tok_kind_t kind = TOK_WORD;
        char *text = NULL;
        int quoted = 0;

        if (!is_quote(*p) && !try_consume_simple_op(&p, &kind)) {
            int er = try_consume_err_redir(&p, &kind);
            if (er < 0) {
                free_tokens(toks, n);
                return -1;
            }
        }

        if (kind == TOK_WORD) {
            text = scan_word(&p, &quoted);
            if (!text) {
                free_tokens(toks, n);
                return -1;
            }
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
        toks[n].quoted = quoted;
        n++;
    }

    *out_toks = toks;
    *out_n = n;
    return 0;
}

static int push_arg(petrush_cmd_t *cmd, char *word, int quoted, size_t *argv_cap)
{
    if (cmd->argc >= (int)*argv_cap) {
        size_t nc = (*argv_cap) * 2;
        char **na = realloc(cmd->argv, sizeof(char *) * nc);
        if (!na) return -1;
        for (size_t k = *argv_cap; k < nc; k++) na[k] = NULL;
        cmd->argv = na;
        int *nq = realloc(cmd->argv_quoted, sizeof(int) * nc);
        if (!nq) return -1;
        cmd->argv_quoted = nq;
        *argv_cap = nc;
    }
    cmd->argv[cmd->argc] = word;
    cmd->argv_quoted[cmd->argc] = quoted ? 1 : 0;
    cmd->argc++;
    return 0;
}

static int finalize_argv(petrush_cmd_t *cmd)
{
    if (!cmd->argv) {
        /* estágio vazio - válido só se for erro depois */
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
    /* calloc: unused slots NULL so cmd_clear never frees uninitialized ptrs */
    cmd->argv = calloc(argv_cap, sizeof(char *));
    if (!cmd->argv) return -1;
    cmd->argv_quoted = malloc(sizeof(int) * argv_cap);
    if (!cmd->argv_quoted) {
        free(cmd->argv);
        cmd->argv = NULL;
        return -1;
    }

    for (int i = begin; i < end; i++) {
        tok_kind_t k = toks[i].kind;
        if (k == TOK_WORD) {
            char *dup = toks[i].text;
            toks[i].text = NULL; /* ownership transfer */
            if (push_arg(cmd, dup, toks[i].quoted, &argv_cap) != 0) {
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
        /* 2>&1: não consome path; last-wins limpa path de stderr */
        if (k == TOK_ERRTOOUT) {
            free(cmd->redir_err);
            cmd->redir_err = NULL;
            cmd->redir_err_append = 0;
            cmd->redir_err_to_out = 1;
            continue;
        }
        /* demais redirs: próximo token deve ser WORD (path) */
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
        } else if (k == TOK_ERRGT || k == TOK_ERRGTGT) {
            /* last-wins: path limpa merge */
            free(cmd->redir_err);
            cmd->redir_err = path;
            cmd->redir_err_append = (k == TOK_ERRGTGT) ? 1 : 0;
            cmd->redir_err_to_out = 0;
        } else if (k == TOK_AMPGT) {
            /* &> file: stdout trunc + merge stderr; path não aliasado */
            free(cmd->redir_out);
            cmd->redir_out = path;
            cmd->redir_append = 0;
            free(cmd->redir_err);
            cmd->redir_err = NULL;
            cmd->redir_err_append = 0;
            cmd->redir_err_to_out = 1;
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

/* Nested case body free walks lists that may contain case (AST). */
/* NOLINTNEXTLINE(misc-no-recursion) */
static void free_case_contents(petrush_case_t *cs)
{
    if (!cs) {
        return;
    }
    free(cs->word);
    cs->word = NULL;
    if (cs->arms) {
        for (int a = 0; a < cs->narms; a++) {
            petrush_case_arm_t *arm = &cs->arms[a];
            if (arm->patterns) {
                for (int p = 0; p < arm->npatterns; p++) {
                    free(arm->patterns[p]);
                }
                free(arm->patterns);
            }
            arm->patterns = NULL;
            arm->npatterns = 0;
            petrush_list_free(&arm->body);
        }
        free(cs->arms);
    }
    cs->arms = NULL;
    cs->narms = 0;
}

/* Nested list/if/while/for/fn/case free is intentional AST walk. */
/* NOLINTNEXTLINE(misc-no-recursion) */
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
        petrush_list_item_t *it = &list->items[i];
        if (it->kind == PETRUSH_ITEM_IF) {
            for (int a = 0; a < it->ifc.narms; a++) {
                petrush_list_free(&it->ifc.arms[a].cond);
                petrush_list_free(&it->ifc.arms[a].body);
            }
            free(it->ifc.arms);
            it->ifc.arms = NULL;
            it->ifc.narms = 0;
        } else if (it->kind == PETRUSH_ITEM_WHILE) {
            petrush_list_free(&it->wh.cond);
            petrush_list_free(&it->wh.body);
        } else if (it->kind == PETRUSH_ITEM_FOR) {
            free(it->fr.name);
            it->fr.name = NULL;
            if (it->fr.words) {
                for (int w = 0; w < it->fr.nwords; w++) {
                    free(it->fr.words[w]);
                }
                free(it->fr.words);
            }
            it->fr.words = NULL;
            it->fr.nwords = 0;
            petrush_list_free(&it->fr.body);
        } else if (it->kind == PETRUSH_ITEM_FN) {
            free(it->fn.name);
            it->fn.name = NULL;
            petrush_list_free(&it->fn.body);
        } else if (it->kind == PETRUSH_ITEM_CASE) {
            free_case_contents(&it->cs);
        } else {
            petrush_pipeline_free(&it->pl);
        }
    }
    free(list->items);
    list->items = NULL;
    list->nitems = 0;
}

/* Encontra próximo &&, ||, ; ou & (bg) fora de aspas. Retorna 1 se achou.
 * *out_len = comprimento do conector (2 para &&/||, 1 para ;/&).
 * *out_bg = 1 se o conector é `&` (marca o item anterior como background).
 * Ordem: && antes de &; pula &> e o `&` de 2>&1. */
static int find_list_connector(const char *s, size_t from, size_t *out_pos,
                               petrush_run_cond_t *out_cond, size_t *out_len,
                               int *out_bg)
{
    char quote = 0;
    if (out_bg) *out_bg = 0;
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
        /* OSH-9: ; & && || dentro de $(...) nao sao conectores de lista */
        if (c == '$') {
            size_t sp = petrush_cmdsubst_span(s + i);
            if (sp > 0) {
                i += sp - 1; /* for ++ */
                continue;
            }
        }
        if (c == '&' && s[i + 1] == '&') {
            *out_pos = i;
            *out_cond = PETRUSH_COND_AND;
            *out_len = 2;
            return 1;
        }
        if (c == '|' && s[i + 1] == '|') {
            *out_pos = i;
            *out_cond = PETRUSH_COND_OR;
            *out_len = 2;
            return 1;
        }
        if (c == ';') {
            /* OSH-10: `;;` barra o segmento (fim do braco); nao consome (clen=0) */
            if (s[i + 1] == ';') {
                *out_pos = i;
                *out_cond = PETRUSH_COND_ALWAYS;
                *out_len = 0;
                if (out_bg) {
                    *out_bg = 0;
                }
                return 1;
            }
            *out_pos = i;
            *out_cond = PETRUSH_COND_ALWAYS;
            *out_len = 1;
            return 1;
        }
        if (c == '&' && s[i + 1] == '>') {
            continue; /* &> redir - não é separador de lista */
        }
        if (c == '&') {
            /* 2>&1: o '&' no meio não é background */
            if (i >= 2 && s[i - 2] == '2' && s[i - 1] == '>' &&
                s[i + 1] == '1') {
                continue;
            }
            *out_pos = i;
            *out_cond = PETRUSH_COND_ALWAYS; /* próximo item como após ';' */
            *out_len = 1;
            if (out_bg) *out_bg = 1;
            return 1;
        }
    }
    return 0;
}

/* OSH-3/4/5/6/10: reserved words só em posição de comando (unquoted). */
enum {
    KW_IF     = 1,
    KW_THEN   = 2,
    KW_ELSE   = 4,
    KW_ELIF   = 8,
    KW_FI     = 16,
    KW_WHILE  = 32,
    KW_DO     = 64,
    KW_DONE   = 128,
    KW_FOR    = 256,
    KW_RBRACE = 512,
    KW_ESAC   = 1024,
    KW_DSEMI  = 2048  /* `;;` terminador de braco case */
};

static void skip_ws_pos(const char *s, size_t *pos)
{
    while (s[*pos] && isspace((unsigned char)s[*pos])) {
        (*pos)++;
    }
}

static int match_kw_at(const char *s, size_t pos, const char *kw, size_t *end_out)
{
    size_t i = pos;
    while (s[i] && isspace((unsigned char)s[i])) {
        i++;
    }
    size_t klen = strlen(kw);
    if (strncmp(s + i, kw, klen) != 0) {
        return 0;
    }
    char after = s[i + klen];
    if (after != '\0' && !isspace((unsigned char)after) && after != ';' &&
        after != '&' && after != '|' && after != '<' && after != '>') {
        return 0;
    }
    if (end_out) {
        *end_out = i + klen;
    }
    return 1;
}

/* OSH-6: `{` / `}` sem exigir delimitador apos (aceita `f(){echo;}`). */
static int match_lbrace_at(const char *s, size_t pos, size_t *end_out)
{
    size_t i = pos;
    while (s[i] && isspace((unsigned char)s[i])) {
        i++;
    }
    if (s[i] != '{') {
        return 0;
    }
    if (end_out) {
        *end_out = i + 1;
    }
    return 1;
}

static int match_rbrace_at(const char *s, size_t pos, size_t *end_out)
{
    size_t i = pos;
    while (s[i] && isspace((unsigned char)s[i])) {
        i++;
    }
    if (s[i] != '}') {
        return 0;
    }
    if (end_out) {
        *end_out = i + 1;
    }
    return 1;
}

/* OSH-10: `;;` (espacos a frente ok; nao e keyword textual). */
static int match_dsemi_at(const char *s, size_t pos, size_t *end_out)
{
    size_t i = pos;
    while (s[i] && isspace((unsigned char)s[i])) {
        i++;
    }
    if (s[i] != ';' || s[i + 1] != ';') {
        return 0;
    }
    if (end_out) {
        *end_out = i + 2;
    }
    return 1;
}

static int peek_kw_mask(const char *s, size_t pos, int mask, size_t *end_out)
{
    size_t end = 0;
    /* Prefixos longos primeiro: elif>else, done>do */
    if ((mask & KW_ELIF) && match_kw_at(s, pos, "elif", &end)) {
        if (end_out) *end_out = end;
        return KW_ELIF;
    }
    if ((mask & KW_ELSE) && match_kw_at(s, pos, "else", &end)) {
        if (end_out) *end_out = end;
        return KW_ELSE;
    }
    if ((mask & KW_THEN) && match_kw_at(s, pos, "then", &end)) {
        if (end_out) *end_out = end;
        return KW_THEN;
    }
    if ((mask & KW_FI) && match_kw_at(s, pos, "fi", &end)) {
        if (end_out) *end_out = end;
        return KW_FI;
    }
    if ((mask & KW_IF) && match_kw_at(s, pos, "if", &end)) {
        if (end_out) *end_out = end;
        return KW_IF;
    }
    if ((mask & KW_WHILE) && match_kw_at(s, pos, "while", &end)) {
        if (end_out) *end_out = end;
        return KW_WHILE;
    }
    if ((mask & KW_DONE) && match_kw_at(s, pos, "done", &end)) {
        if (end_out) *end_out = end;
        return KW_DONE;
    }
    if ((mask & KW_DO) && match_kw_at(s, pos, "do", &end)) {
        if (end_out) *end_out = end;
        return KW_DO;
    }
    /* `}` e token de 1 char; quoted nao chega aqui (fica no argv). */
    if ((mask & KW_RBRACE) && match_rbrace_at(s, pos, &end)) {
        if (end_out) *end_out = end;
        return KW_RBRACE;
    }
    if ((mask & KW_ESAC) && match_kw_at(s, pos, "esac", &end)) {
        if (end_out) *end_out = end;
        return KW_ESAC;
    }
    if ((mask & KW_DSEMI) && match_dsemi_at(s, pos, &end)) {
        if (end_out) *end_out = end;
        return KW_DSEMI;
    }
    return 0;
}

static int list_push_item(petrush_list_item_t **items, int *nitems, int *cap,
                          petrush_list_item_t *item)
{
    if (*nitems >= *cap) {
        int ncap = *cap ? *cap * 2 : 4;
        petrush_list_item_t *p =
            realloc(*items, (size_t)ncap * sizeof(*p));
        if (!p) {
            return -1;
        }
        *items = p;
        *cap = ncap;
    }
    (*items)[*nitems] = *item;
    (*nitems)++;
    return 0;
}

static int if_push_arm(petrush_if_arm_t **arms, int *narms, int *cap,
                       petrush_if_arm_t *arm)
{
    if (*narms >= *cap) {
        int ncap = *cap ? *cap * 2 : 2;
        petrush_if_arm_t *p = realloc(*arms, (size_t)ncap * sizeof(*p));
        if (!p) {
            return -1;
        }
        *arms = p;
        *cap = ncap;
    }
    (*arms)[*narms] = *arm;
    (*narms)++;
    memset(arm, 0, sizeof(*arm));
    return 0;
}

static int parse_if(const char *s, size_t *pos, petrush_if_t *out);
static int parse_while(const char *s, size_t *pos, petrush_while_t *out);
static int parse_for(const char *s, size_t *pos, petrush_for_t *out);
static int parse_case(const char *s, size_t *pos, petrush_case_t *out);
static int parse_fn(const char *s, size_t *pos, petrush_fn_t *out, int from_kw);
static int looks_like_fn_def(const char *s, size_t pos);
static int parse_list_until(const char *s, size_t *pos, int stop_mask,
                            petrush_list_t *out, int require_term);

/*
 * Uma pipeline até o próximo conector de lista.
 * Retorna 0 ok, -1 erro, -2 segmento vazio (trailing ;/& ou leading vazio).
 */
static int parse_pipeline_item(const char *s, size_t *pos,
                               petrush_list_item_t *item,
                               petrush_run_cond_t *next_cond_out,
                               int *had_conn_out)
{
    size_t cpos = 0;
    size_t clen = 0;
    petrush_run_cond_t conn = PETRUSH_COND_ALWAYS;
    int marks_bg = 0;
    int has = find_list_connector(s, *pos, &cpos, &conn, &clen, &marks_bg);

    size_t start = *pos;
    size_t end = has ? cpos : strlen(s);
    while (start < end && isspace((unsigned char)s[start])) {
        start++;
    }
    size_t e = end;
    while (e > start && isspace((unsigned char)s[e - 1])) {
        e--;
    }
    size_t seglen = e - start;

    item->kind = PETRUSH_ITEM_PIPELINE;
    item->background = 0;
    memset(&item->pl, 0, sizeof(item->pl));
    memset(&item->ifc, 0, sizeof(item->ifc));
    memset(&item->wh, 0, sizeof(item->wh));

    if (seglen == 0) {
        *had_conn_out = has;
        if (has) {
            *next_cond_out = conn;
            *pos = cpos + clen;
        } else {
            *pos = end;
        }
        return -2;
    }

    char *seg = malloc(seglen + 1);
    if (!seg) {
        return -1;
    }
    memcpy(seg, s + start, seglen);
    seg[seglen] = '\0';

    if (petrush_parse_pipeline(seg, &item->pl) != 0) {
        free(seg);
        return -1;
    }
    free(seg);

    if (has && marks_bg) {
        item->background = 1;
    }
    if (has) {
        *next_cond_out = conn;
        *pos = cpos + clen;
        *had_conn_out = 1;
    } else {
        *pos = end;
        *had_conn_out = 0;
    }
    return 0;
}

static int take_conn_after(const char *s, size_t *pos,
                           petrush_run_cond_t *next_cond_out,
                           int *bg_out)
{
    skip_ws_pos(s, pos);
    size_t cpos = 0;
    size_t clen = 0;
    petrush_run_cond_t conn = PETRUSH_COND_ALWAYS;
    int marks_bg = 0;
    if (!find_list_connector(s, *pos, &cpos, &conn, &clen, &marks_bg)) {
        return 0;
    }
    if (cpos != *pos) {
        return 0;
    }
    if (bg_out) {
        *bg_out = marks_bg;
    }
    *next_cond_out = conn;
    *pos = cpos + clen;
    return 1;
}

/* Recursive descent: list <-> if/while/for/fn/case. Complexity from keyword dispatch. */
/* NOLINTNEXTLINE(misc-no-recursion, readability-function-cognitive-complexity) */
static int parse_list_until(const char *s, size_t *pos, int stop_mask,
                            petrush_list_t *out, int require_term)
{
    memset(out, 0, sizeof(*out));
    petrush_list_item_t *items = NULL;
    int nitems = 0;
    int cap = 0;
    petrush_run_cond_t next_cond = PETRUSH_COND_ALWAYS;

    while (1) {
        skip_ws_pos(s, pos);
        if (s[*pos] == '\0') {
            if (require_term) {
                goto fail;
            }
            /* trailing && / || sem operando direito */
            if (next_cond == PETRUSH_COND_AND ||
                next_cond == PETRUSH_COND_OR) {
                goto fail;
            }
            break;
        }
        if (stop_mask && peek_kw_mask(s, *pos, stop_mask, NULL)) {
            /* `then`/`fi`/... com &&/|| pendente é erro */
            if (next_cond == PETRUSH_COND_AND ||
                next_cond == PETRUSH_COND_OR) {
                goto fail;
            }
            break;
        }

        petrush_list_item_t item;
        memset(&item, 0, sizeof(item));
        item.cond = next_cond;
        next_cond = PETRUSH_COND_ALWAYS;

        size_t kw_end = 0;
        int had_conn = 0;

        if (match_kw_at(s, *pos, "if", &kw_end)) {
            item.kind = PETRUSH_ITEM_IF;
            if (parse_if(s, pos, &item.ifc) != 0) {
                goto fail;
            }
            int bg = 0;
            if (take_conn_after(s, pos, &next_cond, &bg)) {
                had_conn = 1;
                if (bg) {
                    item.background = 1;
                }
            }
        } else if (match_kw_at(s, *pos, "while", &kw_end)) {
            item.kind = PETRUSH_ITEM_WHILE;
            if (parse_while(s, pos, &item.wh) != 0) {
                goto fail;
            }
            int bg = 0;
            if (take_conn_after(s, pos, &next_cond, &bg)) {
                had_conn = 1;
                if (bg) {
                    item.background = 1;
                }
            }
        } else if (match_kw_at(s, *pos, "for", &kw_end)) {
            item.kind = PETRUSH_ITEM_FOR;
            if (parse_for(s, pos, &item.fr) != 0) {
                goto fail;
            }
            int bg = 0;
            if (take_conn_after(s, pos, &next_cond, &bg)) {
                had_conn = 1;
                if (bg) {
                    item.background = 1;
                }
            }
        } else if (match_kw_at(s, *pos, "case", &kw_end)) {
            item.kind = PETRUSH_ITEM_CASE;
            if (parse_case(s, pos, &item.cs) != 0) {
                goto fail;
            }
            int bg = 0;
            if (take_conn_after(s, pos, &next_cond, &bg)) {
                had_conn = 1;
                if (bg) {
                    item.background = 1;
                }
            }
        } else if (match_kw_at(s, *pos, "function", &kw_end)) {
            item.kind = PETRUSH_ITEM_FN;
            if (parse_fn(s, pos, &item.fn, 1) != 0) {
                goto fail;
            }
            int bg = 0;
            if (take_conn_after(s, pos, &next_cond, &bg)) {
                had_conn = 1;
                if (bg) {
                    item.background = 1;
                }
            }
        } else if (looks_like_fn_def(s, *pos)) {
            item.kind = PETRUSH_ITEM_FN;
            if (parse_fn(s, pos, &item.fn, 0) != 0) {
                goto fail;
            }
            int bg = 0;
            if (take_conn_after(s, pos, &next_cond, &bg)) {
                had_conn = 1;
                if (bg) {
                    item.background = 1;
                }
            }
        } else {
            int rc = parse_pipeline_item(s, pos, &item, &next_cond, &had_conn);
            if (rc == -1) {
                goto fail;
            }
            if (rc == -2) {
                /* trailing ;/& after items → drop; empty after &&/|| → erro */
                if (!had_conn && item.cond == PETRUSH_COND_ALWAYS &&
                    nitems > 0) {
                    break;
                }
                if (!had_conn && nitems == 0 &&
                    item.cond == PETRUSH_COND_ALWAYS) {
                    break; /* input vazio */
                }
                goto fail;
            }
        }

        if (list_push_item(&items, &nitems, &cap, &item) != 0) {
            if (item.kind == PETRUSH_ITEM_IF) {
                for (int a = 0; a < item.ifc.narms; a++) {
                    petrush_list_free(&item.ifc.arms[a].cond);
                    petrush_list_free(&item.ifc.arms[a].body);
                }
                free(item.ifc.arms);
            } else if (item.kind == PETRUSH_ITEM_WHILE) {
                petrush_list_free(&item.wh.cond);
                petrush_list_free(&item.wh.body);
            } else if (item.kind == PETRUSH_ITEM_FOR) {
                free(item.fr.name);
                if (item.fr.words) {
                    for (int w = 0; w < item.fr.nwords; w++) {
                        free(item.fr.words[w]);
                    }
                    free(item.fr.words);
                }
                petrush_list_free(&item.fr.body);
            } else if (item.kind == PETRUSH_ITEM_FN) {
                free(item.fn.name);
                petrush_list_free(&item.fn.body);
            } else if (item.kind == PETRUSH_ITEM_CASE) {
                free_case_contents(&item.cs);
            } else {
                petrush_pipeline_free(&item.pl);
            }
            goto fail;
        }

        if (!had_conn) {
            /* sem conector: próxima iteração vê EOF ou terminator */
            skip_ws_pos(s, pos);
            if (s[*pos] == '\0') {
                if (require_term) {
                    goto fail;
                }
                break;
            }
            if (stop_mask && peek_kw_mask(s, *pos, stop_mask, NULL)) {
                break;
            }
            /* texto residual sem conector (ex.: palavra solta) → erro de lista */
            goto fail;
        }
    }

    out->items = items;
    out->nitems = nitems;
    return 0;

fail:
    {
        petrush_list_t tmp = {.items = items, .nitems = nitems};
        petrush_list_free(&tmp);
    }
    memset(out, 0, sizeof(*out));
    return -1;
}

/* OSH-4: while <list> do <list> done */
/* NOLINTNEXTLINE(misc-no-recursion) */
static int parse_while(const char *s, size_t *pos, petrush_while_t *out)
{
    memset(out, 0, sizeof(*out));
    size_t end = 0;
    if (!match_kw_at(s, *pos, "while", &end)) {
        return -1;
    }
    *pos = end;

    if (parse_list_until(s, pos, KW_DO, &out->cond, 1) != 0) {
        goto fail;
    }
    if (out->cond.nitems == 0) {
        goto fail;
    }
    if (!match_kw_at(s, *pos, "do", &end)) {
        goto fail;
    }
    *pos = end;
    if (parse_list_until(s, pos, KW_DONE, &out->body, 1) != 0) {
        goto fail;
    }
    if (!match_kw_at(s, *pos, "done", &end)) {
        goto fail;
    }
    *pos = end;
    return 0;

fail:
    petrush_list_free(&out->cond);
    petrush_list_free(&out->body);
    memset(out, 0, sizeof(*out));
    return -1;
}

/*
 * OSH-5: palavra da lista for (como scan_word, mas para em `;` unquoted;
 * ex.: `c;do` sem espaco).
 */
static char *scan_for_word(const char **pp, int *quoted_out)
{
    const char *p = *pp;
    char quote = 0;
    int quoted = 0;
    if (*p == '\'' || *p == '"') {
        quote = *p;
        quoted = 1;
        p++;
    }
    const char *start = p;
    while (*p) {
        if (quote) {
            if (*p == quote) {
                p++;
                break;
            }
            p++;
            continue;
        }
        if (isspace((unsigned char)*p)) {
            break;
        }
        if (*p == ';' || *p == '|' || *p == '&' || *p == '<' ||
            *p == '>') {
            break;
        }
        /* OSH-9: nao quebrar dentro de $(...) */
        if (*p == '$') {
            size_t sp = petrush_cmdsubst_span(p);
            if (sp > 0) {
                p += sp;
                continue;
            }
        }
        p++;
    }
    size_t token_len = (size_t)(p - start);
    if (quote && token_len > 0 && start[token_len - 1] == quote) {
        token_len--;
    }
    char *text = malloc(token_len + 1);
    if (!text) {
        *pp = p;
        *quoted_out = quoted;
        return NULL;
    }
    memcpy(text, start, token_len);
    text[token_len] = '\0';
    *pp = p;
    *quoted_out = quoted;
    return text;
}

static int for_push_word(char ***words, int *nwords, int *cap, char *w)
{
    if (*nwords >= *cap) {
        int ncap = *cap ? *cap * 2 : 4;
        char **p = realloc(*words, (size_t)ncap * sizeof(*p));
        if (!p) {
            return -1;
        }
        *words = p;
        *cap = ncap;
    }
    (*words)[*nwords] = w;
    (*nwords)++;
    return 0;
}

/* OSH-5: for name in words; do list; done (in obrigatorio; sem for (() ) */
/* NOLINTNEXTLINE(misc-no-recursion) */
static int parse_for(const char *s, size_t *pos, petrush_for_t *out)
{
    memset(out, 0, sizeof(*out));
    size_t end = 0;
    if (!match_kw_at(s, *pos, "for", &end)) {
        return -1;
    }
    *pos = end;
    skip_ws_pos(s, pos);

    /* Sem C-style for (( */
    if (s[*pos] == '(') {
        goto fail;
    }

    const char *p = s + *pos;
    int quoted = 0;
    char *name = scan_for_word(&p, &quoted);
    *pos = (size_t)(p - s);
    if (!name || name[0] == '\0') {
        free(name);
        goto fail;
    }
    out->name = name;

    skip_ws_pos(s, pos);
    /* in obrigatorio nesta onda */
    if (!match_kw_at(s, *pos, "in", &end)) {
        goto fail;
    }
    *pos = end;

    char **words = NULL;
    int nwords = 0;
    int wcap = 0;
    for (;;) {
        skip_ws_pos(s, pos);
        if (s[*pos] == '\0') {
            for (int i = 0; i < nwords; i++) {
                free(words[i]);
            }
            free(words);
            words = NULL;
            nwords = 0;
            goto fail;
        }
        if (s[*pos] == ';') {
            (*pos)++;
            break;
        }
        if (match_kw_at(s, *pos, "do", &end)) {
            break;
        }
        p = s + *pos;
        quoted = 0;
        char *w = scan_for_word(&p, &quoted);
        *pos = (size_t)(p - s);
        if (!w) {
            for (int i = 0; i < nwords; i++) {
                free(words[i]);
            }
            free(words);
            goto fail;
        }
        if (w[0] == '\0') {
            free(w);
            for (int i = 0; i < nwords; i++) {
                free(words[i]);
            }
            free(words);
            goto fail;
        }
        if (for_push_word(&words, &nwords, &wcap, w) != 0) {
            free(w);
            for (int i = 0; i < nwords; i++) {
                free(words[i]);
            }
            free(words);
            goto fail;
        }
    }
    out->words = words;
    out->nwords = nwords;

    skip_ws_pos(s, pos);
    if (!match_kw_at(s, *pos, "do", &end)) {
        goto fail;
    }
    *pos = end;
    if (parse_list_until(s, pos, KW_DONE, &out->body, 1) != 0) {
        goto fail;
    }
    if (!match_kw_at(s, *pos, "done", &end)) {
        goto fail;
    }
    *pos = end;
    return 0;

fail:
    free(out->name);
    out->name = NULL;
    if (out->words) {
        for (int i = 0; i < out->nwords; i++) {
            free(out->words[i]);
        }
        free(out->words);
    }
    out->words = NULL;
    out->nwords = 0;
    petrush_list_free(&out->body);
    memset(out, 0, sizeof(*out));
    return -1;
}

/*
 * OSH-10: padrao de case — como scan_for_word, mas para em `|` / `)` unquoted.
 */
static char *scan_case_pattern(const char **pp, int *quoted_out)
{
    const char *p = *pp;
    char quote = 0;
    int quoted = 0;
    if (*p == '\'' || *p == '"') {
        quote = *p;
        quoted = 1;
        p++;
    }
    const char *start = p;
    while (*p) {
        if (quote) {
            if (*p == quote) {
                p++;
                break;
            }
            p++;
            continue;
        }
        if (isspace((unsigned char)*p)) {
            break;
        }
        if (*p == '|' || *p == ')' || *p == ';' || *p == '&' || *p == '<' ||
            *p == '>') {
            break;
        }
        if (*p == '$') {
            size_t sp = petrush_cmdsubst_span(p);
            if (sp > 0) {
                p += sp;
                continue;
            }
        }
        p++;
    }
    size_t token_len = (size_t)(p - start);
    if (quote && token_len > 0 && start[token_len - 1] == quote) {
        token_len--;
    }
    char *text = malloc(token_len + 1);
    if (!text) {
        *pp = p;
        *quoted_out = quoted;
        return NULL;
    }
    memcpy(text, start, token_len);
    text[token_len] = '\0';
    *pp = p;
    *quoted_out = quoted;
    return text;
}

static int case_push_arm(petrush_case_arm_t **arms, int *narms, int *cap,
                         petrush_case_arm_t *arm)
{
    if (*narms >= *cap) {
        int ncap = *cap ? *cap * 2 : 4;
        petrush_case_arm_t *p =
            realloc(*arms, (size_t)ncap * sizeof(*p));
        if (!p) {
            return -1;
        }
        *arms = p;
        *cap = ncap;
    }
    (*arms)[*narms] = *arm;
    (*narms)++;
    return 0;
}

/* OSH-10: case word in [ [(] pat[|pat]...) list ;; ]... esac */
/* NOLINTNEXTLINE(misc-no-recursion, readability-function-cognitive-complexity) */
static int parse_case(const char *s, size_t *pos, petrush_case_t *out)
{
    memset(out, 0, sizeof(*out));
    size_t end = 0;
    if (!match_kw_at(s, *pos, "case", &end)) {
        return -1;
    }
    *pos = end;
    skip_ws_pos(s, pos);

    const char *p = s + *pos;
    int quoted = 0;
    char *word = scan_for_word(&p, &quoted);
    *pos = (size_t)(p - s);
    if (!word || word[0] == '\0') {
        free(word);
        goto fail;
    }
    out->word = word;

    skip_ws_pos(s, pos);
    if (!match_kw_at(s, *pos, "in", &end)) {
        goto fail;
    }
    *pos = end;

    petrush_case_arm_t *arms = NULL;
    int narms = 0;
    int acap = 0;

    for (;;) {
        skip_ws_pos(s, pos);
        if (match_kw_at(s, *pos, "esac", &end)) {
            *pos = end;
            break;
        }
        if (s[*pos] == '\0') {
            goto fail_arms;
        }

        /* `(` opcional no inicio do braco */
        if (s[*pos] == '(') {
            (*pos)++;
        }

        char **pats = NULL;
        int npats = 0;
        int pcap = 0;
        for (;;) {
            skip_ws_pos(s, pos);
            if (s[*pos] == '\0') {
                for (int i = 0; i < npats; i++) {
                    free(pats[i]);
                }
                free(pats);
                goto fail_arms;
            }
            p = s + *pos;
            quoted = 0;
            char *pat = scan_case_pattern(&p, &quoted);
            *pos = (size_t)(p - s);
            if (!pat) {
                for (int i = 0; i < npats; i++) {
                    free(pats[i]);
                }
                free(pats);
                goto fail_arms;
            }
            if (pat[0] == '\0') {
                free(pat);
                for (int i = 0; i < npats; i++) {
                    free(pats[i]);
                }
                free(pats);
                goto fail_arms;
            }
            if (for_push_word(&pats, &npats, &pcap, pat) != 0) {
                free(pat);
                for (int i = 0; i < npats; i++) {
                    free(pats[i]);
                }
                free(pats);
                goto fail_arms;
            }
            skip_ws_pos(s, pos);
            if (s[*pos] == '|') {
                (*pos)++;
                continue;
            }
            if (s[*pos] == ')') {
                (*pos)++;
                break;
            }
            for (int i = 0; i < npats; i++) {
                free(pats[i]);
            }
            free(pats);
            goto fail_arms;
        }

        petrush_case_arm_t arm;
        memset(&arm, 0, sizeof(arm));
        arm.patterns = pats;
        arm.npatterns = npats;
        if (parse_list_until(s, pos, KW_DSEMI | KW_ESAC, &arm.body, 1) != 0) {
            for (int i = 0; i < npats; i++) {
                free(pats[i]);
            }
            free(pats);
            goto fail_arms;
        }

        skip_ws_pos(s, pos);
        if (match_dsemi_at(s, *pos, &end)) {
            *pos = end;
        } else if (!match_kw_at(s, *pos, "esac", NULL)) {
            petrush_list_free(&arm.body);
            for (int i = 0; i < npats; i++) {
                free(pats[i]);
            }
            free(pats);
            goto fail_arms;
        }
        /* sem `;;` antes de esac: aceito (braco final) */

        if (case_push_arm(&arms, &narms, &acap, &arm) != 0) {
            petrush_list_free(&arm.body);
            for (int i = 0; i < npats; i++) {
                free(pats[i]);
            }
            free(pats);
            goto fail_arms;
        }
    }

    out->arms = arms;
    out->narms = narms;
    return 0;

fail_arms:
    {
        petrush_case_t tmp = {.word = NULL, .arms = arms, .narms = narms};
        free_case_contents(&tmp);
    }
fail:
    free_case_contents(out);
    memset(out, 0, sizeof(*out));
    return -1;
}

static int is_fn_name_start(char c)
{
    return isalpha((unsigned char)c) || c == '_';
}

static int is_fn_name_char(char c)
{
    return isalnum((unsigned char)c) || c == '_';
}

/* Nome de funcao: [A-Za-z_][A-Za-z0-9_]* (sem aspas). */
static char *scan_fn_name(const char *s, size_t *pos)
{
    skip_ws_pos(s, pos);
    if (!is_fn_name_start(s[*pos])) {
        return NULL;
    }
    size_t start = *pos;
    (*pos)++;
    while (is_fn_name_char(s[*pos])) {
        (*pos)++;
    }
    size_t len = *pos - start;
    char *name = malloc(len + 1);
    if (!name) {
        return NULL;
    }
    memcpy(name, s + start, len);
    name[len] = '\0';
    return name;
}

static int match_empty_parens(const char *s, size_t *pos)
{
    skip_ws_pos(s, pos);
    if (s[*pos] != '(') {
        return 0;
    }
    (*pos)++;
    skip_ws_pos(s, pos);
    if (s[*pos] != ')') {
        return 0;
    }
    (*pos)++;
    return 1;
}

/* Peek: name () {  (POSIX). Nao consome. */
static int looks_like_fn_def(const char *s, size_t pos)
{
    skip_ws_pos(s, &pos);
    if (!is_fn_name_start(s[pos])) {
        return 0;
    }
    pos++;
    while (is_fn_name_char(s[pos])) {
        pos++;
    }
    if (!match_empty_parens(s, &pos)) {
        return 0;
    }
    size_t end = 0;
    return match_lbrace_at(s, pos, &end);
}

/*
 * OSH-6: name() { list; }  ou  function name [()] { list; }
 * from_kw=1 ja viu/consome "function"; from_kw=0 exige ().
 */
/* NOLINTNEXTLINE(misc-no-recursion) */
static int parse_fn(const char *s, size_t *pos, petrush_fn_t *out, int from_kw)
{
    memset(out, 0, sizeof(*out));
    size_t end = 0;

    if (from_kw) {
        if (!match_kw_at(s, *pos, "function", &end)) {
            return -1;
        }
        *pos = end;
    }

    char *name = scan_fn_name(s, pos);
    if (!name || name[0] == '\0') {
        free(name);
        goto fail;
    }
    out->name = name;

    skip_ws_pos(s, pos);
    if (s[*pos] == '(') {
        if (!match_empty_parens(s, pos)) {
            goto fail;
        }
    } else if (!from_kw) {
        /* forma POSIX exige () */
        goto fail;
    }

    if (!match_lbrace_at(s, *pos, &end)) {
        goto fail;
    }
    *pos = end;

    if (parse_list_until(s, pos, KW_RBRACE, &out->body, 1) != 0) {
        goto fail;
    }
    if (!match_rbrace_at(s, *pos, &end)) {
        goto fail;
    }
    *pos = end;
    return 0;

fail:
    free(out->name);
    out->name = NULL;
    petrush_list_free(&out->body);
    memset(out, 0, sizeof(*out));
    return -1;
}

/* NOLINTNEXTLINE(misc-no-recursion) */
static int parse_if(const char *s, size_t *pos, petrush_if_t *out)
{
    memset(out, 0, sizeof(*out));
    size_t end = 0;
    if (!match_kw_at(s, *pos, "if", &end)) {
        return -1;
    }
    *pos = end;

    petrush_if_arm_t *arms = NULL;
    int narms = 0;
    int cap = 0;

    petrush_if_arm_t arm;
    memset(&arm, 0, sizeof(arm));
    if (parse_list_until(s, pos, KW_THEN, &arm.cond, 1) != 0) {
        goto fail;
    }
    if (arm.cond.nitems == 0) {
        petrush_list_free(&arm.cond);
        goto fail;
    }
    if (!match_kw_at(s, *pos, "then", &end)) {
        petrush_list_free(&arm.cond);
        goto fail;
    }
    *pos = end;
    if (parse_list_until(s, pos, KW_ELIF | KW_ELSE | KW_FI, &arm.body, 1) != 0) {
        petrush_list_free(&arm.cond);
        goto fail;
    }
    arm.is_else = 0;
    if (if_push_arm(&arms, &narms, &cap, &arm) != 0) {
        petrush_list_free(&arm.cond);
        petrush_list_free(&arm.body);
        goto fail;
    }

    while (match_kw_at(s, *pos, "elif", &end)) {
        *pos = end;
        memset(&arm, 0, sizeof(arm));
        if (parse_list_until(s, pos, KW_THEN, &arm.cond, 1) != 0) {
            goto fail;
        }
        if (arm.cond.nitems == 0) {
            petrush_list_free(&arm.cond);
            goto fail;
        }
        if (!match_kw_at(s, *pos, "then", &end)) {
            petrush_list_free(&arm.cond);
            goto fail;
        }
        *pos = end;
        if (parse_list_until(s, pos, KW_ELIF | KW_ELSE | KW_FI, &arm.body,
                             1) != 0) {
            petrush_list_free(&arm.cond);
            goto fail;
        }
        arm.is_else = 0;
        if (if_push_arm(&arms, &narms, &cap, &arm) != 0) {
            petrush_list_free(&arm.cond);
            petrush_list_free(&arm.body);
            goto fail;
        }
    }

    if (match_kw_at(s, *pos, "else", &end)) {
        *pos = end;
        memset(&arm, 0, sizeof(arm));
        arm.is_else = 1;
        if (parse_list_until(s, pos, KW_FI, &arm.body, 1) != 0) {
            goto fail;
        }
        if (if_push_arm(&arms, &narms, &cap, &arm) != 0) {
            petrush_list_free(&arm.body);
            goto fail;
        }
    }

    if (!match_kw_at(s, *pos, "fi", &end)) {
        goto fail;
    }
    *pos = end;

    out->arms = arms;
    out->narms = narms;
    return 0;

fail:
    for (int i = 0; i < narms; i++) {
        petrush_list_free(&arms[i].cond);
        petrush_list_free(&arms[i].body);
    }
    free(arms);
    memset(out, 0, sizeof(*out));
    return -1;
}

int petrush_parse_list(const char *input, petrush_list_t *out)
{
    if (!input || !out) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    size_t pos = 0;
    skip_ws_pos(input, &pos);
    if (input[pos] == '\0') {
        return 0;
    }
    return parse_list_until(input, &pos, 0, out, 0);
}
