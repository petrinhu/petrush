/*
 * target_open.c - Open do alvo com O_NOFOLLOW (SEC-07)
 *
 * Fail closed: symlink no componente final => open falha (ELOOP no Linux).
 * Nao enfraquece SEC-05 (allow resolve) nem SEC-06 (fstat root-exec no fd).
 */

#include "target_open.h"

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>

int pudod_target_open_flags(void)
{
    return O_RDONLY | O_CLOEXEC | O_NOFOLLOW;
}

int pudod_open_target(const char *path)
{
    if (path == NULL) {
        errno = EINVAL;
        return -1;
    }
    return open(path, pudod_target_open_flags());
}
