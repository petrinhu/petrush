/*
 * rc_trust.c - Validacao de uid/mode do ~/.petrushrc (SEC-10)
 *
 * Fail closed: regular + dono == self_uid + sem write group/other (0022).
 */

#include "petrush/rc_trust.h"

#include <stddef.h>

int petrush_rc_stat_ok(const struct stat *st, uid_t self_uid)
{
    if (st == NULL) {
        return -1;
    }
    if (!S_ISREG(st->st_mode)) {
        return -1;
    }
    if (st->st_uid != self_uid) {
        return -1;
    }
    if ((st->st_mode & 0022) != 0) {
        return -1;
    }
    return 0;
}
