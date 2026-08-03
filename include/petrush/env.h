/*
 * env.h — Gerenciamento básico de variáveis de ambiente (Foundation layer)
 *
 * Parte de PR-07 (ENV+RC). Fornece wrappers finos sobre setenv/getenv/unsetenv
 * com semântica clara para o shell. Prepara o terreno para os builtins de PR-08
 * (export, unset, env) sem inflar o MVP.
 */

#ifndef PETRUSH_ENV_H
#define PETRUSH_ENV_H

/*
 * Retorna o valor da variável de ambiente ou NULL se não existir.
 */
const char *petrush_getenv(const char *name);

/*
 * Define (ou redefine) uma variável de ambiente.
 * overwrite: se != 0, sobrescreve o valor existente.
 *
 * Retorna 0 em sucesso, -1 em erro.
 */
int petrush_setenv(const char *name, const char *value, int overwrite);

/*
 * Remove uma variável de ambiente.
 * Retorna 0 em sucesso, -1 se a variável não existia ou erro.
 */
int petrush_unsetenv(const char *name);

#endif /* PETRUSH_ENV_H */
