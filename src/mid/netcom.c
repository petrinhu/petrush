/*
 * netcom.c - ASM-NET: scan wifi/eth/bt (sysfs + netlink GET) e -up/-down.
 * petrush_netcom_scan_impl = corpo I/O (ASan). Entrada ASM em netcom_scan.S.
 * Builtin netcom: -wifi -eth -bt; -up IFACE; -down IFACE.
 * Sem CAP_NET_ADMIN → -EPERM imediato (sem hang, sem spawn). Sem 4755.
 */

#define _GNU_SOURCE

#include "petrush/asm.h"
#include "petrush/dispatcher.h"
#include "petrush/i18n.h"
#include "petrush/parser.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/if_arp.h>
#include <linux/if_link.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef IFNAMSIZ
#define IFNAMSIZ 16
#endif

#ifndef CAP_NET_ADMIN
#define CAP_NET_ADMIN 12
#endif

#define NETCOM_ROOT_MAX 256
#define NETCOM_PATH_MAX 512
#define NETCOM_HELPER_TIMEOUT_MS 2000

static char g_netcom_root[NETCOM_ROOT_MAX];

void petrush_netcom_set_root(const char *root)
{
    if (!root || root[0] == '\0') {
        g_netcom_root[0] = '\0';
        return;
    }
    size_t n = strlen(root);
    if (n >= NETCOM_ROOT_MAX) {
        n = NETCOM_ROOT_MAX - 1;
    }
    memcpy(g_netcom_root, root, n);
    while (n > 1 && g_netcom_root[n - 1] == '/') {
        n--;
    }
    g_netcom_root[n] = '\0';
}

static int join_path(char *dst, size_t dst_cap, const char *rel)
{
    int n;

    if (rel[0] == '/') {
        rel++;
    }
    if (g_netcom_root[0] == '\0') {
        n = snprintf(dst, dst_cap, "/%s", rel);
    } else {
        n = snprintf(dst, dst_cap, "%s/%s", g_netcom_root, rel);
    }
    if (n < 0 || (size_t)n >= dst_cap) {
        return -1;
    }
    return 0;
}

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

static int read_trim(const char *rel, char *buf, size_t buf_cap)
{
    char path[NETCOM_PATH_MAX];
    FILE *f;
    size_t n;

    if (join_path(path, sizeof(path), rel) != 0) {
        return -1;
    }
    f = fopen(path, "r");
    if (!f) {
        return -1;
    }
    if (!fgets(buf, (int)buf_cap, f)) {
        fclose(f);
        return -1;
    }
    fclose(f);
    n = strlen(buf);
    while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) {
        buf[--n] = '\0';
    }
    return 0;
}

static int path_is_dir(const char *rel)
{
    char path[NETCOM_PATH_MAX];
    struct stat st;

    if (join_path(path, sizeof(path), rel) != 0) {
        return 0;
    }
    if (stat(path, &st) != 0) {
        return 0;
    }
    return S_ISDIR(st.st_mode);
}

static int path_exists(const char *rel)
{
    char path[NETCOM_PATH_MAX];

    if (join_path(path, sizeof(path), rel) != 0) {
        return 0;
    }
    return access(path, F_OK) == 0;
}

static int iface_name_ok(const char *iface)
{
    size_t i;
    size_t n;

    if (!iface || iface[0] == '\0') {
        return 0;
    }
    n = strlen(iface);
    if (n >= IFNAMSIZ) {
        return 0;
    }
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)iface[i];
        if (!(isalnum(c) || c == '_' || c == '.' || c == ':' || c == '-')) {
            return 0;
        }
    }
    return 1;
}

int petrush_netcom_have_cap_net_admin(void)
{
    FILE *f;
    char line[256];
    unsigned long long eff = 0;
    int found = 0;

    f = fopen("/proc/self/status", "r");
    if (!f) {
        return 0;
    }
    while (fgets(line, sizeof(line), f) != NULL) {
        if (strncmp(line, "CapEff:", 7) == 0) {
            if (sscanf(line + 7, "%llx", &eff) == 1) {
                found = 1;
            }
            break;
        }
    }
    fclose(f);
    if (!found) {
        return 0;
    }
    return (eff & (1ULL << CAP_NET_ADMIN)) != 0;
}

