/*
 * test_tty_mode.c - ASM-TTY: petrush_tty_mode (RAW/COOKED via ioctl).
 * Contrato: 0 sucesso; -ENOTTY se fd nao e tty; -EINVAL modo invalido;
 * nao toca errno TLS. Exercicio so com PTY (openpty), nunca display :0.
 */

#define _GNU_SOURCE

#include "acutest.h"
#include "petrush/asm.h"

#include <errno.h>
#include <pty.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

void test_tty_notty_pipe(void)
{
    int p[2];
    TEST_ASSERT(pipe(p) == 0);

    errno = 4242;
    int rc = petrush_tty_mode(p[0], PETRUSH_TTY_RAW);
    TEST_CHECK(rc == -ENOTTY);
    TEST_CHECK(errno == 4242);

    close(p[0]);
    close(p[1]);
}

void test_tty_invalid_mode(void)
{
    int master = -1;
    int slave = -1;
    TEST_ASSERT(openpty(&master, &slave, NULL, NULL, NULL) == 0);

    errno = 4242;
    int rc = petrush_tty_mode(slave, (petrush_tty_mode_t)2);
    TEST_CHECK(rc == -EINVAL);
    TEST_CHECK(errno == 4242);

    close(slave);
    close(master);
}

void test_tty_raw_clears_canonical(void)
{
    int master = -1;
    int slave = -1;
    TEST_ASSERT(openpty(&master, &slave, NULL, NULL, NULL) == 0);

    errno = 4242;
    int rc = petrush_tty_mode(slave, PETRUSH_TTY_RAW);
    TEST_CHECK(rc == 0);
    TEST_CHECK(errno == 4242);

    struct termios t;
    TEST_ASSERT(tcgetattr(slave, &t) == 0);
    TEST_CHECK((t.c_lflag & ICANON) == 0);
    TEST_CHECK((t.c_lflag & ECHO) == 0);
    TEST_CHECK((t.c_lflag & ISIG) == 0);
    TEST_CHECK(t.c_cc[VMIN] == 1);
    TEST_CHECK(t.c_cc[VTIME] == 0);

    close(slave);
    close(master);
}

void test_tty_cooked_sets_canonical(void)
{
    int master = -1;
    int slave = -1;
    TEST_ASSERT(openpty(&master, &slave, NULL, NULL, NULL) == 0);

    TEST_ASSERT(petrush_tty_mode(slave, PETRUSH_TTY_RAW) == 0);

    errno = 4242;
    int rc = petrush_tty_mode(slave, PETRUSH_TTY_COOKED);
    TEST_CHECK(rc == 0);
    TEST_CHECK(errno == 4242);

    struct termios t;
    TEST_ASSERT(tcgetattr(slave, &t) == 0);
    TEST_CHECK((t.c_lflag & ICANON) != 0);
    TEST_CHECK((t.c_lflag & ECHO) != 0);
    TEST_CHECK((t.c_lflag & ISIG) != 0);
    TEST_CHECK((t.c_oflag & OPOST) != 0);

    close(slave);
    close(master);
}

void test_tty_raw_then_cooked_roundtrip(void)
{
    int master = -1;
    int slave = -1;
    TEST_ASSERT(openpty(&master, &slave, NULL, NULL, NULL) == 0);

    TEST_CHECK(petrush_tty_mode(slave, PETRUSH_TTY_RAW) == 0);
    TEST_CHECK(petrush_tty_mode(slave, PETRUSH_TTY_COOKED) == 0);
    TEST_CHECK(petrush_tty_mode(slave, PETRUSH_TTY_RAW) == 0);

    struct termios t;
    TEST_ASSERT(tcgetattr(slave, &t) == 0);
    TEST_CHECK((t.c_lflag & (ICANON | ECHO)) == 0);

    close(slave);
    close(master);
}

TEST_LIST = {
    { "tty_notty_pipe", test_tty_notty_pipe },
    { "tty_invalid_mode", test_tty_invalid_mode },
    { "tty_raw_clears_canonical", test_tty_raw_clears_canonical },
    { "tty_cooked_sets_canonical", test_tty_cooked_sets_canonical },
    { "tty_raw_then_cooked_roundtrip", test_tty_raw_then_cooked_roundtrip },
    { NULL, NULL }
};
