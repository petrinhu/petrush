/*
 * test_wai.c - ASM-WAI: petrush_wai_scan + overlay sysfs/proc.
 * Sem root. Sem serial/uuid no inventario. Nunca display :0.
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

static void setup_overlay(void)
{
    snprintf(g_overlay, sizeof(g_overlay), "%s/petrush-wai-ov-XXXXXX",
             getenv("TMPDIR") ? getenv("TMPDIR") : "/var/tmp");
    TEST_ASSERT(mkdtemp(g_overlay) != NULL);

    TEST_ASSERT(write_path("proc/meminfo",
                           "MemTotal:       16384000 kB\n"
                           "MemFree:         4096000 kB\n"
                           "MemAvailable:    8192000 kB\n"
                           "Buffers:          128000 kB\n"
                           "Cached:          2048000 kB\n") == 0);

    TEST_ASSERT(write_path("proc/cpuinfo",
                           "processor\t: 0\n"
                           "model name\t: Test CPU\n"
                           "cpu cores\t: 4\n"
                           "Serial\t\t: MUST-NOT-APPEAR\n"
                           "\n") == 0);

    TEST_ASSERT(write_path("sys/block/sda/size", "2097152\n") == 0);
    TEST_ASSERT(write_path("sys/block/sda/device/model", "TESTDISK\n") == 0);
    TEST_ASSERT(write_path("sys/block/loop0/size", "0\n") == 0);

    TEST_ASSERT(write_path("sys/class/drm/card0-HDMI-A-1/status", "connected\n") == 0);
    TEST_ASSERT(write_path("sys/class/sound/card0/id", "PCH\n") == 0);
    TEST_ASSERT(write_path("sys/class/video4linux/video0/name", "TestCam\n") == 0);
    TEST_ASSERT(write_path("sys/class/input/input0/name", "Test Keyboard\n") == 0);

    TEST_ASSERT(write_path("sys/bus/usb/devices/1-1/product", "Hub\n") == 0);
    TEST_ASSERT(write_path("sys/bus/usb/devices/1-1/manufacturer", "Test\n") == 0);
    TEST_ASSERT(write_path("sys/bus/usb/devices/1-1/serial", "USB-SERIAL-BAN\n") == 0);

    TEST_ASSERT(write_path("sys/bus/pci/devices/0000:00:00.0/vendor", "0x8086\n") == 0);
    TEST_ASSERT(write_path("sys/bus/pci/devices/0000:00:00.0/device", "0x1234\n") == 0);
    TEST_ASSERT(write_path("sys/bus/pci/devices/0000:00:00.0/class", "0x060000\n") == 0);

    TEST_ASSERT(write_path("sys/class/power_supply/BAT0/type", "Battery\n") == 0);
    TEST_ASSERT(write_path("sys/class/power_supply/BAT0/status", "Full\n") == 0);
    TEST_ASSERT(write_path("sys/class/power_supply/BAT0/capacity", "100\n") == 0);
    TEST_ASSERT(write_path("sys/class/power_supply/BAT0/serial_number", "BAT-SERIAL-BAN\n") == 0);

    TEST_ASSERT(write_path("sys/class/thermal/thermal_zone0/type", "x86_pkg_temp\n") == 0);
    TEST_ASSERT(write_path("sys/class/thermal/thermal_zone0/temp", "45000\n") == 0);

    TEST_ASSERT(write_path("sys/class/dmi/id/board_name", "TestBoard\n") == 0);
    TEST_ASSERT(write_path("sys/class/dmi/id/board_vendor", "TestVendor\n") == 0);
    TEST_ASSERT(write_path("sys/class/dmi/id/product_name", "TestProduct\n") == 0);
    TEST_ASSERT(write_path("sys/class/dmi/id/product_serial", "DMI-SERIAL-BAN\n") == 0);
    TEST_ASSERT(write_path("sys/class/dmi/id/product_uuid", "DMI-UUID-BAN\n") == 0);
    TEST_ASSERT(write_path("sys/class/dmi/id/board_serial", "BOARD-SERIAL-BAN\n") == 0);

    petrush_wai_set_root(g_overlay);
}

static void teardown_overlay(void)
{
    petrush_wai_set_root(NULL);
    if (g_overlay[0] != '\0') {
        rm_rf(g_overlay);
        g_overlay[0] = '\0';
    }
}

void test_wai_null_out(void)
{
    TEST_CHECK(petrush_wai_scan(PETRUSH_WAI_MEM, NULL, 64) == -1);
}

void test_wai_zero_cap(void)
{
    char buf[8];
    TEST_CHECK(petrush_wai_scan(PETRUSH_WAI_MEM, buf, 0) == -ENOSPC);
}

void test_wai_mem_overlay(void)
{
    char buf[2048];
    setup_overlay();
    int n = petrush_wai_scan(PETRUSH_WAI_MEM, buf, sizeof(buf));
    TEST_CHECK(n > 0);
    TEST_CHECK((size_t)n == strlen(buf));
    TEST_CHECK(strstr(buf, "# mem") != NULL);
    TEST_CHECK(strstr(buf, "MemTotal:") != NULL);
    TEST_CHECK(strstr(buf, "16384000") != NULL);
    teardown_overlay();
}

void test_wai_disk_lists_sda(void)
{
    char buf[4096];
    setup_overlay();
    int n = petrush_wai_scan(PETRUSH_WAI_DISK, buf, sizeof(buf));
    TEST_CHECK(n > 0);
    TEST_CHECK(strstr(buf, "# disk") != NULL);
    TEST_CHECK(strstr(buf, "sda") != NULL);
    TEST_CHECK(strstr(buf, "2097152") != NULL);
    TEST_CHECK(strstr(buf, "TESTDISK") != NULL);
    teardown_overlay();
}

void test_wai_board_skips_serial_uuid(void)
{
    char buf[4096];
    setup_overlay();
    int n = petrush_wai_scan(PETRUSH_WAI_BOARD, buf, sizeof(buf));
    TEST_CHECK(n > 0);
    TEST_CHECK(strstr(buf, "TestBoard") != NULL);
    TEST_CHECK(strstr(buf, "TestVendor") != NULL);
    TEST_CHECK(strstr(buf, "TestProduct") != NULL);
    TEST_CHECK(strstr(buf, "DMI-SERIAL-BAN") == NULL);
    TEST_CHECK(strstr(buf, "DMI-UUID-BAN") == NULL);
    TEST_CHECK(strstr(buf, "BOARD-SERIAL-BAN") == NULL);
    TEST_CHECK(strstr(buf, "product_serial") == NULL);
    TEST_CHECK(strstr(buf, "product_uuid") == NULL);
    teardown_overlay();
}

void test_wai_cpu_skips_serial_line(void)
{
    char buf[4096];
    setup_overlay();
    int n = petrush_wai_scan(PETRUSH_WAI_CPU, buf, sizeof(buf));
    TEST_CHECK(n > 0);
    TEST_CHECK(strstr(buf, "Test CPU") != NULL);
    TEST_CHECK(strstr(buf, "MUST-NOT-APPEAR") == NULL);
    teardown_overlay();
}

void test_wai_usb_skips_serial_file(void)
{
    char buf[4096];
    setup_overlay();
    int n = petrush_wai_scan(PETRUSH_WAI_USB, buf, sizeof(buf));
    TEST_CHECK(n > 0);
    TEST_CHECK(strstr(buf, "Hub") != NULL);
    TEST_CHECK(strstr(buf, "USB-SERIAL-BAN") == NULL);
    teardown_overlay();
}

void test_wai_battery_skips_serial_number(void)
{
    char buf[4096];
    setup_overlay();
    int n = petrush_wai_scan(PETRUSH_WAI_BATTERY, buf, sizeof(buf));
    TEST_CHECK(n > 0);
    TEST_CHECK(strstr(buf, "BAT0") != NULL);
    TEST_CHECK(strstr(buf, "capacity=100") != NULL || strstr(buf, "100") != NULL);
    TEST_CHECK(strstr(buf, "BAT-SERIAL-BAN") == NULL);
    TEST_CHECK(strstr(buf, "serial_number") == NULL);
    teardown_overlay();
}

void test_wai_all_sections_present(void)
{
    char buf[16384];
    setup_overlay();
    int n = petrush_wai_scan(0, buf, sizeof(buf)); /* 0 = all */
    TEST_CHECK(n > 0);
    TEST_CHECK(strstr(buf, "# disk") != NULL);
    TEST_CHECK(strstr(buf, "# video") != NULL);
    TEST_CHECK(strstr(buf, "# mem") != NULL);
    TEST_CHECK(strstr(buf, "# audio") != NULL);
    TEST_CHECK(strstr(buf, "# camera") != NULL);
    TEST_CHECK(strstr(buf, "# keyboard") != NULL);
    TEST_CHECK(strstr(buf, "# usb") != NULL);
    TEST_CHECK(strstr(buf, "# pci") != NULL);
    TEST_CHECK(strstr(buf, "# battery") != NULL);
    TEST_CHECK(strstr(buf, "# thermal") != NULL);
    TEST_CHECK(strstr(buf, "# cpu") != NULL);
    TEST_CHECK(strstr(buf, "# board") != NULL);
    TEST_CHECK(strstr(buf, "serial") == NULL || strstr(buf, "MUST-NOT") == NULL);
    TEST_CHECK(strstr(buf, "UUID-BAN") == NULL);
    TEST_CHECK(strstr(buf, "SERIAL-BAN") == NULL);
    teardown_overlay();
}