/* ---- netlink GET (read-only dump); skipped under overlay ---- */

struct nl_if_info {
    char name[IFNAMSIZ];
    unsigned int flags;
    unsigned char operstate; /* IF_OPER_* */
    int has_operstate;
};

#define NL_IF_MAX 64

static int netlink_dump_links(struct nl_if_info *out, int out_cap, int *out_n)
{
    int fd = -1;
    struct sockaddr_nl sa;
    struct {
        struct nlmsghdr nh;
        struct ifinfomsg ifm;
    } req;
    char buf[8192];
    ssize_t nr;
    int count = 0;
    int done = 0;

    *out_n = 0;
    if (g_netcom_root[0] != '\0') {
        return 0; /* overlay: sysfs only */
    }

    fd = socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_ROUTE);
    if (fd < 0) {
        return 0; /* degrade: sysfs only */
    }

    memset(&sa, 0, sizeof(sa));
    sa.nl_family = AF_NETLINK;
    if (bind(fd, (struct sockaddr *)&sa, sizeof(sa)) != 0) {
        close(fd);
        return 0;
    }

    memset(&req, 0, sizeof(req));
    req.nh.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
    req.nh.nlmsg_type = RTM_GETLINK;
    req.nh.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
    req.nh.nlmsg_seq = 1;
    req.nh.nlmsg_pid = 0;
    req.ifm.ifi_family = AF_UNSPEC;

    if (send(fd, &req, req.nh.nlmsg_len, 0) < 0) {
        close(fd);
        return 0;
    }

    while (!done) {
        struct pollfd pfd;
        struct nlmsghdr *nh;

        pfd.fd = fd;
        pfd.events = POLLIN;
        if (poll(&pfd, 1, 500) <= 0) {
            break;
        }
        nr = recv(fd, buf, sizeof(buf), 0);
        if (nr <= 0) {
            break;
        }
        for (nh = (struct nlmsghdr *)buf;
             NLMSG_OK(nh, (unsigned)nr);
             nh = NLMSG_NEXT(nh, nr)) {
            struct ifinfomsg *ifi;
            struct rtattr *rta;
            int rta_len;
            struct nl_if_info *slot;

            if (nh->nlmsg_type == NLMSG_DONE) {
                done = 1;
                break;
            }
            if (nh->nlmsg_type == NLMSG_ERROR) {
                done = 1;
                break;
            }
            if (nh->nlmsg_type != RTM_NEWLINK) {
                continue;
            }
            if (count >= out_cap) {
                continue;
            }
            ifi = NLMSG_DATA(nh);
            slot = &out[count];
            memset(slot, 0, sizeof(*slot));
            slot->flags = ifi->ifi_flags;
            rta = IFLA_RTA(ifi);
            rta_len = (int)IFLA_PAYLOAD(nh);
            /* RTA_NEXT mistura int e unsigned (RTA_ALIGNTO=4U); -Wsign-conversion. */
            while (RTA_OK(rta, rta_len)) {
                if (rta->rta_type == IFLA_IFNAME) {
                    size_t n = (size_t)RTA_PAYLOAD(rta);
                    if (n >= IFNAMSIZ) {
                        n = IFNAMSIZ - 1;
                    }
                    memcpy(slot->name, RTA_DATA(rta), n);
                    slot->name[n] = '\0';
                } else if (rta->rta_type == IFLA_OPERSTATE &&
                           RTA_PAYLOAD(rta) >= 1) {
                    slot->operstate = *(unsigned char *)RTA_DATA(rta);
                    slot->has_operstate = 1;
                }
                {
                    unsigned int step = RTA_ALIGN(rta->rta_len);
                    rta_len -= (int)step;
                    rta = (struct rtattr *)((char *)rta + step);
                }
            }
            if (slot->name[0] != '\0') {
                count++;
            }
        }
    }
    close(fd);
    *out_n = count;
    return 0;
}

