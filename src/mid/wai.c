/*
 * wai.c - ASM-WAI: inventario sysfs/proc (sem root, sem serial/uuid).
 * petrush_wai_scan_impl = corpo I/O (ASan). Entrada ASM em wai_scan.S.
 * Builtin wai: flags -disk -video -mem -audio -camera -keyboard -usb -pci
 *              -battery -thermal -cpu -board (nenhuma = todas).
 */

#define _GNU_SOURCE

#include "petrush/asm.h"
#include "petrush/dispatcher.h"
#include "petrush/i18n.h"
#include "petrush/parser.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define WAI_ROOT_MAX 256
#define WAI_PATH_MAX 512

static char g_wai_root[WAI_ROOT_MAX];

void petrush_wai_set_root(const char *root)
{
    if (!root || root[0] == '\0') {
        g_wai_root[0] = '\0';
        return;
    }
    size_t n = strlen(root);
    if (n >= WAI_ROOT_MAX) {
        n = WAI_ROOT_MAX - 1;
    }
    memcpy(g_wai_root, root, n);
    while (n > 1 && g_wai_root[n - 1] == '/') {
        n--;
    }
    g_wai_root[n] = '\0';
}

static int name_is_banned(const char *name)
{
    char low[128];
    size_t i;
    size_t n;

    if (!name) {
        return 1;
    }
    n = strlen(name);
    if (n >= sizeof(low)) {
        n = sizeof(low) - 1;
    }
    for (i = 0; i < n; i++) {
        low[i] = (char)tolower((unsigned char)name[i]);
    }
    low[n] = '\0';
    if (strstr(low, "serial") != NULL) {
        return 1;
    }
    if (strstr(low, "uuid") != NULL) {
        return 1;
    }
    return 0;
}

static int line_is_banned(const char *line)
{
    return name_is_banned(line);
}

/* Append raw bytes; returns 0 ok, -1 error, -ENOSPC. Updates *written. */
static int append_bytes(char *out, size_t cap, size_t *written,
                        const char *data, size_t len)
{
    size_t w;

    if (!out || cap == 0) {
        return -ENOSPC;
    }
    w = *written;
    if (w >= cap) {
        return -ENOSPC;
    }
    /* Reserve 1 byte for NUL. */
    if (len > cap - 1 - w) {
        return -ENOSPC;
    }
    memcpy(out + w, data, len);
    *written = w + len;
    out[*written] = '\0';
    return 0;
}

static int append_str(char *out, size_t cap, size_t *written, const char *s)
{
    return append_bytes(out, cap, written, s, strlen(s));
}

static int join_path(char *dst, size_t dst_cap, const char *rel)
{
    int n;

    if (rel[0] == '/') {
        rel++;
    }
    if (g_wai_root[0] == '\0') {
        n = snprintf(dst, dst_cap, "/%s", rel);
    } else {
        n = snprintf(dst, dst_cap, "%s/%s", g_wai_root, rel);
    }
    if (n < 0 || (size_t)n >= dst_cap) {
        return -1;
    }
    return 0;
}

static int append_section(char *out, size_t cap, size_t *written,
                          const char *name)
{
    int rc = append_str(out, cap, written, "# ");
    if (rc != 0) {
        return rc;
    }
    rc = append_str(out, cap, written, name);
    if (rc != 0) {
        return rc;
    }
    return append_str(out, cap, written, "\n");
}

static int read_file_filtered(char *out, size_t cap, size_t *written,
                              const char *rel, int filter_banned_lines)
{
    char path[WAI_PATH_MAX];
    FILE *f;
    char line[512];

    if (join_path(path, sizeof(path), rel) != 0) {
        return -1;
    }
    f = fopen(path, "r");
    if (!f) {
        return 0; /* missing = empty section body */
    }
    while (fgets(line, sizeof(line), f) != NULL) {
        if (filter_banned_lines && line_is_banned(line)) {
            continue;
        }
        {
            int rc = append_str(out, cap, written, line);
            if (rc != 0) {
                fclose(f);
                return rc;
            }
        }
    }
    fclose(f);
    return 0;
}

