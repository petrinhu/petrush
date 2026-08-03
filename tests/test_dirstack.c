/*
 * test_dirstack.c — TDD for pushd/popd/dirs (NEW-25)
 * Feature amada em bash/zsh para navegação rápida.
 */

#include "acutest.h"
#include "petrush/dirstack.h"

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <stdio.h>

void test_dirs_empty_then_push(void)
{
    dirstack_clear();
    TEST_CHECK(dirstack_size() == 0);

    char cwd[PATH_MAX];
    TEST_CHECK(getcwd(cwd, sizeof(cwd)) != NULL);

    /* pushd /tmp (or TMPDIR) */
    const char *tmp = getenv("TMPDIR");
    if (!tmp || !*tmp) tmp = "/tmp";

    TEST_CHECK(dirstack_pushd(tmp) == 0);
    TEST_CHECK(dirstack_size() == 1);

    char now[PATH_MAX];
    TEST_CHECK(getcwd(now, sizeof(now)) != NULL);
    /* should be under tmp */
    TEST_CHECK(strstr(now, "tmp") != NULL || strcmp(now, tmp) == 0);

    TEST_CHECK(dirstack_popd() == 0);
    char back[PATH_MAX];
    TEST_CHECK(getcwd(back, sizeof(back)) != NULL);
    TEST_CHECK(strcmp(back, cwd) == 0);
    TEST_CHECK(dirstack_size() == 0);
    dirstack_clear();
}

void test_popd_empty_fails(void)
{
    dirstack_clear();
    TEST_CHECK(dirstack_popd() != 0);
}

void test_pushd_bad_path_fails(void)
{
    dirstack_clear();
    TEST_CHECK(dirstack_pushd("/no/such/dir/petrush_xyz_999") != 0);
    TEST_CHECK(dirstack_size() == 0);
}

TEST_LIST = {
    { "dirs_empty_then_push", test_dirs_empty_then_push },
    { "popd_empty_fails", test_popd_empty_fails },
    { "pushd_bad_path_fails", test_pushd_bad_path_fails },
    { NULL, NULL }
};
