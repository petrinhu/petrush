/*
 * test_netcom.c - ASM-NET: petrush_netcom_scan + overlay sysfs + EPERM -up.
 * Sem root. Sem CAP_NET_ADMIN. Sem hang. Nunca display :0.
 */

#define _GNU_SOURCE

#include "acutest.h"
#include "petrush/asm.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char g_overlay[256];

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
        char child[768];
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
static void rm_rf(const char *path)
{
    char cmd[512];
    int n = snprintf(cmd, sizeof(cmd), "rm -rf '%s'", path);
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

static int write_file(const char *path, const char *data)
{
    FILE *f = fopen(path, "w");
    if (!f) {
        return -1;
    }
    if (fputs(data, f) < 0) {
        fclose(f);
        return -1;
    }
    fclose(f);
    return 0;
}

static int mkpath_parent(const char *path)
{
    char tmp[512];
    size_t n = strlen(path);
    if (n >= sizeof(tmp)) {
        return -1;
    }
    memcpy(tmp, path, n + 1);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
                return -1;
            }
            *p = '/';
        }
    }
    return 0;
}

static int write_path(const char *rel, const char *data)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", g_overlay, rel);
    if (mkpath_parent(path) != 0) {
        return -1;
    }
    return write_file(path, data);
}

static int mkdir_path(const char *rel)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", g_overlay, rel);
    if (mkpath_parent(path) != 0) {
        return -1;
    }
    if (mkdir(path, 0755) != 0 && errno != EEXIST) {
        return -1;
    }
    return 0;
}

static void setup_overlay(void)
{
    snprintf(g_overlay, sizeof(g_overlay), "%s/petrush-netcom-ov-XXXXXX",
             getenv("TMPDIR") ? getenv("TMPDIR") : "/var/tmp");
    TEST_ASSERT(mkdtemp(g_overlay) != NULL);

    /* eth */
    TEST_ASSERT(write_path("sys/class/net/eth0/type", "1\n") == 0);
    TEST_ASSERT(write_path("sys/class/net/eth0/operstate", "up\n") == 0);
    TEST_ASSERT(write_path("sys/class/net/enp0s3/type", "1\n") == 0);
    TEST_ASSERT(write_path("sys/class/net/enp0s3/operstate", "down\n") == 0);

    /* wifi: ether + wireless/ dir */
    TEST_ASSERT(write_path("sys/class/net/wlan0/type", "1\n") == 0);
    TEST_ASSERT(write_path("sys/class/net/wlan0/operstate", "up\n") == 0);
    TEST_ASSERT(mkdir_path("sys/class/net/wlan0/wireless") == 0);

    /* loopback: must not appear as eth */
    TEST_ASSERT(write_path("sys/class/net/lo/type", "772\n") == 0);
    TEST_ASSERT(write_path("sys/class/net/lo/operstate", "unknown\n") == 0);

    /* bluetooth */
    TEST_ASSERT(write_path("sys/class/bluetooth/hci0/name", "TestBT\n") == 0);
    TEST_ASSERT(write_path("sys/class/bluetooth/hci0/address",
                           "00:11:22:33:44:55\n") == 0);

    petrush_netcom_set_root(g_overlay);
}

static void teardown_overlay(void)
{
    petrush_netcom_set_root(NULL);
    if (g_overlay[0] != '\0') {
        rm_rf(g_overlay);
        g_overlay[0] = '\0';
    }
}

void test_netcom_null_out(void)
{
    TEST_CHECK(petrush_netcom_scan(PETRUSH_NETCOM_ETH, NULL, 64) == -1);
}

void test_netcom_zero_cap(void)
{
    char buf[8];
    TEST_CHECK(petrush_netcom_scan(PETRUSH_NETCOM_ETH, buf, 0) == -ENOSPC);
}

void test_netcom_eth_overlay(void)
{
    char buf[4096];
    setup_overlay();
    int n = petrush_netcom_scan(PETRUSH_NETCOM_ETH, buf, sizeof(buf));
    TEST_CHECK(n > 0);
    TEST_CHECK((size_t)n == strlen(buf));
    TEST_CHECK(strstr(buf, "# eth") != NULL);
    TEST_CHECK(strstr(buf, "eth0") != NULL);
    TEST_CHECK(strstr(buf, "enp0s3") != NULL);
    TEST_CHECK(strstr(buf, "operstate=up") != NULL ||
               strstr(buf, "operstate=down") != NULL);
    /* lo type=772 must not be listed as eth */
    TEST_CHECK(strstr(buf, "lo ") == NULL && strstr(buf, "\nlo\n") == NULL);
    /* wifi must not appear under eth-only scan */
    TEST_CHECK(strstr(buf, "wlan0") == NULL);
    teardown_overlay();
}

