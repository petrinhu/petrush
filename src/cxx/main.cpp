/*
 * configsh - TUI de configuracao do petrush (CXX-00 stub).
 *
 * C++23, -fno-exceptions -fno-rtti. Binario separado do REPL.
 * TUI raw fica em CXX-TUI; esta fatia so prova o alvo + ldd.
 */

#include <cstdio>

#include "petrush/asm.h"

namespace {

void print_help()
{
    std::fputs(
        "configsh - petrush configuration helper\n"
        "Usage: configsh [--help]\n"
        "\n"
        "Options:\n"
        "  --help      show this help and exit\n"
        "  --dump      dump config (CXX-TUI)\n"
        "  --check     validate config (CXX-TUI)\n"
        "  --section   select section (CXX-TUI)\n"
        "\n"
        "Note: TUI raw lands in CXX-TUI. This binary is the C++23 target only.\n",
        stdout);
}

} // namespace

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

#ifdef PETRUSH_HAVE_ASM
    /* Keep tty_mode + utf8_width linked for the future TUI (CXX-TUI). */
    if (argc > 1000000) {
        (void)petrush_utf8_width("", 0);
        (void)petrush_tty_mode(0, PETRUSH_TTY_COOKED);
    }
#endif

    print_help();
    return 0;
}
