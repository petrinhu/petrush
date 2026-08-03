/*
 * dirstack.c — directory stack for pushd/popd
 */

#include "petrush/dirstack.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>

#define DIRSTACK_MAX 32

static char g_stack[DIRSTACK_MAX][PATH_MAX];
static int g_size;

void dirstack_clear(void)
{
    g_size = 0;
}

int dirstack_size(void)
{
    return g_size;
}

int dirstack_pushd(const char *path)
{
    if (!path || !*path) return -1;
    if (g_size >= DIRSTACK_MAX) {
        fprintf(stderr, "pushd: stack full\n");
        return -1;
    }

    char cwd[PATH_MAX];
    if (!getcwd(cwd, sizeof(cwd))) {
        perror("pushd: getcwd");
        return -1;
    }

    if (chdir(path) != 0) {
        fprintf(stderr, "pushd: %s: %s\n", path, strerror(errno));
        return -1;
    }

    snprintf(g_stack[g_size], sizeof(g_stack[g_size]), "%s", cwd);
    g_size++;
    return 0;
}

int dirstack_popd(void)
{
    if (g_size <= 0) {
        fprintf(stderr, "popd: directory stack empty\n");
        return -1;
    }
    g_size--;
    if (chdir(g_stack[g_size]) != 0) {
        fprintf(stderr, "popd: %s: %s\n", g_stack[g_size], strerror(errno));
        return -1;
    }
    return 0;
}

void dirstack_print(void)
{
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd))) {
        printf("%s\n", cwd);
    }
    for (int i = g_size - 1; i >= 0; i--) {
        printf("%s\n", g_stack[i]);
    }
}
