/*
 * parser.h — Parser de linha de comando para PetRush
 *
 * Responsável por transformar uma string de entrada em argv/argc.
 * Suporte inicial: argumentos separados por espaço + aspas simples e duplas básicas.
 */

#ifndef PETRUSH_PARSER_H
#define PETRUSH_PARSER_H

#include <stddef.h>

typedef struct {
    char **argv;
    int argc;
} petrush_cmd_t;

/*
 * Analisa a string de entrada e preenche out com argc/argv.
 * Retorna 0 em caso de sucesso, -1 em erro de parsing.
 *
 * O chamador é responsável por chamar petrush_cmd_free() depois.
 */
int petrush_parse(const char *input, petrush_cmd_t *out);

/* Libera memória alocada por petrush_parse() */
void petrush_cmd_free(petrush_cmd_t *cmd);

#endif /* PETRUSH_PARSER_H */