static const char *operstate_name(const char *from_sysfs,
                                  const struct nl_if_info *nl, int nl_n,
                                  const char *ifname)
{
    static const char *const names[] = {
        "unknown", "notpresent", "down", "lowerlayerdown",
        "testing", "dormant", "up"
    };
    int i;

    if (from_sysfs && from_sysfs[0] != '\0') {
        return from_sysfs;
    }
    for (i = 0; i < nl_n; i++) {
        if (strcmp(nl[i].name, ifname) == 0 && nl[i].has_operstate) {
            if (nl[i].operstate < (sizeof(names) / sizeof(names[0]))) {
                return names[nl[i].operstate];
            }
            return "unknown";
        }
    }
    return "unknown";
}

static int iface_is_wifi(const char *ifname)
{
    char rel[NETCOM_PATH_MAX];

    snprintf(rel, sizeof(rel), "sys/class/net/%s/wireless", ifname);
    if (path_is_dir(rel) || path_exists(rel)) {
        return 1;
    }
    snprintf(rel, sizeof(rel), "sys/class/net/%s/phy80211", ifname);
    if (path_exists(rel)) {
        return 1;
    }
    return 0;
}

static int append_net_iface(char *out, size_t cap, size_t *written,
                            const char *ifname, int wifi,
                            const struct nl_if_info *nl, int nl_n)
{
    char rel[NETCOM_PATH_MAX];
    char typebuf[32];
    char opbuf[64];
    const char *op;
    int rc;
    long type_val = -1;

    snprintf(rel, sizeof(rel), "sys/class/net/%s/type", ifname);
    if (read_trim(rel, typebuf, sizeof(typebuf)) != 0) {
        return 0;
    }
    type_val = strtol(typebuf, NULL, 10);
    if (type_val == ARPHRD_LOOPBACK) {
        return 0;
    }
    if (type_val != ARPHRD_ETHER) {
        return 0;
    }

    if (wifi) {
        if (!iface_is_wifi(ifname)) {
            return 0;
        }
    } else {
        if (iface_is_wifi(ifname)) {
            return 0;
        }
    }

    snprintf(rel, sizeof(rel), "sys/class/net/%s/operstate", ifname);
    if (read_trim(rel, opbuf, sizeof(opbuf)) != 0) {
        opbuf[0] = '\0';
    }
    op = operstate_name(opbuf, nl, nl_n, ifname);

    rc = append_str(out, cap, written, ifname);
    if (rc != 0) {
        return rc;
    }
    rc = append_str(out, cap, written, " type=");
    if (rc != 0) {
        return rc;
    }
    rc = append_str(out, cap, written, typebuf);
    if (rc != 0) {
        return rc;
    }
    rc = append_str(out, cap, written, " operstate=");
    if (rc != 0) {
        return rc;
    }
    rc = append_str(out, cap, written, op);
    if (rc != 0) {
        return rc;
    }
    if (wifi) {
        rc = append_str(out, cap, written, " wireless=1");
        if (rc != 0) {
            return rc;
        }
    }
    return append_str(out, cap, written, "\n");
}

static int scan_net_class(char *out, size_t cap, size_t *written, int wifi,
                          const struct nl_if_info *nl, int nl_n)
{
    char path[NETCOM_PATH_MAX];
    DIR *d;
    struct dirent *ent;

    if (join_path(path, sizeof(path), "sys/class/net") != 0) {
        return -1;
    }
    d = opendir(path);
    if (!d) {
        return 0;
    }
    while ((ent = readdir(d)) != NULL) {
        int rc;
        if (ent->d_name[0] == '.') {
            continue;
        }
        if (!iface_name_ok(ent->d_name)) {
            continue;
        }
        rc = append_net_iface(out, cap, written, ent->d_name, wifi, nl, nl_n);
        if (rc != 0) {
            closedir(d);
            return rc;
        }
    }
    closedir(d);
    return 0;
}

