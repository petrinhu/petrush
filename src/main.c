/*
 * petrush - Shell interativo em C23
 * Opção A escolhida (REPL clássico)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>

#include "petrush/petrush.h"
#include "petrush/parser.h"
#include "petrush/dispatcher.h"
#include "petrush/process.h"
#include "petrush/job.h"
#include "petrush/env.h"
#include "petrush/alias.h"
#include "petrush/complete.h"
#include "petrush/hist_expand.h"
#include "petrush/prompt.h"
#include "petrush/rc_trust.h"
#include "petrush/source.h"
#include "petrush/i18n.h"
#include "linenoise.h"

/* Caminho padrão para histórico persistente */
#define PETRUSH_HISTORY_MAXLEN 1000

/* Nome do arquivo de configuração (rc) — sem o ponto inicial (adicionado no get_rc_file) */
#define PETRUSH_RC_FILE "petrushrc"

static const char *get_history_file(void)
{
    static char path[PATH_MAX];
    const char *home = petrush_getenv("HOME");

    if (home && *home) {
        snprintf(path, sizeof(path), "%s/.petrush_history", home);
    } else {
        /* Fallback seguro se HOME não estiver definido */
        snprintf(path, sizeof(path), ".petrush_history");
        fprintf(stderr, "petrush: aviso: HOME não definido, usando ./.petrush_history\n");
    }
    return path;
}

static const char *get_rc_file(void)
{
    static char path[PATH_MAX];
    const char *home = petrush_getenv("HOME");

    if (home && *home) {
        snprintf(path, sizeof(path), "%s/.%s", home, PETRUSH_RC_FILE);
    } else {
        snprintf(path, sizeof(path), ".%s", PETRUSH_RC_FILE);
    }
    return path;
}

/* Carrega e executa ~/.petrushrc (se existir) via runner UX-22 */
static void load_rc_file(void)
{
    (void)petrush_source_file(get_rc_file(), 1);
}

/* ==================== PR-09: Tratamento robusto de sinais ==================== */

/* Salva histórico e sai de forma limpa em SIGTERM/SIGHUP */
static void cleanup_and_exit(int signum)
{
    /* Tenta salvar o histórico de forma mais segura possível */
    const char *histfile = get_history_file();
    if (histfile) {
        linenoiseHistorySave(histfile);
    }

    if (signum > 0) {
        fprintf(stderr, "\npetrush: recebido sinal %d, saindo...\n", signum);
    }

    _exit(0);
}

/* Handler para sinais de término */
static void handle_terminate(int signum)
{
    cleanup_and_exit(signum);
}

/* Handler vazio para SIGINT (deixa o read retornar EINTR) */
static void sigint_handler(int signum)
{
    (void)signum;
    /* Não faz nada — o importante é gerar EINTR no linenoise */
}

/* ======================================================================== */

int main(int argc, char *argv[])
{
    /* line-buffered mesmo em pipe (smoke/CI): evita vazar banner em redirs */
    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IOLBF, 0);

    /* I18N-GETTEXT: bindtextdomain before any user-visible string path.
     * Catalogs msgid=en; ASM never calls gettext. */
    (void)petrush_i18n_init(NULL);

    /* OSH-0: petrush arquivo → script mode (shebang). Sem banner/linenoise/rc.
     * Posicionais $1..$n fora desta fatia (argv[2+] ignorados). */
    if (argc >= 2 && argv[1] && argv[1][0] != '\0') {
        petrush_init_shell_termios();
        return petrush_run_script(argv[1]);
    }

    printf("%s %s (C23 shell)\n", PETRUSH_NAME, PETRUSH_VERSION);
    printf("Digite 'exit' ou 'quit' para sair.\n\n");
    fflush(stdout);

    /* Salva o estado original do terminal para restauração após comandos externos
     * que modificam o termios (vim, less, etc.). Essencial para job control correto. */
    petrush_init_shell_termios();

    /* ==================== PR-09: Tratamento robusto de sinais ==================== */

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;   /* Importante: sem SA_RESTART para capturarmos EINTR */

    /* Handler vazio para SIGINT — permite que linenoise retorne com EINTR */
    sa.sa_handler = sigint_handler;
    sigaction(SIGINT, &sa, NULL);

    /* Handlers para término gracioso (SIGTERM / SIGHUP) */
    sa.sa_handler = handle_terminate;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);

    /* Ignora SIGTSTP (Ctrl-Z) no nível do shell (job control ainda mínimo) */
    sa.sa_handler = SIG_IGN;
    sigaction(SIGTSTP, &sa, NULL);

    /* ======================================================================== */

    /* Histórico persistente (PR-07) */
    const char *histfile = get_history_file();
    linenoiseHistorySetMaxLen(PETRUSH_HISTORY_MAXLEN);
    linenoiseHistoryLoad(histfile);

    /* Tab-complete + history autosuggest (NEW-23) */
    petrush_setup_linenoise_ux();

    /* Executa configuração do usuário (~/.petrushrc) antes do loop interativo */
    load_rc_file();

    /* Loop principal do REPL com tratamento robusto de SIGINT (PR-09) */
    char *line;
    while (1) {
        errno = 0;
        /* UX-23: reap + notify Done antes do prompt */
        petrush_job_reap();
        petrush_job_notify();
        /* PETRUSH_PS1 + escapes \w \u \h \n \$ \\ (UX-15) */
        const char *ps1 = petrush_getenv("PETRUSH_PS1");
        char prompt_buf[512];
        prompt_render(ps1, prompt_buf, sizeof(prompt_buf));
        line = linenoise(prompt_buf);

        if (line == NULL) {
            if (errno == EINTR) {
                /* Ctrl-C no prompt: imprime newline e redisplay o prompt */
                printf("\n");
                continue;
            }
            /* EOF real (Ctrl-D) ou outro erro */
            break;
        }

        if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) {
            free(line);
            break;
        }

        if (strlen(line) > 0) {
            /* !! / !n / !$ / !^ antes de history add (não gravar o bang cru) */
            char *hist_exp = hist_expand_line(line);
            const char *after_hist = hist_exp ? hist_exp : line;
            if (hist_exp) {
                printf("%s\n", hist_exp); /* como bash: ecoa expansão */
            }

            linenoiseHistoryAdd(after_hist);

            char *expanded = alias_expand_line(after_hist);
            const char *to_run = expanded ? expanded : after_hist;

            petrush_list_t list = {0};
            if (petrush_parse_list(to_run, &list) == 0) {
                if (list.nitems > 0) {
                    dispatch_list(&list);
                }
            } else {
                fprintf(stderr, "petrush: erro ao analisar comando\n");
            }
            petrush_list_free(&list);
            free(expanded);
            free(hist_exp);
        }

        free(line);
    }

    /* Salva histórico antes de sair (PR-07) */
    linenoiseHistorySave(histfile);

    printf("saindo...\n");
    return EXIT_SUCCESS;
}
