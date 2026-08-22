/*
 * test_linenoise_history.c — SEC-08: history save must not follow symlink
 * (CVE-2025-9810 residual: fopen("w") overwrites through a symlink).
 */

#include "acutest.h"
#include "linenoise.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int read_file_all(const char *path, char *buf, size_t buflen)
{
    FILE *fp = fopen(path, "r");
    size_t n;

    if (!fp) return -1;
    n = fread(buf, 1, buflen - 1, fp);
    buf[n] = '\0';
    fclose(fp);
    return (int)n;
}

/* fopen("w") follows symlink and would truncate/overwrite the victim. */
void test_history_save_refuses_symlink(void)
{
    char dir[] = "/var/tmp/petrush-sec08-XXXXXX";
    char victim[128];
    char linkpath[128];
    char histpath[128];
    char buf[128];
    const char *marker = "SENSITIVE-DO-NOT-OVERWRITE\n";
    FILE *fp;
    int rc;

    TEST_ASSERT(mkdtemp(dir) != NULL);

    snprintf(victim, sizeof(victim), "%s/victim", dir);
    snprintf(linkpath, sizeof(linkpath), "%s/hist.link", dir);
    snprintf(histpath, sizeof(histpath), "%s/hist.real", dir);

    fp = fopen(victim, "w");
    TEST_ASSERT(fp != NULL);
    fputs(marker, fp);
    fclose(fp);

    TEST_ASSERT(symlink(victim, linkpath) == 0);

    linenoiseHistorySetMaxLen(32);
    linenoiseHistoryAdd("echo should-not-land-in-victim");

    rc = linenoiseHistorySave(linkpath);
    TEST_CHECK(rc == -1);

    TEST_ASSERT(read_file_all(victim, buf, sizeof(buf)) >= 0);
    TEST_CHECK(strcmp(buf, marker) == 0);

    /* Regular path still works after refusing the symlink. */
    TEST_CHECK(linenoiseHistorySave(histpath) == 0);
    TEST_ASSERT(read_file_all(histpath, buf, sizeof(buf)) >= 0);
    TEST_CHECK(strstr(buf, "echo should-not-land-in-victim") != NULL);

    unlink(linkpath);
    unlink(victim);
    unlink(histpath);
    rmdir(dir);
}

/* UX-20: substring search, newest-first, start_exclusive. */
void test_history_search_newest_first(void)
{
    int i0, i1, i2, i3;

    linenoiseHistorySetMaxLen(64);
    linenoiseHistoryAdd("ux20-alpha-unique-zzz");
    linenoiseHistoryAdd("ux20-beta-unique-zzz");
    linenoiseHistoryAdd("ux20-gamma-unique-zzz");

    i0 = linenoiseHistorySearch("ux20-", linenoiseHistoryLen());
    TEST_ASSERT(i0 >= 0);
    TEST_CHECK(strcmp(linenoiseHistoryGet(i0), "ux20-gamma-unique-zzz") == 0);

    i1 = linenoiseHistorySearch("ux20-", i0);
    TEST_ASSERT(i1 >= 0);
    TEST_CHECK(strcmp(linenoiseHistoryGet(i1), "ux20-beta-unique-zzz") == 0);

    i2 = linenoiseHistorySearch("ux20-", i1);
    TEST_ASSERT(i2 >= 0);
    TEST_CHECK(strcmp(linenoiseHistoryGet(i2), "ux20-alpha-unique-zzz") == 0);

    i3 = linenoiseHistorySearch("ux20-", i2);
    TEST_CHECK(i3 == -1);
}

void test_history_search_substring_and_miss(void)
{
    int idx;

    linenoiseHistorySetMaxLen(64);
    linenoiseHistoryAdd("ux20-cat-meow-unique");
    linenoiseHistoryAdd("ux20-dog-bark-unique");

    idx = linenoiseHistorySearch("dog-bark", linenoiseHistoryLen());
    TEST_ASSERT(idx >= 0);
    TEST_CHECK(strcmp(linenoiseHistoryGet(idx), "ux20-dog-bark-unique") == 0);

    TEST_CHECK(linenoiseHistorySearch("ux20-no-such-token", linenoiseHistoryLen()) == -1);
}

void test_history_search_empty_query_and_bounds(void)
{
    int idx;
    const char *newest;

    linenoiseHistorySetMaxLen(64);
    linenoiseHistoryAdd("ux20-empty-q-unique-one");
    linenoiseHistoryAdd("ux20-empty-q-unique-two");

    newest = linenoiseHistoryGet(linenoiseHistoryLen() - 1);
    TEST_ASSERT(newest != NULL);

    idx = linenoiseHistorySearch("", linenoiseHistoryLen());
    TEST_ASSERT(idx >= 0);
    TEST_CHECK(strcmp(linenoiseHistoryGet(idx), newest) == 0);

    idx = linenoiseHistorySearch(NULL, linenoiseHistoryLen());
    TEST_ASSERT(idx >= 0);
    TEST_CHECK(strcmp(linenoiseHistoryGet(idx), newest) == 0);

    TEST_CHECK(linenoiseHistorySearch("ux20-empty-q", 0) == -1);
    TEST_CHECK(linenoiseHistorySearch("ux20-empty-q", -5) == -1);
}

TEST_LIST = {
    { "history_save_refuses_symlink", test_history_save_refuses_symlink },
    { "history_search_newest_first", test_history_search_newest_first },
    { "history_search_substring_and_miss", test_history_search_substring_and_miss },
    { "history_search_empty_query_and_bounds", test_history_search_empty_query_and_bounds },
    { NULL, NULL }
};