static int append_kv_file(char *out, size_t cap, size_t *written,
                          const char *rel, const char *key)
{
    char path[WAI_PATH_MAX];
    FILE *f;
    char buf[256];
    size_t n;
    int rc;

    if (name_is_banned(key)) {
        return 0;
    }
    if (join_path(path, sizeof(path), rel) != 0) {
        return -1;
    }
    f = fopen(path, "r");
    if (!f) {
        return 0;
    }
    if (!fgets(buf, sizeof(buf), f)) {
        fclose(f);
        return 0;
    }
    fclose(f);
    n = strlen(buf);
    while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) {
        buf[--n] = '\0';
    }
    rc = append_str(out, cap, written, key);
    if (rc != 0) {
        return rc;
    }
    rc = append_str(out, cap, written, "=");
    if (rc != 0) {
        return rc;
    }
    rc = append_str(out, cap, written, buf);
    if (rc != 0) {
        return rc;
    }
    return append_str(out, cap, written, "\n");
}

typedef int (*wai_ent_fn)(char *out, size_t cap, size_t *written,
                          const char *dir_rel, const char *ent);

static int walk_dir(char *out, size_t cap, size_t *written,
                    const char *dir_rel, wai_ent_fn fn)
{
    char path[WAI_PATH_MAX];
    DIR *d;
    struct dirent *ent;

    if (join_path(path, sizeof(path), dir_rel) != 0) {
        return -1;
    }
    d = opendir(path);
    if (!d) {
        return 0;
    }
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') {
            continue;
        }
        if (name_is_banned(ent->d_name)) {
            continue;
        }
        {
            int rc = fn(out, cap, written, dir_rel, ent->d_name);
            if (rc != 0) {
                closedir(d);
                return rc;
            }
        }
    }
    closedir(d);
    return 0;
}

static int ent_list_name(char *out, size_t cap, size_t *written,
                         const char *dir_rel, const char *ent)
{
    char rel[WAI_PATH_MAX];
    int rc;

    (void)dir_rel;
    rc = append_str(out, cap, written, ent);
    if (rc != 0) {
        return rc;
    }
    /* Optional status/name/id sidecar */
    snprintf(rel, sizeof(rel), "%s/%s/status", dir_rel, ent);
    {
        char path[WAI_PATH_MAX];
        FILE *f;
        char buf[128];
        if (join_path(path, sizeof(path), rel) == 0 && (f = fopen(path, "r"))) {
            if (fgets(buf, sizeof(buf), f)) {
                size_t n = strlen(buf);
                while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) {
                    buf[--n] = '\0';
                }
                rc = append_str(out, cap, written, " status=");
                if (rc == 0) {
                    rc = append_str(out, cap, written, buf);
                }
            }
            fclose(f);
            if (rc != 0) {
                return rc;
            }
        }
    }
    snprintf(rel, sizeof(rel), "%s/%s/name", dir_rel, ent);
    {
        char path[WAI_PATH_MAX];
        FILE *f;
        char buf[128];
        if (join_path(path, sizeof(path), rel) == 0 && (f = fopen(path, "r"))) {
            if (fgets(buf, sizeof(buf), f)) {
                size_t n = strlen(buf);
                while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) {
                    buf[--n] = '\0';
                }
                rc = append_str(out, cap, written, " name=");
                if (rc == 0) {
                    rc = append_str(out, cap, written, buf);
                }
            }
            fclose(f);
            if (rc != 0) {
                return rc;
            }
        }
    }
    snprintf(rel, sizeof(rel), "%s/%s/id", dir_rel, ent);
    {
        char path[WAI_PATH_MAX];
        FILE *f;
        char buf[128];
        if (join_path(path, sizeof(path), rel) == 0 && (f = fopen(path, "r"))) {
            if (fgets(buf, sizeof(buf), f)) {
                size_t n = strlen(buf);
                while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) {
                    buf[--n] = '\0';
                }
                rc = append_str(out, cap, written, " id=");
                if (rc == 0) {
                    rc = append_str(out, cap, written, buf);
                }
            }
            fclose(f);
            if (rc != 0) {
                return rc;
            }
        }
    }
    return append_str(out, cap, written, "\n");
}

