/*
 * process.c — Execução de processos externos
 */

#include "petrush/process.h"
#include "petrush/env.h"
#ifdef PETRUSH_HAVE_ASM
#include "petrush/asm.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <termios.h>

/* ASM-PGID: wrapper setpgid; fallback libc se PETRUSH_ASM=OFF. */
static int petrush_setpgid(pid_t pid, pid_t pgid)
{
#ifdef PETRUSH_HAVE_ASM
    return petrush_job_setpgid(pid, pgid);
#else
    return setpgid(pid, pgid);
#endif
}

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

/* Aplica redirecionamentos no processo atual (filho). Retorna 0 ok, -1 erro.
 * Ordem: stdin → stdout arquivo → stderr (path OU merge).
 * Assim `> file 2>&1` e `&> file` mandam os dois para o arquivo.
 * `2>&1 > file` NÃO replica bash (sem lista ordenada de ops). */
static int apply_redirs(const petrush_cmd_t *cmd)
{
    if (!cmd) return -1;

    if (cmd->redir_in) {
        int fd = open(cmd->redir_in, O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "petrush: não foi possível abrir '%s' para leitura: %s\n",
                    cmd->redir_in, strerror(errno));
            return -1;
        }
        if (dup2(fd, STDIN_FILENO) < 0) {
            close(fd);
            perror("petrush: dup2 stdin");
            return -1;
        }
        close(fd);
    }

    if (cmd->redir_out) {
        /* SEC-09: `>` usa O_EXCL (noclobber); `>>` continua O_APPEND. */
        int flags = O_WRONLY | O_CREAT | (cmd->redir_append ? O_APPEND : O_EXCL);
        int fd = open(cmd->redir_out, flags, 0644);
        if (fd < 0) {
            fprintf(stderr, "petrush: não foi possível abrir '%s' para escrita: %s\n",
                    cmd->redir_out, strerror(errno));
            return -1;
        }
        if (dup2(fd, STDOUT_FILENO) < 0) {
            close(fd);
            perror("petrush: dup2 stdout");
            return -1;
        }
        close(fd);
    }

    if (cmd->redir_err) {
        /* SEC-09: `2>` usa O_EXCL; `2>>` continua O_APPEND. */
        int flags = O_WRONLY | O_CREAT | (cmd->redir_err_append ? O_APPEND : O_EXCL);
        int fd = open(cmd->redir_err, flags, 0644);
        if (fd < 0) {
            fprintf(stderr, "petrush: não foi possível abrir '%s' para escrita: %s\n",
                    cmd->redir_err, strerror(errno));
            return -1;
        }
        if (dup2(fd, STDERR_FILENO) < 0) {
            close(fd);
            perror("petrush: dup2 stderr");
            return -1;
        }
        close(fd);
    } else if (cmd->redir_err_to_out) {
        if (dup2(STDOUT_FILENO, STDERR_FILENO) < 0) {
            perror("petrush: dup2 stderr→stdout");
            return -1;
        }
    }
    return 0;
}

