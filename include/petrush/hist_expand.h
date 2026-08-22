/*
 * hist_expand.h — expansão simples de histórico (NEW-26 + FEAT-BANG)
 * !! = último comando; !n = entrada n (1-based oldest)
 * !$ = último arg do último evento; !^ = primeiro arg (word 1)
 */

#ifndef PETRUSH_HIST_EXPAND_H
#define PETRUSH_HIST_EXPAND_H

/*
 * Se a linha for exatamente "!!", "!N", "!$" ou "!^", retorna cópia
 * expandida do history. Senão retorna NULL (sem expansão).
 * Caller free() se não NULL.
 */
char *hist_expand_line(const char *line);

#endif /* PETRUSH_HIST_EXPAND_H */
