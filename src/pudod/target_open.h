/*
 * target_open.h - Open do alvo com O_NOFOLLOW (SEC-07)
 *
 * Fecha TOCTOU realpath -> open: se o componente final virar symlink
 * entre o realpath e o open, O_NOFOLLOW falha (ELOOP) em vez de seguir.
 * O fd retornado alimenta fstat (SEC-06) e fexecve (Linux).
 */

#ifndef PETRUSH_PUDOD_TARGET_OPEN_H
#define PETRUSH_PUDOD_TARGET_OPEN_H

/* Flags canônicas: O_RDONLY | O_CLOEXEC | O_NOFOLLOW */
int pudod_target_open_flags(void);

/*
 * Abre path com pudod_target_open_flags().
 * Retorna fd >= 0 ou -1 (path NULL, open falhou, symlink final, etc.).
 */
int pudod_open_target(const char *path);

#endif /* PETRUSH_PUDOD_TARGET_OPEN_H */
