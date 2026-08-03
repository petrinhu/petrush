/*
 * alias.h — Aliases de comando (Mid layer)
 * NEW-22: feature UX #5 da pesquisa de shells (Fish/Zsh/Bash).
 */

#ifndef PETRUSH_ALIAS_H
#define PETRUSH_ALIAS_H

/* Define ou redefine alias. Retorna 0 ok, -1 erro. */
int alias_set(const char *name, const char *value);

/* Remove alias. Retorna 0 se removido, -1 se não existia. */
int alias_unset(const char *name);

/* Retorna valor ou NULL. Ponteiro válido até próximo set/unset/clear. */
const char *alias_get(const char *name);

/*
 * Expande a primeira palavra da linha se for alias.
 * Retorna nova string (malloc); caller free(). Nunca NULL se input != NULL
 * (em OOM retorna NULL).
 */
char *alias_expand_line(const char *line);

/* Imprime "name='value'" por linha em stdout. */
void alias_list_print(void);

/* Limpa todos (testes / shutdown). */
void alias_clear_all(void);

#endif /* PETRUSH_ALIAS_H */
