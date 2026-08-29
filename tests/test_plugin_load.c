/*
 * test_plugin_load.c - PLG-LOAD: XDG/PETRUSH_PLUGIN_PATH, SHA-256,
 * recusa world-writable, allow-list, dlopen so apos checks.
 * pudod nao carrega .so. Sem 4755.
 */

#include "acutest.h"
#include "petrush/plugin_load.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef PLUGIN_TEST_OK_SO
#define PLUGIN_TEST_OK_SO ""
#endif
#ifndef PLUGIN_TEST_BAD_SO
#define PLUGIN_TEST_BAD_SO ""
#endif

enum { PLG_PATH_SNPRINTF_BUF = PETRUSH_PLG_PATH_MAX + 64 };

static char g_tmpdir[PETRUSH_PLG_PATH_MAX];
static int g_tmpdir_ok;

/* Walk + unlink/rmdir; ENOENT ok. Fallback when system(rm) fails (CI-SYSTEM-RESULT). */
static void empty_tree_best_effort(const char *dir)
{
    DIR *d = opendir(dir);
    if (!d)
        return;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;
        char child[PLG_PATH_SNPRINTF_BUF];
        int n = snprintf(child, sizeof(child), "%s/%s", dir, ent->d_name);
        if (n < 0 || (size_t)n >= sizeof(child))
            continue;
        struct stat st;
        if (lstat(child, &st) != 0) {
            if (errno != ENOENT)
                TEST_CHECK_(0, "lstat(%s): %s", child, strerror(errno));
            continue;
        }
        if (S_ISDIR(st.st_mode)) {
            empty_tree_best_effort(child);
            if (rmdir(child) != 0 && errno != ENOENT)
                TEST_CHECK_(0, "rmdir(%s): %s", child, strerror(errno));
        } else if (unlink(child) != 0 && errno != ENOENT) {
            TEST_CHECK_(0, "unlink(%s): %s", child, strerror(errno));
        }
    }
    closedir(d);
}

/* Primary: system(rm -rf). On failure, walk+rmdir/unlink (CI-SYSTEM-RESULT). */
static void rm_rf_best_effort(const char *path)
{
    char cmd[PLG_PATH_SNPRINTF_BUF];
    if (!path || !path[0]) {
        return;
    }
    /* path sob HOME/.cache/petrush-plg-* controlado pelo teste */
    int n = snprintf(cmd, sizeof(cmd), "rm -rf -- '%s'", path);
    if (n < 0 || (size_t)n >= sizeof(cmd)) {
        empty_tree_best_effort(path);
        if (rmdir(path) != 0 && errno != ENOENT)
            TEST_CHECK_(0, "rmdir(%s) path too long for system: %s", path,
                        strerror(errno));
        return;
    }
    int rc = system(cmd);
    if (rc == 0)
        return;
    empty_tree_best_effort(path);
    if (rmdir(path) != 0 && errno != ENOENT)
        TEST_CHECK_(0, "rmdir(%s) after system rc=%d: %s", path, rc,
                    strerror(errno));
}

static int path_has_world_writable_prefix(const char *abs)
{
    /* Heuristica de teste: se algum prefixo e /tmp ou /var/tmp, o walk falha. */
    if (!abs || abs[0] != '/') {
        return 1;
    }
    if (strncmp(abs, "/tmp/", 5) == 0 || strcmp(abs, "/tmp") == 0) {
        return 1;
    }
    if (strncmp(abs, "/var/tmp/", 9) == 0 || strcmp(abs, "/var/tmp") == 0) {
        return 1;
    }
    return 0;
}