static int ent_disk(char *out, size_t cap, size_t *written,
                    const char *dir_rel, const char *ent)
{
    char rel[WAI_PATH_MAX];
    int rc;

    rc = append_str(out, cap, written, ent);
    if (rc != 0) {
        return rc;
    }
    snprintf(rel, sizeof(rel), "%s/%s/size", dir_rel, ent);
    {
        char path[WAI_PATH_MAX];
        FILE *f;
        char buf[64];
        if (join_path(path, sizeof(path), rel) == 0 && (f = fopen(path, "r"))) {
            if (fgets(buf, sizeof(buf), f)) {
                size_t n = strlen(buf);
                while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) {
                    buf[--n] = '\0';
                }
                rc = append_str(out, cap, written, " size=");
                if (rc == 0) {
                    rc = append_str(out, cap, written, buf);
                }
            }
            fclose(f);
            if (rc != 0) {
                return rc;
            }
        }
    }
    snprintf(rel, sizeof(rel), "%s/%s/device/model", dir_rel, ent);
    {
        char path[WAI_PATH_MAX];
        FILE *f;
        char buf[128];
        if (join_path(path, sizeof(path), rel) == 0 && (f = fopen(path, "r"))) {
            if (fgets(buf, sizeof(buf), f)) {
                size_t n = strlen(buf);
                while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) {
                    buf[--n] = '\0';
                }
                rc = append_str(out, cap, written, " model=");
                if (rc == 0) {
                    rc = append_str(out, cap, written, buf);
                }
            }
            fclose(f);
            if (rc != 0) {
                return rc;
            }
        }
    }
    return append_str(out, cap, written, "\n");
}

static int ent_usb(char *out, size_t cap, size_t *written,
                   const char *dir_rel, const char *ent)
{
    char rel[WAI_PATH_MAX];
    int rc;
    int any = 0;

    snprintf(rel, sizeof(rel), "%s/%s/product", dir_rel, ent);
    {
        char path[WAI_PATH_MAX];
        if (join_path(path, sizeof(path), rel) == 0 && access(path, R_OK) == 0) {
            any = 1;
        }
    }
    if (!any) {
        snprintf(rel, sizeof(rel), "%s/%s/manufacturer", dir_rel, ent);
        {
            char path[WAI_PATH_MAX];
            if (join_path(path, sizeof(path), rel) == 0 && access(path, R_OK) == 0) {
                any = 1;
            }
        }
    }
    if (!any) {
        return 0;
    }

    rc = append_str(out, cap, written, ent);
    if (rc != 0) {
        return rc;
    }
    snprintf(rel, sizeof(rel), "%s/%s/product", dir_rel, ent);
    {
        char path[WAI_PATH_MAX];
        FILE *f;
        char buf[128];
        if (join_path(path, sizeof(path), rel) == 0 && (f = fopen(path, "r"))) {
            if (fgets(buf, sizeof(buf), f)) {
                size_t n = strlen(buf);
                while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) {
                    buf[--n] = '\0';
                }
                rc = append_str(out, cap, written, " product=");
                if (rc == 0) {
                    rc = append_str(out, cap, written, buf);
                }
            }
            fclose(f);
            if (rc != 0) {
                return rc;
            }
        }
    }
    snprintf(rel, sizeof(rel), "%s/%s/manufacturer", dir_rel, ent);
    {
        char path[WAI_PATH_MAX];
        FILE *f;
        char buf[128];
        if (join_path(path, sizeof(path), rel) == 0 && (f = fopen(path, "r"))) {
            if (fgets(buf, sizeof(buf), f)) {
                size_t n = strlen(buf);
                while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) {
                    buf[--n] = '\0';
                }
                rc = append_str(out, cap, written, " manufacturer=");
                if (rc == 0) {
                    rc = append_str(out, cap, written, buf);
                }
            }
            fclose(f);
            if (rc != 0) {
                return rc;
            }
        }
    }
    /* Never open serial. */
    return append_str(out, cap, written, "\n");
}

