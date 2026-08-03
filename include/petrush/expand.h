/*
 * expand.h — ~ e $VAR expansion (UX-12, UX-13)
 */

#ifndef PETRUSH_EXPAND_H
#define PETRUSH_EXPAND_H

#include "petrush/parser.h"

/* Expande uma palavra (malloc). Nunca retorna NULL se word != NULL (exceto OOM). */
char *expand_word(const char *word);

/* Expande argv[] in-place (realoca cada argv[i]). */
void expand_cmd_argv(petrush_cmd_t *cmd);

#endif /* PETRUSH_EXPAND_H */
