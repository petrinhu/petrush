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

/*
 * SEC-02: política de aceitação do path do helper pudod.
 * release_mode != 0: só path absoluto em localização de install confiável.
 * release_mode == 0: também aceita absolutos de build e fallbacks relativos de debug.
 * Fail closed: NULL/vazio/desconhecido → 0.
 */
int pudo_allow_pudod_candidate(const char *path, int release_mode);

/*
 * SEC-04: o argv montado para o helper cabe em buffer[PUDO_HELPER_ARGV_MAX]?
 * via_sudo != 0: layout sudo + "--" + cmd->argv[1..]
 * via_sudo == 0: layout pudod + target + cmd->argv[2..]
 * Retorna 1 se cabe (com room para NULL), 0 se excederia (fail closed).
 */
#define PUDO_HELPER_ARGV_MAX 128
int pudo_helper_argv_fits(int cmd_argc, int via_sudo);

#endif /* PETRUSH_PUDO_H */
