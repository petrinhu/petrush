/*
 * source.c - source / . no processo atual (UX-22)
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
            if (list.nitems > 0) {
                status = dispatch_list(&list);
            }
        } else {
            fprintf(stderr, "petrush: erro em %s (linha %d): %s\n",
                    path, lineno, start);
            status = 1;
        }
        petrush_list_free(&list);
        free(expanded);
    }

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