void test_netcom_wifi_overlay(void)
{
    char buf[4096];
    setup_overlay();
    int n = petrush_netcom_scan(PETRUSH_NETCOM_WIFI, buf, sizeof(buf));
    TEST_CHECK(n > 0);
    TEST_CHECK(strstr(buf, "# wifi") != NULL);
    TEST_CHECK(strstr(buf, "wlan0") != NULL);
    TEST_CHECK(strstr(buf, "wireless=1") != NULL);
    TEST_CHECK(strstr(buf, "eth0") == NULL);
    teardown_overlay();
}

void test_netcom_bt_overlay(void)
{
    char buf[4096];
    setup_overlay();
    int n = petrush_netcom_scan(PETRUSH_NETCOM_BT, buf, sizeof(buf));
    TEST_CHECK(n > 0);
    TEST_CHECK(strstr(buf, "# bt") != NULL);
    TEST_CHECK(strstr(buf, "hci0") != NULL);
    TEST_CHECK(strstr(buf, "TestBT") != NULL);
    teardown_overlay();
}

void test_netcom_all_sections(void)
{
    char buf[8192];
    setup_overlay();
    int n = petrush_netcom_scan(0, buf, sizeof(buf)); /* 0 = all */
    TEST_CHECK(n > 0);
    TEST_CHECK(strstr(buf, "# wifi") != NULL);
    TEST_CHECK(strstr(buf, "# eth") != NULL);
    TEST_CHECK(strstr(buf, "# bt") != NULL);
    TEST_CHECK(strstr(buf, "wlan0") != NULL);
    TEST_CHECK(strstr(buf, "eth0") != NULL);
    TEST_CHECK(strstr(buf, "hci0") != NULL);
    teardown_overlay();
}

void test_netcom_enospc(void)
{
    char buf[16];
    setup_overlay();
    int n = petrush_netcom_scan(0, buf, sizeof(buf));
    TEST_CHECK(n == -ENOSPC);
    teardown_overlay();
}

void test_netcom_no_cap_net_admin(void)
{
    /* Host process CapEff has no CAP_NET_ADMIN (unprivileged). */
    TEST_CHECK(petrush_netcom_have_cap_net_admin() == 0);
}

void test_netcom_link_set_eperm_no_hang(void)
{
    int rc;
    TEST_ASSERT(petrush_netcom_have_cap_net_admin() == 0);
    rc = petrush_netcom_link_set("lo", 1);
    TEST_CHECK(rc == -EPERM);
    rc = petrush_netcom_link_set("lo", 0);
    TEST_CHECK(rc == -EPERM);
}

void test_netcom_link_set_bad_iface(void)
{
    int rc = petrush_netcom_link_set("", 1);
    TEST_CHECK(rc == -EINVAL);
    rc = petrush_netcom_link_set(NULL, 1);
    TEST_CHECK(rc == -EINVAL);
    rc = petrush_netcom_link_set("bad iface", 1);
    TEST_CHECK(rc == -EINVAL);
    rc = petrush_netcom_link_set("../../etc/passwd", 1);
    TEST_CHECK(rc == -EINVAL);
}

TEST_LIST = {
    { "netcom_null_out", test_netcom_null_out },
    { "netcom_zero_cap", test_netcom_zero_cap },
    { "netcom_eth_overlay", test_netcom_eth_overlay },
    { "netcom_wifi_overlay", test_netcom_wifi_overlay },
    { "netcom_bt_overlay", test_netcom_bt_overlay },
    { "netcom_all_sections", test_netcom_all_sections },
    { "netcom_enospc", test_netcom_enospc },
    { "netcom_no_cap_net_admin", test_netcom_no_cap_net_admin },
    { "netcom_link_set_eperm_no_hang", test_netcom_link_set_eperm_no_hang },
    { "netcom_link_set_bad_iface", test_netcom_link_set_bad_iface },
    { NULL, NULL }
};