static int ent_pci(char *out, size_t cap, size_t *written,
                   const char *dir_rel, const char *ent)
{
    char rel[WAI_PATH_MAX];
    int rc;

    rc = append_str(out, cap, written, ent);
    if (rc != 0) {
        return rc;
    }
    snprintf(rel, sizeof(rel), "%s/%s/vendor", dir_rel, ent);
    rc = append_str(out, cap, written, " ");
    if (rc != 0) {
        return rc;
    }
    {
        /* reuse append_kv but inline short */
        char path[WAI_PATH_MAX];
        FILE *f;
        char buf[64];
        if (join_path(path, sizeof(path), rel) == 0 && (f = fopen(path, "r"))) {
            if (fgets(buf, sizeof(buf), f)) {
                size_t n = strlen(buf);
                while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) {
                    buf[--n] = '\0';
                }
                rc = append_str(out, cap, written, "vendor=");
                if (rc == 0) {
                    rc = append_str(out, cap, written, buf);
                }
            }
            fclose(f);
            if (rc != 0) {
                return rc;
            }
        }
    }
    snprintf(rel, sizeof(rel), "%s/%s/device", dir_rel, ent);
    {
        char path[WAI_PATH_MAX];
        FILE *f;
        char buf[64];
        if (join_path(path, sizeof(path), rel) == 0 && (f = fopen(path, "r"))) {
            if (fgets(buf, sizeof(buf), f)) {
                size_t n = strlen(buf);
                while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) {
                    buf[--n] = '\0';
                }
                rc = append_str(out, cap, written, " device=");
                if (rc == 0) {
                    rc = append_str(out, cap, written, buf);
                }
            }
            fclose(f);
            if (rc != 0) {
                return rc;
            }
        }
    }
    snprintf(rel, sizeof(rel), "%s/%s/class", dir_rel, ent);
    {
        char path[WAI_PATH_MAX];
        FILE *f;
        char buf[64];
        if (join_path(path, sizeof(path), rel) == 0 && (f = fopen(path, "r"))) {
            if (fgets(buf, sizeof(buf), f)) {
                size_t n = strlen(buf);
                while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) {
                    buf[--n] = '\0';
                }
                rc = append_str(out, cap, written, " class=");
                if (rc == 0) {
                    rc = append_str(out, cap, written, buf);
                }
            }
            fclose(f);
            if (rc != 0) {
                return rc;
            }
        }
    }
    return append_str(out, cap, written, "\n");
}

static int ent_battery(char *out, size_t cap, size_t *written,
                       const char *dir_rel, const char *ent)
{
    char rel[WAI_PATH_MAX];
    int rc;

    rc = append_str(out, cap, written, ent);
    if (rc != 0) {
        return rc;
    }
    snprintf(rel, sizeof(rel), "%s/%s/type", dir_rel, ent);
    {
        char path[WAI_PATH_MAX];
        FILE *f;
        char buf[64];
        if (join_path(path, sizeof(path), rel) == 0 && (f = fopen(path, "r"))) {
            if (fgets(buf, sizeof(buf), f)) {
                size_t n = strlen(buf);
                while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) {
                    buf[--n] = '\0';
                }
                rc = append_str(out, cap, written, " type=");
                if (rc == 0) {
                    rc = append_str(out, cap, written, buf);
                }
            }
            fclose(f);
            if (rc != 0) {
                return rc;
            }
        }
    }
    snprintf(rel, sizeof(rel), "%s/%s/status", dir_rel, ent);
    {
        char path[WAI_PATH_MAX];
        FILE *f;
        char buf[64];
        if (join_path(path, sizeof(path), rel) == 0 && (f = fopen(path, "r"))) {
            if (fgets(buf, sizeof(buf), f)) {
                size_t n = strlen(buf);
                while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) {
                    buf[--n] = '\0';
                }
                rc = append_str(out, cap, written, " status=");
                if (rc == 0) {
                    rc = append_str(out, cap, written, buf);
                }
            }
            fclose(f);
            if (rc != 0) {
                return rc;
            }
        }
    }
    snprintf(rel, sizeof(rel), "%s/%s/capacity", dir_rel, ent);
    {
        char path[WAI_PATH_MAX];
        FILE *f;
        char buf[64];
        if (join_path(path, sizeof(path), rel) == 0 && (f = fopen(path, "r"))) {
            if (fgets(buf, sizeof(buf), f)) {
                size_t n = strlen(buf);
                while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) {
                    buf[--n] = '\0';
                }
                rc = append_str(out, cap, written, " capacity=");
                if (rc == 0) {
                    rc = append_str(out, cap, written, buf);
                }
            }
            fclose(f);
            if (rc != 0) {
                return rc;
            }
        }
    }
    return append_str(out, cap, written, "\n");
}

static int ent_thermal(char *out, size_t cap, size_t *written,
                       const char *dir_rel, const char *ent)
{
    char rel[WAI_PATH_MAX];
    int rc;

    if (strncmp(ent, "thermal_zone", 12) != 0) {
        return 0;
    }
    rc = append_str(out, cap, written, ent);
    if (rc != 0) {
        return rc;
    }
    snprintf(rel, sizeof(rel), "%s/%s/type", dir_rel, ent);
    {
        char path[WAI_PATH_MAX];
        FILE *f;
        char buf[64];
        if (join_path(path, sizeof(path), rel) == 0 && (f = fopen(path, "r"))) {
            if (fgets(buf, sizeof(buf), f)) {
                size_t n = strlen(buf);
                while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) {
                    buf[--n] = '\0';
                }
                rc = append_str(out, cap, written, " type=");
                if (rc == 0) {
                    rc = append_str(out, cap, written, buf);
                }
            }
            fclose(f);
            if (rc != 0) {
                return rc;
            }
        }
    }
    snprintf(rel, sizeof(rel), "%s/%s/temp", dir_rel, ent);
    {
        char path[WAI_PATH_MAX];
        FILE *f;
        char buf[64];
        if (join_path(path, sizeof(path), rel) == 0 && (f = fopen(path, "r"))) {
            if (fgets(buf, sizeof(buf), f)) {
                size_t n = strlen(buf);
                while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) {
                    buf[--n] = '\0';
                }
                rc = append_str(out, cap, written, " temp=");
                if (rc == 0) {
                    rc = append_str(out, cap, written, buf);
                }
            }
            fclose(f);
            if (rc != 0) {
                return rc;
            }
        }
    }
    return append_str(out, cap, written, "\n");
}

