/*
 * pudo.h — Frontend do builtin 'pudo' (similar ao sudo)
 *
 * AVISO DE SEGURANÇA:
 * Este módulo deve ser tratado com extremo cuidado.
 * Toda lógica de elevação de privilégio real deve acontecer
 * fora deste processo (via /usr/bin/sudo ou helper setuid/setcap).
 *
 * Este código roda SEM privilégios elevados.
 */

#ifndef PETRUSH_PUDO_H
#define PETRUSH_PUDO_H

#include "petrush/parser.h"

/* Ponto de entrada do builtin */
int builtin_pudo(petrush_cmd_t *cmd);

/*
 * Expõe a função de sanitização para testes de segurança.
 * Não deve ser usada em código de produção fora de testes.
 */
int pudo_sanitize_environment(void);

#endif /* PETRUSH_PUDO_H */
