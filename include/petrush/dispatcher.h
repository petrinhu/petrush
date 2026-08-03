/*
 * dispatcher.h — Dispatcher de comandos (tabela de builtins)
 */

#ifndef PETRUSH_DISPATCHER_H
#define PETRUSH_DISPATCHER_H

#include "petrush/parser.h"

typedef int (*builtin_fn_t)(petrush_cmd_t *cmd);

/* Estrutura da tabela de builtins */
typedef struct {
    const char *name;
    builtin_fn_t fn;
} builtin_entry_t;

/* Despacha o comando. Retorna 0 se executado, 1 se comando desconhecido, -1 erro */
int dispatch_command(petrush_cmd_t *cmd);

/* Funções de builtins (implementadas em src/back ou aqui por enquanto) */
int builtin_cd(petrush_cmd_t *cmd);
int builtin_pwd(petrush_cmd_t *cmd);
int builtin_echo(petrush_cmd_t *cmd);
int builtin_exit(petrush_cmd_t *cmd);
int builtin_help(petrush_cmd_t *cmd);

/* Novos builtins PR-08 */
int builtin_clear(petrush_cmd_t *cmd);
int builtin_env(petrush_cmd_t *cmd);
int builtin_export(petrush_cmd_t *cmd);
int builtin_unset(petrush_cmd_t *cmd);
int builtin_history(petrush_cmd_t *cmd);

/* FEAT-02: pudo - sudo-like builtin (segurança extrema)
 * Declaração em include/petrush/pudo.h (única fonte da verdade) */

/* Onda 3 placeholder */
int builtin_info(petrush_cmd_t *cmd);

#endif /* PETRUSH_DISPATCHER_H */
