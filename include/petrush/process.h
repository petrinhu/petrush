/*
 * process.h — Execução de processos externos (Foundation layer)
 */

#ifndef PETRUSH_PROCESS_H
#define PETRUSH_PROCESS_H

#include "petrush/parser.h"

/*
 * Executa um comando externo via fork + execvp + waitpid.
 *
 * Busca o executável no PATH se não for um caminho absoluto/relativo.
 *
 * Parâmetros:
 *   cmd         - comando já parseado (argv/argc)
 *   exit_status - ponteiro onde será armazenado o status de saída
 *
 * Retorno:
 *    0  -> sucesso (processo executado, status em *exit_status)
 *   -1  -> falha (comando não encontrado, permissão negada, etc.)
 */
int execute_external(petrush_cmd_t *cmd, int *exit_status);

/*
 * OSH-13: aplica redirecionamentos no processo atual (stdin/out/err).
 * Here-doc via memfd (fallback /dev/shm); path `<` via open.
 * Retorna 0 ok, -1 erro. Usado por execute_* e builtins (Mid).
 */
int petrush_apply_redirs(const petrush_cmd_t *cmd);

/*
 * Hook opcional no filho (após pipes + apply_redirs):
 *   0 = handled (builtin; hook deve fflush + _exit)
 *   1 = não handled → find_executable + execv
 * process.h não inclui dispatcher.h (Foundation não conhece Mid).
 */
typedef int (*pipeline_child_hook_t)(petrush_cmd_t *cmd);

/*
 * Executa um pipeline (1+ estágios). Estágios externos via fork/exec;
 * redirecionamentos aplicados no filho. Para 1 estágio, equivalente a
 * execute_external com redirs. Retorna 0 se o pipeline rodou; status
 * do último estágio em *exit_status (convenção shell).
 * Equivale a execute_pipeline_with_hook(..., NULL).
 */
int execute_pipeline(petrush_pipeline_t *pl, int *exit_status);

/*
 * Como execute_pipeline; se hook != NULL (ncmds>=2), cada filho chama
 * hook antes do PATH. find_executable só ocorre se hook retornar 1.
 */
int execute_pipeline_with_hook(petrush_pipeline_t *pl, int *exit_status,
                               pipeline_child_hook_t hook);

/*
 * Salva o estado atual do terminal (termios) como sendo o estado "limpo" do shell.
 * Deve ser chamado uma única vez no início do REPL (antes de qualquer execução
 * de comando externo). Usado por take_terminal_back() para restaurar modos
 * (echo, canonical, etc.) que jobs interativos (vim, less...) possam ter alterado.
 */
void petrush_init_shell_termios(void);

#endif /* PETRUSH_PROCESS_H */