static int append_bt_dev(char *out, size_t cap, size_t *written,
                         const char *dev)
{
    char rel[NETCOM_PATH_MAX];
    char name[128];
    char addr[64];
    int rc;
    int has_name = 0;
    int has_addr = 0;

    snprintf(rel, sizeof(rel), "sys/class/bluetooth/%s/name", dev);
    if (read_trim(rel, name, sizeof(name)) == 0) {
        has_name = 1;
    }
    snprintf(rel, sizeof(rel), "sys/class/bluetooth/%s/address", dev);
    if (read_trim(rel, addr, sizeof(addr)) == 0) {
        has_addr = 1;
    }

    rc = append_str(out, cap, written, dev);
    if (rc != 0) {
        return rc;
    }
    if (has_name) {
        rc = append_str(out, cap, written, " name=");
        if (rc != 0) {
            return rc;
        }
        rc = append_str(out, cap, written, name);
        if (rc != 0) {
            return rc;
        }
    }
    if (has_addr) {
        rc = append_str(out, cap, written, " address=");
        if (rc != 0) {
            return rc;
        }
        rc = append_str(out, cap, written, addr);
        if (rc != 0) {
            return rc;
        }
    }
    return append_str(out, cap, written, "\n");
}

static int scan_bt(char *out, size_t cap, size_t *written)
{
    char path[NETCOM_PATH_MAX];
    DIR *d;
    struct dirent *ent;

    if (join_path(path, sizeof(path), "sys/class/bluetooth") != 0) {
        return -1;
    }
    d = opendir(path);
    if (!d) {
        return 0;
    }
    while ((ent = readdir(d)) != NULL) {
        int rc;
        if (ent->d_name[0] == '.') {
            continue;
        }
        if (!iface_name_ok(ent->d_name)) {
            continue;
        }
        rc = append_bt_dev(out, cap, written, ent->d_name);
        if (rc != 0) {
            closedir(d);
            return rc;
        }
    }
    closedir(d);
    return 0;
}

int petrush_netcom_scan_impl(unsigned flags, char *out, size_t out_cap)
{
    size_t written = 0;
    int rc;
    struct nl_if_info nl[NL_IF_MAX];
    int nl_n = 0;

    if (!out) {
        return -1;
    }
    if (out_cap == 0) {
        return -ENOSPC;
    }
    out[0] = '\0';

    if (flags == 0) {
        flags = PETRUSH_NETCOM_WIFI | PETRUSH_NETCOM_ETH | PETRUSH_NETCOM_BT;
    }

    (void)netlink_dump_links(nl, NL_IF_MAX, &nl_n);

    if (flags & PETRUSH_NETCOM_WIFI) {
        rc = append_section(out, out_cap, &written, "wifi");
        if (rc != 0) {
            return rc;
        }
        rc = scan_net_class(out, out_cap, &written, 1, nl, nl_n);
        if (rc != 0) {
            return rc;
        }
    }
    if (flags & PETRUSH_NETCOM_ETH) {
        rc = append_section(out, out_cap, &written, "eth");
        if (rc != 0) {
            return rc;
        }
        rc = scan_net_class(out, out_cap, &written, 0, nl, nl_n);
        if (rc != 0) {
            return rc;
        }
    }
    if (flags & PETRUSH_NETCOM_BT) {
        rc = append_section(out, out_cap, &written, "bt");
        if (rc != 0) {
            return rc;
        }
        rc = scan_bt(out, out_cap, &written);
        if (rc != 0) {
            return rc;
        }
    }

    return (int)written;
}

#ifndef PETRUSH_HAVE_ASM
int petrush_netcom_scan(unsigned flags, char *out, size_t out_cap)
{
    return petrush_netcom_scan_impl(flags, out, out_cap);
}
#endif

/* ---- -up/-down helpers (C only; timeout; EPERM sem CAP) ---- */

static int which_exec(const char *name, char *out, size_t out_cap)
{
    const char *path_env;
    char tmp[PATH_MAX];
    char *save = NULL;
    char *dir;
    char *path_copy;
    size_t n;

    path_env = getenv("PATH");
    if (!path_env || path_env[0] == '\0') {
        path_env = "/usr/sbin:/usr/bin:/sbin:/bin";
    }
    n = strlen(path_env);
    path_copy = malloc(n + 1);
    if (!path_copy) {
        return -1;
    }
    memcpy(path_copy, path_env, n + 1);
    for (dir = strtok_r(path_copy, ":", &save); dir != NULL;
         dir = strtok_r(NULL, ":", &save)) {
        int m = snprintf(tmp, sizeof(tmp), "%s/%s", dir, name);
        if (m < 0 || (size_t)m >= sizeof(tmp)) {
            continue;
        }
        if (access(tmp, X_OK) == 0) {
            if (strlen(tmp) >= out_cap) {
                free(path_copy);
                return -1;
            }
            memcpy(out, tmp, strlen(tmp) + 1);
            free(path_copy);
            return 0;
        }
    }
    free(path_copy);
    return -1;
}

