/*
 * allow_resolve.c - Resolve entrada da allow-list (SEC-05) + shell genérico (SEC-12)
 *
 * Fail closed: se realpath falhar, recusa a linha (não copia o literal).
 * SEC-12: basename sh/bash/dash/ash/busybox é shell genérico (R-C3).
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "allow_resolve.h"

#include <limits.h>
#include <string.h>
#include <stdlib.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

int pudod_resolve_allow_entry(const char *entry, char *out, size_t outsz)
{
    if (!entry || !out || outsz == 0) {
        return -1;
    }
    if (entry[0] != '/') {
        return -1;
    }

    char canonical[PATH_MAX];
    if (realpath(entry, canonical) == NULL) {
        return -1;
    }

    size_t len = strlen(canonical);
    if (len >= outsz) {
        len = outsz - 1;
    }
    memcpy(out, canonical, len);
    out[len] = '\0';
    return 0;
}

int pudod_path_is_generic_shell(const char *canonical)
{
    if (canonical == NULL || canonical[0] == '\0') {
        return -1;
    }

    const char *slash = strrchr(canonical, '/');
    if (slash == NULL) {
        return -1;
    }

    const char *base = slash + 1;
    if (base[0] == '\0') {
        return -1;
    }

    static const char *const shells[] = {
        "sh", "bash", "dash", "ash", "busybox", NULL
    };
    for (int i = 0; shells[i] != NULL; i++) {
        if (strcmp(base, shells[i]) == 0) {
            return 1;
        }
    }
    return 0;
}
