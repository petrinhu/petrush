/*
 * test_job_setpgid.c - ASM-PGID: petrush_job_setpgid (SYS_setpgid).
 * Contrato: 0 sucesso; -errno em falha; nao toca errno TLS.
 * ESRCH / EPERM = -ESRCH / -EPERM.
 */

#include "acutest.h"
#include "petrush/asm.h"

#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

void test_pgid_self_zero(void)
{
    TEST_CHECK(petrush_job_setpgid(0, 0) == 0);
}

void test_pgid_self_pid(void)
{
    pid_t me = getpid();
    TEST_CHECK(petrush_job_setpgid(me, me) == 0);
}

void test_pgid_child(void)
{
    pid_t child = fork();
    TEST_ASSERT(child >= 0);
    if (child == 0) {
        for (;;) {
            pause();
        }
    }

    int rc = petrush_job_setpgid(child, child);
    TEST_CHECK(rc == 0);

    kill(child, SIGKILL);
    (void)waitpid(child, NULL, 0);
}

void test_pgid_esrch(void)
{
    /* PID bem acima do espaco tipico: processo inexistente -> ESRCH */
    int rc = petrush_job_setpgid((pid_t)0x7ffffffe, (pid_t)0x7ffffffe);
    TEST_CHECK(rc == -ESRCH);
}

void test_pgid_eperm(void)
{
    /*
     * Filho vira session leader (setsid): mudar o pgid dele
     * devolve EPERM. (pid 1 nesta maquina devolve ESRCH, nao EPERM.)
     */
    pid_t child = fork();
    TEST_ASSERT(child >= 0);
    if (child == 0) {
        if (setsid() < 0) {
            _exit(2);
        }
        for (;;) {
            pause();
        }
    }

    /* garantir que setsid ja correu no filho */
    for (int i = 0; i < 50; i++) {
        if (getsid(child) == child) {
            break;
        }
        usleep(1000);
    }

    int rc = petrush_job_setpgid(child, getpid());
    TEST_CHECK(rc == -EPERM);

    kill(child, SIGKILL);
    (void)waitpid(child, NULL, 0);
}

void test_pgid_errno_tls_untouched(void)
{
    errno = 4242;
    (void)petrush_job_setpgid((pid_t)0x7ffffffe, (pid_t)0x7ffffffe);
    TEST_CHECK(errno == 4242);

    errno = 4242;
    (void)petrush_job_setpgid(0, 0);
    TEST_CHECK(errno == 4242);
}

TEST_LIST = {
    { "pgid_self_zero", test_pgid_self_zero },
    { "pgid_self_pid", test_pgid_self_pid },
    { "pgid_child", test_pgid_child },
    { "pgid_esrch", test_pgid_esrch },
    { "pgid_eperm", test_pgid_eperm },
    { "pgid_errno_tls_untouched", test_pgid_errno_tls_untouched },
    { NULL, NULL }
};
