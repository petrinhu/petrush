/*
 * prompt.h — render PETRUSH_PS1 with escapes (UX-15)
 * \w cwd  \u user  \h hostname  \n newline  \\ backslash  \$ prompt char
 */

#ifndef PETRUSH_PROMPT_H
#define PETRUSH_PROMPT_H

#include <stddef.h>

/* Renderiza PS1 (ou default). Buffer out[out_sz]. Retorna out. */
char *prompt_render(const char *ps1, char *out, size_t out_sz);

#endif /* PETRUSH_PROMPT_H */
