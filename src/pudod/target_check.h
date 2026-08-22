/*
 * target_check.h - Validacao do alvo apos fstat (SEC-06)
 *
 * Fail closed: só regular, root-owned (st_uid==0) e com bit de exec (0111).
 */

#ifndef PETRUSH_PUDOD_TARGET_CHECK_H
#define PETRUSH_PUDOD_TARGET_CHECK_H

#include <sys/stat.h>

/*
 * Retorna 0 se st é arquivo regular, dono root e com algum bit de exec.
 * Retorna -1 se st for NULL ou se qualquer invariante falhar.
 */
int pudod_target_is_root_exec(const struct stat *st);

#endif /* PETRUSH_PUDOD_TARGET_CHECK_H */
