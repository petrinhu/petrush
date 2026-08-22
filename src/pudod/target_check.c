/*
 * target_check.c - Validacao do alvo apos fstat (SEC-06)
 *
 * Fail closed: regular + st_uid==0 + algum bit de exec (0111).
 */

#include "target_check.h"

#include <stddef.h>

int pudod_target_is_root_exec(const struct stat *st)
{
    if (st == NULL) {
        return -1;
    }
    if (!S_ISREG(st->st_mode)) {
        return -1;
    }
    if (st->st_uid != 0) {
        return -1;
    }
    if ((st->st_mode & 0111) == 0) {
        return -1;
    }
    return 0;
}
