/*
 * test_pipeline_builtin.c — TDD UX-19: builtins no pipeline (fork por estágio)
 *
 * Casos do plano CTO: 1-7 e 9 (8 omitido).
 * ncmds>=2: cada estágio em filho; builtin via hook; pai intacto.
 * ncmds==1: cd no pai (caminho único intocado).
 */

#include "acutest.h"
#include "petrush/dispatcher.h"
#include "petrush/env.h"
#include "petrush/parser.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int run_pipeline(const char *line, int *status_out)
{
    petrush_pipeline_t pl = {0};
    if (petrush_parse_pipeline(line, &pl) != 0) {
        petrush_pipeline_free(&pl);
        return -1;
    }
    int st = dispatch_pipeline(&pl);
    if (status_out) *status_out = st;
    petrush_pipeline_free(&pl);
    return 0;
}

/* Captura stdout do dispatch_pipeline (último estágio escreve no stdout do pai). */
static int run_pipeline_capture(const char *line, char *out, size_t outsz,
                                int *status_out)
{
    petrush_pipeline_t pl = {0};
    if (petrush_parse_pipeline(line, &pl) != 0) {
        petrush_pipeline_free(&pl);
        return -1;
    }

    int fds[2];
    if (pipe(fds) != 0) {
        petrush_pipeline_free(&pl);
        return -1;
    }
    int saved = dup(STDOUT_FILENO);
    if (saved < 0) {
        close(fds[0]);
        close(fds[1]);
        petrush_pipeline_free(&pl);
        return -1;
    }
    fflush(stdout);
    if (dup2(fds[1], STDOUT_FILENO) < 0) {
        close(saved);
        close(fds[0]);
        close(fds[1]);
        petrush_pipeline_free(&pl);
        return -1;
    }
    close(fds[1]);

    int st = dispatch_pipeline(&pl);
    fflush(stdout);
    dup2(saved, STDOUT_FILENO);
    close(saved);

    if (status_out) *status_out = st;

    ssize_t n = read(fds[0], out, outsz > 0 ? outsz - 1 : 0);
    close(fds[0]);
    if (n < 0) n = 0;
    if (outsz > 0) out[n] = '\0';

    petrush_pipeline_free(&pl);
    return 0;
}

/* 1: echo | cat */
void test_echo_pipe_cat(void)
{
    char buf[256] = {0};
    int status = -1;
    TEST_CHECK(run_pipeline_capture("echo hello-ux19 | /bin/cat",
                                    buf, sizeof(buf), &status) == 0);
    TEST_CHECK(status == 0);
    TEST_CHECK(strstr(buf, "hello-ux19") != NULL);
}

/* 2: pwd | cat */
void test_pwd_pipe_cat(void)
{
    char cwd[PATH_MAX];
    TEST_CHECK(getcwd(cwd, sizeof(cwd)) != NULL);

    char buf[PATH_MAX + 64] = {0};
    int status = -1;
    TEST_CHECK(run_pipeline_capture("pwd | /bin/cat",
                                    buf, sizeof(buf), &status) == 0);
    TEST_CHECK(status == 0);
    TEST_CHECK(strstr(buf, cwd) != NULL);
}

/* 3: cd no pipe não muda o pai */
void test_cd_in_pipe_keeps_parent_cwd(void)
{
    char before[PATH_MAX], after[PATH_MAX];
    TEST_CHECK(getcwd(before, sizeof(before)) != NULL);

    int status = -1;
    TEST_CHECK(run_pipeline("cd /tmp | /bin/true", &status) == 0);
    TEST_CHECK(status == 0);

    TEST_CHECK(getcwd(after, sizeof(after)) != NULL);
    TEST_CHECK(strcmp(before, after) == 0);
}

/* 4: export no pipe não vaza para o pai */
void test_export_in_pipe_does_not_leak(void)
{
    const char *name = "UX19_EXPORT_PIPE";
    petrush_unsetenv(name);
    TEST_CHECK(petrush_getenv(name) == NULL);

    int status = -1;
    TEST_CHECK(run_pipeline("export UX19_EXPORT_PIPE=leaked | /bin/true",
                            &status) == 0);
    TEST_CHECK(status == 0);
    TEST_CHECK(petrush_getenv(name) == NULL);
}

/* 5: exit no pipe não mata o processo de teste */
void test_exit_in_pipe_does_not_kill_test(void)
{
    int status = -1;
    TEST_CHECK(run_pipeline("exit | /bin/true", &status) == 0);
    /* se chegamos aqui, exit ficou no filho */
    TEST_CHECK(status == 0);
}

/* 6: status = último estágio */
void test_status_is_last_stage(void)
{
    int status = -1;
    TEST_CHECK(run_pipeline("echo ok | /bin/false", &status) == 0);
    TEST_CHECK(status == 1);

    status = -1;
    TEST_CHECK(run_pipeline("echo ok | /bin/true", &status) == 0);
    TEST_CHECK(status == 0);
}

/* 7: três estágios */
void test_three_stages_echo_cat_cat(void)
{
    char buf[256] = {0};
    int status = -1;
    TEST_CHECK(run_pipeline_capture("echo three-ux19 | /bin/cat | /bin/cat",
                                    buf, sizeof(buf), &status) == 0);
    TEST_CHECK(status == 0);
    TEST_CHECK(strstr(buf, "three-ux19") != NULL);
}

/* 9: estágio único cd ainda muda o pai */
void test_single_stage_cd_changes_parent(void)
{
    char before[PATH_MAX], after[PATH_MAX];
    TEST_CHECK(getcwd(before, sizeof(before)) != NULL);

    int status = -1;
    TEST_CHECK(run_pipeline("cd /tmp", &status) == 0);
    TEST_CHECK(status == 0);

    TEST_CHECK(getcwd(after, sizeof(after)) != NULL);
    TEST_CHECK(strcmp(after, "/tmp") == 0);

    TEST_CHECK(chdir(before) == 0);
}

TEST_LIST = {
    { "echo_pipe_cat",                    test_echo_pipe_cat },
    { "pwd_pipe_cat",                     test_pwd_pipe_cat },
    { "cd_in_pipe_keeps_parent_cwd",      test_cd_in_pipe_keeps_parent_cwd },
    { "export_in_pipe_does_not_leak",     test_export_in_pipe_does_not_leak },
    { "exit_in_pipe_does_not_kill_test",  test_exit_in_pipe_does_not_kill_test },
    { "status_is_last_stage",             test_status_is_last_stage },
    { "three_stages_echo_cat_cat",        test_three_stages_echo_cat_cat },
    { "single_stage_cd_changes_parent",   test_single_stage_cd_changes_parent },
    { NULL, NULL }
};
