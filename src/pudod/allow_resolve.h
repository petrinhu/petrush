/*
 * allow_resolve.h — Canonicalização de entrada da allow-list do pudod (SEC-05)
 *
 * Fail closed: realpath falhou => recusar a linha (nunca copiar o literal).
 */

#ifndef PETRUSH_PUDOD_ALLOW_RESOLVE_H
#define PETRUSH_PUDOD_ALLOW_RESOLVE_H

#include <stddef.h>

/*
 * Resolve entry via realpath para out.
 * Retorna 0 e grava caminho canônico em out (NUL-terminated) em sucesso.
 * Retorna -1 se entry/out inválidos ou se realpath falhar (não copia literal).
 */
int pudod_resolve_allow_entry(const char *entry, char *out, size_t outsz);

#endif /* PETRUSH_PUDOD_ALLOW_RESOLVE_H */