static int scan_board(char *out, size_t cap, size_t *written)
{
    static const char *const keys[] = {
        "board_name",
        "board_vendor",
        "product_name",
        "product_family",
        "sys_vendor",
        "bios_vendor",
        "bios_version",
        "bios_date",
        NULL
    };
    int i;
    for (i = 0; keys[i] != NULL; i++) {
        char rel[WAI_PATH_MAX];
        int rc;
        snprintf(rel, sizeof(rel), "sys/class/dmi/id/%s", keys[i]);
        rc = append_kv_file(out, cap, written, rel, keys[i]);
        if (rc != 0) {
            return rc;
        }
    }
    return 0;
}

int petrush_wai_scan_impl(unsigned flags, char *out, size_t out_cap)
{
    size_t written = 0;
    int rc;

    if (!out) {
        return -1;
    }
    if (out_cap == 0) {
        return -ENOSPC;
    }
    out[0] = '\0';

    if (flags == 0) {
        flags = PETRUSH_WAI_ALL;
    }

    if (flags & PETRUSH_WAI_DISK) {
        rc = append_section(out, out_cap, &written, "disk");
        if (rc != 0) {
            return rc;
        }
        rc = walk_dir(out, out_cap, &written, "sys/block", ent_disk);
        if (rc != 0) {
            return rc;
        }
    }
    if (flags & PETRUSH_WAI_VIDEO) {
        rc = append_section(out, out_cap, &written, "video");
        if (rc != 0) {
            return rc;
        }
        rc = walk_dir(out, out_cap, &written, "sys/class/drm", ent_list_name);
        if (rc != 0) {
            return rc;
        }
    }
    if (flags & PETRUSH_WAI_MEM) {
        rc = append_section(out, out_cap, &written, "mem");
        if (rc != 0) {
            return rc;
        }
        rc = read_file_filtered(out, out_cap, &written, "proc/meminfo", 1);
        if (rc != 0) {
            return rc;
        }
    }
    if (flags & PETRUSH_WAI_AUDIO) {
        rc = append_section(out, out_cap, &written, "audio");
        if (rc != 0) {
            return rc;
        }
        rc = walk_dir(out, out_cap, &written, "sys/class/sound", ent_list_name);
        if (rc != 0) {
            return rc;
        }
    }
    if (flags & PETRUSH_WAI_CAMERA) {
        rc = append_section(out, out_cap, &written, "camera");
        if (rc != 0) {
            return rc;
        }
        rc = walk_dir(out, out_cap, &written, "sys/class/video4linux",
                      ent_list_name);
        if (rc != 0) {
            return rc;
        }
    }
    if (flags & PETRUSH_WAI_KEYBOARD) {
        rc = append_section(out, out_cap, &written, "keyboard");
        if (rc != 0) {
            return rc;
        }
        rc = walk_dir(out, out_cap, &written, "sys/class/input", ent_list_name);
        if (rc != 0) {
            return rc;
        }
    }
    if (flags & PETRUSH_WAI_USB) {
        rc = append_section(out, out_cap, &written, "usb");
        if (rc != 0) {
            return rc;
        }
        rc = walk_dir(out, out_cap, &written, "sys/bus/usb/devices", ent_usb);
        if (rc != 0) {
            return rc;
        }
    }
    if (flags & PETRUSH_WAI_PCI) {
        rc = append_section(out, out_cap, &written, "pci");
        if (rc != 0) {
            return rc;
        }
        rc = walk_dir(out, out_cap, &written, "sys/bus/pci/devices", ent_pci);
        if (rc != 0) {
            return rc;
        }
    }
    if (flags & PETRUSH_WAI_BATTERY) {
        rc = append_section(out, out_cap, &written, "battery");
        if (rc != 0) {
            return rc;
        }
        rc = walk_dir(out, out_cap, &written, "sys/class/power_supply",
                      ent_battery);
        if (rc != 0) {
            return rc;
        }
    }
    if (flags & PETRUSH_WAI_THERMAL) {
        rc = append_section(out, out_cap, &written, "thermal");
        if (rc != 0) {
            return rc;
        }
        rc = walk_dir(out, out_cap, &written, "sys/class/thermal", ent_thermal);
        if (rc != 0) {
            return rc;
        }
    }
    if (flags & PETRUSH_WAI_CPU) {
        rc = append_section(out, out_cap, &written, "cpu");
        if (rc != 0) {
            return rc;
        }
        rc = read_file_filtered(out, out_cap, &written, "proc/cpuinfo", 1);
        if (rc != 0) {
            return rc;
        }
    }
    if (flags & PETRUSH_WAI_BOARD) {
        rc = append_section(out, out_cap, &written, "board");
        if (rc != 0) {
            return rc;
        }
        rc = scan_board(out, out_cap, &written);
        if (rc != 0) {
            return rc;
        }
    }

    return (int)written;
}

