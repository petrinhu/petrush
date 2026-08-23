/*
 * test_xdg_paths.c - XDG-1: rc + history path resolution (TDD)
 */

#include "acutest.h"
#include "petrush/xdg_paths.h"
#include "petrush/env.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char g_tmp[256];
static int g_have_tmp;

static void mk_tmp_root(void)
{
    char tmpl[] = "/var/tmp/petrush_xdg_XXXXXX";
    char *d = mkdtemp(tmpl);
    TEST_ASSERT(d != NULL);
    snprintf(g_tmp, sizeof(g_tmp), "%s", d);
    g_have_tmp = 1;
}

static void rm_tree(const char *path)
{
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", path);
    (void)system(cmd);
}

static void setup_home(void)
{
    mk_tmp_root();
    TEST_ASSERT(petrush_setenv("HOME", g_tmp, 1) == 0);
    (void)unsetenv("XDG_CONFIG_HOME");
    (void)unsetenv("XDG_STATE_HOME");
}

static void teardown(void)
{
    if (g_have_tmp) {
        rm_tree(g_tmp);
        g_have_tmp = 0;
    }
}

static int touch_file(const char *path, mode_t mode)
{
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fputc('\n', f);
    fclose(f);
    return chmod(path, mode);
}

static int path_is_dir_mode(const char *path, mode_t want_bits)
{
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    if (!S_ISDIR(st.st_mode)) return 0;
    return (st.st_mode & 0777) == want_bits;
}

void test_rc_default_xdg_when_absent(void)
{
    char buf[512];
    char expect[512];
    setup_home();
    TEST_CHECK(petrush_rc_path(buf, sizeof(buf)) == 0);
    snprintf(expect, sizeof(expect), "%s/.config/petrush/rc", g_tmp);
    TEST_CHECK(strcmp(buf, expect) == 0);
    teardown();
}

void test_rc_xdg_config_home_override(void)
{
    char buf[512];
    char expect[512];
    char xdg[512];
    setup_home();
    snprintf(xdg, sizeof(xdg), "%s/xdgcfg", g_tmp);
    TEST_ASSERT(mkdir(xdg, 0700) == 0);
    TEST_ASSERT(petrush_setenv("XDG_CONFIG_HOME", xdg, 1) == 0);
    TEST_CHECK(petrush_rc_path(buf, sizeof(buf)) == 0);
    snprintf(expect, sizeof(expect), "%s/petrush/rc", xdg);
    TEST_CHECK(strcmp(buf, expect) == 0);
    teardown();
}

void test_rc_compat_legacy_when_xdg_missing(void)
{
    char buf[512];
    char legacy[512];
    setup_home();
    snprintf(legacy, sizeof(legacy), "%s/.petrushrc", g_tmp);
    TEST_ASSERT(touch_file(legacy, 0600) == 0);
    TEST_CHECK(petrush_rc_path(buf, sizeof(buf)) == 0);
    TEST_CHECK(strcmp(buf, legacy) == 0);
    teardown();
}

void test_rc_xdg_wins_when_both_exist(void)
{
    char buf[512];
    char xdg_rc[512];
    char legacy[512];
    char dir[512];
    setup_home();
    snprintf(dir, sizeof(dir), "%s/.config", g_tmp);
    TEST_ASSERT(mkdir(dir, 0700) == 0);
    snprintf(dir, sizeof(dir), "%s/.config/petrush", g_tmp);
    TEST_ASSERT(mkdir(dir, 0700) == 0);
    snprintf(xdg_rc, sizeof(xdg_rc), "%s/.config/petrush/rc", g_tmp);
    snprintf(legacy, sizeof(legacy), "%s/.petrushrc", g_tmp);
    TEST_ASSERT(touch_file(xdg_rc, 0600) == 0);
    TEST_ASSERT(touch_file(legacy, 0600) == 0);
    TEST_CHECK(petrush_rc_path(buf, sizeof(buf)) == 0);
    TEST_CHECK(strcmp(buf, xdg_rc) == 0);
    teardown();
}

void test_history_default_xdg_when_absent(void)
{
    char buf[512];
    char expect[512];
    setup_home();
    TEST_CHECK(petrush_history_path(buf, sizeof(buf)) == 0);
    snprintf(expect, sizeof(expect), "%s/.local/state/petrush/history", g_tmp);
    TEST_CHECK(strcmp(buf, expect) == 0);
    teardown();
}

void test_history_xdg_state_home_override(void)
{
    char buf[512];
    char expect[512];
    char xdg[512];
    setup_home();
    snprintf(xdg, sizeof(xdg), "%s/xdgstate", g_tmp);
    TEST_ASSERT(mkdir(xdg, 0700) == 0);
    TEST_ASSERT(petrush_setenv("XDG_STATE_HOME", xdg, 1) == 0);
    TEST_CHECK(petrush_history_path(buf, sizeof(buf)) == 0);
    snprintf(expect, sizeof(expect), "%s/petrush/history", xdg);
    TEST_CHECK(strcmp(buf, expect) == 0);
    teardown();
}

void test_history_compat_legacy_when_xdg_missing(void)
{
    char buf[512];
    char legacy[512];
    setup_home();
    snprintf(legacy, sizeof(legacy), "%s/.petrush_history", g_tmp);
    TEST_ASSERT(touch_file(legacy, 0600) == 0);
    TEST_CHECK(petrush_history_path(buf, sizeof(buf)) == 0);
    TEST_CHECK(strcmp(buf, legacy) == 0);
    teardown();
}

void test_history_ensure_dir_0700(void)
{
    char hist[512];
    char parent[512];
    setup_home();
    snprintf(hist, sizeof(hist), "%s/.local/state/petrush/history", g_tmp);
    snprintf(parent, sizeof(parent), "%s/.local/state/petrush", g_tmp);
    TEST_CHECK(petrush_history_ensure_dir(hist) == 0);
    TEST_CHECK(path_is_dir_mode(parent, 0700));
    /* intermediate dirs also created */
    snprintf(parent, sizeof(parent), "%s/.local", g_tmp);
    TEST_CHECK(path_is_dir_mode(parent, 0700));
    snprintf(parent, sizeof(parent), "%s/.local/state", g_tmp);
    TEST_CHECK(path_is_dir_mode(parent, 0700));
    teardown();
}

void test_rc_path_rejects_tiny_buf(void)
{
    char buf[4];
    setup_home();
    TEST_CHECK(petrush_rc_path(buf, sizeof(buf)) == -1);
    teardown();
}

TEST_LIST = {
    { "rc_default_xdg_when_absent", test_rc_default_xdg_when_absent },
    { "rc_xdg_config_home_override", test_rc_xdg_config_home_override },
    { "rc_compat_legacy_when_xdg_missing", test_rc_compat_legacy_when_xdg_missing },
    { "rc_xdg_wins_when_both_exist", test_rc_xdg_wins_when_both_exist },
    { "history_default_xdg_when_absent", test_history_default_xdg_when_absent },
    { "history_xdg_state_home_override", test_history_xdg_state_home_override },
    { "history_compat_legacy_when_xdg_missing", test_history_compat_legacy_when_xdg_missing },
    { "history_ensure_dir_0700", test_history_ensure_dir_0700 },
    { "rc_path_rejects_tiny_buf", test_rc_path_rejects_tiny_buf },
    { NULL, NULL }
};