static int run_helper_timeout(char *const argv[], int timeout_ms)
{
    pid_t pid;
    int status = 0;
    int elapsed = 0;
    const int step = 50;

    pid = fork();
    if (pid < 0) {
        return -errno;
    }
    if (pid == 0) {
        int devnull = open("/dev/null", O_RDWR);
        if (devnull >= 0) {
            (void)dup2(devnull, STDIN_FILENO);
            (void)dup2(devnull, STDOUT_FILENO);
            (void)dup2(devnull, STDERR_FILENO);
            if (devnull > 2) {
                close(devnull);
            }
        }
        execv(argv[0], argv);
        _exit(127);
    }

    while (elapsed < timeout_ms) {
        pid_t w = waitpid(pid, &status, WNOHANG);
        if (w == pid) {
            if (WIFEXITED(status)) {
                int code = WEXITSTATUS(status);
                if (code == 0) {
                    return 0;
                }
                if (code == 127) {
                    return -ENOENT;
                }
                /* Helper ran but failed; often EPERM from kernel. */
                return -EPERM;
            }
            if (WIFSIGNALED(status)) {
                return -EPERM;
            }
            return -EIO;
        }
        if (w < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -errno;
        }
        poll(NULL, 0, step);
        elapsed += step;
    }
    (void)kill(pid, SIGKILL);
    (void)waitpid(pid, &status, 0);
    return -ETIMEDOUT;
}

static int try_ip_link(const char *iface, int up)
{
    char ip_path[PATH_MAX];
    char *argv[8];

    if (which_exec("ip", ip_path, sizeof(ip_path)) != 0) {
        return -ENOENT;
    }
    argv[0] = ip_path;
    argv[1] = "link";
    argv[2] = "set";
    argv[3] = "dev";
    argv[4] = (char *)iface;
    argv[5] = up ? "up" : "down";
    argv[6] = NULL;
    return run_helper_timeout(argv, NETCOM_HELPER_TIMEOUT_MS);
}

static int try_iw(const char *iface, int up)
{
    char iw_path[PATH_MAX];
    char *argv[8];

    if (which_exec("iw", iw_path, sizeof(iw_path)) != 0) {
        return -ENOENT;
    }
    /* iw has no direct up/down; use `iw dev IFACE set power_save off` as
     * presence probe is wrong. Prefer ip for link state; iw only if we
     * need phy bring-up via `ip link` already tried. Fallback: reject. */
    (void)iface;
    (void)up;
    (void)argv;
    return -ENOENT;
}

static int try_iwd(int up)
{
    char path[PATH_MAX];
    char *argv[8];

    if (which_exec("iwctl", path, sizeof(path)) != 0) {
        return -ENOENT;
    }
    /* iwctl is interactive; avoid hang. Do not spawn without scripted args.
     * Documented helper exists only if non-interactive form is available. */
    (void)up;
    (void)argv;
    return -ENOENT;
}

static int try_bluetoothctl(int up)
{
    char path[PATH_MAX];
    char *argv[8];

    if (which_exec("bluetoothctl", path, sizeof(path)) != 0) {
        return -ENOENT;
    }
    argv[0] = path;
    argv[1] = "power";
    argv[2] = up ? "on" : "off";
    argv[3] = NULL;
    return run_helper_timeout(argv, NETCOM_HELPER_TIMEOUT_MS);
}

int petrush_netcom_link_set(const char *iface, int up)
{
    int rc;
    int any_helper = 0;

    if (!iface_name_ok(iface)) {
        return -EINVAL;
    }
    if (!petrush_netcom_have_cap_net_admin()) {
        return -EPERM;
    }

    rc = try_ip_link(iface, up);
    if (rc == 0) {
        return 0;
    }
    if (rc != -ENOENT) {
        return rc;
    }

    rc = try_iw(iface, up);
    if (rc == 0) {
        return 0;
    }
    if (rc != -ENOENT) {
        any_helper = 1;
        return rc;
    }

    rc = try_iwd(up);
    if (rc == 0) {
        return 0;
    }
    if (rc != -ENOENT) {
        any_helper = 1;
        return rc;
    }

    /* BT adapter names often start with hci */
    if (strncmp(iface, "hci", 3) == 0) {
        rc = try_bluetoothctl(up);
        if (rc == 0) {
            return 0;
        }
        if (rc != -ENOENT) {
            return rc;
        }
    }

    (void)any_helper;
    return -ENOENT;
}

