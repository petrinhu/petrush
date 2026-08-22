/*
 * rc_trust.h - Confianca do ~/.petrushrc (SEC-10)
 *
 * Fail closed: regular, st_uid == self_uid, sem write de group/other (0022).
 */

#ifndef PETRUSH_RC_TRUST_H
#define PETRUSH_RC_TRUST_H

#include <sys/stat.h>
#include <sys/types.h>

/*
 * Retorna 0 se st e seguro para carregar como rc do usuario self_uid.
 * Retorna -1 se st for NULL ou se qualquer invariante falhar.
 */
int petrush_rc_stat_ok(const struct stat *st, uid_t self_uid);

#endif /* PETRUSH_RC_TRUST_H */
