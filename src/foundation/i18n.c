/*
 * i18n.c - bindtextdomain / textdomain bootstrap (I18N-GETTEXT)
 * Locale-agnostic callers live in C/C++ only; ASM never calls gettext.
 */

#include "petrush/i18n.h"

#include <locale.h>
#include <stdlib.h>

#ifndef PETRUSH_LOCALEDIR
#define PETRUSH_LOCALEDIR "/usr/local/share/locale"
#endif

int petrush_i18n_init(const char *localedir)
{
    const char *dir = localedir;

    if (!dir || !*dir) {
        dir = getenv("PETRUSH_LOCALEDIR");
    }
    if (!dir || !*dir) {
        dir = PETRUSH_LOCALEDIR;
    }

    /* Empty locale string = follow env (LANG / LC_* / LANGUAGE). */
    if (!setlocale(LC_ALL, "")) {
        /* Soft fail: keep going with C locale messages. */
        (void)setlocale(LC_ALL, "C");
    }

    if (!bindtextdomain(PETRUSH_TEXTDOMAIN, dir)) {
        return -1;
    }
    if (!textdomain(PETRUSH_TEXTDOMAIN)) {
        return -1;
    }
    return 0;
}
