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
#include "petrush/xdg_paths.h"
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
#include <sys/wait.h>

extern char **environ;

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
    { "shift",   builtin_shift   }, /* OSH-2: shift [n] */
    { "return",  builtin_return  }, /* OSH-7: return [n] so em funcao */
    { "local",   builtin_local   }, /* OSH-8: local name[=value] so em fn */
    { "set",     builtin_set     }, /* OSH-16: set / -- / -x / $? / $- */
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

static int cmd_needs_redir(const petrush_cmd_t *cmd)
{
    return cmd->redir_in || cmd->here_body || cmd->here_delim ||
           cmd->redir_out || cmd->redir_err || cmd->redir_err_to_out;
}

/* Roda builtin no processo atual com redirecionamentos temporários.
 * Save FDs → petrush_apply_redirs (Foundation) → restore. */
static int run_builtin_with_redirs(builtin_fn_t fn, petrush_cmd_t *cmd)
{
    int saved_in = -1, saved_out = -1, saved_err = -1;
    int rc;

    /* flush antes de trocar FDs — buffer do stdio (non-tty = fully buffered)
     * ainda aponta para o FD antigo e vaza no arquivo de redir. */
    fflush(stdout);
    fflush(stderr);

    if (cmd->redir_in || cmd->here_body || cmd->here_delim) {
        saved_in = dup(STDIN_FILENO);
        if (saved_in < 0) {
            return 1;
        }
    }
    if (cmd->redir_out) {
        saved_out = dup(STDOUT_FILENO);
        if (saved_out < 0) {
            restore_saved_fds(saved_in, -1, -1);
            return 1;
        }
    }
    if (cmd->redir_err || cmd->redir_err_to_out) {
        saved_err = dup(STDERR_FILENO);
        if (saved_err < 0) {
            restore_saved_fds(saved_in, saved_out, -1);
            return 1;
        }
    }

    if (petrush_apply_redirs(cmd) != 0) {
        restore_saved_fds(saved_in, saved_out, saved_err);
        return 1;
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

static int clone_list(const petrush_list_t *src, petrush_list_t *dst);

/* OSH-6: tabela de funcoes (body owned; clone em cada call). */
#define PETRUSH_FN_MAX 64

typedef struct {
    int used;
    char *name;
    petrush_list_t body;
} petrush_fn_entry_t;

static petrush_fn_entry_t g_fns[PETRUSH_FN_MAX];

/* OSH-7: profundidade de call_fn + pedido de return (unwind ate o frame). */
static int g_fn_depth;
static int g_returning;
static int g_return_status;

/* OSH-16: special-builtin / expansion abort do runner de script. */
static int g_shell_abort;

int petrush_take_shell_abort(void)
{
    int a = g_shell_abort;
    g_shell_abort = 0;
    return a;
}

void petrush_shell_abort_clear(void)
{
    g_shell_abort = 0;
}

static void shell_abort_raise(void)
{
    g_shell_abort = 1;
}

/* OSH-16: PS4 fixo "+ " (espaco); argv ja expandido. */
static void xtrace_print_cmd(const petrush_cmd_t *cmd)
{
    if (!petrush_shellopt_get('x')) {
        return;
    }
    if (!cmd || cmd->argc <= 0 || !cmd->argv) {
        return;
    }
    fputs("+ ", stderr);
    for (int i = 0; i < cmd->argc; i++) {
        if (i > 0) {
            fputc(' ', stderr);
        }
        fputs(cmd->argv[i] ? cmd->argv[i] : "", stderr);
    }
    fputc('\n', stderr);
}

/* OSH-8: stack de valores salvos por `local` (LIFO; tag = g_fn_depth). */
#define PETRUSH_LOCAL_STACK_MAX 256

typedef struct {
    int depth;
    int had;       /* 1 se a var existia antes deste local */
    char *name;
    char *old;     /* valor anterior (strdup); NULL se !had */
} petrush_local_save_t;

static petrush_local_save_t g_locals[PETRUSH_LOCAL_STACK_MAX];
static int g_local_n;

static int local_push(const char *name)
{
    if (!name || !name[0] || g_local_n >= PETRUSH_LOCAL_STACK_MAX) {
        return -1;
    }
    petrush_local_save_t *e = &g_locals[g_local_n];
    const char *cur = petrush_getenv(name);
    e->depth = g_fn_depth;
    e->had = (cur != NULL) ? 1 : 0;
    e->name = strdup(name);
    e->old = (cur != NULL) ? strdup(cur) : NULL;
    if (!e->name || (cur && !e->old)) {
        free(e->name);
        free(e->old);
        e->name = NULL;
        e->old = NULL;
        return -1;
    }
    g_local_n++;
    return 0;
}

static void local_restore_frame(int depth)
{
    while (g_local_n > 0 && g_locals[g_local_n - 1].depth == depth) {
        petrush_local_save_t *e = &g_locals[g_local_n - 1];
        if (e->had) {
            (void)petrush_setenv(e->name, e->old ? e->old : "", 1);
        } else {
            (void)petrush_unsetenv(e->name);
        }
        free(e->name);
        free(e->old);
        e->name = NULL;
        e->old = NULL;
        g_local_n--;
    }
}

static void fn_entry_clear(petrush_fn_entry_t *e)
{
    if (!e) {
        return;
    }
    free(e->name);
    e->name = NULL;
    petrush_list_free(&e->body);
    e->used = 0;
}

static int fn_set(const char *name, const petrush_list_t *body)
{
    if (!name || !name[0] || !body) {
        return -1;
    }
    int slot = -1;
    for (int i = 0; i < PETRUSH_FN_MAX; i++) {
        if (g_fns[i].used && g_fns[i].name &&
            strcmp(g_fns[i].name, name) == 0) {
            slot = i;
            break;
        }
        if (slot < 0 && !g_fns[i].used) {
            slot = i;
        }
    }
    if (slot < 0) {
        return -1;
    }
    petrush_list_t copy = {0};
    if (clone_list(body, &copy) != 0) {
        return -1;
    }
    if (g_fns[slot].used) {
        fn_entry_clear(&g_fns[slot]);
    }
    g_fns[slot].name = strdup(name);
    if (!g_fns[slot].name) {
        petrush_list_free(&copy);
        return -1;
    }
    g_fns[slot].body = copy;
    g_fns[slot].used = 1;
    return 0;
}

static const petrush_list_t *fn_get(const char *name)
{
    if (!name) {
        return NULL;
    }
    for (int i = 0; i < PETRUSH_FN_MAX; i++) {
        if (g_fns[i].used && g_fns[i].name &&
            strcmp(g_fns[i].name, name) == 0) {
            return &g_fns[i].body;
        }
    }
    return NULL;
}

/* Define posicionais da chamada; restaura os anteriores ao sair. */
/* NOLINTNEXTLINE(misc-no-recursion) */
static int call_fn(const char *name, int argc, char **argv,
                   const petrush_list_t *body)
{
    const char *old0 = petrush_positional_get(0);
    char *saved0 = strdup(old0 ? old0 : "");
    if (!saved0) {
        return 1;
    }
    unsigned oldn = petrush_positional_count();
    char **saved = NULL;
    if (oldn > 0) {
        saved = calloc(oldn, sizeof(char *));
        if (!saved) {
            free(saved0);
            return 1;
        }
        for (unsigned i = 0; i < oldn; i++) {
            const char *v = petrush_positional_get(i + 1);
            saved[i] = strdup(v ? v : "");
            if (!saved[i]) {
                for (unsigned j = 0; j < i; j++) {
                    free(saved[j]);
                }
                free(saved);
                free(saved0);
                return 1;
            }
        }
    }

    int status = 1;
    int nargs = (argc > 1) ? (argc - 1) : 0;
    char **args = (nargs > 0) ? (argv + 1) : NULL;
    g_fn_depth++;
    if (petrush_positional_set(name, nargs, args) != 0) {
        status = 1;
    } else {
        petrush_list_t copy = {0};
        if (clone_list(body, &copy) != 0) {
            status = 1;
        } else {
            status = dispatch_list(&copy);
            petrush_list_free(&copy);
        }
    }
    if (g_returning) {
        status = g_return_status;
        g_returning = 0;
    }
    /* OSH-8: restaura locals deste frame (inclusive apos return). */
    local_restore_frame(g_fn_depth);
    g_fn_depth--;

    (void)petrush_positional_set(saved0, (int)oldn, saved);
    free(saved0);
    if (saved) {
        for (unsigned i = 0; i < oldn; i++) {
            free(saved[i]);
        }
        free(saved);
    }
    return status;
}

static int dispatch_fn_def(petrush_fn_t *fn)
{
    if (!fn || !fn->name) {
        return 1;
    }
    if (fn_set(fn->name, &fn->body) != 0) {
        return 1;
    }
    return 0;
}

/* NOLINTNEXTLINE(misc-no-recursion) */
int dispatch_command(petrush_cmd_t *cmd)
{
    if (!cmd || cmd->argc == 0 || !cmd->argv[0]) {
        return 0;
    }

    /* UX-12/13: ~ e $VAR em argv e redirs antes do dispatch */
    expand_cmd_argv(cmd);
    /* OSH-11: div0 → palavra "0" ja emitida; status do comando != 0 */
    if (petrush_take_arith_error()) {
        return 1;
    }

    /* OSH-16: xtrace apos expand, antes de executar (set -x nao se auto-traca). */
    xtrace_print_cmd(cmd);

    /* OSH-6: funcao antes de builtin/PATH (bash-like). */
    const petrush_list_t *fbody = fn_get(cmd->argv[0]);
    if (fbody) {
        return call_fn(cmd->argv[0], cmd->argc, cmd->argv, fbody);
    }

    builtin_fn_t fn = find_builtin(cmd->argv[0]);
    if (fn) {
        if (cmd_needs_redir(cmd)) {
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
/* NOLINTNEXTLINE(misc-no-recursion) */
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
            !pl->cmds[0].redir_in && !pl->cmds[0].here_body &&
            !pl->cmds[0].here_delim) {
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

/* OSH-3: status = ultimo comando do corpo tomado; nenhum braço → 0. */
/* NOLINTNEXTLINE(misc-no-recursion) */
static int dispatch_if(petrush_if_t *ifc)
{
    if (!ifc || ifc->narms <= 0) {
        return 0;
    }
    for (int i = 0; i < ifc->narms; i++) {
        petrush_if_arm_t *arm = &ifc->arms[i];
        if (arm->is_else) {
            return dispatch_list(&arm->body);
        }
        int st = dispatch_list(&arm->cond);
        if (g_returning) {
            return g_return_status;
        }
        if (st == 0) {
            return dispatch_list(&arm->body);
        }
    }
    return 0;
}

/*
 * OSH-4: while cond; do body; done
 * Condicao = status do ultimo comando da lista cond (==0 continua).
 * Status do while = ultimo body executado; 0 se body nunca rodou (POSIX).
 */
/* NOLINTNEXTLINE(misc-no-recursion) */
static int dispatch_while(petrush_while_t *wh)
{
    if (!wh) {
        return 0;
    }
    int status = 0;
    for (;;) {
        int st = dispatch_list(&wh->cond);
        if (g_returning) {
            return g_return_status;
        }
        if (st != 0) {
            return status;
        }
        status = dispatch_list(&wh->body);
        if (g_returning) {
            return g_return_status;
        }
    }
}

/*
 * expand_cmd_argv muta argv no AST. for/fn precisam re-expandir a cada
 * iteracao/call → clonar o body antes do dispatch e liberar a copia.
 */
static int clone_cmd(const petrush_cmd_t *src, petrush_cmd_t *dst)
{
    memset(dst, 0, sizeof(*dst));
    if (!src) {
        return 0;
    }
    dst->argc = src->argc;
    dst->redir_append = src->redir_append;
    dst->redir_err_append = src->redir_err_append;
    dst->redir_err_to_out = src->redir_err_to_out;
    dst->here_quoted = src->here_quoted;
    dst->here_strip = src->here_strip;
    dst->here_feed_i = src->here_feed_i;
    if (src->argc > 0) {
        dst->argv = calloc((size_t)src->argc + 1, sizeof(char *));
        if (!dst->argv) {
            return -1;
        }
        for (int i = 0; i < src->argc; i++) {
            if (src->argv[i]) {
                dst->argv[i] = strdup(src->argv[i]);
                if (!dst->argv[i]) {
                    petrush_cmd_free(dst);
                    return -1;
                }
            }
        }
    }
    if (src->argv_quoted && src->argc > 0) {
        dst->argv_quoted = malloc(sizeof(int) * (size_t)src->argc);
        if (!dst->argv_quoted) {
            petrush_cmd_free(dst);
            return -1;
        }
        memcpy(dst->argv_quoted, src->argv_quoted,
               sizeof(int) * (size_t)src->argc);
    }
    if (src->redir_in) {
        dst->redir_in = strdup(src->redir_in);
        if (!dst->redir_in) {
            petrush_cmd_free(dst);
            return -1;
        }
    }
    if (src->redir_out) {
        dst->redir_out = strdup(src->redir_out);
        if (!dst->redir_out) {
            petrush_cmd_free(dst);
            return -1;
        }
    }
    if (src->redir_err) {
        dst->redir_err = strdup(src->redir_err);
        if (!dst->redir_err) {
            petrush_cmd_free(dst);
            return -1;
        }
    }
    if (src->here_delim) {
        dst->here_delim = strdup(src->here_delim);
        if (!dst->here_delim) {
            petrush_cmd_free(dst);
            return -1;
        }
    }
    if (src->here_body) {
        dst->here_body = strdup(src->here_body);
        if (!dst->here_body) {
            petrush_cmd_free(dst);
            return -1;
        }
    }
    if (src->here_skip_n > 0 && src->here_skip_delims) {
        dst->here_skip_delims = calloc((size_t)src->here_skip_n, sizeof(char *));
        if (!dst->here_skip_delims) {
            petrush_cmd_free(dst);
            return -1;
        }
        dst->here_skip_n = src->here_skip_n;
        for (int i = 0; i < src->here_skip_n; i++) {
            if (src->here_skip_delims[i]) {
                dst->here_skip_delims[i] = strdup(src->here_skip_delims[i]);
                if (!dst->here_skip_delims[i]) {
                    petrush_cmd_free(dst);
                    return -1;
                }
            }
        }
    }
    return 0;
}

static int clone_pipeline(const petrush_pipeline_t *src, petrush_pipeline_t *dst)
{
    memset(dst, 0, sizeof(*dst));
    if (!src || src->ncmds <= 0) {
        return 0;
    }
    dst->cmds = calloc((size_t)src->ncmds, sizeof(petrush_cmd_t));
    if (!dst->cmds) {
        return -1;
    }
    dst->ncmds = src->ncmds;
    for (int i = 0; i < src->ncmds; i++) {
        if (clone_cmd(&src->cmds[i], &dst->cmds[i]) != 0) {
            petrush_pipeline_free(dst);
            return -1;
        }
    }
    return 0;
}

static int dispatch_dbracket(petrush_dbracket_t *db);

/* Nested if/while/for/fn/case/[[ clone is intentional AST walk. */
/* NOLINTNEXTLINE(misc-no-recursion, readability-function-cognitive-complexity) */
static int clone_list(const petrush_list_t *src, petrush_list_t *dst)
{
    memset(dst, 0, sizeof(*dst));
    if (!src || src->nitems <= 0) {
        return 0;
    }
    dst->items = calloc((size_t)src->nitems, sizeof(petrush_list_item_t));
    if (!dst->items) {
        return -1;
    }
    dst->nitems = src->nitems;
    for (int i = 0; i < src->nitems; i++) {
        const petrush_list_item_t *si = &src->items[i];
        petrush_list_item_t *di = &dst->items[i];
        di->kind = si->kind;
        di->cond = si->cond;
        di->background = si->background;
        if (si->kind == PETRUSH_ITEM_IF) {
            if (si->ifc.narms > 0) {
                di->ifc.arms =
                    calloc((size_t)si->ifc.narms, sizeof(petrush_if_arm_t));
                if (!di->ifc.arms) {
                    petrush_list_free(dst);
                    return -1;
                }
                di->ifc.narms = si->ifc.narms;
                for (int a = 0; a < si->ifc.narms; a++) {
                    di->ifc.arms[a].is_else = si->ifc.arms[a].is_else;
                    if (clone_list(&si->ifc.arms[a].cond,
                                   &di->ifc.arms[a].cond) != 0 ||
                        clone_list(&si->ifc.arms[a].body,
                                   &di->ifc.arms[a].body) != 0) {
                        petrush_list_free(dst);
                        return -1;
                    }
                }
            }
        } else if (si->kind == PETRUSH_ITEM_WHILE) {
            if (clone_list(&si->wh.cond, &di->wh.cond) != 0 ||
                clone_list(&si->wh.body, &di->wh.body) != 0) {
                petrush_list_free(dst);
                return -1;
            }
        } else if (si->kind == PETRUSH_ITEM_FOR) {
            if (si->fr.name) {
                di->fr.name = strdup(si->fr.name);
                if (!di->fr.name) {
                    petrush_list_free(dst);
                    return -1;
                }
            }
            if (si->fr.nwords > 0) {
                di->fr.words =
                    calloc((size_t)si->fr.nwords, sizeof(char *));
                if (!di->fr.words) {
                    petrush_list_free(dst);
                    return -1;
                }
                di->fr.nwords = si->fr.nwords;
                for (int w = 0; w < si->fr.nwords; w++) {
                    if (si->fr.words[w]) {
                        di->fr.words[w] = strdup(si->fr.words[w]);
                        if (!di->fr.words[w]) {
                            petrush_list_free(dst);
                            return -1;
                        }
                    }
                }
            }
            if (clone_list(&si->fr.body, &di->fr.body) != 0) {
                petrush_list_free(dst);
                return -1;
            }
        } else if (si->kind == PETRUSH_ITEM_FN) {
            if (si->fn.name) {
                di->fn.name = strdup(si->fn.name);
                if (!di->fn.name) {
                    petrush_list_free(dst);
                    return -1;
                }
            }
            if (clone_list(&si->fn.body, &di->fn.body) != 0) {
                petrush_list_free(dst);
                return -1;
            }
        } else if (si->kind == PETRUSH_ITEM_CASE) {
            if (si->cs.word) {
                di->cs.word = strdup(si->cs.word);
                if (!di->cs.word) {
                    petrush_list_free(dst);
                    return -1;
                }
            }
            if (si->cs.narms > 0) {
                di->cs.arms =
                    calloc((size_t)si->cs.narms, sizeof(petrush_case_arm_t));
                if (!di->cs.arms) {
                    petrush_list_free(dst);
                    return -1;
                }
                di->cs.narms = si->cs.narms;
                for (int a = 0; a < si->cs.narms; a++) {
                    const petrush_case_arm_t *sa = &si->cs.arms[a];
                    petrush_case_arm_t *da = &di->cs.arms[a];
                    if (sa->npatterns > 0) {
                        da->patterns =
                            calloc((size_t)sa->npatterns, sizeof(char *));
                        if (!da->patterns) {
                            petrush_list_free(dst);
                            return -1;
                        }
                        da->npatterns = sa->npatterns;
                        for (int p = 0; p < sa->npatterns; p++) {
                            if (sa->patterns[p]) {
                                da->patterns[p] = strdup(sa->patterns[p]);
                                if (!da->patterns[p]) {
                                    petrush_list_free(dst);
                                    return -1;
                                }
                            }
                        }
                    }
                    if (clone_list(&sa->body, &da->body) != 0) {
                        petrush_list_free(dst);
                        return -1;
                    }
                }
            }
        } else if (si->kind == PETRUSH_ITEM_DBRACKET) {
            if (si->db.argc > 0) {
                di->db.argv =
                    calloc((size_t)si->db.argc, sizeof(char *));
                if (!di->db.argv) {
                    petrush_list_free(dst);
                    return -1;
                }
                di->db.argc = si->db.argc;
                for (int a = 0; a < si->db.argc; a++) {
                    if (si->db.argv[a]) {
                        di->db.argv[a] = strdup(si->db.argv[a]);
                        if (!di->db.argv[a]) {
                            petrush_list_free(dst);
                            return -1;
                        }
                    }
                }
                if (si->db.argv_quoted) {
                    di->db.argv_quoted =
                        malloc(sizeof(int) * (size_t)si->db.argc);
                    if (!di->db.argv_quoted) {
                        petrush_list_free(dst);
                        return -1;
                    }
                    memcpy(di->db.argv_quoted, si->db.argv_quoted,
                           sizeof(int) * (size_t)si->db.argc);
                }
            }
        } else {
            if (clone_pipeline(&si->pl, &di->pl) != 0) {
                petrush_list_free(dst);
                return -1;
            }
        }
    }
    return 0;
}

/*
 * OSH-5: for name in words; do body; done
 * Cada palavra: petrush_setenv(name, word) depois dispatch body clonado.
 * Status = ultimo body; 0 se nwords==0 (body nunca rodou).
 */
/* NOLINTNEXTLINE(misc-no-recursion) */
static int dispatch_for(petrush_for_t *fr)
{
    if (!fr || !fr->name) {
        return 0;
    }
    int status = 0;
    for (int i = 0; i < fr->nwords; i++) {
        const char *val = fr->words[i] ? fr->words[i] : "";
        if (petrush_setenv(fr->name, val, 1) != 0) {
            return 1;
        }
        petrush_list_t body_copy = {0};
        if (clone_list(&fr->body, &body_copy) != 0) {
            return 1;
        }
        status = dispatch_list(&body_copy);
        petrush_list_free(&body_copy);
        if (g_returning) {
            return g_return_status;
        }
    }
    return status;
}

/* OSH-10: glob * ? nos padroes (mesmo contrato de expand/ASM-GLOB). */
/* NOLINTNEXTLINE(bugprone-easily-swappable-parameters) */
static int case_pat_match(const char *pat, const char *str)
{
#ifdef PETRUSH_HAVE_ASM
    return petrush_glob_match(pat, str);
#else
    const char *p = pat;
    const char *s = str;
    const char *star_p = NULL;
    const char *star_s = NULL;
    if (!p) {
        p = "";
    }
    if (!s) {
        s = "";
    }
    while (*s) {
        if (*p == '*') {
            star_p = ++p;
            star_s = s;
            continue;
        }
        if (*p == '?' || *p == *s) {
            p++;
            s++;
            continue;
        }
        if (star_p) {
            p = star_p;
            s = ++star_s;
            continue;
        }
        return 0;
    }
    while (*p == '*') {
        p++;
    }
    return *p == '\0';
#endif
}

/*
 * OSH-10: case word in arms... esac
 * Word expandida uma vez. Primeiro padrao que casa (glob * ?) ganha.
 * Status = ultimo cmd do braco; 0 se nenhum casou ou narms==0.
 */
/* NOLINTNEXTLINE(misc-no-recursion) */
static int dispatch_case(petrush_case_t *cs)
{
    if (!cs || !cs->word) {
        return 0;
    }
    char *word = expand_word(cs->word);
    if (!word) {
        return 1;
    }
    if (petrush_take_arith_error()) {
        free(word);
        return 1;
    }
    int status = 0;
    for (int a = 0; a < cs->narms; a++) {
        petrush_case_arm_t *arm = &cs->arms[a];
        int matched = 0;
        for (int p = 0; p < arm->npatterns; p++) {
            const char *pat = arm->patterns[p] ? arm->patterns[p] : "";
            if (case_pat_match(pat, word)) {
                matched = 1;
                break;
            }
        }
        if (!matched) {
            continue;
        }
        petrush_list_t body_copy = {0};
        if (clone_list(&arm->body, &body_copy) != 0) {
            free(word);
            return 1;
        }
        status = dispatch_list(&body_copy);
        petrush_list_free(&body_copy);
        free(word);
        if (g_returning) {
            return g_return_status;
        }
        return status;
    }
    free(word);
    return 0;
}

/* OSH-9: profundidade de cmdsubst (herdada via fork). */
static int g_cmdsubst_depth;

char *petrush_run_cmdsubst(const char *inner_cmd)
{
    if (g_cmdsubst_depth >= PETRUSH_CMDSUBST_MAX_DEPTH) {
        char *empty = malloc(1);
        if (empty) {
            empty[0] = '\0';
        }
        return empty;
    }

    int fds[2];
    if (pipe(fds) != 0) {
        char *empty = malloc(1);
        if (empty) {
            empty[0] = '\0';
        }
        return empty;
    }

    g_cmdsubst_depth++;
    pid_t pid = fork();
    if (pid < 0) {
        g_cmdsubst_depth--;
        close(fds[0]);
        close(fds[1]);
        char *empty = malloc(1);
        if (empty) {
            empty[0] = '\0';
        }
        return empty;
    }

    if (pid == 0) {
        /* Filho: stdout → pipe; status interno nao sobe ao pai. */
        close(fds[0]);
        if (dup2(fds[1], STDOUT_FILENO) < 0) {
            _exit(127);
        }
        close(fds[1]);

        if (!inner_cmd) {
            inner_cmd = "";
        }
        petrush_list_t list = {0};
        if (petrush_parse_list(inner_cmd, &list) != 0) {
            _exit(1);
        }
        (void)dispatch_list(&list);
        petrush_list_free(&list);
        fflush(stdout);
        _exit(0);
    }

    close(fds[1]);
    /* cap inclui byte NUL; captura ate MAX_BYTES de payload. */
    size_t cap = 4096;
    size_t len = 0;
    char *buf = malloc(cap);
    if (!buf) {
        close(fds[0]);
        int st = 0;
        (void)waitpid(pid, &st, 0);
        g_cmdsubst_depth--;
        return NULL;
    }

    for (;;) {
        if (len >= PETRUSH_CMDSUBST_MAX_BYTES) {
            break;
        }
        if (len + 1 >= cap) {
            size_t ncap = cap * 2;
            if (ncap < len + 2) {
                ncap = len + 2;
            }
            if (ncap > PETRUSH_CMDSUBST_MAX_BYTES + 1) {
                ncap = PETRUSH_CMDSUBST_MAX_BYTES + 1;
            }
            if (ncap <= cap) {
                break;
            }
            char *nbuf = realloc(buf, ncap);
            if (!nbuf) {
                free(buf);
                close(fds[0]);
                int st = 0;
                (void)waitpid(pid, &st, 0);
                g_cmdsubst_depth--;
                return NULL;
            }
            buf = nbuf;
            cap = ncap;
        }
        size_t room = cap - len - 1; /* reserva NUL */
        size_t remain = PETRUSH_CMDSUBST_MAX_BYTES - len;
        size_t want = room < remain ? room : remain;
        if (want == 0) {
            break;
        }
        ssize_t n = read(fds[0], buf + len, want);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        if (n == 0) {
            break;
        }
        len += (size_t)n;
    }

    /* Drena resto se estourou teto (evita SIGPIPE no filho). */
    if (len >= PETRUSH_CMDSUBST_MAX_BYTES) {
        char drain[256];
        ssize_t dn;
        while ((dn = read(fds[0], drain, sizeof(drain))) > 0) {
            (void)dn;
        }
    }
    close(fds[0]);

    int st = 0;
    (void)waitpid(pid, &st, 0);
    g_cmdsubst_depth--;

    /* Status do inner ignorado de proposito (nao altera status do pai). */
    (void)st;

    /* Loop acima mantém cap >= len+1; analyzer nao rastreia o realloc. */
    /* NOLINTNEXTLINE(clang-analyzer-security.ArrayBound) */
    buf[len] = '\0';
    return buf;
}

/* NOLINTNEXTLINE(misc-no-recursion) */
int dispatch_list(petrush_list_t *list)
{
    /* OSH-9: liga hook DIP (idempotente). */
    petrush_set_cmdsubst_hook(petrush_run_cmdsubst);

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
        if (it->kind == PETRUSH_ITEM_IF) {
            /* background em if fica fora desta onda; ignora flag */
            status = dispatch_if(&it->ifc);
        } else if (it->kind == PETRUSH_ITEM_WHILE) {
            /* background em while fica fora desta onda; ignora flag */
            status = dispatch_while(&it->wh);
        } else if (it->kind == PETRUSH_ITEM_FOR) {
            /* background em for fica fora desta onda; ignora flag */
            status = dispatch_for(&it->fr);
        } else if (it->kind == PETRUSH_ITEM_CASE) {
            /* background em case fica fora desta onda; ignora flag */
            status = dispatch_case(&it->cs);
        } else if (it->kind == PETRUSH_ITEM_DBRACKET) {
            status = dispatch_dbracket(&it->db);
        } else if (it->kind == PETRUSH_ITEM_FN) {
            /* definicao: registra body; chamada e via dispatch_command */
            status = dispatch_fn_def(&it->fn);
        } else if (it->background) {
            status = dispatch_pipeline_background(&it->pl);
        } else {
            status = dispatch_pipeline(&it->pl);
        }
        /* OSH-16: $? = status do item mais recente (pipeline/compound/[[/fn). */
        petrush_last_status_set(status);
        if (g_returning) {
            return g_return_status;
        }
        /* OSH-16: special builtin error → para a lista (script aborta no runner). */
        if (g_shell_abort) {
            break;
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

/* NOLINTNEXTLINE(misc-no-recursion) */
int dispatch_pipeline(petrush_pipeline_t *pl)
{
    if (!pl || pl->ncmds <= 0) {
        return 0;
    }

    /* Expandir todas as etapas cedo (UX-12/13) */
    for (int i = 0; i < pl->ncmds; i++) {
        expand_cmd_argv(&pl->cmds[i]);
    }
    if (petrush_take_arith_error()) {
        return 1;
    }

    /* Um estágio: caminho normal (builtin ou externo + redirs) */
    if (pl->ncmds == 1) {
        /* expand já feito; dispatch_command expandiria de novo (idempotente) */
        return dispatch_command(&pl->cmds[0]);
    }

    /* OSH-16: um "+ " por estágio do pipeline (pai; filho nao re-traca). */
    for (int i = 0; i < pl->ncmds; i++) {
        xtrace_print_cmd(&pl->cmds[i]);
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
    printf("  shift [n]    - Desloca posicionais (default 1; n>$# erro)\n");
    printf("  return [n]   - Sai da funcao com status n (default 0; so em fn)\n");
    printf("  local NAME[=VALUE] - Var local na funcao (restaura ao sair; sem flags)\n");
    printf("  set [--] [args] / set [-+]x / set [-+]o xtrace - Opcoes e posicionais\n");
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

/* OSH-12 helpers: primaries FEAT-TEST + == glob; && || ! short-circuit. */
static int dbr_is_unary(const char *op)
{
    return op && (strcmp(op, "-f") == 0 || strcmp(op, "-d") == 0 ||
                  strcmp(op, "-e") == 0 || strcmp(op, "-z") == 0 ||
                  strcmp(op, "-n") == 0);
}

static int dbr_is_binop(const char *op)
{
    return op && (strcmp(op, "=") == 0 || strcmp(op, "==") == 0 ||
                  strcmp(op, "!=") == 0 || strcmp(op, "-eq") == 0 ||
                  strcmp(op, "-ne") == 0 || strcmp(op, "-lt") == 0 ||
                  strcmp(op, "-gt") == 0);
}

static int dbr_str_cmp(const char *lhs, const char *op, const char *rhs,
                       int rhs_quoted)
{
    int eq;
    if (!rhs_quoted) {
        eq = case_pat_match(rhs, lhs) ? 1 : 0;
    } else {
        eq = (strcmp(lhs, rhs) == 0) ? 1 : 0;
    }
    if (strcmp(op, "!=") == 0) {
        return eq ? 1 : 0;
    }
    return eq ? 0 : 1;
}

static int dbr_skip_term(char **argv, int argc, int *idx)
{
    while (*idx < argc && strcmp(argv[*idx], "!") == 0) {
        (*idx)++;
    }
    if (*idx >= argc) {
        return 2;
    }
    if (*idx + 1 < argc && dbr_is_unary(argv[*idx])) {
        *idx += 2;
        return 0;
    }
    if (*idx + 2 < argc && dbr_is_binop(argv[*idx + 1])) {
        *idx += 3;
        return 0;
    }
    (*idx)++;
    return 0;
}

static int dbr_eval_primary(char **argv, const int *quoted, int argc, int *idx)
{
    if (*idx >= argc) {
        return 2;
    }
    if (*idx + 1 < argc && dbr_is_unary(argv[*idx])) {
        int st = feat_test_unary((feat_test_unary_args){
            .unary_op = argv[*idx],
            .operand = argv[*idx + 1] ? argv[*idx + 1] : "",
        });
        *idx += 2;
        return st;
    }
    if (*idx + 2 < argc && dbr_is_binop(argv[*idx + 1])) {
        const char *lhs = argv[*idx] ? argv[*idx] : "";
        const char *op = argv[*idx + 1];
        const char *rhs = argv[*idx + 2] ? argv[*idx + 2] : "";
        int rq = (quoted && quoted[*idx + 2]) ? 1 : 0;
        int st;
        if (strcmp(op, "=") == 0 || strcmp(op, "==") == 0 ||
            strcmp(op, "!=") == 0) {
            st = dbr_str_cmp(lhs, op, rhs, rq);
        } else {
            st = feat_test_binary(lhs, op, rhs);
        }
        *idx += 3;
        return st;
    }
    /* string nao-vazia → true (como test ARG) */
    const char *s = argv[*idx] ? argv[*idx] : "";
    (*idx)++;
    return (s[0] != '\0') ? 0 : 1;
}

static int dbr_eval_term(char **argv, const int *quoted, int argc, int *idx)
{
    int neg = 0;
    while (*idx < argc && strcmp(argv[*idx], "!") == 0) {
        neg ^= 1;
        (*idx)++;
    }
    int st = dbr_eval_primary(argv, quoted, argc, idx);
    if (st == 2) {
        return 2;
    }
    if (neg) {
        return (st == 0) ? 1 : 0;
    }
    return st;
}

static int dbr_eval_expr(char **argv, const int *quoted, int argc)
{
    if (argc <= 0) {
        return 1;
    }
    int idx = 0;
    int status = dbr_eval_term(argv, quoted, argc, &idx);
    while (idx < argc) {
        const char *op = argv[idx];
        if (strcmp(op, "&&") == 0) {
            idx++;
            if (status == 2) {
                return 2;
            }
            if (status == 0) {
                int r = dbr_eval_term(argv, quoted, argc, &idx);
                if (r == 2) {
                    return 2;
                }
                status = r;
            } else if (dbr_skip_term(argv, argc, &idx) != 0) {
                return 2;
            }
        } else if (strcmp(op, "||") == 0) {
            idx++;
            if (status == 2) {
                return 2;
            }
            if (status != 0) {
                int r = dbr_eval_term(argv, quoted, argc, &idx);
                if (r == 2) {
                    return 2;
                }
                status = r;
            } else if (dbr_skip_term(argv, argc, &idx) != 0) {
                return 2;
            }
        } else {
            fprintf(stderr, "[[: %s: operator expected\n", op);
            return 2;
        }
    }
    return status;
}

/* OSH-12: [[ ... ]] — expand_word por token; RHS unquoted de =/==/!= → glob. */
static int dispatch_dbracket(petrush_dbracket_t *db)
{
    if (!db) {
        return 2;
    }
    if (db->argc <= 0) {
        return 1;
    }

    char **argv = calloc((size_t)db->argc, sizeof(char *));
    int *quoted = calloc((size_t)db->argc, sizeof(int));
    if (!argv || !quoted) {
        free(argv);
        free(quoted);
        return 2;
    }

    for (int i = 0; i < db->argc; i++) {
        const char *raw = db->argv[i] ? db->argv[i] : "";
        char *e = expand_word(raw);
        if (!e) {
            for (int j = 0; j < i; j++) {
                free(argv[j]);
            }
            free(argv);
            free(quoted);
            return 2;
        }
        argv[i] = e;
        quoted[i] = (db->argv_quoted && db->argv_quoted[i]) ? 1 : 0;
    }
    if (petrush_take_arith_error()) {
        for (int i = 0; i < db->argc; i++) {
            free(argv[i]);
        }
        free(argv);
        free(quoted);
        return 1;
    }

    int status = dbr_eval_expr(argv, quoted, db->argc);
    for (int i = 0; i < db->argc; i++) {
        free(argv[i]);
    }
    free(argv);
    free(quoted);
    return status;
}

/*
 * OSH-16: set — special builtin.
 * sem args: dump environ "%s=%s\n"; -- / args: posicionais ($0 intacto);
 * -x/+x/-o xtrace; -C no-op; +C erro; -e/-u ainda invalid nesta fatia.
 */
static int set_apply_letter(char letter, int on)
{
    switch (letter) {
    case 'x':
        (void)petrush_shellopt_set('x', on);
        return 0;
    case 'C':
        if (!on) {
            fprintf(stderr, "petrush: set: +C: invalid option\n");
            return -1;
        }
        return 0; /* noclobber always-on */
    default:
        /* Inclui -e/-u (OSH-17/18) e letras desconhecidas. */
        fprintf(stderr, "petrush: set: %c%c: invalid option\n",
                on ? '-' : '+', letter);
        return -1;
    }
}

static int set_apply_o_name(const char *name, int on)
{
    if (!name) {
        fprintf(stderr, "petrush: set: %so: option argument expected\n",
                on ? "-" : "+");
        return -1;
    }
    if (strcmp(name, "xtrace") == 0) {
        (void)petrush_shellopt_set('x', on);
        return 0;
    }
    if (strcmp(name, "noclobber") == 0) {
        if (!on) {
            fprintf(stderr, "petrush: set: noclobber: invalid option\n");
            return -1;
        }
        return 0;
    }
    /* errexit/nounset = -e/-u: ainda unknown em OSH-16. */
    fprintf(stderr, "petrush: set: %s: invalid option\n", name);
    return -1;
}

static int set_rewrite_positionals(int argc, char **argv)
{
    const char *arg0 = petrush_positional_get(0);
    if (!arg0) {
        arg0 = "petrush";
    }
    /* strdup $0 antes do clear interno de positional_set. */
    char *saved0 = strdup(arg0);
    if (!saved0) {
        return 1;
    }
    int rc = petrush_positional_set(saved0, argc, argv);
    free(saved0);
    return (rc == 0) ? 0 : 1;
}

int builtin_set(petrush_cmd_t *cmd)
{
    if (!cmd) {
        return 1;
    }

    if (cmd->argc <= 1) {
        for (char **ep = environ; ep && *ep; ep++) {
            const char *e = *ep;
            const char *eq = strchr(e, '=');
            if (eq) {
                printf("%.*s=%s\n", (int)(eq - e), e, eq + 1);
            } else {
                printf("%s=\n", e);
            }
        }
        return 0;
    }

    int i = 1;
    int rewrite_pos = 0;
    while (i < cmd->argc) {
        const char *arg = cmd->argv[i];
        if (!arg) {
            i++;
            continue;
        }
        if (strcmp(arg, "--") == 0) {
            i++;
            rewrite_pos = 1;
            break;
        }
        if ((arg[0] == '-' || arg[0] == '+') && arg[1] != '\0') {
            int on = (arg[0] == '-');
            if (arg[1] == 'o' && arg[2] == '\0') {
                if (i + 1 >= cmd->argc) {
                    fprintf(stderr, "petrush: set: %so: option argument expected\n",
                            on ? "-" : "+");
                    shell_abort_raise();
                    return 1;
                }
                if (set_apply_o_name(cmd->argv[i + 1], on) != 0) {
                    shell_abort_raise();
                    return 1;
                }
                i += 2;
                continue;
            }
            for (const char *p = arg + 1; *p; p++) {
                if (set_apply_letter(*p, on) != 0) {
                    shell_abort_raise();
                    return 1;
                }
            }
            i++;
            continue;
        }
        rewrite_pos = 1; /* args que nao sao opcao → posicionais */
        break;
    }

    if (rewrite_pos) {
        if (set_rewrite_positionals(cmd->argc - i, cmd->argv + i) != 0) {
            shell_abort_raise();
            return 1;
        }
    }

    return 0;
}

/* OSH-2: shift [n]; default 1; n>$# → 1 e intactos; shift 0 no-op. */
int builtin_shift(petrush_cmd_t *cmd)
{
    if (!cmd) {
        return 1;
    }
    if (cmd->argc > 2) {
        fprintf(stderr, "shift: too many arguments\n");
        return 1;
    }

    unsigned n = 1;
    if (cmd->argc == 2) {
        const char *arg = cmd->argv[1];
        char *end = NULL;
        errno = 0;
        long v = strtol(arg, &end, 10);
        if (arg[0] == '\0' || end == arg || *end != '\0' || errno == ERANGE
            || v < 0) {
            fprintf(stderr, "shift: %s: numeric argument required\n", arg);
            return 1;
        }
        if (v > (long)UINT_MAX) {
            fprintf(stderr, "shift: %ld: shift count out of range\n", v);
            return 1;
        }
        n = (unsigned)v;
    }

    if (petrush_positional_shift(n) != 0) {
        fprintf(stderr, "shift: %u: shift count out of range\n", n);
        return 1;
    }
    return 0;
}

/*
 * OSH-7: return [n] so dentro de funcao.
 * Default n=0 (nao usa status do ultimo comando; documentado).
 * Fora de funcao: erro !=0, sem exit (script segue).
 */
int builtin_return(petrush_cmd_t *cmd)
{
    if (!cmd) {
        return 1;
    }
    if (g_fn_depth <= 0) {
        fprintf(stderr, "return: can only return from a function\n");
        return 1;
    }
    if (cmd->argc > 2) {
        fprintf(stderr, "return: too many arguments\n");
        return 1;
    }

    int code = 0;
    if (cmd->argc == 2) {
        const char *arg = cmd->argv[1];
        char *end = NULL;
        errno = 0;
        long v = strtol(arg, &end, 10);
        if (arg[0] == '\0' || end == arg || *end != '\0' || errno == ERANGE) {
            fprintf(stderr, "return: %s: numeric argument required\n", arg);
            return 1;
        }
        code = (int)(v & 0xff);
    }

    g_returning = 1;
    g_return_status = code;
    return code;
}

/*
 * OSH-8: local name[=value] so dentro de funcao.
 * Sem flags (local -a/-r/...). local name sem =: unset local.
 * Ao sair do frame (return ou fim do body), restaura valor anterior
 * ou unset se a var nao existia.
 */
int builtin_local(petrush_cmd_t *cmd)
{
    if (!cmd) {
        return 1;
    }
    if (g_fn_depth <= 0) {
        fprintf(stderr, "local: can only be used in a function\n");
        return 1;
    }
    if (cmd->argc < 2) {
        fprintf(stderr, "local: usage: local name[=value]...\n");
        return 1;
    }

    int ret = 0;
    for (int i = 1; i < cmd->argc; i++) {
        const char *arg = cmd->argv[i];
        if (!arg || arg[0] == '\0') {
            fprintf(stderr, "local: invalid variable name\n");
            ret = 1;
            continue;
        }
        if (arg[0] == '-') {
            fprintf(stderr, "local: options not supported\n");
            return 1;
        }

        const char *eq = strchr(arg, '=');
        char namebuf[256];
        const char *name;
        const char *val = NULL;
        int assign = 0;

        if (eq) {
            size_t nlen = (size_t)(eq - arg);
            if (nlen == 0 || nlen >= sizeof(namebuf)) {
                fprintf(stderr, "local: invalid variable name\n");
                ret = 1;
                continue;
            }
            memcpy(namebuf, arg, nlen);
            namebuf[nlen] = '\0';
            name = namebuf;
            val = eq + 1;
            assign = 1;
        } else {
            name = arg;
        }

        if (local_push(name) != 0) {
            fprintf(stderr, "local: stack full or out of memory\n");
            return 1;
        }
        if (assign) {
            if (petrush_setenv(name, val, 1) != 0) {
                perror("local");
                ret = 1;
            }
        } else {
            (void)petrush_unsetenv(name);
        }
    }
    return ret;
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

    char path[PATH_MAX];

    if (petrush_history_path(path, sizeof(path)) != 0) {
        printf("No history yet.\n");
        return 0;
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
    printf("Features: history, rc, signals, pudo, pipes, redir (noclobber), alias, PS1, complete, hints, &&/||/;/&, glob, source/., jobs, set/-x.\n");
    printf("Anti-OE: noclobber always-on; sem trap/arrays/pipefail/fg\n");
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
