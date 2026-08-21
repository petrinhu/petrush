/*
 * parser.h — Parser de linha de comando para PetRush
 *
 * Onda 3 (NEW-20, decisão autônoma 2026-08-03):
 * - pipes `|`
 * - redirecionamento `>`, `>>`, `<`
 * UX-16: `2>`, `2>>`, `2>&1`, `&>`
 * NÃO: background `&`, `2>&N` genérico, globbing, scripting de arquivo.
 */

#ifndef PETRUSH_PARSER_H
#define PETRUSH_PARSER_H

#include <stddef.h>

/* Um estágio (comando simples) com argv e redirecionamentos opcionais. */
typedef struct {
    char **argv;
    int argc;
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

/* NEW-24: listas && e || (left-to-right, short-circuit) */
typedef enum {
    PETRUSH_COND_ALWAYS = 0, /* first item */
    PETRUSH_COND_AND,        /* run if previous status == 0 */
    PETRUSH_COND_OR          /* run if previous status != 0 */
} petrush_run_cond_t;

typedef struct {
    petrush_pipeline_t pl;
    petrush_run_cond_t cond;
} petrush_list_item_t;

typedef struct {
    petrush_list_item_t *items;
    int nitems;
} petrush_list_t;

int petrush_parse_list(const char *input, petrush_list_t *out);
void petrush_list_free(petrush_list_t *list);

#endif /* PETRUSH_PARSER_H */