#ifndef PETRUSH_HAVE_ASM
int petrush_wai_scan(unsigned flags, char *out, size_t out_cap)
{
    return petrush_wai_scan_impl(flags, out, out_cap);
}
#endif

static unsigned parse_wai_flag(const char *arg)
{
    if (strcmp(arg, "-disk") == 0) {
        return PETRUSH_WAI_DISK;
    }
    if (strcmp(arg, "-video") == 0) {
        return PETRUSH_WAI_VIDEO;
    }
    if (strcmp(arg, "-mem") == 0) {
        return PETRUSH_WAI_MEM;
    }
    if (strcmp(arg, "-audio") == 0) {
        return PETRUSH_WAI_AUDIO;
    }
    if (strcmp(arg, "-camera") == 0) {
        return PETRUSH_WAI_CAMERA;
    }
    if (strcmp(arg, "-keyboard") == 0) {
        return PETRUSH_WAI_KEYBOARD;
    }
    if (strcmp(arg, "-usb") == 0) {
        return PETRUSH_WAI_USB;
    }
    if (strcmp(arg, "-pci") == 0) {
        return PETRUSH_WAI_PCI;
    }
    if (strcmp(arg, "-battery") == 0) {
        return PETRUSH_WAI_BATTERY;
    }
    if (strcmp(arg, "-thermal") == 0) {
        return PETRUSH_WAI_THERMAL;
    }
    if (strcmp(arg, "-cpu") == 0) {
        return PETRUSH_WAI_CPU;
    }
    if (strcmp(arg, "-board") == 0) {
        return PETRUSH_WAI_BOARD;
    }
    return 0;
}

int builtin_wai(petrush_cmd_t *cmd)
{
    unsigned flags = 0;
    char *buf;
    size_t cap = 65536;
    int n;
    int i;

    if (!cmd) {
        return 1;
    }

    for (i = 1; i < cmd->argc; i++) {
        const char *a = cmd->argv[i];
        unsigned bit;
        if (strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0) {
            printf("%s\n",
                   _("wai - hardware inventory via sysfs/proc (no root)"));
            printf("%s\n",
                   _("Usage: wai [-disk] [-video] [-mem] [-audio] [-camera] "
                     "[-keyboard] [-usb] [-pci] [-battery] [-thermal] "
                     "[-cpu] [-board]"));
            printf("%s\n", _("No flags: all sections. Skips serial/uuid."));
            return 0;
        }
        bit = parse_wai_flag(a);
        if (bit == 0) {
            fprintf(stderr, _("wai: unknown flag '%s'\n"), a);
            return 2;
        }
        flags |= bit;
    }

    buf = malloc(cap);
    if (!buf) {
        fprintf(stderr, "%s\n", _("wai: out of memory"));
        return 1;
    }

    n = petrush_wai_scan(flags, buf, cap);
    if (n < 0) {
        if (n == -ENOSPC) {
            fprintf(stderr, "%s\n", _("wai: output truncated"));
        } else {
            fprintf(stderr, "%s\n", _("wai: scan failed"));
        }
        free(buf);
        return 1;
    }
    if (n > 0) {
        fputs(buf, stdout);
        if (buf[n - 1] != '\n') {
            fputc('\n', stdout);
        }
    }
    free(buf);
    return 0;
}
