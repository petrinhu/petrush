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
 * Salva o estado atual do terminal (termios) como sendo o estado "limpo" do shell.
 * Deve ser chamado uma única vez no início do REPL (antes de qualquer execução
 * de comando externo). Usado por take_terminal_back() para restaurar modos
 * (echo, canonical, etc.) que jobs interativos (vim, less...) possam ter alterado.
 */
void petrush_init_shell_termios(void);

#endif /* PETRUSH_PROCESS_H */
