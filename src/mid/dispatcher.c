/*
 * dispatcher.c — Implementação do dispatcher de comandos
 */

#include "petrush/dispatcher.h"
#include "petrush/petrush.h"
#include "petrush/process.h"
#include "petrush/job.h"
#include "petrush/env.h"
#include "petrush/pudo.h"
#include "petrush/alias.h"
#include "petrush/dirstack.h"
#include "petrush/expand.h"
#include "petrush/source.h"
#include "petrush/ui_port.h"
#ifdef PETRUSH_HAVE_ASM
#include "petrush/asm.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>

/* ASM-PGID: wrapper setpgid; fallback libc se PETRUSH_ASM=OFF. */
static int dispatcher_setpgid(pid_t pid, pid_t pgid)
{
#ifdef PETRUSH_HAVE_ASM
    return petrush_job_setpgid(pid, pgid);
#else
    return setpgid(pid, pgid);
#endif
}

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
    { "info",    builtin_info    },
    { "wai",     builtin_wai     }, /* ASM-WAI: inventario sysfs/proc */
    { "netcom",  builtin_netcom  }, /* ASM-NET: wifi/eth/bt + -up/-down */
    { "alias",   builtin_alias   },
    { "unalias", builtin_unalias },
    { "which",   builtin_which   },
    { "pushd",   builtin_pushd   },
    { "popd",    builtin_popd    },
    { "dirs",    builtin_dirs    },
    { "source",  builtin_source  },
    { ".",       builtin_source  },
    { "jobs",    builtin_jobs    },
    { "true",    builtin_true    },
    { "false",   builtin_false   },
    { ":",       builtin_true    }, /* FEAT-TRUE: null command */
    { "umask",   builtin_umask   }, /* FEAT-UMASK: máscara do shell */
    { "read",    builtin_read    }, /* FEAT-READ: 1 linha → 1 var */
    { "test",    builtin_test    }, /* FEAT-TEST: primaries curtos */
    { "[",       builtin_test    }, /* FEAT-TEST: [ exige ] final */
    { NULL,      NULL            }   /* sentinela */
};

int petrush_builtin_count(void)
{
    int n = 0;
    while (builtins[n].name != NULL) n++;
    return n;
}

const char *petrush_builtin_name(int index)
{
    int n = petrush_builtin_count();
    if (index < 0 || index >= n) return NULL;
    return builtins[index].name;
}

/* Restaura FDs salvos após falha parcial em run_builtin_with_redirs. */
static void restore_saved_fds(int saved_in, int saved_out, int saved_err)
{
    if (saved_in >= 0) {
        dup2(saved_in, STDIN_FILENO);
        close(saved_in);
    }
    if (saved_out >= 0) {
        dup2(saved_out, STDOUT_FILENO);
        close(saved_out);
    }
    if (saved_err >= 0) {
        dup2(saved_err, STDERR_FILENO);
        close(saved_err);
    }
}

/* Roda builtin no processo atual com redirecionamentos temporários.
 * Ordem: stdin → stdout arquivo → stderr (path OU merge). */
