/*
 * hist_expand.h — expansão simples de histórico (NEW-26)
 * !! = último comando; !n = entrada n (1-based oldest)
 */

#ifndef PETRUSH_HIST_EXPAND_H
#define PETRUSH_HIST_EXPAND_H

/*
 * Se a linha for exatamente "!!" ou "!N", retorna cópia do history.
 * Senão retorna NULL (sem expansão).
 * Caller free() se não NULL.
 */
char *hist_expand_line(const char *line);

#endif /* PETRUSH_HIST_EXPAND_H */
