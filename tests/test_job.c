/*
 * test_job.c — TDD UX-23 tabela de jobs (Foundation)
 * RED → GREEN
 */

#include "acutest.h"
#include "petrush/job.h"

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

void test_job_add_and_count(void)
{
    petrush_job_reset_for_tests();
    int id = petrush_job_add(12345, "sleep 1");
    TEST_CHECK(id == 1);
    TEST_CHECK(petrush_job_count() == 1);

    petrush_job_state_t st = PETRUSH_JOB_DONE;
    int status = -1;
    const char *cmd = NULL;
    TEST_CHECK(petrush_job_probe(1, &st, &status, &cmd) == 0);
    TEST_CHECK(st == PETRUSH_JOB_RUNNING);
    TEST_CHECK(cmd != NULL && strcmp(cmd, "sleep 1") == 0);

    petrush_job_reset_for_tests();
    TEST_CHECK(petrush_job_count() == 0);
    TEST_CHECK(petrush_job_probe(1, &st, &status, &cmd) != 0);
}

void test_job_ceiling_16(void)
{
    petrush_job_reset_for_tests();
    for (int i = 0; i < PETRUSH_JOB_MAX; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "cmd%d", i);
        int id = petrush_job_add((pid_t)(1000 + i), buf);
        TEST_CHECK(id == i + 1);
    }
    TEST_CHECK(petrush_job_add(9999, "overflow") == -1);
    TEST_CHECK(petrush_job_count() == PETRUSH_JOB_MAX);
    petrush_job_reset_for_tests();
}

void test_job_reap_marks_done(void)
{
    petrush_job_reset_for_tests();
    pid_t pid = fork();
    TEST_CHECK(pid >= 0);
    if (pid < 0) return;
    if (pid == 0) {
        usleep(30000);
        _exit(7);
    }
    int id = petrush_job_add(pid, "exit7");
    TEST_CHECK(id == 1);

    petrush_job_state_t st = PETRUSH_JOB_RUNNING;
    int status = -1;
    int saw_done = 0;
    for (int i = 0; i < 100; i++) {
        petrush_job_reap();
        if (petrush_job_probe(1, &st, &status, NULL) == 0 &&
            st == PETRUSH_JOB_DONE) {
            saw_done = 1;
            TEST_CHECK(status == 7);
            break;
        }
        usleep(10000);
    }
    TEST_CHECK(saw_done);

    petrush_job_notify(); /* remove DONE notificado */
    TEST_CHECK(petrush_job_count() == 0);
    petrush_job_reset_for_tests();
}

void test_job_ids_reuse_after_notify(void)
{
    petrush_job_reset_for_tests();
    pid_t pid = fork();
    TEST_CHECK(pid >= 0);
    if (pid < 0) return;
    if (pid == 0) {
        _exit(0);
    }
    TEST_CHECK(petrush_job_add(pid, "t") == 1);
    for (int i = 0; i < 100; i++) {
        petrush_job_reap();
        petrush_job_state_t st;
        if (petrush_job_probe(1, &st, NULL, NULL) == 0 &&
            st == PETRUSH_JOB_DONE)
            break;
        usleep(5000);
    }
    petrush_job_notify();
    TEST_CHECK(petrush_job_add(2222, "next") == 1);
    petrush_job_reset_for_tests();
}

TEST_LIST = {
    { "job_add_and_count", test_job_add_and_count },
    { "job_ceiling_16", test_job_ceiling_16 },
    { "job_reap_marks_done", test_job_reap_marks_done },
    { "job_ids_reuse_after_notify", test_job_ids_reuse_after_notify },
    { NULL, NULL }
};
