/*
 * job.c — Tabela de jobs em background (Foundation, UX-23)
 */

#include "petrush/job.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

static petrush_job_t g_jobs[PETRUSH_JOB_MAX];
static int g_used[PETRUSH_JOB_MAX]; /* 1 se slot ocupado */

static int status_to_code(int status)
{
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    if (WIFSTOPPED(status)) return 128 + WSTOPSIG(status);
    return 1;
}

static void clear_slot(int i)
{
    free(g_jobs[i].command);
    memset(&g_jobs[i], 0, sizeof(g_jobs[i]));
    g_used[i] = 0;
}

void petrush_job_reset_for_tests(void)
{
    for (int i = 0; i < PETRUSH_JOB_MAX; i++) {
        if (g_used[i]) clear_slot(i);
    }
}

int petrush_job_count(void)
{
    int n = 0;
    for (int i = 0; i < PETRUSH_JOB_MAX; i++) {
        if (g_used[i]) n++;
    }
    return n;
}

int petrush_job_add(pid_t pid, const char *command)
{
    if (pid <= 0) return -1;
    int slot = -1;
    for (int i = 0; i < PETRUSH_JOB_MAX; i++) {
        if (!g_used[i]) {
            slot = i;
            break;
        }
    }
    if (slot < 0) return -1;

    char *cmd = NULL;
    if (command && command[0]) {
        cmd = strdup(command);
        if (!cmd) return -1;
    } else {
        cmd = strdup("?");
        if (!cmd) return -1;
    }

    g_jobs[slot].id = slot + 1;
    g_jobs[slot].pid = pid;
    g_jobs[slot].state = PETRUSH_JOB_RUNNING;
    g_jobs[slot].status = 0;
    g_jobs[slot].command = cmd;
    g_jobs[slot].notified = 0;
    g_used[slot] = 1;
    return g_jobs[slot].id;
}

void petrush_job_reap(void)
{
    for (int i = 0; i < PETRUSH_JOB_MAX; i++) {
        if (!g_used[i]) continue;
        if (g_jobs[i].state != PETRUSH_JOB_RUNNING) continue;

        int st = 0;
        pid_t r = waitpid(g_jobs[i].pid, &st, WNOHANG);
        if (r == g_jobs[i].pid) {
            g_jobs[i].state = PETRUSH_JOB_DONE;
            g_jobs[i].status = status_to_code(st);
        } else if (r < 0) {
            /* ECHILD: já reaped por outro caminho — trate como Done. */
            g_jobs[i].state = PETRUSH_JOB_DONE;
            g_jobs[i].status = 0;
        }
    }
}

void petrush_job_notify(void)
{
    for (int i = 0; i < PETRUSH_JOB_MAX; i++) {
        if (!g_used[i]) continue;
        if (g_jobs[i].state != PETRUSH_JOB_DONE) continue;
        if (g_jobs[i].notified) {
            clear_slot(i);
            continue;
        }
        printf("[%d]+ Done  %s\n", g_jobs[i].id,
               g_jobs[i].command ? g_jobs[i].command : "?");
        fflush(stdout);
        clear_slot(i);
    }
}

void petrush_job_print(void)
{
    for (int i = 0; i < PETRUSH_JOB_MAX; i++) {
        if (!g_used[i]) continue;
        const char *st =
            (g_jobs[i].state == PETRUSH_JOB_RUNNING) ? "Running" : "Done";
        printf("[%d]  %s  %s\n", g_jobs[i].id, st,
               g_jobs[i].command ? g_jobs[i].command : "?");
    }
    fflush(stdout);
}

void petrush_job_prune_done(void)
{
    for (int i = 0; i < PETRUSH_JOB_MAX; i++) {
        if (!g_used[i]) continue;
        if (g_jobs[i].state == PETRUSH_JOB_DONE) clear_slot(i);
    }
}

int petrush_job_probe(int id, petrush_job_state_t *state, int *status,
                      const char **cmd_out)
{
    if (id < 1 || id > PETRUSH_JOB_MAX) return -1;
    int i = id - 1;
    if (!g_used[i]) return -1;
    if (state) *state = g_jobs[i].state;
    if (status) *status = g_jobs[i].status;
    if (cmd_out) *cmd_out = g_jobs[i].command;
    return 0;
}
