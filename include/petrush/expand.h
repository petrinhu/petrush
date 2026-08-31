/*
 * expand.h — ~ / $VAR / ${VAR:-} / ${VAR:+} / ${#VAR} (UX-12/13, FEAT-PARAM)
 * + glob * ? (UX-18)
 * + posicionais $0 $1.. $# $@ $* (OSH-1)
 * + shift [n] (OSH-2; sem ${1:-} / arrays)
 * + cmdsubst $(cmd) via hook DIP (OSH-9; sem backticks)
 * + arith $((expr)) int64 (OSH-11; + - * / % ( ) unary)
 * + $? / $- / shellopt e|u|x (OSH-16; -e/-u honrados depois)
 */

#ifndef PETRUSH_EXPAND_H
#define PETRUSH_EXPAND_H

#include "petrush/parser.h"

#define PETRUSH_GLOB_MAX 256

/*
 * OSH-9: hook DIP para $(cmd). Recebe o texto interno (sem delimitadores).
 * Retorna malloc (stdout capturado) ou NULL (OOM / falha → expand emite "").
 * Hook NULL → '$' literal (nao consome o '(').
 * Trailing \\n sao stripados no expand, nao no hook.
 */
typedef char *(*petrush_cmdsubst_hook_t)(const char *inner_cmd);
void petrush_set_cmdsubst_hook(petrush_cmdsubst_hook_t hook);

/*
 * OSH-11: 1 se a ultima expansao aritmetica teve div/mod por zero.
 * Consome e limpa o flag (take). Dispatcher usa para status != 0.
 */
int petrush_take_arith_error(void);

/*
 * OSH-16: flags de set (-e/-u/-x). C (noclobber) always-on em $-; nao e bit.
 * set/get: so 'e'|'u'|'x'; outro → !=0. flags(): static "C" + eux ligados.
 */
int petrush_shellopt_set(char flag, int on);
int petrush_shellopt_get(char flag);
const char *petrush_shellopt_flags(void);
void petrush_last_status_set(int status);
int petrush_last_status(void);
/* Zera e/u/x e $?; nao desliga noclobber (C). */
void petrush_shellopt_reset_for_tests(void);

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

/*
 * OSH-2: desloca $1..$# em n posicoes; $0 intacto.
 * n==0 → no-op, retorna 0.
 * n > $# → retorna -1 e nao altera posicionais.
 * senao retorna 0.
 */
int petrush_positional_shift(unsigned n);

/* Expande uma palavra (malloc). Nunca retorna NULL se word != NULL (exceto OOM). */
char *expand_word(const char *word);

/*
 * OSH-14: expand $ / $( ) / $(( )) no corpo here-doc unquoted.
 * Sem tilde, sem field split, sem pathname. \ escapa $, `, \ e newline.
 * " nao e especial no corpo. malloc; NULL so se body==NULL ou OOM.
 */
char *expand_heredoc_body(const char *body);

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
