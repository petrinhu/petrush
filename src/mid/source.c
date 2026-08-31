/*
 * source.c - source / . (UX-22) + script mode (OSH-0/1)
 */

#include "petrush/source.h"

#include "petrush/alias.h"
#include "petrush/dispatcher.h"
#include "petrush/rc_trust.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int g_source_depth;

/*
 * OSH-13: apos parse, enche here-docs pendentes (corpo nao tokeniza; # literal).
 * Retorno: 0 ok; 1 erro; 2 EOF do ficheiro (caller nao deve fgets de novo).
 */
static int fill_heredocs(FILE *f, const char *path, int *lineno,
                         petrush_list_t *list)
{
    char linebuf[4096];
    while (petrush_list_heredoc_pending(list)) {
        if (!fgets(linebuf, sizeof(linebuf), f)) {
            if (ferror(f)) {
                fprintf(stderr, "petrush: %s: erro de leitura (linha %d): %s\n",
                        path, *lineno, strerror(errno));
                return 1;
            }
            /* feof: sinaliza EOF ao fill; nao voltar a ler no runner */
            if (petrush_heredoc_feed_line(list, NULL) < 0) {
                fprintf(stderr,
                        "petrush: %s: here-document delimitado sem fim"
                        " (linha %d)\n",
                        path, *lineno);
                return 1;
            }
            return 2;
        }
        (*lineno)++;
        size_t len = strlen(linebuf);
        if (len > 0 && linebuf[len - 1] == '\n') {
            linebuf[len - 1] = '\0';
        }
        int fr = petrush_heredoc_feed_line(list, linebuf);
        if (fr < 0) {
            fprintf(stderr,
                    "petrush: %s: here-document overflow ou erro (linha %d)\n",
                    path, *lineno);
            return 1;
        }
    }
    return 0;
}

/* Runner compartilhado: linha a linha, #/vazio skip, alias+parse_list+dispatch. */
static int run_file_lines(FILE *f, const char *path)
{
    char linebuf[4096];
    int lineno = 0;
    int status = 0;

    while (fgets(linebuf, sizeof(linebuf), f)) {
        lineno++;

        size_t len = strlen(linebuf);
        if (len > 0 && linebuf[len - 1] == '\n') {
            linebuf[len - 1] = '\0';
        }

        char *start = linebuf;
        while (*start == ' ' || *start == '\t') {
            start++;
        }
        if (*start == '\0' || *start == '#') {
            continue;
        }

        char *expanded = alias_expand_line(start);
        const char *to_run = expanded ? expanded : start;
        petrush_list_t list = {0};
        if (petrush_parse_list(to_run, &list) == 0) {
            int fill_rc = 0;
            if (petrush_list_heredoc_pending(&list)) {
                fill_rc = fill_heredocs(f, path, &lineno, &list);
                if (fill_rc == 1) {
                    status = 1;
                    petrush_list_free(&list);
                    free(expanded);
                    return status;
                }
            }
            if (list.nitems > 0) {
                status = dispatch_list(&list);
            }
            petrush_list_free(&list);
            free(expanded);
            if (fill_rc == 2) {
                break; /* EOF ja consumido no fill */
            }
            continue;
        }
        fprintf(stderr, "petrush: erro em %s (linha %d): %s\n",
                path, lineno, start);
        status = 1;
        petrush_list_free(&list);
        free(expanded);
    }

    return status;
}

int petrush_source_file(const char *path, int missing_ok)
{
    if (!path || !*path) {
        fprintf(stderr, "petrush: source: caminho vazio\n");
        return 1;
    }

    if (g_source_depth >= PETRUSH_SOURCE_MAX_DEPTH) {
        fprintf(stderr,
                "petrush: source: nesting too deep (max %d)\n",
                PETRUSH_SOURCE_MAX_DEPTH);
        return 1;
    }

    FILE *f = fopen(path, "r");
    if (!f) {
        if (missing_ok && errno == ENOENT) {
            return 0;
        }
        fprintf(stderr, "petrush: source: %s: %s\n", path, strerror(errno));
        return 1;
    }

    struct stat st;
    if (fstat(fileno(f), &st) != 0) {
        fprintf(stderr, "petrush: source: erro ao inspecionar %s: %s\n",
                path, strerror(errno));
        fclose(f);
        return 1;
    }
    if (petrush_rc_stat_ok(&st, getuid()) != 0) {
        fprintf(stderr,
                "petrush: recusando arquivo inseguro %s "
                "(nao regular, uid!=getuid() ou mode&0022)\n",
                path);
        fclose(f);
        return 1;
    }

    g_source_depth++;
    int status = run_file_lines(f, path);
    fclose(f);
    g_source_depth--;
    return status;
}

int petrush_run_script(const char *path)
{
    if (!path || !*path) {
        fprintf(stderr, "petrush: caminho de script vazio\n");
        return 1;
    }

    FILE *f = fopen(path, "r");
    if (!f) {
        if (errno == ENOENT) {
            fprintf(stderr, "petrush: %s: No such file or directory\n", path);
            return 127;
        }
        fprintf(stderr, "petrush: %s: %s\n", path, strerror(errno));
        return 1;
    }

    struct stat st;
    if (fstat(fileno(f), &st) != 0) {
        fprintf(stderr, "petrush: erro ao inspecionar %s: %s\n",
                path, strerror(errno));
        fclose(f);
        return 1;
    }
    /* OSH-0: so regular+legivel. Sem SEC-10 mode&0022 (quebra /tmp de teste). */
    if (!S_ISREG(st.st_mode)) {
        fprintf(stderr, "petrush: %s: not a regular file\n", path);
        fclose(f);
        return 1;
    }

    if (g_source_depth >= PETRUSH_SOURCE_MAX_DEPTH) {
        fprintf(stderr,
                "petrush: source: nesting too deep (max %d)\n",
                PETRUSH_SOURCE_MAX_DEPTH);
        fclose(f);
        return 1;
    }

    g_source_depth++;
    int status = run_file_lines(f, path);
    fclose(f);
    g_source_depth--;
    return status;
}

int builtin_source(petrush_cmd_t *cmd)
{
    if (!cmd || cmd->argc != 2 || !cmd->argv || !cmd->argv[1]) {
        fprintf(stderr, "source: usage: source file\n");
        return 1;
    }
    return petrush_source_file(cmd->argv[1], 0);
}
