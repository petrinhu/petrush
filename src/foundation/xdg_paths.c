/*
 * xdg_paths.c - XDG-1 path resolution for rc + history
 */

#include "petrush/xdg_paths.h"
#include "petrush/env.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static int path_exists(const char *path)
{
    return path && path[0] && access(path, F_OK) == 0;
}

static int copy_out(char *buf, size_t buflen, const char *src)
{
    size_t n;

    if (!buf || buflen == 0 || !src) {
        errno = EINVAL;
        return -1;
    }
    n = strlen(src);
    if (n + 1 > buflen) {
        errno = ENAMETOOLONG;
        return -1;
    }
    memcpy(buf, src, n + 1);
    return 0;
}

static void build_rc_xdg(char *out, size_t outlen)
{
    const char *xdg = petrush_getenv("XDG_CONFIG_HOME");
    const char *home = petrush_getenv("HOME");

    if (xdg && xdg[0]) {
        snprintf(out, outlen, "%s/petrush/rc", xdg);
    } else if (home && home[0]) {
        snprintf(out, outlen, "%s/.config/petrush/rc", home);
    } else {
        snprintf(out, outlen, ".config/petrush/rc");
    }
}

static void build_rc_legacy(char *out, size_t outlen)
{
    const char *home = petrush_getenv("HOME");

    if (home && home[0]) {
        snprintf(out, outlen, "%s/.petrushrc", home);
    } else {
        snprintf(out, outlen, ".petrushrc");
    }
}

static void build_hist_xdg(char *out, size_t outlen)
{
    const char *xdg = petrush_getenv("XDG_STATE_HOME");
    const char *home = petrush_getenv("HOME");

    if (xdg && xdg[0]) {
        snprintf(out, outlen, "%s/petrush/history", xdg);
    } else if (home && home[0]) {
        snprintf(out, outlen, "%s/.local/state/petrush/history", home);
    } else {
        snprintf(out, outlen, ".local/state/petrush/history");
    }
}

static void build_hist_legacy(char *out, size_t outlen)
{
    const char *home = petrush_getenv("HOME");

    if (home && home[0]) {
        snprintf(out, outlen, "%s/.petrush_history", home);
    } else {
        snprintf(out, outlen, ".petrush_history");
    }
}

int petrush_rc_path(char *buf, size_t buflen)
{
    char xdg[PATH_MAX];
    char legacy[PATH_MAX];
    const char *chosen;

    build_rc_xdg(xdg, sizeof(xdg));
    build_rc_legacy(legacy, sizeof(legacy));

    chosen = xdg;
    if (!path_exists(xdg) && path_exists(legacy)) {
        chosen = legacy;
    }
    return copy_out(buf, buflen, chosen);
}

int petrush_history_path(char *buf, size_t buflen)
{
    char xdg[PATH_MAX];
    char legacy[PATH_MAX];
    const char *chosen;

    build_hist_xdg(xdg, sizeof(xdg));
    build_hist_legacy(legacy, sizeof(legacy));

    chosen = xdg;
    if (!path_exists(xdg) && path_exists(legacy)) {
        chosen = legacy;
    }
    return copy_out(buf, buflen, chosen);
}

static int mkdir_p_0700(const char *dir)
{
    char tmp[PATH_MAX];
    size_t len;
    char *p;

    if (!dir || !dir[0]) {
        errno = EINVAL;
        return -1;
    }
    len = strlen(dir);
    if (len >= sizeof(tmp)) {
        errno = ENAMETOOLONG;
        return -1;
    }
    memcpy(tmp, dir, len + 1);

    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0700) != 0 && errno != EEXIST) {
                return -1;
            }
            *p = '/';
        }
    }
    if (mkdir(tmp, 0700) != 0 && errno != EEXIST) {
        return -1;
    }
    return 0;
}

int petrush_history_ensure_dir(const char *hist_path)
{
    char dir[PATH_MAX];
    size_t len;
    char *slash;

    if (!hist_path || !hist_path[0]) {
        errno = EINVAL;
        return -1;
    }
    len = strlen(hist_path);
    if (len >= sizeof(dir)) {
        errno = ENAMETOOLONG;
        return -1;
    }
    memcpy(dir, hist_path, len + 1);
    slash = strrchr(dir, '/');
    if (!slash) {
        /* ficheiro no cwd: nada a criar */
        return 0;
    }
    *slash = '\0';
    if (dir[0] == '\0') {
        return 0; /* path absoluto na raiz: "/history" */
    }
    return mkdir_p_0700(dir);
}