static int make_tmpdir(void)
{
    /* Nao usar /tmp nem /var/tmp: 1777 o+w falha o walk de dirs (PLG-NARC §6). */
    const char *base = getenv("PETRUSH_TEST_HOME");
    if (!base || base[0] != '/') {
        base = getenv("HOME");
    }
    if (!base || base[0] != '/' || path_has_world_writable_prefix(base)) {
        base = "/opt/petrush-plg-tests";
        if (mkdir(base, 0755) != 0 && errno != EEXIST) {
            return -1;
        }
    }
    char cache[PETRUSH_PLG_PATH_MAX];
    if (snprintf(cache, sizeof(cache), "%s/.cache", base) >= (int)sizeof(cache)) {
        return -1;
    }
    if (mkdir(cache, 0755) != 0 && errno != EEXIST) {
        return -1;
    }
    if (chmod(cache, 0755) != 0) {
        return -1;
    }

    char tmpl[PETRUSH_PLG_PATH_MAX];
    if (snprintf(tmpl, sizeof(tmpl), "%s/petrush-plg-XXXXXX", cache) >=
        (int)sizeof(tmpl)) {
        return -1;
    }
    char *p = mkdtemp(tmpl);
    if (!p) {
        return -1;
    }
    if (chmod(p, 0755) != 0) {
        rm_rf_best_effort(p);
        return -1;
    }
    if (snprintf(g_tmpdir, sizeof(g_tmpdir), "%s", p) >= (int)sizeof(g_tmpdir)) {
        rm_rf_best_effort(p);
        return -1;
    }
    g_tmpdir_ok = 1;
    return 0;
}

static void cleanup_tmpdir(void)
{
    if (g_tmpdir_ok) {
        rm_rf_best_effort(g_tmpdir);
        g_tmpdir_ok = 0;
        g_tmpdir[0] = '\0';
    }
}

static int write_file(const char *path, const void *data, size_t n, mode_t mode)
{
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, mode);
    if (fd < 0) {
        return -1;
    }
    const char *p = data;
    size_t left = n;
    while (left > 0) {
        ssize_t w = write(fd, p, left);
        if (w < 0) {
            if (errno == EINTR) {
                continue;
            }
            close(fd);
            return -1;
        }
        p += (size_t)w;
        left -= (size_t)w;
    }
    if (fchmod(fd, mode) != 0) {
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}

static int copy_file(const char *src, const char *dst, mode_t mode)
{
    int in = open(src, O_RDONLY | O_CLOEXEC);
    if (in < 0) {
        return -1;
    }
    int out = open(dst, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, mode);
    if (out < 0) {
        close(in);
        return -1;
    }
    char buf[8192];
    for (;;) {
        ssize_t r = read(in, buf, sizeof(buf));
        if (r < 0) {
            if (errno == EINTR) {
                continue;
            }
            close(in);
            close(out);
            return -1;
        }
        if (r == 0) {
            break;
        }
        const char *p = buf;
        size_t left = (size_t)r;
        while (left > 0) {
            ssize_t w = write(out, p, left);
            if (w < 0) {
                if (errno == EINTR) {
                    continue;
                }
                close(in);
                close(out);
                return -1;
            }
            p += (size_t)w;
            left -= (size_t)w;
        }
    }
    close(in);
    if (fchmod(out, mode) != 0) {
        close(out);
        return -1;
    }
    close(out);
    return 0;
}

static int sha_file_hex(const char *path, char out[PETRUSH_PLUGIN_SHA256_HEX_LEN + 1])
{
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        return -1;
    }
    unsigned char dig[PETRUSH_PLUGIN_SHA256_LEN];
    if (petrush_plugin_sha256_fd(fd, dig) != 0) {
        close(fd);
        return -1;
    }
    close(fd);
    petrush_plugin_sha256_to_hex(dig, out);
    return 0;
}

void test_plugin_sha256_abc_mkstemp(void)
{
    const char *expect =
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    TEST_ASSERT(make_tmpdir() == 0);
    char tmpl[PETRUSH_PLG_PATH_MAX];
    TEST_ASSERT(snprintf(tmpl, sizeof(tmpl), "%s/abc-XXXXXX", g_tmpdir) <
                (int)sizeof(tmpl));
    int fd = mkstemp(tmpl);
    TEST_ASSERT(fd >= 0);
    TEST_CHECK(write(fd, "abc", 3) == 3);
    TEST_CHECK(lseek(fd, 0, SEEK_SET) == 0);
    unsigned char dig[PETRUSH_PLUGIN_SHA256_LEN];
    TEST_CHECK(petrush_plugin_sha256_fd(fd, dig) == 0);
    char hex[PETRUSH_PLUGIN_SHA256_HEX_LEN + 1];
    petrush_plugin_sha256_to_hex(dig, hex);
    TEST_CHECK(strcmp(hex, expect) == 0);
    TEST_CHECK(petrush_plugin_sha256_hex_eq(hex, expect) == 0);
    TEST_CHECK(petrush_plugin_sha256_hex_eq(hex,
        "0000000000000000000000000000000000000000000000000000000000000000") == 1);
    close(fd);
    unlink(tmpl);
    cleanup_tmpdir();
}

