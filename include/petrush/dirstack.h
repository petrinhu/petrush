/*
 * dirstack.h — pushd / popd / dirs (NEW-25)
 * Navegação amada em bash/zsh; stack simples.
 */

#ifndef PETRUSH_DIRSTACK_H
#define PETRUSH_DIRSTACK_H

/* Salva cwd atual e chdir(path). Retorna 0 ok, -1 erro. */
int dirstack_pushd(const char *path);

/* Volta ao topo da stack. Retorna 0 ok, -1 se vazia/erro. */
int dirstack_popd(void);

/* Número de entradas na stack (não inclui cwd atual). */
int dirstack_size(void);

/* Imprime stack (uma por linha) + cwd. */
void dirstack_print(void);

/* Limpa stack sem mudar cwd (testes). */
void dirstack_clear(void);

#endif /* PETRUSH_DIRSTACK_H */
