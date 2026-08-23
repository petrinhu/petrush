/*
 * expand.h — ~ / $VAR / ${VAR:-} / ${VAR:+} / ${#VAR} (UX-12/13, FEAT-PARAM)
 * + glob * ? (UX-18)
 * + posicionais $0 $1.. $# $@ $* (OSH-1; sem shift / ${1:-} / arrays)
 */

#ifndef PETRUSH_EXPAND_H
#define PETRUSH_EXPAND_H

#include "petrush/parser.h"

#define PETRUSH_GLOB_MAX 256

/*
 * OSH-1: estado de posicionais (struct, nao environ com "1"/"#").
 * arg0 = $0; args[0..nargs) = $1..$#. Copia os strings.
 * Retorna 0 ok, -1 OOM (estado limpo).
 */
int petrush_positional_set(const char *arg0, int nargs, char *const *args);
void petrush_positional_clear(void);
/* n=0 → $0 ("" se unset); n>=1 → $n ou NULL se fora do range. */
const char *petrush_positional_get(unsigned n);
unsigned petrush_positional_count(void); /* $# */

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

/*
 * Expande argv[] (~/$VAR/$n/$#) e, em unquoted, glob * ?.
 * "$@" → N palavras; "$*" → 1 (IFS[0], default espaco); $@ / $* sem aspas → palavras.
 * Redirs: só ~/$VAR/$n (sem glob, sem splice $@).
 */
void expand_cmd_argv(petrush_cmd_t *cmd);

#endif /* PETRUSH_EXPAND_H */
