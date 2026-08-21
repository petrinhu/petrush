/*
 * expand.h — ~ e $VAR (UX-12/13) + glob * ? (UX-18)
 */

#ifndef PETRUSH_EXPAND_H
#define PETRUSH_EXPAND_H

#include "petrush/parser.h"

#define PETRUSH_GLOB_MAX 256

/* Expande uma palavra (malloc). Nunca retorna NULL se word != NULL (exceto OOM). */
char *expand_word(const char *word);

/*
 * Pathname expansion de uma palavra.
 * 0 matches: 1 elemento = cópia do padrão.
 * N matches: N paths ordenados (qsort/strcmp).
 * Estouro > PETRUSH_GLOB_MAX: NULL (fail closed).
 * Chamador libera cada string e o vetor.
 */
char **glob_word(const char *pattern, int *n);

/* Expande argv[] (~/$VAR) e, em unquoted, glob * ?. Redirs: só ~/$VAR. */
void expand_cmd_argv(petrush_cmd_t *cmd);

#endif /* PETRUSH_EXPAND_H */