static int run_builtin_with_redirs(builtin_fn_t fn, petrush_cmd_t *cmd)
{
    int saved_in = -1, saved_out = -1, saved_err = -1;
    int rc;

    /* flush antes de trocar FDs — buffer do stdio (non-tty = fully buffered)
     * ainda aponta para o FD antigo e vaza no arquivo de redir. */
    fflush(stdout);
    fflush(stderr);

    if (cmd->redir_in) {
        saved_in = dup(STDIN_FILENO);
        int fd = open(cmd->redir_in, O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "petrush: não foi possível abrir '%s' para leitura: %s\n",
                    cmd->redir_in, strerror(errno));
            if (saved_in >= 0) close(saved_in);
            return 1;
        }
        if (dup2(fd, STDIN_FILENO) < 0) {
            close(fd);
            if (saved_in >= 0) close(saved_in);
            return 1;
        }
        close(fd);
    }

    if (cmd->redir_out) {
        saved_out = dup(STDOUT_FILENO);
        /* SEC-09: `>` usa O_EXCL (noclobber); `>>` continua O_APPEND. */
        int flags = O_WRONLY | O_CREAT | (cmd->redir_append ? O_APPEND : O_EXCL);
        int fd = open(cmd->redir_out, flags, 0644);
        if (fd < 0) {
            fprintf(stderr, "petrush: não foi possível abrir '%s' para escrita: %s\n",
                    cmd->redir_out, strerror(errno));
            restore_saved_fds(saved_in, saved_out, -1);
            return 1;
        }
        if (dup2(fd, STDOUT_FILENO) < 0) {
            close(fd);
            restore_saved_fds(saved_in, saved_out, -1);
            return 1;
        }
        close(fd);
    }

    if (cmd->redir_err || cmd->redir_err_to_out) {
        saved_err = dup(STDERR_FILENO);
        if (cmd->redir_err) {
            /* SEC-09: `2>` usa O_EXCL; `2>>` continua O_APPEND. */
            int flags = O_WRONLY | O_CREAT | (cmd->redir_err_append ? O_APPEND : O_EXCL);
            int fd = open(cmd->redir_err, flags, 0644);
            if (fd < 0) {
                fprintf(stderr, "petrush: não foi possível abrir '%s' para escrita: %s\n",
                        cmd->redir_err, strerror(errno));
                restore_saved_fds(saved_in, saved_out, saved_err);
                return 1;
            }
            if (dup2(fd, STDERR_FILENO) < 0) {
                close(fd);
                restore_saved_fds(saved_in, saved_out, saved_err);
                return 1;
            }
            close(fd);
        } else if (dup2(STDOUT_FILENO, STDERR_FILENO) < 0) {
            restore_saved_fds(saved_in, saved_out, saved_err);
            return 1;
        }
    }

    rc = fn(cmd);

    /* flush antes de restaurar FDs — senão o buffer do stdio
     * (pipe no smoke / non-tty) escreve no FD errado depois do dup2. */
    fflush(stdout);
    fflush(stderr);

    restore_saved_fds(saved_in, saved_out, saved_err);
    return rc;
}

static builtin_fn_t find_builtin(const char *name)
{
    if (!name) return NULL;
    for (int i = 0; builtins[i].name != NULL; i++) {
        if (strcmp(name, builtins[i].name) == 0) {
            return builtins[i].fn;
        }
    }
    return NULL;
}

int dispatch_command(petrush_cmd_t *cmd)
{
    if (!cmd || cmd->argc == 0 || !cmd->argv[0]) {
        return 0;
    }

    /* UX-12/13: ~ e $VAR em argv e redirs antes do dispatch */
    expand_cmd_argv(cmd);

    builtin_fn_t fn = find_builtin(cmd->argv[0]);
    if (fn) {
        if (cmd->redir_in || cmd->redir_out || cmd->redir_err || cmd->redir_err_to_out) {
            return run_builtin_with_redirs(fn, cmd);
        }
        return fn(cmd);
    }

    /* Não é builtin → tenta executar como comando externo */
    int status = 0;
    if (execute_external(cmd, &status) == 0) {
        return status;
    }

    return (status != 0) ? status : 127;
}

/* Rótulo curto pro job table (argv do 1º estágio; "|…" se pipeline). */
static char *pipeline_job_label(const petrush_pipeline_t *pl)
{
    if (!pl || pl->ncmds <= 0 || !pl->cmds[0].argv || !pl->cmds[0].argv[0]) {
        return strdup("?");
    }
    const petrush_cmd_t *cmd = &pl->cmds[0];
    size_t cap = 64;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    buf[0] = '\0';
    size_t len = 0;
    for (int i = 0; i < cmd->argc; i++) {
        const char *a = cmd->argv[i] ? cmd->argv[i] : "";
        size_t alen = strlen(a);
        size_t need = len + alen + (i ? 1 : 0) + 1;
        if (need > cap) {
            size_t ncap = need + 32;
            char *nbuf = realloc(buf, ncap);
            if (!nbuf) {
                free(buf);
                return NULL;
            }
            buf = nbuf;
            cap = ncap;
        }
        if (i) buf[len++] = ' ';
        memcpy(buf + len, a, alen);
        len += alen;
        buf[len] = '\0';
    }
    if (pl->ncmds > 1) {
        const char *suf = " |...";
        size_t sl = strlen(suf);
        if (len + sl + 1 > cap) {
            char *nbuf = realloc(buf, len + sl + 1);
            if (!nbuf) {
                free(buf);
                return NULL;
            }
            buf = nbuf;
        }
        memcpy(buf + len, suf, sl + 1);
    }
    return buf;
}

