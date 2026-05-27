/*
 * parser.c — Implementação simples do parser de linha de comando
 */

#include "petrush/parser.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define INITIAL_ARGV_CAPACITY 8

static int is_quote(char c) {
    return c == '"' || c == '\'';
}

int petrush_parse(const char *input, petrush_cmd_t *out)
{
    if (!input || !out) return -1;

    memset(out, 0, sizeof(*out));

    size_t len = strlen(input);
    if (len == 0) {
        out->argv = NULL;
        out->argc = 0;
        return 0;
    }

    char **argv = malloc(sizeof(char*) * INITIAL_ARGV_CAPACITY);
    if (!argv) return -1;

    int argc = 0;
    size_t argv_capacity = INITIAL_ARGV_CAPACITY;

    const char *p = input;

    while (*p) {
        /* Pular espaços */
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;

        char quote = 0;
        if (is_quote(*p)) {
            quote = *p;
            p++;
        }

        const char *start = p;

        /* Encontrar o fim do token */
        while (*p) {
            if (quote) {
                if (*p == quote) {
                    p++;
                    break;
                }
            } else {
                if (isspace((unsigned char)*p)) {
                    break;
                }
            }
            p++;
        }

        size_t token_len = (size_t)(p - start);

        /* Se terminou com quote, já pulamos o quote de fechamento */
        if (quote && token_len > 0 && start[token_len-1] == quote) {
            token_len--;
        }

        char *token = malloc(token_len + 1);
        if (!token) {
            /* limpeza parcial */
            for (int i = 0; i < argc; i++) free(argv[i]);
            free(argv);
            return -1;
        }

        memcpy(token, start, token_len);
        token[token_len] = '\0';

        /* Expandir argv se necessário */
        if (argc >= (int)argv_capacity) {
            argv_capacity *= 2;
            char **new_argv = realloc(argv, sizeof(char*) * argv_capacity);
            if (!new_argv) {
                free(token);
                for (int i = 0; i < argc; i++) free(argv[i]);
                free(argv);
                return -1;
            }
            argv = new_argv;
        }

        argv[argc++] = token;
    }

    /* Garantir que argv termine com NULL (útil para exec) */
    char **final_argv = realloc(argv, sizeof(char*) * ((size_t)argc + 1));
    if (final_argv) {
        argv = final_argv;
    }
    argv[argc] = NULL;

    out->argv = argv;
    out->argc = argc;

    return 0;
}

void petrush_cmd_free(petrush_cmd_t *cmd)
{
    if (!cmd || !cmd->argv) return;

    for (int i = 0; i < cmd->argc; i++) {
        free(cmd->argv[i]);
    }
    free(cmd->argv);

    cmd->argv = NULL;
    cmd->argc = 0;
}
