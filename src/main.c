/*
 * petrush - Shell interativo em C23
 * Opção A escolhida (REPL clássico)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "petrush/petrush.h"
#include "petrush/parser.h"
#include "petrush/dispatcher.h"
#include "petrush/process.h"
#include "linenoise.h"

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    printf("%s %s (C23 shell)\n", PETRUSH_NAME, PETRUSH_VERSION);
    printf("Digite 'exit' ou 'quit' para sair.\n\n");

    /* Salva o estado original do terminal para restauração após comandos externos
     * que modificam o termios (vim, less, etc.). Essencial para job control correto. */
    petrush_init_shell_termios();

    linenoiseHistoryLoad(""); /* desabilita load por enquanto */

    char *line;
    while ((line = linenoise("petrush> ")) != NULL) {
        if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) {
            free(line);
            break;
        }

        if (strlen(line) > 0) {
            linenoiseHistoryAdd(line);

            petrush_cmd_t cmd = {0};
            if (petrush_parse(line, &cmd) == 0) {
                if (cmd.argc > 0) {
                    dispatch_command(&cmd);
                }
            } else {
                fprintf(stderr, "petrush: erro ao analisar comando\n");
            }
            petrush_cmd_free(&cmd);
        }

        free(line);
    }

    printf("saindo...\n");
    return EXIT_SUCCESS;
}
