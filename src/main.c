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

#include "petrush/petrush.h"
#include "petrush/parser.h"
#include "petrush/dispatcher.h"
#include "petrush/process.h"
#include "petrush/env.h"
#include "petrush/alias.h"
#include "petrush/complete.h"
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

/* Carrega e executa ~/.petrushrc (se existir) */
static void load_rc_file(void)
{
    const char *rcfile = get_rc_file();
    FILE *f = fopen(rcfile, "r");
    if (!f) {
        /* Arquivo não existe é normal — não é erro */
        return;
    }

    char linebuf[4096];
    int lineno = 0;

    while (fgets(linebuf, sizeof(linebuf), f)) {
        lineno++;

        /* Remove newline */
        size_t len = strlen(linebuf);
        if (len > 0 && linebuf[len-1] == '\n') {
            linebuf[len-1] = '\0';
        }

        /* Ignora linhas vazias e comentários */
        char *start = linebuf;
        while (*start == ' ' || *start == '\t') {
            start++;
        }
        if (*start == '\0' || *start == '#') {
            continue;
        }

        /* Executa a linha como lista/pipeline (rc); expande alias */
        char *expanded = alias_expand_line(start);
        const char *to_run = expanded ? expanded : start;
        petrush_list_t list = {0};
        if (petrush_parse_list(to_run, &list) == 0) {
            if (list.nitems > 0) {
                (void)dispatch_list(&list);
            }
        } else {
            fprintf(stderr, "petrush: erro no rc (linha %d): %s\n", lineno, start);
        }
        petrush_list_free(&list);
        free(expanded);
    }

    fclose(f);
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
    (void)argc;
    (void)argv;

    /* line-buffered mesmo em pipe (smoke/CI): evita vazar banner em redirs */
    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IOLBF, 0);

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
        /* PETRUSH_PS1: prompt customizável (feature #4 da pesquisa UX) */
        const char *ps1 = petrush_getenv("PETRUSH_PS1");
        const char *prompt = (ps1 && ps1[0]) ? ps1 : "petrush> ";
        line = linenoise(prompt);

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
            linenoiseHistoryAdd(line);

            char *expanded = alias_expand_line(line);
            const char *to_run = expanded ? expanded : line;

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
        }

        free(line);
    }

    /* Salva histórico antes de sair (PR-07) */
    linenoiseHistorySave(histfile);

    printf("saindo...\n");
    return EXIT_SUCCESS;
}
