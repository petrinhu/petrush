/*
 * test_env.c — TDD tests for ENV foundation (PR-07, NEW-04)
 */

#include "acutest.h"
#include "petrush/env.h"

#include <string.h>
#include <stdlib.h>

void test_getenv_existing(void)
{
    /* Assume HOME is set in test env */
    const char *home = petrush_getenv("HOME");
    TEST_CHECK(home != NULL);
    TEST_CHECK(strlen(home) > 0);
}

void test_getenv_nonexistent(void)
{
    const char *val = petrush_getenv("DEFINITELY_NOT_SET_12345");
    TEST_CHECK(val == NULL);
}

void test_setenv_getenv_unsetenv(void)
{
    const char *name = "PETRUSH_TEST_VAR";
    const char *value = "42";

    /* set */
    TEST_CHECK(petrush_setenv(name, value, 1) == 0);

    /* get */
    const char *got = petrush_getenv(name);
    TEST_CHECK(got != NULL);
    TEST_CHECK(strcmp(got, value) == 0);

    /* overwrite */
    TEST_CHECK(petrush_setenv(name, "43", 1) == 0);
    got = petrush_getenv(name);
    TEST_CHECK(strcmp(got, "43") == 0);

    /* unset */
    TEST_CHECK(petrush_unsetenv(name) == 0);
    TEST_CHECK(petrush_getenv(name) == NULL);

    /* no crash on invalid */
    TEST_CHECK(petrush_setenv(NULL, "x", 1) != 0);
}

TEST_LIST = {
    { "getenv_existing", test_getenv_existing },
    { "getenv_nonexistent", test_getenv_nonexistent },
    { "setenv_getenv_unsetenv", test_setenv_getenv_unsetenv },
    { NULL, NULL }
};