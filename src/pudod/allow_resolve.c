/*
 * allow_resolve.c — Resolve entrada da allow-list (SEC-05)
 *
 * Fail closed: se realpath falhar, recusa a linha (não copia o literal).
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