/* UX-23: subshell por item bg; não altera execute_external/execute_pipeline. */
static int dispatch_pipeline_background(petrush_pipeline_t *pl)
{
    if (!pl) return 1;
    petrush_job_reap();
    if (petrush_job_count() >= PETRUSH_JOB_MAX) {
        fprintf(stderr, "petrush: teto de %d jobs em background\n",
                PETRUSH_JOB_MAX);
        return 1;
    }

    char *label = pipeline_job_label(pl);
    if (!label) return 1;

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        free(label);
        return 1;
    }
    if (pid == 0) {
        (void)dispatcher_setpgid(0, 0);
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        /* stdin /dev/null se tty e sem `<` no 1º estágio */
        if (pl->ncmds > 0 && isatty(STDIN_FILENO) &&
            !pl->cmds[0].redir_in) {
            int fd = open("/dev/null", O_RDONLY);
            if (fd >= 0) {
                if (dup2(fd, STDIN_FILENO) < 0) {
                    /* segue com stdin atual */
                }
                close(fd);
            }
        }
        int st = dispatch_pipeline(pl);
        _exit(st & 0xff);
    }

    (void)dispatcher_setpgid(pid, pid);
    int id = petrush_job_add(pid, label);
    free(label);
    if (id < 0) {
        fprintf(stderr, "petrush: não foi possível registrar job\n");
        return 1;
    }
    printf("[%d] %d\n", id, (int)pid);
    fflush(stdout);
    return 0;
}

int dispatch_list(petrush_list_t *list)
{
    if (!list || list->nitems <= 0) {
        return 0;
    }
    int status = 0;
    for (int i = 0; i < list->nitems; i++) {
        petrush_list_item_t *it = &list->items[i];
        if (i > 0) {
            if (it->cond == PETRUSH_COND_AND && status != 0) {
                continue; /* short-circuit && */
            }
            if (it->cond == PETRUSH_COND_OR && status == 0) {
                continue; /* short-circuit || */
            }
        }
        if (it->background) {
            status = dispatch_pipeline_background(&it->pl);
        } else {
            status = dispatch_pipeline(&it->pl);
        }
    }
    return status;
}

/* UX-19: no filho do pipe, builtin da tabela antes do PATH. */
static int pipeline_child_builtin_hook(petrush_cmd_t *cmd)
{
    if (!cmd || cmd->argc <= 0 || !cmd->argv || !cmd->argv[0]) {
        return 1;
    }
    builtin_fn_t fn = find_builtin(cmd->argv[0]);
    if (!fn) {
        return 1;
    }
    int rc = fn(cmd);
    fflush(stdout);
    fflush(stderr);
    _exit(rc & 0xff);
    return 0;
}

int dispatch_pipeline(petrush_pipeline_t *pl)
{
    if (!pl || pl->ncmds <= 0) {
        return 0;
    }

    /* Expandir todas as etapas cedo (UX-12/13) */
    for (int i = 0; i < pl->ncmds; i++) {
        expand_cmd_argv(&pl->cmds[i]);
    }

    /* Um estágio: caminho normal (builtin ou externo + redirs) */
    if (pl->ncmds == 1) {
        /* expand já feito; dispatch_command expandiria de novo (idempotente) */
        return dispatch_command(&pl->cmds[0]);
    }

    /*
     * UX-19: ncmds>=2 — todo estágio em fork; builtin via hook no filho
     * (find_builtin antes de PATH). Sem lastpipe / pipefail / jobs.
     * cd/export/exit no pipe não alteram o pai; status = último estágio.
     */
    int status = 0;
    if (execute_pipeline_with_hook(pl, &status, pipeline_child_builtin_hook) == 0) {
        return status;
    }
    return (status != 0) ? status : 127;
}

/* ===================== BUILTINS BÁSICOS ===================== */

