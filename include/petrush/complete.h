/*
 * complete.h — Tab completion + history hints (Front/Mid UX)
 * NEW-23: features #1 e #2 da pesquisa (Fish/Zsh).
 */

#ifndef PETRUSH_COMPLETE_H
#define PETRUSH_COMPLETE_H

/* Registra callbacks linenoise (completion + hints). Chamar uma vez no startup.
 * Declaração sem expor linenoiseCompletions / linenoise.h (ARCH-01 / R-I13). */
void petrush_setup_linenoise_ux(void);

/* Testável: quantos completions seriam gerados para buf (prefixo). */
int petrush_complete_count(const char *buf);

/* Testável: encontra hint de history (sufixo após buf) ou NULL (caller free se free_hint). */
char *petrush_history_hint(const char *buf);

#endif /* PETRUSH_COMPLETE_H */