static unsigned parse_netcom_flag(const char *arg)
{
    if (strcmp(arg, "-wifi") == 0) {
        return PETRUSH_NETCOM_WIFI;
    }
    if (strcmp(arg, "-eth") == 0) {
        return PETRUSH_NETCOM_ETH;
    }
    if (strcmp(arg, "-bt") == 0) {
        return PETRUSH_NETCOM_BT;
    }
    return 0;
}

int builtin_netcom(petrush_cmd_t *cmd)
{
    unsigned flags = 0;
    int want_up = 0;
    int want_down = 0;
    const char *iface = NULL;
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
                   _("netcom - network inventory / link up-down (no setuid)"));
            printf("%s\n",
                   _("Usage: netcom [-wifi] [-eth] [-bt]"));
            printf("%s\n",
                   _("       netcom -up IFACE | -down IFACE"));
            printf("%s\n",
                   _("Scan needs no CAP. -up/-down need CAP_NET_ADMIN "
                     "(else EPERM, exit 1, no hang)."));
            return 0;
        }
        if (strcmp(a, "-up") == 0) {
            want_up = 1;
            if (i + 1 >= cmd->argc) {
                fprintf(stderr, "%s\n", _("netcom: -up needs IFACE"));
                return 2;
            }
            iface = cmd->argv[++i];
            continue;
        }
        if (strcmp(a, "-down") == 0) {
            want_down = 1;
            if (i + 1 >= cmd->argc) {
                fprintf(stderr, "%s\n", _("netcom: -down needs IFACE"));
                return 2;
            }
            iface = cmd->argv[++i];
            continue;
        }
        bit = parse_netcom_flag(a);
        if (bit == 0) {
            fprintf(stderr, _("netcom: unknown flag '%s'\n"), a);
            return 2;
        }
        flags |= bit;
    }

    if (want_up && want_down) {
        fprintf(stderr, "%s\n", _("netcom: -up and -down are mutually exclusive"));
        return 2;
    }
    if ((want_up || want_down) && flags != 0) {
        fprintf(stderr, "%s\n",
                _("netcom: do not mix scan flags with -up/-down"));
        return 2;
    }

    if (want_up || want_down) {
        int rc;
        if (!petrush_netcom_have_cap_net_admin()) {
            fprintf(stderr, "%s\n",
                    _("netcom: EPERM: CAP_NET_ADMIN required for -up/-down"));
            return 1;
        }
        rc = petrush_netcom_link_set(iface, want_up ? 1 : 0);
        if (rc == 0) {
            return 0;
        }
        if (rc == -EPERM) {
            fprintf(stderr, "%s\n",
                    _("netcom: EPERM: CAP_NET_ADMIN required for -up/-down"));
            return 1;
        }
        if (rc == -EINVAL) {
            fprintf(stderr, _("netcom: invalid IFACE '%s'\n"),
                    iface ? iface : "");
            return 2;
        }
        if (rc == -ENOENT) {
            fprintf(stderr, "%s\n",
                    _("netcom: no helper (ip/iw/iwd/bluetoothctl) in PATH"));
            return 1;
        }
        if (rc == -ETIMEDOUT) {
            fprintf(stderr, "%s\n", _("netcom: helper timed out"));
            return 1;
        }
        fprintf(stderr, _("netcom: link set failed (%d)\n"), rc);
        return 1;
    }

    buf = malloc(cap);
    if (!buf) {
        fprintf(stderr, "%s\n", _("netcom: out of memory"));
        return 1;
    }

    n = petrush_netcom_scan(flags, buf, cap);
    if (n < 0) {
        if (n == -ENOSPC) {
            fprintf(stderr, "%s\n", _("netcom: output truncated"));
        } else {
            fprintf(stderr, "%s\n", _("netcom: scan failed"));
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