void test_plugin_ww_file(void)
{
    TEST_ASSERT(make_tmpdir() == 0);
    char path[PETRUSH_PLG_PATH_MAX];
    TEST_ASSERT(snprintf(path, sizeof(path), "%s/x.so", g_tmpdir) < (int)sizeof(path));
    TEST_ASSERT(write_file(path, "x", 1, 0644) == 0);
    TEST_ASSERT(chmod(path, 0666) == 0); /* o+w */
    char canon[PETRUSH_PLG_PATH_MAX];
    TEST_ASSERT(realpath(path, canon) != NULL);
    TEST_CHECK(petrush_plugin_path_writable_ok(canon) == -1);
    cleanup_tmpdir();
}

void test_plugin_ww_parent_dir(void)
{
    TEST_ASSERT(make_tmpdir() == 0);
    char sub[PETRUSH_PLG_PATH_MAX];
    TEST_ASSERT(snprintf(sub, sizeof(sub), "%s/sub", g_tmpdir) < (int)sizeof(sub));
    TEST_ASSERT(mkdir(sub, 0755) == 0);
    char path[PETRUSH_PLG_PATH_MAX];
    TEST_ASSERT(snprintf(path, sizeof(path), "%s/y.so", sub) < (int)sizeof(path));
    TEST_ASSERT(write_file(path, "y", 1, 0644) == 0);
    TEST_ASSERT(chmod(sub, 0777) == 0); /* dir o+w */
    char canon[PETRUSH_PLG_PATH_MAX];
    TEST_ASSERT(realpath(path, canon) != NULL);
    TEST_CHECK(petrush_plugin_path_writable_ok(canon) == -1);
    (void)chmod(sub, 0755);
    cleanup_tmpdir();
}

void test_plugin_path_safe_ok(void)
{
    TEST_ASSERT(make_tmpdir() == 0);
    char path[PETRUSH_PLG_PATH_MAX];
    TEST_ASSERT(snprintf(path, sizeof(path), "%s/z.so", g_tmpdir) < (int)sizeof(path));
    TEST_ASSERT(write_file(path, "z", 1, 0644) == 0);
    TEST_ASSERT(chmod(g_tmpdir, 0755) == 0);
    char canon[PETRUSH_PLG_PATH_MAX];
    TEST_ASSERT(realpath(path, canon) != NULL);
    TEST_CHECK(petrush_plugin_path_writable_ok(canon) == 0);
    cleanup_tmpdir();
}

void test_plugin_allow_default_deny(void)
{
    petrush_plugin_allow_t list;
    TEST_CHECK(petrush_plugin_allow_parse_file("/var/tmp/petrush-no-such-allow", &list) == -1);
    TEST_CHECK(list.n == 0);
}

void test_plugin_allow_basename_reject(void)
{
    TEST_ASSERT(make_tmpdir() == 0);
    char ap[PETRUSH_PLG_PATH_MAX];
    TEST_ASSERT(snprintf(ap, sizeof(ap), "%s/plugins.allow", g_tmpdir) < (int)sizeof(ap));
    const char *body =
        "evil.so  ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad\n";
    TEST_ASSERT(write_file(ap, body, strlen(body), 0600) == 0);
    petrush_plugin_allow_t list;
    TEST_CHECK(petrush_plugin_allow_parse_file(ap, &list) == 0);
    TEST_CHECK(list.n == 0); /* basename rejeitado; ficheiro ok mas 0 entradas */
    cleanup_tmpdir();
}