int builtin_cd(petrush_cmd_t *cmd)
{
    /* UX-14: cd - volta a OLDPWD; atualiza OLDPWD/PWD após sucesso */
    char oldcwd[PATH_MAX];
    if (!getcwd(oldcwd, sizeof(oldcwd))) {
        oldcwd[0] = '\0';
    }

    const char *path = (cmd->argc > 1) ? cmd->argv[1] : petrush_getenv("HOME");
    if (!path || !path[0]) {
        path = ".";
    }

    if (strcmp(path, "-") == 0) {
        const char *op = petrush_getenv("OLDPWD");
        if (!op || !op[0]) {
            fprintf(stderr, "cd: OLDPWD not set\n");
            return 1;
        }
        path = op;
        /* bash imprime o destino em cd - */
        printf("%s\n", path);
    }

    if (chdir(path) != 0) {
        perror("cd");
        return 1;
    }

    if (oldcwd[0]) {
        (void)petrush_setenv("OLDPWD", oldcwd, 1);
    }
    {
        char newcwd[PATH_MAX];
        if (getcwd(newcwd, sizeof(newcwd))) {
            (void)petrush_setenv("PWD", newcwd, 1);
        }
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
    int code = 0;
    if (cmd && cmd->argc >= 2 && cmd->argv && cmd->argv[1]) {
        const char *a = cmd->argv[1];
        char *end = NULL;
        errno = 0;
        long v = strtol(a, &end, 10);
        if (errno != 0 || end == a || (end && *end != '\0')) {
            fprintf(stderr, "petrush: exit: %s: numeric argument required\n", a);
            exit(2);
        }
        code = (int)(v & 0xff);
    }
    exit(code);
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
    printf("  info         - Mostra informações do shell\n");
    printf("  wai […]      - Inventário de hardware (sysfs/proc, sem root)\n");
    printf("  netcom […]   - Rede wifi/eth/bt; -up/-down precisa CAP_NET_ADMIN\n");
    printf("  alias        - Define/lista aliases\n");
    printf("  unalias      - Remove alias\n");
    printf("  which        - Localiza comando (builtin ou PATH)\n");
    printf("  pushd/popd   - Stack de diretórios\n");
    printf("  dirs         - Mostra a stack\n");
    printf("  source/.     - Executa arquivo no shell atual\n");
    printf("  jobs         - Lista jobs em background\n");
    printf("  true / :     - Sempre status 0 (no-op)\n");
    printf("  false        - Sempre status 1 (no-op)\n");
    printf("  umask [oct]  - Mostra/define máscara octal do shell\n");
    printf("  read NAME    - Lê 1 linha de stdin para NAME\n");
    printf("  test / [     - Primaries (-f -d -e -z -n = != -eq -ne -lt -gt)\n");
    printf("\n");
    printf("Também: pipes |, redirs > >> < 2> 2>> 2>&1 &>, listas && || ; &,\n");
    printf("  glob * ? (unquoted), !! / !n, Tab, history hints.\n");
    printf("noclobber: > e 2> recusam overwrite se o destino existe; >> / 2>> ok.\n");
    printf("Comandos externos via PATH. Prompt: PETRUSH_PS1.\n");
    return 0;
}

int builtin_jobs(petrush_cmd_t *cmd)
{
    (void)cmd;
    petrush_job_reap();
    petrush_job_print();
    petrush_job_prune_done();
    return 0;
}

/* FEAT-TRUE: silent status helpers (sem printf). */
int builtin_true(petrush_cmd_t *cmd)
{
    (void)cmd;
    return 0;
}

int builtin_false(petrush_cmd_t *cmd)
{
    (void)cmd;
    return 1;
}

/* FEAT-UMASK: POSIX magro; print %04o / set octal; sem -S. */
int builtin_umask(petrush_cmd_t *cmd)
{
    if (cmd->argc > 2) {
        fprintf(stderr, "umask: too many arguments\n");
        return 1;
    }

    if (cmd->argc == 1) {
        mode_t cur = umask(0);
        umask(cur);
        printf("%04o\n", (unsigned)cur);
        return 0;
    }

    const char *arg = cmd->argv[1];
    char *end = NULL;
    errno = 0;
    unsigned long v = strtoul(arg, &end, 8);
    if (arg[0] == '\0' || end == arg || *end != '\0' || errno == ERANGE) {
        fprintf(stderr, "umask: %s: octal number expected\n", arg);
        return 1;
    }

    umask((mode_t)v);
    return 0;
}

/* FEAT-READ: argc==2 only; linha inteira (sem IFS split); EOF → 1. */
int builtin_read(petrush_cmd_t *cmd)
{
    if (!cmd || cmd->argc != 2 || !cmd->argv[1] || cmd->argv[1][0] == '\0') {
        fprintf(stderr, "read: usage: read NAME\n");
        return 1;
    }

    /* dup2 de `<` troca o FD sob stdin; limpa EOF/erro residual do stream. */
    clearerr(stdin);

    char buf[8192];
    if (fgets(buf, (int)sizeof(buf), stdin) == NULL) {
        return 1;
    }

    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') {
        buf[len - 1] = '\0';
    }

    if (petrush_setenv(cmd->argv[1], buf, 1) != 0) {
        perror("read");
        return 1;
    }
    return 0;
}

