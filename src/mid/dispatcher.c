/*
 * dispatcher.c — Implementação do dispatcher de comandos
 */

#include "petrush/dispatcher.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const builtin_entry_t builtins[] = {
    { "cd",   builtin_cd   },
    { "pwd",  builtin_pwd  },
    { "echo", builtin_echo },
    { "exit", builtin_exit },
    { "help", builtin_help },
    { NULL,   NULL         }   /* sentinela */
};

int dispatch_command(petrush_cmd_t *cmd)
{
    if (!cmd || cmd->argc == 0 || !cmd->argv[0]) {
        return 0;
    }

    const char *name = cmd->argv[0];

    for (int i = 0; builtins[i].name != NULL; i++) {
        if (strcmp(name, builtins[i].name) == 0) {
            return builtins[i].fn(cmd);
        }
    }

    /* Comando desconhecido — por enquanto só avisa */
    fprintf(stderr, "petrush: comando não encontrado: %s\n", name);
    return 1;
}

/* ===================== BUILTINS BÁSICOS ===================== */

int builtin_cd(petrush_cmd_t *cmd)
{
    const char *path = (cmd->argc > 1) ? cmd->argv[1] : getenv("HOME");

    if (!path) {
        path = ".";
    }

    if (chdir(path) != 0) {
        perror("cd");
        return 1;
    }

    return 0;
}

int builtin_pwd(petrush_cmd_t *cmd)
{
    (void)cmd;

    char cwd[4096];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd);
        return 0;
    } else {
        perror("pwd");
        return 1;
    }
}

int builtin_echo(petrush_cmd_t *cmd)
{
    for (int i = 1; i < cmd->argc; i++) {
        if (i > 1) printf(" ");
        printf("%s", cmd->argv[i]);
    }
    printf("\n");
    return 0;
}

int builtin_exit(petrush_cmd_t *cmd)
{
    (void)cmd;
    exit(0);
    return 0; /* nunca chega aqui */
}

int builtin_help(petrush_cmd_t *cmd)
{
    (void)cmd;

    printf("PetRush — Shell interativo em C\n\n");
    printf("Comandos embutidos disponíveis:\n");
    printf("  cd [dir]     - Muda de diretório\n");
    printf("  pwd          - Mostra diretório atual\n");
    printf("  echo [args]  - Imprime argumentos\n");
    printf("  exit         - Sai do shell\n");
    printf("  help         - Mostra esta ajuda\n");
    printf("\n");
    printf("Comandos externos são executados via PATH.\n");
    return 0;
}
