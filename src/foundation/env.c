/*
 * env.c — Implementação do gerenciamento básico de ambiente (Foundation)
 */

#include "petrush/env.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>

const char *petrush_getenv(const char *name)
{
    if (!name) return NULL;
    return getenv(name);
}

int petrush_setenv(const char *name, const char *value, int overwrite)
{
    if (!name || !value) {
        errno = EINVAL;
        return -1;
    }

    /* Usa setenv da libc (POSIX.1-2001 / C99) */
    return setenv(name, value, overwrite);
}

int petrush_unsetenv(const char *name)
{
    if (!name) {
        errno = EINVAL;
        return -1;
    }

    return unsetenv(name);
}
