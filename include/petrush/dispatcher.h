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

/* Despacha o comando. Retorna status de saída (convenção shell). */
int dispatch_command(petrush_cmd_t *cmd);

/* Despacha pipeline (pipes + redirs). Retorna status do último estágio. */
int dispatch_pipeline(petrush_pipeline_t *pl);

/* Despacha lista && / || / ; (ALWAYS sem short-circuit; &&/|| com). */
int dispatch_list(petrush_list_t *list);

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

/* ASM-WAI: inventario sysfs/proc (flags -disk … -board) */
int builtin_wai(petrush_cmd_t *cmd);

/* NEW-22 UX */
int builtin_alias(petrush_cmd_t *cmd);
int builtin_unalias(petrush_cmd_t *cmd);
int builtin_which(petrush_cmd_t *cmd);
int builtin_pushd(petrush_cmd_t *cmd);
int builtin_popd(petrush_cmd_t *cmd);
int builtin_dirs(petrush_cmd_t *cmd);

/* UX-23: lista jobs em background */
int builtin_jobs(petrush_cmd_t *cmd);

/* FEAT-TRUE: true / : → 0; false → 1; silent no-op */
int builtin_true(petrush_cmd_t *cmd);
int builtin_false(petrush_cmd_t *cmd);

/* FEAT-UMASK: print/set máscara octal do processo do shell (sem -S) */
int builtin_umask(petrush_cmd_t *cmd);

/* FEAT-READ: read NAME — 1 linha de stdin → 1 variável (sem -a/-d/timeout/IFS) */
int builtin_read(petrush_cmd_t *cmd);

/* FEAT-TEST: test / [ primaries curtos (-f -d -e -z -n = != -eq -ne -lt -gt); [ exige ] */
int builtin_test(petrush_cmd_t *cmd);

/* Para completion: número de builtins e nome por índice */
int petrush_builtin_count(void);
const char *petrush_builtin_name(int index);

#endif /* PETRUSH_DISPATCHER_H */
