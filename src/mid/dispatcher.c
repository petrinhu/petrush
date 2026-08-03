/*
 * dispatcher.c — Implementação do dispatcher de comandos
 */

#include "petrush/dispatcher.h"
#include "petrush/petrush.h"
#include "petrush/process.h"
#include "petrush/env.h"
#include "petrush/pudo.h"

#include "linenoise.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

static const builtin_entry_t builtins[] = {
    { "cd",      builtin_cd      },
    { "pwd",     builtin_pwd     },
    { "echo",    builtin_echo    },
    { "exit",    builtin_exit    },
    { "help",    builtin_help    },
    { "clear",   builtin_clear   },
    { "env",     builtin_env     },
    { "export",  builtin_export  },
    { "unset",   builtin_unset   },
    { "history", builtin_history },
    { "pudo",    builtin_pudo    },
    { "info",    builtin_info    },  /* Onda 3 placeholder - diagnóstico básico */
    { NULL,      NULL            }   /* sentinela */
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

    /* Não é builtin → tenta executar como comando externo */
    int status = 0;
    if (execute_external(cmd, &status) == 0) {
        /* execute_external já normaliza para convenção de shell:
         * 0-255 para saídas normais, 128+sig para terminação por sinal.
         * Não aplicar WEXITSTATUS aqui (status já não é um valor raw de waitpid). */
        return status;
    }

    /* execute_external retornou -1 (falha antes do fork ou find).
     * Para casos de "não encontrado" / "permissão negada" já setamos o código
     * correto (127 ou 126) em *status. Usamos ele quando disponível. */
    return (status != 0) ? status : 127;
}

/* ===================== BUILTINS BÁSICOS ===================== */

int builtin_cd(petrush_cmd_t *cmd)
{
    const char *path = (cmd->argc > 1) ? cmd->argv[1] : petrush_getenv("HOME");

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
    }
    perror("pwd");
    return 1;
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
    printf("  clear        - Limpa a tela\n");
    printf("  env          - Lista variáveis de ambiente\n");
    printf("  export       - Exporta variável (export VAR ou export VAR=valor)\n");
    printf("  unset        - Remove variável de ambiente\n");
    printf("  history      - Mostra histórico de comandos\n");
    printf("  info         - Mostra informações do shell (diagnóstico básico - Onda 3)\n");
    printf("\n");
    printf("Comandos externos são executados via PATH.\n");
    return 0;
}

int builtin_clear(petrush_cmd_t *cmd)
{
    (void)cmd;
    linenoiseClearScreen();
    return 0;
}

int builtin_env(petrush_cmd_t *cmd)
{
    (void)cmd;

    extern char **environ;

    for (char **env = environ; *env != NULL; env++) {
        printf("%s\n", *env);
    }
    return 0;
}

int builtin_export(petrush_cmd_t *cmd)
{
    if (cmd->argc < 2) {
        fprintf(stderr, "export: usage: export NAME[=VALUE]...\n");
        return 1;
    }

    int ret = 0;

    for (int i = 1; i < cmd->argc; i++) {
        const char *arg = cmd->argv[i];
        const char *eq = strchr(arg, '=');

        if (eq) {
            /* NAME=VALUE form */
            size_t name_len = (size_t)(eq - arg);
            char name[256];
            if (name_len >= sizeof(name)) {
                fprintf(stderr, "export: variable name too long\n");
                ret = 1;
                continue;
            }
            memcpy(name, arg, name_len);
            name[name_len] = '\0';

            if (petrush_setenv(name, eq + 1, 1) != 0) {
                perror("export");
                ret = 1;
            }
        } else {
            /* Just NAME — if it already exists in env, re-export it (noop in our model) */
            const char *val = petrush_getenv(arg);
            if (!val) {
                /* In a more advanced shell we would have shell-local vars */
                fprintf(stderr, "export: %s: not found (no shell-local variables yet)\n", arg);
                ret = 1;
            } else {
                /* Already in environment — nothing to do */
            }
        }
    }

    return ret;
}

int builtin_unset(petrush_cmd_t *cmd)
{
    if (cmd->argc < 2) {
        fprintf(stderr, "unset: usage: unset NAME...\n");
        return 1;
    }

    int ret = 0;

    for (int i = 1; i < cmd->argc; i++) {
        if (petrush_unsetenv(cmd->argv[i]) != 0) {
            /* unsetenv usually succeeds even if var didn't exist */
            /* We only error on invalid name */
            if (errno == EINVAL) {
                fprintf(stderr, "unset: %s: invalid variable name\n", cmd->argv[i]);
                ret = 1;
            }
        }
    }

    return ret;
}

int builtin_history(petrush_cmd_t *cmd)
{
    (void)cmd;

    const char *home = petrush_getenv("HOME");
    char path[4096];

    if (home && *home) {
        snprintf(path, sizeof(path), "%s/.petrush_history", home);
    } else {
        snprintf(path, sizeof(path), ".petrush_history");
    }

    FILE *f = fopen(path, "r");
    if (!f) {
        printf("No history yet.\n");
        return 0;
    }

    char line[4096];
    int lineno = 1;

    while (fgets(line, sizeof(line), f)) {
        /* Remove trailing newline for nicer output */
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') {
            line[len-1] = '\0';
        }
        printf("%5d  %s\n", lineno++, line);
    }

    fclose(f);
    return 0;
}

/* Onda 3: builtin diagnóstico básico (placeholder, só ativa com demanda/Caio) */
int builtin_info(petrush_cmd_t *cmd)
{
    (void)cmd;
    printf("petrush %s\n", PETRUSH_VERSION);
    printf("C23 REPL shell\n");
    printf("Build: %s %s\n", __DATE__, __TIME__);
    printf("Features: history, rc, signals, pudo (helper)\n");
    printf("Anti-OE: sem pipes/redir/scripting no MVP\n");
    return 0;
}
