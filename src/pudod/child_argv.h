/*
 * child_argv.h - Montagem fail-closed do argv do filho (SEC-04)
 *
 * Se argc exceder PUDOD_MAX_ARGS, recusar (nunca truncar em silencio).
 */

#ifndef PETRUSH_PUDOD_CHILD_ARGV_H
#define PETRUSH_PUDOD_CHILD_ARGV_H

#include <stddef.h>

#define PUDOD_MAX_ARGS 128

/*
 * Monta out[] = { resolved, pudod_argv[2], ..., NULL }.
 * pudod_argc inclui argv[0] (pudod) e argv[1] (comando).
 * Fail closed: se (pudod_argc - 2) > PUDOD_MAX_ARGS, retorna -1
 * sem entregar argv truncado.
 * out_cap deve ser >= PUDOD_MAX_ARGS + 2.
 * Retorna 0 em sucesso.
 */
int pudod_build_child_argv(char *const pudod_argv[], int pudod_argc,
                           const char *resolved,
                           char *out[], size_t out_cap);

#endif /* PETRUSH_PUDOD_CHILD_ARGV_H */
