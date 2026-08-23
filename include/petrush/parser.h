/*
 * parser.h — Parser de linha de comando para PetRush
 *
 * Onda 3 (NEW-20, decisão autônoma 2026-08-03):
 * - pipes `|`
 * - redirecionamento `>`, `>>`, `<`
 * UX-16: `2>`, `2>>`, `2>&1`, `&>`
 * UX-18: `argv_quoted` paralelo (1 = token nasceu quoted; sem re-glob).
 * UX-23: sufixo/separador `&` → `petrush_list_item_t.background`.
 * OSH-3: `if` / `then` / `elif` / `else` / `fi` (compound sobre parse_list).
 * NÃO: `2>&N` genérico, `[]`/`**`, `[[`, fg/bg/Ctrl-Z/%n.
 */

#ifndef PETRUSH_PARSER_H
#define PETRUSH_PARSER_H

#include <stddef.h>

/* Um estágio (comando simples) com argv e redirecionamentos opcionais. */
typedef struct {
    char **argv;
    int argc;
    int *argv_quoted;  /* paralelo a argv; NULL = todos unquoted */
    char *redir_in;    /* caminho de `< file` (owned) ou NULL */
    char *redir_out;   /* caminho de `> file` / `>> file` (owned) ou NULL */
    int redir_append;  /* 1 se stdout for `>>` */
    char *redir_err;        /* path 2> / 2>>; NULL se merge-only */
    int redir_err_append;   /* 1 se 2>> */
    int redir_err_to_out;   /* 1 se 2>&1 ou &> → dup2(stdout, stderr) */
} petrush_cmd_t;

/* Pipeline: um ou mais estágios ligados por `|`. */
typedef struct {
    petrush_cmd_t *cmds;
    int ncmds;
} petrush_pipeline_t;

/*
 * Analisa a string em um pipeline (1 estágio se não houver `|`).
 * Retorna 0 em sucesso, -1 em erro de parsing.
 * O chamador deve chamar petrush_pipeline_free().
 */
int petrush_parse_pipeline(const char *input, petrush_pipeline_t *out);

/* Libera pipeline e todos os estágios. */
void petrush_pipeline_free(petrush_pipeline_t *pl);

/*
 * Compat: analisa linha com no máximo 1 estágio (sem `|`).
 * Aceita redirecionamentos no estágio único.
 * Se houver pipe, retorna -1.
 * O chamador deve chamar petrush_cmd_free().
 */
int petrush_parse(const char *input, petrush_cmd_t *out);

/* Libera memória alocada por petrush_parse() / um estágio. */
void petrush_cmd_free(petrush_cmd_t *cmd);

/* NEW-24 / UX-17: listas && || ; (left-to-right; &&/|| short-circuit) */
typedef enum {
    PETRUSH_COND_ALWAYS = 0, /* first item, or after ';' */
    PETRUSH_COND_AND,        /* run if previous status == 0 */
    PETRUSH_COND_OR          /* run if previous status != 0 */
} petrush_run_cond_t;

/* OSH-3: item de lista = pipeline simples ou compound if */
typedef enum {
    PETRUSH_ITEM_PIPELINE = 0,
    PETRUSH_ITEM_IF
} petrush_item_kind_t;

typedef struct petrush_list_item petrush_list_item_t;

typedef struct petrush_list {
    petrush_list_item_t *items;
    int nitems;
} petrush_list_t;

/* Um braço: if/elif (cond+body) ou else (só body; is_else=1). */
typedef struct {
    petrush_list_t cond; /* vazia se is_else */
    petrush_list_t body;
    int is_else;
} petrush_if_arm_t;

typedef struct {
    petrush_if_arm_t *arms;
    int narms;
} petrush_if_t;

struct petrush_list_item {
    petrush_item_kind_t kind;
    petrush_pipeline_t pl;   /* kind == PIPELINE */
    petrush_if_t ifc;        /* kind == IF */
    petrush_run_cond_t cond;
    int background; /* UX-23: 1 se o item terminou com `&` */
};

int petrush_parse_list(const char *input, petrush_list_t *out);
void petrush_list_free(petrush_list_t *list);

#endif /* PETRUSH_PARSER_H */