/* FEAT-TEST: 0=true, 1=false, 2=erro; sem [[ / -a/-o / !. */
static int feat_test_parse_long(const char *s, long *out)
{
    char *end = NULL;
    if (!s || !out || s[0] == '\0') {
        return -1;
    }
    errno = 0;
    long v = strtol(s, &end, 10);
    if (end == s || *end != '\0' || errno == ERANGE) {
        return -1;
    }
    *out = v;
    return 0;
}

/* Struct evita bugprone-easily-swappable-parameters (dois const char*). */
typedef struct {
    const char *unary_op;
    const char *operand;
} feat_test_unary_args;

static int feat_test_unary(feat_test_unary_args a)
{
    const char *unary_op = a.unary_op;
    const char *operand = a.operand;
    if (strcmp(unary_op, "-z") == 0) {
        return (operand[0] == '\0') ? 0 : 1;
    }
    if (strcmp(unary_op, "-n") == 0) {
        return (operand[0] != '\0') ? 0 : 1;
    }
    if (strcmp(unary_op, "-e") == 0) {
        return (access(operand, F_OK) == 0) ? 0 : 1;
    }
    if (strcmp(unary_op, "-f") == 0) {
        struct stat st;
        if (stat(operand, &st) != 0) return 1;
        return S_ISREG(st.st_mode) ? 0 : 1;
    }
    if (strcmp(unary_op, "-d") == 0) {
        struct stat st;
        if (stat(operand, &st) != 0) return 1;
        return S_ISDIR(st.st_mode) ? 0 : 1;
    }
    fprintf(stderr, "test: %s: unary operator expected\n", unary_op);
    return 2;
}

static int feat_test_binary(const char *lhs, const char *op, const char *rhs)
{
    if (strcmp(op, "=") == 0) {
        return (strcmp(lhs, rhs) == 0) ? 0 : 1;
    }
    if (strcmp(op, "!=") == 0) {
        return (strcmp(lhs, rhs) != 0) ? 0 : 1;
    }

    long a = 0, b = 0;
    if (feat_test_parse_long(lhs, &a) != 0 || feat_test_parse_long(rhs, &b) != 0) {
        fprintf(stderr, "test: integer expression expected\n");
        return 2;
    }
    if (strcmp(op, "-eq") == 0) return (a == b) ? 0 : 1;
    if (strcmp(op, "-ne") == 0) return (a != b) ? 0 : 1;
    if (strcmp(op, "-lt") == 0) return (a < b) ? 0 : 1;
    if (strcmp(op, "-gt") == 0) return (a > b) ? 0 : 1;

    fprintf(stderr, "test: %s: binary operator expected\n", op);
    return 2;
}

int builtin_test(petrush_cmd_t *cmd)
{
    if (!cmd || !cmd->argv || !cmd->argv[0]) {
        return 2;
    }

    int bracket = (strcmp(cmd->argv[0], "[") == 0);
    int argc = cmd->argc;
    char **argv = cmd->argv;
    const char *prog = bracket ? "[" : "test";

    if (bracket) {
        if (argc < 2 || !argv[argc - 1] || strcmp(argv[argc - 1], "]") != 0) {
            fprintf(stderr, "%s: missing `]'\n", prog);
            return 2;
        }
        argc--; /* descarta ] final */
    }

    int n = argc - 1; /* args da expressão (sem argv[0]) */
    if (n == 0) {
        return 1;
    }
    if (n == 1) {
        return (argv[1][0] != '\0') ? 0 : 1;
    }
    if (n == 2) {
        /* unary: OP ARG (ex.: -f path). 1 arg só = string (POSIX). */
        return feat_test_unary((feat_test_unary_args){
            .unary_op = argv[1],
            .operand = argv[2],
        });
    }
    if (n == 3) {
        return feat_test_binary(argv[1], argv[2], argv[3]);
    }

    fprintf(stderr, "%s: too many arguments\n", prog);
    return 2;
}

int builtin_clear(petrush_cmd_t *cmd)
{
    (void)cmd;
    petrush_ui_clear_screen();
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
    printf("Features: history, rc, signals, pudo, pipes, redir (noclobber), alias, PS1, complete, hints, &&/||/;/&, glob, source/., jobs.\n");
    printf("Anti-OE: sem fg/bg/Ctrl-Z/%%n, sem []/**, sem $1/return em source, sem set -C\n");
    return 0;
}

