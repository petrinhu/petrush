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

TEST_LIST = {
    { "history_save_refuses_symlink", test_history_save_refuses_symlink },
    { NULL, NULL }
};
