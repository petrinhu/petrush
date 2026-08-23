/*
 * i18n.h - gettext facade (I18N-GETTEXT)
 * msgid source language: English. Catalogs: en, pt_BR, es_419.
 * ASM must not include this header or call gettext.
 */

#ifndef PETRUSH_I18N_H
#define PETRUSH_I18N_H

#include <libintl.h>

#ifndef PETRUSH_TEXTDOMAIN
#define PETRUSH_TEXTDOMAIN "petrush"
#endif

/* Mark runtime strings for gettext. */
#define _(String) gettext(String)

/* Mark strings for extraction without translating at the call site. */
#define N_(String) (String)

#ifdef __cplusplus
extern "C" {
#endif

/*
 * setlocale + bindtextdomain + textdomain.
 * localedir: override directory, or NULL to use PETRUSH_LOCALEDIR env,
 * then the compile-time PETRUSH_LOCALEDIR default.
 * Returns 0 on success, -1 on soft failure (still safe to call _()).
 */
int petrush_i18n_init(const char *localedir);

#ifdef __cplusplus
}
#endif

#endif /* PETRUSH_I18N_H */
