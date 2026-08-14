/*
 * prompt.c — PS1 escape expansion
 */

#include "petrush/prompt.h"
#include "petrush/env.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <pwd.h>

char *prompt_render(const char *ps1, char *out, size_t out_sz)
{
    if (!out || out_sz == 0) return out;
    out[0] = '\0';
    if (!ps1 || !ps1[0]) {
        snprintf(out, out_sz, "petrush> ");
        return out;
    }

    size_t o = 0;
    for (const char *p = ps1; *p && o + 1 < out_sz; p++) {
        if (*p != '\\' || !p[1]) {
            out[o++] = *p;
            continue;
        }
        p++;
        char tmp[PATH_MAX];
        tmp[0] = '\0';
        switch (*p) {
        case 'w':
            if (!getcwd(tmp, sizeof(tmp))) snprintf(tmp, sizeof(tmp), "?");
            break;
        case 'u': {
            const char *u = petrush_getenv("USER");
            if (!u || !u[0]) {
                struct passwd *pw = getpwuid(getuid());
                u = pw ? pw->pw_name : "?";
            }
            snprintf(tmp, sizeof(tmp), "%s", u);
            break;
        }
        case 'h': {
            if (gethostname(tmp, sizeof(tmp)) != 0) {
                snprintf(tmp, sizeof(tmp), "?");
            } else {
                tmp[sizeof(tmp) - 1] = '\0';
                char *dot = strchr(tmp, '.');
                if (dot) *dot = '\0';
            }
            break;
        }
        case 'n':
            snprintf(tmp, sizeof(tmp), "\n");
            break;
        case '$':
            snprintf(tmp, sizeof(tmp), "%c", (geteuid() == 0) ? '#' : '$');
            break;
        case '\\':
            snprintf(tmp, sizeof(tmp), "\\");
            break;
        default:
            /* unknown escape: keep literal */
            snprintf(tmp, sizeof(tmp), "\\%c", *p);
            break;
        }
        size_t tl = strlen(tmp);
        if (o + tl >= out_sz) tl = out_sz - o - 1;
        memcpy(out + o, tmp, tl);
        o += tl;
    }
    out[o] = '\0';
    return out;
}
