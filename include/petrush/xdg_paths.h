/*
 * xdg_paths.h - XDG-1: rc + history paths (Foundation)
 *
 * rc:      $XDG_CONFIG_HOME/petrush/rc  else ~/.config/petrush/rc
 * history: $XDG_STATE_HOME/petrush/history else ~/.local/state/petrush/history
 * Compat: ~/.petrushrc / ~/.petrush_history se o caminho XDG ainda nao existir.
 */

#ifndef PETRUSH_XDG_PATHS_H
#define PETRUSH_XDG_PATHS_H

#include <stddef.h>

/*
 * Resolve o path do rc interativo em buf (NUL-terminated).
 * Retorna 0 em sucesso, -1 se buf for curto ou argumentos invalidos.
 */
int petrush_rc_path(char *buf, size_t buflen);

/*
 * Resolve o path do ficheiro de history em buf.
 * Retorna 0 em sucesso, -1 se buf for curto ou argumentos invalidos.
 */
int petrush_history_path(char *buf, size_t buflen);

/*
 * mkdir -p do diretorio pai de hist_path com mode 0700 (XDG-1).
 * Chamar antes de linenoiseHistorySave.
 * Retorna 0 em sucesso, -1 em erro.
 */
int petrush_history_ensure_dir(const char *hist_path);

#endif /* PETRUSH_XDG_PATHS_H */