void test_plugin_allow_parse_and_find(void)
{
    TEST_ASSERT(make_tmpdir() == 0);
    char so[PETRUSH_PLG_PATH_MAX];
    TEST_ASSERT(snprintf(so, sizeof(so), "%s/foo.so", g_tmpdir) < (int)sizeof(so));
    TEST_ASSERT(write_file(so, "abc", 3, 0644) == 0);
    char canon[PETRUSH_PLG_PATH_MAX];
    TEST_ASSERT(realpath(so, canon) != NULL);
    char hex[PETRUSH_PLUGIN_SHA256_HEX_LEN + 1];
    TEST_ASSERT(sha_file_hex(canon, hex) == 0);

    char ap[PETRUSH_PLG_PATH_MAX];
    TEST_ASSERT(snprintf(ap, sizeof(ap), "%s/plugins.allow", g_tmpdir) < (int)sizeof(ap));
    char line[PETRUSH_PLG_PATH_MAX + 80];
    int n = snprintf(line, sizeof(line), "%s  %s\n", canon, hex);
    TEST_ASSERT(n > 0 && n < (int)sizeof(line));
    TEST_ASSERT(write_file(ap, line, (size_t)n, 0600) == 0);

    petrush_plugin_allow_t list;
    TEST_CHECK(petrush_plugin_allow_parse_file(ap, &list) == 0);
    TEST_CHECK(list.n == 1);
    char got[PETRUSH_PLUGIN_SHA256_HEX_LEN + 1];
    TEST_CHECK(petrush_plugin_allow_find(&list, canon, got) == 0);
    TEST_CHECK(petrush_plugin_sha256_hex_eq(got, hex) == 0);
    TEST_CHECK(petrush_plugin_allow_find(&list, "/no/such.so", got) == -1);
    cleanup_tmpdir();
}

void test_plugin_search_rejects_relative(void)
{
    char dirs[PETRUSH_PLUGIN_SEARCH_MAX][PETRUSH_PLG_PATH_MAX];
    size_t n = 99;
    TEST_ASSERT(make_tmpdir() == 0);
    char pathenv[PETRUSH_PLG_PATH_MAX];
    TEST_ASSERT(snprintf(pathenv, sizeof(pathenv), "relative/path:%s", g_tmpdir) <
                (int)sizeof(pathenv));
    TEST_ASSERT(setenv("PETRUSH_PLUGIN_PATH", pathenv, 1) == 0);
    TEST_ASSERT(setenv("XDG_DATA_HOME", g_tmpdir, 1) == 0);
    TEST_CHECK(petrush_plugin_search_dirs(dirs, PETRUSH_PLUGIN_SEARCH_MAX, &n) == 0);
    int saw_rel = 0;
    int saw_abs = 0;
    for (size_t i = 0; i < n; i++) {
        if (dirs[i][0] != '/') {
            saw_rel = 1;
        } else {
            saw_abs = 1;
        }
    }
    TEST_CHECK(saw_rel == 0);
    TEST_CHECK(saw_abs == 1);
    TEST_CHECK(n >= 1);
    unsetenv("PETRUSH_PLUGIN_PATH");
    unsetenv("XDG_DATA_HOME");
    cleanup_tmpdir();
}

void test_plugin_load_hash_mismatch(void)
{
    if (!PLUGIN_TEST_OK_SO[0]) {
        TEST_CHECK(0); /* CMake deve definir o path do .so */
        return;
    }
    TEST_ASSERT(make_tmpdir() == 0);
    char so[PETRUSH_PLG_PATH_MAX];
    TEST_ASSERT(snprintf(so, sizeof(so), "%s/ok_plugin.so", g_tmpdir) < (int)sizeof(so));
    TEST_ASSERT(copy_file(PLUGIN_TEST_OK_SO, so, 0644) == 0);
    TEST_ASSERT(chmod(g_tmpdir, 0755) == 0);
    char canon[PETRUSH_PLG_PATH_MAX];
    TEST_ASSERT(realpath(so, canon) != NULL);

    char ap[PETRUSH_PLG_PATH_MAX];
    TEST_ASSERT(snprintf(ap, sizeof(ap), "%s/plugins.allow", g_tmpdir) < (int)sizeof(ap));
    char line[PETRUSH_PLG_PATH_MAX + 80];
    int n = snprintf(line, sizeof(line),
                     "%s  0000000000000000000000000000000000000000000000000000000000000000\n",
                     canon);
    TEST_ASSERT(n > 0 && n < (int)sizeof(line));
    TEST_ASSERT(write_file(ap, line, (size_t)n, 0600) == 0);

    char pathenv[PETRUSH_PLG_PATH_MAX];
    TEST_ASSERT(snprintf(pathenv, sizeof(pathenv), "%s", g_tmpdir) < (int)sizeof(pathenv));
    TEST_ASSERT(setenv("PETRUSH_PLUGIN_PATH", pathenv, 1) == 0);

    petrush_plugin_t plug;
    memset(&plug, 0, sizeof(plug));
    int rc = petrush_plugin_load("ok_plugin", ap, &plug);
    TEST_CHECK(rc == PETRUSH_PLG_ERR_HASH);
    unsetenv("PETRUSH_PLUGIN_PATH");
    cleanup_tmpdir();
}

