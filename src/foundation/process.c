/*
 * process.c — Execução de processos externos
 */

#include "petrush/process.h"
#include "petrush/env.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <termios.h>

/* Retorna o nome legível de um sinal (com fallback seguro) */
static const char *signal_name(int sig)
{
    switch (sig) {
        case SIGINT:  return "SIGINT";
        case SIGQUIT: return "SIGQUIT";
        case SIGTERM: return "SIGTERM";
        case SIGKILL: return "SIGKILL";
        case SIGSTOP: return "SIGSTOP";
        case SIGTSTP: return "SIGTSTP";
        case SIGSEGV: return "SIGSEGV";
        case SIGABRT: return "SIGABRT";
        case SIGPIPE: return "SIGPIPE";
        default:      return "desconhecido";
    }
}

/* Estado do terminal salvo pelo shell (para restauração após jobs que alteram raw/cbreak) */
static struct termios shell_termios;
static int shell_termios_saved = 0;

/* Dá o terminal para o processo filho (job control básico) */
static void give_terminal_to(pid_t pid)
{
    /* Ignora SIGTTOU temporariamente para não receber sinal ao mudar controlling terminal */
    struct sigaction old_ttou, ign;
    memset(&ign, 0, sizeof(ign));
    ign.sa_handler = SIG_IGN;
    sigaction(SIGTTOU, &ign, &old_ttou);

    tcsetpgrp(STDIN_FILENO, pid);

    sigaction(SIGTTOU, &old_ttou, NULL);
}

/* Devolve o terminal para o shell + restaura atributos (termios) do shell */
static void take_terminal_back(void)
{
    struct sigaction old_ttou, ign;
    memset(&ign, 0, sizeof(ign));
    ign.sa_handler = SIG_IGN;
    sigaction(SIGTTOU, &ign, &old_ttou);

    tcsetpgrp(STDIN_FILENO, getpid());

    /* Restaura o modo de terminal que o shell tinha no início (PR-06.10).
     * Isso desfaz mudanças feitas por editores/paginadores (vim, less, etc.). */
    if (shell_termios_saved) {
        tcsetattr(STDIN_FILENO, TCSADRAIN, &shell_termios);
    }

    sigaction(SIGTTOU, &old_ttou, NULL);
}

/* API pública — chamada uma vez pelo REPL no startup */
void petrush_init_shell_termios(void)
{
    if (tcgetattr(STDIN_FILENO, &shell_termios) == 0) {
        shell_termios_saved = 1;
    }
    /* Se tcgetattr falhar (ex.: não é tty), simplesmente não restauramos — seguro. */
}

static char *find_executable(const char *name)
{
    if (!name) return NULL;

    /* Se já tem /, é caminho relativo ou absoluto */
    if (strchr(name, '/')) {
        if (access(name, X_OK) == 0) {
            return strdup(name);
        }
        return NULL;
    }

    const char *path_env = petrush_getenv("PATH");
    if (!path_env) path_env = "/bin:/usr/bin";

    char *path_copy = strdup(path_env);
    if (!path_copy) return NULL;

    char *dir = strtok(path_copy, ":");
    char fullpath[PATH_MAX];

    while (dir) {
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dir, name);
        if (access(fullpath, X_OK) == 0) {
            free(path_copy);
            return strdup(fullpath);
        }
        dir = strtok(NULL, ":");
    }

    free(path_copy);
    return NULL;
}

/* Mapeia falha de find_executable para o código tradicional de shell:
 * 127 = comando não encontrado
 * 126 = encontrado mas sem permissão de execução (para caminhos explícitos)
 */
static int shell_error_code_for(const char *name)
{
    if (!name) return 127;

    if (strchr(name, '/')) {
        /* Caminho explícito fornecido pelo usuário */
        if (access(name, F_OK) == 0) {
            return 126; /* arquivo existe, mas não é executável */
        }
        return 127;
    }

    /* Busca via PATH: não achamos nada → não encontrado */
    return 127;
}

int execute_external(petrush_cmd_t *cmd, int *exit_status)
{
    if (!cmd || cmd->argc == 0 || !cmd->argv[0]) {
        return -1;
    }

    char *exe_path = find_executable(cmd->argv[0]);
    if (!exe_path) {
        int errcode = shell_error_code_for(cmd->argv[0]);
        if (errcode == 126) {
            fprintf(stderr, "petrush: permissão negada: %s\n", cmd->argv[0]);
        } else {
            fprintf(stderr, "petrush: comando não encontrado: %s\n", cmd->argv[0]);
        }
        if (exit_status) {
            *exit_status = errcode;
        }
        return -1;
    }

    /* Salva handlers antigos de sinais que o shell não deve receber enquanto o filho roda */
    struct sigaction old_int, old_quit;
    struct sigaction new_act;
    memset(&new_act, 0, sizeof(new_act));
    new_act.sa_handler = SIG_IGN;

    sigaction(SIGINT,  &new_act, &old_int);
    sigaction(SIGQUIT, &new_act, &old_quit);

    /* Bloqueia sinais brevemente durante o fork para evitar race conditions */
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGQUIT);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);

    pid_t pid = fork();

    /* Restaura máscara imediatamente após o fork */
    sigprocmask(SIG_SETMASK, &oldmask, NULL);

    if (pid < 0) {
        perror("fork");

        /* Restaura handlers em caso de erro no fork */
        sigaction(SIGINT,  &old_int, NULL);
        sigaction(SIGQUIT, &old_quit, NULL);

        free(exe_path);
        return -1;
    }

    /* Coloca o filho em seu próprio process group (básico de job control) */
    setpgid(pid, pid);

    /* Dá o terminal para o processo filho */
    give_terminal_to(pid);

    if (pid == 0) {
        /* Processo filho - restaura handlers padrão */
        signal(SIGINT,  SIG_DFL);
        signal(SIGQUIT, SIG_DFL);

        execv(exe_path, cmd->argv);
        int exec_errno = errno;
        perror("execv");
        _exit((exec_errno == EACCES || exec_errno == EPERM) ? 126 : 127);
    } else {
        /* Processo pai */
        int status;
        if (waitpid(pid, &status, WUNTRACED) == -1) {
            perror("waitpid");
        }

        /* Devolve o terminal para o shell */
        take_terminal_back();

        /* Restaura os handlers antigos */
        sigaction(SIGINT,  &old_int, NULL);
        sigaction(SIGQUIT, &old_quit, NULL);

        free(exe_path);

        if (exit_status) {
            if (WIFEXITED(status)) {
                *exit_status = WEXITSTATUS(status);
            } else if (WIFSIGNALED(status)) {
                *exit_status = 128 + WTERMSIG(status);
            } else if (WIFSTOPPED(status)) {
                *exit_status = 128 + WSTOPSIG(status);
            } else {
                *exit_status = 1;
            }
        }

        /* Relatório amigável de terminação */
        if (WIFSIGNALED(status)) {
            int sig = WTERMSIG(status);
            const char *name = signal_name(sig);
            fprintf(stderr, "petrush: %s terminado por sinal %d (%s)\n",
                    cmd->argv[0], sig, name);
        } else if (WIFSTOPPED(status)) {
            int sig = WSTOPSIG(status);
            fprintf(stderr, "petrush: %s parado por sinal %d (%s)\n",
                    cmd->argv[0], sig, signal_name(sig));
        }

        return 0;
    }
}
