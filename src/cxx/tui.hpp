/*
 * configsh raw ANSI TUI (CXX-TUI). Sem ncurses, sem Qt.
 */
#pragma once

#include "config.hpp"

namespace configsh {

/* Interactive browser. Returns 0 on clean quit, 1 on error.
 * Requires stdin TTY. Uses petrush_tty_mode when PETRUSH_HAVE_ASM. */
int run_tui(const Config *cfg, const char *section_filter);

} // namespace configsh