void test_plugin_load_ok(void)
{
    if (!PLUGIN_TEST_OK_SO[0]) {
        TEST_CHECK(0);
        return;
    }
    TEST_ASSERT(make_tmpdir() == 0);
    char so[PETRUSH_PLG_PATH_MAX];
    TEST_ASSERT(snprintf(so, sizeof(so), "%s/ok_plugin.so", g_tmpdir) < (int)sizeof(so));
    TEST_ASSERT(copy_file(PLUGIN_TEST_OK_SO, so, 0644) == 0);
    TEST_ASSERT(chmod(g_tmpdir, 0755) == 0);
    char canon[PETRUSH_PLG_PATH_MAX];
    TEST_ASSERT(realpath(so, canon) != NULL);
    char hex[PETRUSH_PLUGIN_SHA256_HEX_LEN + 1];
    TEST_ASSERT(sha_file_hex(canon, hex) == 0);

    char ap[PETRUSH_PLG_PATH_MAX];
    TEST_ASSERT(snprintf(ap, sizeof(ap), "%s/plugins.allow", g_tmpdir) < (int)sizeof(ap));
    char line[PETRUSH_PLG_PATH_MAX + 80];
    int n = snprintf(line, sizeof(line), "%s  %s\n", canon, hex);
    TEST_ASSERT(n > 0 && n < (int)sizeof(line));
    TEST_ASSERT(write_file(ap, line, (size_t)n, 0600) == 0);

    TEST_ASSERT(setenv("PETRUSH_PLUGIN_PATH", g_tmpdir, 1) == 0);

    petrush_plugin_t plug;
    memset(&plug, 0, sizeof(plug));
    int rc = petrush_plugin_load("ok_plugin", ap, &plug);
    TEST_CHECK(rc == PETRUSH_PLG_OK);
    if (rc == PETRUSH_PLG_OK) {
        TEST_CHECK(plug.initialized == 1);
        TEST_CHECK(plug.abi_major == 1);
        TEST_CHECK(strcmp(plug.plug_name, "ok_plugin") == 0);
        TEST_CHECK(petrush_plugin_unload(&plug) == 0);
    }
    unsetenv("PETRUSH_PLUGIN_PATH");
    cleanup_tmpdir();
}

void test_plugin_load_abi_major_mismatch(void)
{
    if (!PLUGIN_TEST_BAD_SO[0]) {
        TEST_CHECK(0);
        return;
    }
    TEST_ASSERT(make_tmpdir() == 0);
    char so[PETRUSH_PLG_PATH_MAX];
    TEST_ASSERT(snprintf(so, sizeof(so), "%s/bad_major_plugin.so", g_tmpdir) < (int)sizeof(so));
    TEST_ASSERT(copy_file(PLUGIN_TEST_BAD_SO, so, 0644) == 0);
    TEST_ASSERT(chmod(g_tmpdir, 0755) == 0);
    char canon[PETRUSH_PLG_PATH_MAX];
    TEST_ASSERT(realpath(so, canon) != NULL);
    char hex[PETRUSH_PLUGIN_SHA256_HEX_LEN + 1];
    TEST_ASSERT(sha_file_hex(canon, hex) == 0);

    char ap[PETRUSH_PLG_PATH_MAX];
    TEST_ASSERT(snprintf(ap, sizeof(ap), "%s/plugins.allow", g_tmpdir) < (int)sizeof(ap));
    char line[PETRUSH_PLG_PATH_MAX + 80];
    int n = snprintf(line, sizeof(line), "%s  %s\n", canon, hex);
    TEST_ASSERT(n > 0 && n < (int)sizeof(line));
    TEST_ASSERT(write_file(ap, line, (size_t)n, 0600) == 0);
    TEST_ASSERT(setenv("PETRUSH_PLUGIN_PATH", g_tmpdir, 1) == 0);

    petrush_plugin_t plug;
    memset(&plug, 0, sizeof(plug));
    int rc = petrush_plugin_load("bad_major_plugin", ap, &plug);
    TEST_CHECK(rc == PETRUSH_PLG_ERR_ABI);
    unsetenv("PETRUSH_PLUGIN_PATH");
    cleanup_tmpdir();
}