void test_wai_enospc(void)
{
    char buf[16];
    setup_overlay();
    int n = petrush_wai_scan(PETRUSH_WAI_MEM | PETRUSH_WAI_CPU | PETRUSH_WAI_BOARD,
                             buf, sizeof(buf));
    TEST_CHECK(n == -ENOSPC);
    teardown_overlay();
}

void test_wai_flags_video_audio_camera_keyboard_pci_thermal(void)
{
    char buf[8192];
    setup_overlay();
    unsigned f = PETRUSH_WAI_VIDEO | PETRUSH_WAI_AUDIO | PETRUSH_WAI_CAMERA |
                 PETRUSH_WAI_KEYBOARD | PETRUSH_WAI_PCI | PETRUSH_WAI_THERMAL;
    int n = petrush_wai_scan(f, buf, sizeof(buf));
    TEST_CHECK(n > 0);
    TEST_CHECK(strstr(buf, "card0-HDMI-A-1") != NULL);
    TEST_CHECK(strstr(buf, "card0") != NULL);
    TEST_CHECK(strstr(buf, "video0") != NULL);
    TEST_CHECK(strstr(buf, "input0") != NULL);
    TEST_CHECK(strstr(buf, "0x8086") != NULL);
    TEST_CHECK(strstr(buf, "x86_pkg_temp") != NULL);
    teardown_overlay();
}

TEST_LIST = {
    { "wai_null_out", test_wai_null_out },
    { "wai_zero_cap", test_wai_zero_cap },
    { "wai_mem_overlay", test_wai_mem_overlay },
    { "wai_disk_lists_sda", test_wai_disk_lists_sda },
    { "wai_board_skips_serial_uuid", test_wai_board_skips_serial_uuid },
    { "wai_cpu_skips_serial_line", test_wai_cpu_skips_serial_line },
    { "wai_usb_skips_serial_file", test_wai_usb_skips_serial_file },
    { "wai_battery_skips_serial_number", test_wai_battery_skips_serial_number },
    { "wai_all_sections_present", test_wai_all_sections_present },
    { "wai_enospc", test_wai_enospc },
    { "wai_flags_misc", test_wai_flags_video_audio_camera_keyboard_pci_thermal },
    { NULL, NULL }
};