int builtin_alias(petrush_cmd_t *cmd)
{
    if (!cmd || cmd->argc < 2) {
        alias_list_print();
        return 0;
    }
    /* alias name=value  OR  alias name value... */
    const char *arg = cmd->argv[1];
    const char *eq = strchr(arg, '=');
    if (eq && eq != arg) {
        char name[64];
        size_t nlen = (size_t)(eq - arg);
        if (nlen >= sizeof(name)) {
            fprintf(stderr, "alias: nome muito longo\n");
            return 1;
        }
        memcpy(name, arg, nlen);
        name[nlen] = '\0';
        if (alias_set(name, eq + 1) != 0) {
            fprintf(stderr, "alias: falha ao definir '%s'\n", name);
            return 1;
        }
        return 0;
    }
    if (cmd->argc >= 3) {
        /* alias name rest... */
        size_t total = 0;
        for (int i = 2; i < cmd->argc; i++) total += strlen(cmd->argv[i]) + 1;
        char *val = malloc(total + 1);
        if (!val) return 1;
        size_t off = 0;
        size_t cap = total + 1;
        val[0] = '\0';
        for (int i = 2; i < cmd->argc; i++) {
            int n = snprintf(val + off, cap - off, "%s%s",
                             (i > 2) ? " " : "", cmd->argv[i]);
            if (n < 0 || (size_t)n >= cap - off) {
                free(val);
                return 1;
            }
            off += (size_t)n;
        }
        int rc = alias_set(arg, val);
        free(val);
        if (rc != 0) {
            fprintf(stderr, "alias: falha ao definir '%s'\n", arg);
            return 1;
        }
        return 0;
    }
    /* alias name → show one */
    const char *v = alias_get(arg);
    if (!v) {
        fprintf(stderr, "alias: %s: not found\n", arg);
        return 1;
    }
    printf("alias %s='%s'\n", arg, v);
    return 0;
}

int builtin_unalias(petrush_cmd_t *cmd)
{
    if (!cmd || cmd->argc < 2) {
        fprintf(stderr, "unalias: usage: unalias NAME\n");
        return 1;
    }
    if (alias_unset(cmd->argv[1]) != 0) {
        fprintf(stderr, "unalias: %s: not found\n", cmd->argv[1]);
        return 1;
    }
    return 0;
}

int builtin_pushd(petrush_cmd_t *cmd)
{
    if (!cmd || cmd->argc < 2) {
        fprintf(stderr, "pushd: usage: pushd DIR\n");
        return 1;
    }
    return dirstack_pushd(cmd->argv[1]) == 0 ? 0 : 1;
}

int builtin_popd(petrush_cmd_t *cmd)
{
    (void)cmd;
    return dirstack_popd() == 0 ? 0 : 1;
}

int builtin_dirs(petrush_cmd_t *cmd)
{
    (void)cmd;
    dirstack_print();
    return 0;
}

int builtin_which(petrush_cmd_t *cmd)
{
    if (!cmd || cmd->argc < 2) {
        fprintf(stderr, "which: usage: which NAME\n");
        return 1;
    }
    const char *name = cmd->argv[1];
    if (find_builtin(name)) {
        printf("%s: shell builtin\n", name);
        return 0;
    }
    /* search PATH like process.c - simple reimplementation */
    if (strchr(name, '/')) {
        if (access(name, X_OK) == 0) {
            printf("%s\n", name);
            return 0;
        }
        fprintf(stderr, "which: no %s in PATH\n", name);
        return 1;
    }
    const char *path_env = petrush_getenv("PATH");
    if (!path_env) path_env = "/bin:/usr/bin";
    char *path_copy = strdup(path_env);
    if (!path_copy) return 1;
    char full[PATH_MAX];
    int found = 0;
    for (char *dir = strtok(path_copy, ":"); dir; dir = strtok(NULL, ":")) {
        snprintf(full, sizeof(full), "%s/%s", dir, name);
        if (access(full, X_OK) == 0) {
            printf("%s\n", full);
            found = 1;
            break;
        }
    }
    free(path_copy);
    if (!found) {
        fprintf(stderr, "which: no %s in PATH\n", name);
        return 1;
    }
    return 0;
}
