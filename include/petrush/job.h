/*
 * job.h — Tabela de jobs em background (Foundation, UX-23)
 *
 * Teto 16. Reaper só waitpid(pid, WNOHANG) dos PIDs conhecidos
 * (sem waitpid(-1)). Sem fg/bg/Ctrl-Z/%n/wait builtin.
 */

#ifndef PETRUSH_JOB_H
#define PETRUSH_JOB_H

#include <sys/types.h>

#define PETRUSH_JOB_MAX 16

typedef enum {
    PETRUSH_JOB_RUNNING = 0,
    PETRUSH_JOB_DONE
} petrush_job_state_t;

typedef struct {
    int id;                     /* 1..PETRUSH_JOB_MAX */
    pid_t pid;
    petrush_job_state_t state;
    int status;                 /* exit status shell-style quando DONE */
    char *command;              /* owned */
    int notified;               /* 1 se Done já foi reportado ao prompt */
} petrush_job_t;

/* Registra job RUNNING. Copia command. Retorna job id (>=1) ou -1 (teto/OOM). */
int petrush_job_add(pid_t pid, const char *command);

/* Reap WNOHANG só dos PIDs da tabela. Marca DONE. */
void petrush_job_reap(void);

/*
 * Imprime notificações "[N]+ Done  cmd" dos jobs DONE ainda não notificados,
 * marca notified e remove da tabela. Chamar antes do prompt.
 */
void petrush_job_notify(void);

/* Lista jobs ainda na tabela (após reap). Formato: "[N]  Running|Done  cmd\n" */
void petrush_job_print(void);

/* Remove slots DONE sem mensagem (após `jobs` já ter listado). */
void petrush_job_prune_done(void);

/* Quantidade de slots ocupados (RUNNING + DONE não notificados). */
int petrush_job_count(void);

/*
 * Consulta slot por id (1-based). Retorna 0 se ocupado; -1 se livre/ inválido.
 * *state / *status / cmd_out opcionais (cmd_out = ponteiro interno, não free).
 */
int petrush_job_probe(int id, petrush_job_state_t *state, int *status,
                      const char **cmd_out);

/* Testes: zera tabela (libera commands). */
void petrush_job_reset_for_tests(void);

#endif /* PETRUSH_JOB_H */
