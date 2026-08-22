/*
 * test_rc_trust.c - SEC-10: ~/.petrushrc so carrega se regular,
 * st_uid == getuid() e sem write de group/other (mode & 0022).
 */

#include "acutest.h"
#include "petrush/rc_trust.h"

#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void fill_stat(struct stat *st, mode_t mode, uid_t uid)
{
    memset(st, 0, sizeof(*st));
    st->st_mode = mode;
    st->st_uid = uid;
}

void test_sec10_rejects_null_stat(void)
{
    TEST_CHECK(petrush_rc_stat_ok(NULL, 1000) == -1);
}

void test_sec10_accepts_owner_private(void)
{
    struct stat st;
    uid_t self = 1000;

    fill_stat(&st, S_IFREG | 0600, self);
    TEST_CHECK(petrush_rc_stat_ok(&st, self) == 0);

    fill_stat(&st, S_IFREG | 0400, self);
    TEST_CHECK(petrush_rc_stat_ok(&st, self) == 0);

    fill_stat(&st, S_IFREG | 0644, self);
    TEST_CHECK(petrush_rc_stat_ok(&st, self) == 0);
}

void test_sec10_rejects_group_or_other_write(void)
{
    struct stat st;
    uid_t self = 1000;

    fill_stat(&st, S_IFREG | 0620, self); /* group write */
    TEST_CHECK(petrush_rc_stat_ok(&st, self) == -1);

    fill_stat(&st, S_IFREG | 0602, self); /* other write */
    TEST_CHECK(petrush_rc_stat_ok(&st, self) == -1);

    fill_stat(&st, S_IFREG | 0666, self);
    TEST_CHECK(petrush_rc_stat_ok(&st, self) == -1);

    fill_stat(&st, S_IFREG | 0022, self);
    TEST_CHECK(petrush_rc_stat_ok(&st, self) == -1);
}

void test_sec10_rejects_wrong_uid(void)
{
    struct stat st;

    fill_stat(&st, S_IFREG | 0600, 1000);
    TEST_CHECK(petrush_rc_stat_ok(&st, 1001) == -1);

    fill_stat(&st, S_IFREG | 0600, 0);
    TEST_CHECK(petrush_rc_stat_ok(&st, 1000) == -1);
}

void test_sec10_rejects_non_regular(void)
{
    struct stat st;
    uid_t self = 1000;

    fill_stat(&st, S_IFDIR | 0700, self);
    TEST_CHECK(petrush_rc_stat_ok(&st, self) == -1);

    fill_stat(&st, S_IFLNK | 0777, self);
    TEST_CHECK(petrush_rc_stat_ok(&st, self) == -1);

    fill_stat(&st, S_IFCHR | 0600, self);
    TEST_CHECK(petrush_rc_stat_ok(&st, self) == -1);
}

void test_sec10_self_uid_matches_getuid(void)
{
    struct stat st;
    uid_t self = getuid();

    fill_stat(&st, S_IFREG | 0600, self);
    TEST_CHECK(petrush_rc_stat_ok(&st, self) == 0);

    fill_stat(&st, S_IFREG | 0600, self == 0 ? 1 : 0);
    TEST_CHECK(petrush_rc_stat_ok(&st, self) == -1);
}

TEST_LIST = {
    { "sec10_rejects_null_stat", test_sec10_rejects_null_stat },
    { "sec10_accepts_owner_private", test_sec10_accepts_owner_private },
    { "sec10_rejects_group_or_other_write", test_sec10_rejects_group_or_other_write },
    { "sec10_rejects_wrong_uid", test_sec10_rejects_wrong_uid },
    { "sec10_rejects_non_regular", test_sec10_rejects_non_regular },
    { "sec10_self_uid_matches_getuid", test_sec10_self_uid_matches_getuid },
    { NULL, NULL }
};
