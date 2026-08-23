/*
 * i18n_probe - bindtextdomain smoke helper (I18N-GETTEXT)
 * Prints one gettext lookup of the seed msgid and exits 0.
 */

#include "petrush/i18n.h"

#include <stdio.h>

int main(void)
{
    (void)petrush_i18n_init(NULL);
    puts(_("I18N-GETTEXT probe"));
    return 0;
}