static int status_to_code(int status)
{
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    if (WIFSTOPPED(status)) return 128 + WSTOPSIG(status);
    return 1;
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
    (void)petrush_setpgid(pid, pid);

    /* Dá o terminal para o processo filho */
    give_terminal_to(pid);

    if (pid == 0) {
        /* Processo filho - restaura handlers padrão */
        signal(SIGINT,  SIG_DFL);
        signal(SIGQUIT, SIG_DFL);

        if (apply_redirs(cmd) != 0) {
            _exit(1);
        }

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
            *exit_status = status_to_code(status);
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

static void pipeline_close_pipes(int (*pipes)[2], int n_pipes)
{
    for (int j = 0; j < n_pipes; j++) {
        close(pipes[j][0]);
        close(pipes[j][1]);
    }
}

static void pipeline_restore_signals(const struct sigaction *old_int,
                                     const struct sigaction *old_quit)
{
    sigaction(SIGINT, old_int, NULL);
    sigaction(SIGQUIT, old_quit, NULL);
}

/* Fecha pipes, mata n_kill filhos (0 = nenhum), libera e restaura sinais. */
static void pipeline_abort(int (*pipes)[2], int n_pipes, pid_t *pids, int n_kill,
                           const struct sigaction *old_int,
                           const struct sigaction *old_quit)
{
    pipeline_close_pipes(pipes, n_pipes);
    for (int j = 0; j < n_kill; j++) {
        if (pids[j] > 0) {
            kill(pids[j], SIGTERM);
            waitpid(pids[j], NULL, 0);
        }
    }
    free(pipes);
    free(pids);
    pipeline_restore_signals(old_int, old_quit);
}

_Noreturn static void pipeline_child(petrush_cmd_t *cmd, int i, int n,
                                     int (*pipes)[2], pid_t pgid, char *exe_path,
                                     pipeline_child_hook_t hook)
{
    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);

    if (i == 0) {
        (void)petrush_setpgid(0, 0);
    } else {
        (void)petrush_setpgid(0, pgid);
    }

    if (i > 0) {
        if (dup2(pipes[i - 1][0], STDIN_FILENO) < 0) {
            _exit(1);
        }
    }
    if (i < n - 1) {
        if (dup2(pipes[i][1], STDOUT_FILENO) < 0) {
            _exit(1);
        }
    }

    pipeline_close_pipes(pipes, n - 1);

    if (apply_redirs(cmd) != 0) {
        /* exe_path: ownership no pai (ou heap do filho morre no _exit). */
        _exit(1);
    }

    /* UX-19: builtin no filho (0=handled/_exit; 1=seguir execv) */
    if (hook) {
        int h = hook(cmd);
        if (h == 0) {
            _exit(0);
        }
    }

    if (!exe_path) {
        exe_path = find_executable(cmd->argv[0]);
        if (!exe_path) {
            fprintf(stderr, "petrush: comando não encontrado: %s\n",
                    cmd->argv[0]);
            _exit(shell_error_code_for(cmd->argv[0]));
        }
    }

    execv(exe_path, cmd->argv);
    perror("execv");
    _exit(127);
}

int execute_pipeline(petrush_pipeline_t *pl, int *exit_status)
{
    return execute_pipeline_with_hook(pl, exit_status, NULL);
}

int execute_pipeline_with_hook(petrush_pipeline_t *pl, int *exit_status,
                               pipeline_child_hook_t hook)
{
    if (!pl || pl->ncmds <= 0) {
        return -1;
    }

    /* Um estágio: reutiliza execute_external (com redirs); hook ignorado */
    if (pl->ncmds == 1) {
        return execute_external(&pl->cmds[0], exit_status);
    }

    const int n = pl->ncmds;
    int (*pipes)[2] = calloc((size_t)(n - 1), sizeof(int[2]));
    pid_t *pids = calloc((size_t)n, sizeof(pid_t));
    if (!pipes || !pids) {
        free(pipes);
        free(pids);
        return -1;
    }

    for (int i = 0; i < n - 1; i++) {
        if (pipe(pipes[i]) < 0) {
            perror("pipe");
            pipeline_close_pipes(pipes, i);
            free(pipes);
            free(pids);
            return -1;
        }
    }

    struct sigaction old_int, old_quit, new_act;
    memset(&new_act, 0, sizeof(new_act));
    new_act.sa_handler = SIG_IGN;
    sigaction(SIGINT, &new_act, &old_int);
    sigaction(SIGQUIT, &new_act, &old_quit);

    pid_t pgid = 0;

    for (int i = 0; i < n; i++) {
        petrush_cmd_t *cmd = &pl->cmds[i];
        /* Sem hook: resolve PATH no pai (contrato test_process intacto).
         * Com hook: find_builtin ANTES de PATH, no filho. */
        char *exe_path = NULL;
        if (!hook) {
            exe_path = find_executable(cmd->argv[0]);
            if (!exe_path) {
                int errcode = shell_error_code_for(cmd->argv[0]);
                fprintf(stderr, "petrush: comando não encontrado: %s\n",
                        cmd->argv[0]);
                pipeline_abort(pipes, n - 1, pids, i, &old_int, &old_quit);
                if (exit_status) {
                    *exit_status = errcode;
                }
                return -1;
            }
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            free(exe_path);
            exe_path = NULL;
            pipeline_abort(pipes, n - 1, pids, 0, &old_int, &old_quit);
            return -1;
        }

        if (pid == 0) {
            pipeline_child(cmd, i, n, pipes, pgid, exe_path, hook);
        }

        /* pai: unico free de exe_path (filho nao libera; espelha execute_external) */
        if (i == 0) {
            pgid = pid;
            (void)petrush_setpgid(pid, pgid);
            give_terminal_to(pgid);
        } else {
            (void)petrush_setpgid(pid, pgid);
        }
        pids[i] = pid;
        free(exe_path);
        exe_path = NULL;
    }

    pipeline_close_pipes(pipes, n - 1);

    int last_status = 0;
    for (int i = 0; i < n; i++) {
        int st = 0;
        if (waitpid(pids[i], &st, WUNTRACED) == -1) {
            perror("waitpid");
        }
        if (i == n - 1) {
            last_status = st;
        }
    }

    take_terminal_back();
    pipeline_restore_signals(&old_int, &old_quit);

    free(pipes);
    free(pids);

    if (exit_status) {
        *exit_status = status_to_code(last_status);
    }
    return 0;
}