void test_plugin_load_ww_denied(void)
{
    if (!PLUGIN_TEST_OK_SO[0]) {
        TEST_CHECK(0);
        return;
    }
    TEST_ASSERT(make_tmpdir() == 0);
    char so[PETRUSH_PLG_PATH_MAX];
    TEST_ASSERT(snprintf(so, sizeof(so), "%s/ok_plugin.so", g_tmpdir) < (int)sizeof(so));
    TEST_ASSERT(copy_file(PLUGIN_TEST_OK_SO, so, 0644) == 0);
    TEST_ASSERT(chmod(so, 0666) == 0);
    char canon[PETRUSH_PLG_PATH_MAX];
    TEST_ASSERT(realpath(so, canon) != NULL);
    char hex[PETRUSH_PLUGIN_SHA256_HEX_LEN + 1];
    /* hash still of content; mode does not change digest */
    TEST_ASSERT(chmod(so, 0644) == 0);
    TEST_ASSERT(sha_file_hex(canon, hex) == 0);
    TEST_ASSERT(chmod(so, 0666) == 0);

    char ap[PETRUSH_PLG_PATH_MAX];
    TEST_ASSERT(snprintf(ap, sizeof(ap), "%s/plugins.allow", g_tmpdir) < (int)sizeof(ap));
    char line[PETRUSH_PLG_PATH_MAX + 80];
    int n = snprintf(line, sizeof(line), "%s  %s\n", canon, hex);
    TEST_ASSERT(n > 0 && n < (int)sizeof(line));
    TEST_ASSERT(write_file(ap, line, (size_t)n, 0600) == 0);
    TEST_ASSERT(setenv("PETRUSH_PLUGIN_PATH", g_tmpdir, 1) == 0);

    petrush_plugin_t plug;
    memset(&plug, 0, sizeof(plug));
    int rc = petrush_plugin_load("ok_plugin", ap, &plug);
    TEST_CHECK(rc == PETRUSH_PLG_ERR_PERM);
    unsetenv("PETRUSH_PLUGIN_PATH");
    (void)chmod(so, 0644);
    cleanup_tmpdir();
}

void test_plugin_load_no_allow_file(void)
{
    if (!PLUGIN_TEST_OK_SO[0]) {
        TEST_CHECK(0);
        return;
    }
    TEST_ASSERT(make_tmpdir() == 0);
    char so[PETRUSH_PLG_PATH_MAX];
    TEST_ASSERT(snprintf(so, sizeof(so), "%s/ok_plugin.so", g_tmpdir) < (int)sizeof(so));
    TEST_ASSERT(copy_file(PLUGIN_TEST_OK_SO, so, 0644) == 0);
    TEST_ASSERT(setenv("PETRUSH_PLUGIN_PATH", g_tmpdir, 1) == 0);
    petrush_plugin_t plug;
    memset(&plug, 0, sizeof(plug));
    char missing[PETRUSH_PLG_PATH_MAX];
    TEST_ASSERT(snprintf(missing, sizeof(missing), "%s/missing.allow", g_tmpdir) < (int)sizeof(missing));
    int rc = petrush_plugin_load("ok_plugin", missing, &plug);
    TEST_CHECK(rc == PETRUSH_PLG_ERR_ALLOW);
    unsetenv("PETRUSH_PLUGIN_PATH");
    cleanup_tmpdir();
}

TEST_LIST = {
    { "plugin_sha256_abc", test_plugin_sha256_abc_mkstemp },
    { "plugin_ww_file", test_plugin_ww_file },
    { "plugin_ww_parent_dir", test_plugin_ww_parent_dir },
    { "plugin_path_safe_ok", test_plugin_path_safe_ok },
    { "plugin_allow_default_deny", test_plugin_allow_default_deny },
    { "plugin_allow_basename_reject", test_plugin_allow_basename_reject },
    { "plugin_allow_parse_and_find", test_plugin_allow_parse_and_find },
    { "plugin_search_rejects_relative", test_plugin_search_rejects_relative },
    { "plugin_load_no_allow_file", test_plugin_load_no_allow_file },
    { "plugin_load_hash_mismatch", test_plugin_load_hash_mismatch },
    { "plugin_load_ww_denied", test_plugin_load_ww_denied },
    { "plugin_load_abi_major_mismatch", test_plugin_load_abi_major_mismatch },
    { "plugin_load_ok", test_plugin_load_ok },
    { NULL, NULL }
};
