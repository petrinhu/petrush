/*
 * source.h - Builtin source / . (UX-22) + script mode (OSH-0)
 *
 * Roda arquivo linha a linha no processo atual.
 * Mesma fn para "source" e ".". Sem PATH search.
 * Teto PETRUSH_SOURCE_MAX_DEPTH. Sem $1/return (posicionais fora de OSH-0).
 */

#ifndef PETRUSH_SOURCE_H
#define PETRUSH_SOURCE_H

#include "petrush/parser.h"

#define PETRUSH_SOURCE_MAX_DEPTH 8

/*
 * Executa path no shell atual (alias + parse_list + dispatch_list).
 * missing_ok != 0: arquivo ausente nao e erro (boot ~/.petrushrc).
 * fopen + fstat + petrush_rc_stat_ok (SEC-10). Sem busca em PATH.
 * Retorna status shell (0 ok).
 */
int petrush_source_file(const char *path, int missing_ok);

/*
 * OSH-0: modo script (argv[1] / shebang).
 * Mesmo runner linha a linha que source; SEM SEC-10 mode&0022 / uid.
 * Recusa nao-regular. Ausente → 127. Legivel via fopen.
 * Posicionais $1..$n fora desta fatia (ignorados pelo main).
 */
int petrush_run_script(const char *path);

/* Builtin: exige argc == 2 (nome + arquivo). */
int builtin_source(petrush_cmd_t *cmd);

#endif /* PETRUSH_SOURCE_H */
