/*
 * configsh - TUI de configuracao do petrush (CXX-TUI).
 *
 * C++23, -fno-exceptions -fno-rtti. Binario separado do REPL.
 * Raw ANSI (sem ncurses). Flags: --section --dump --check --help.
 * Config XDG: $XDG_CONFIG_HOME/petrush/config.ini (ou ~/.config/...).
 */

#include "config.hpp"
#include "tui.hpp"

#include <cstdio>
#include <cstring>
#include <unistd.h>

namespace {

void print_help()
{
    std::fputs(
        "configsh - petrush configuration helper (CXX-TUI)\n"
        "Usage: configsh [--help] [--dump] [--check] [--section NAME]\n"
        "\n"
        "Options:\n"
        "  --help              show this help and exit\n"
        "  --dump              print config (INI) to stdout\n"
        "  --check             validate config; exit 0 if ok\n"
        "  --section NAME      limit dump/check/TUI to section NAME\n"
        "\n"
        "Config path (XDG):\n"
        "  $PETRUSH_CONFIG, else $XDG_CONFIG_HOME/petrush/config.ini,\n"
        "  else ~/.config/petrush/config.ini\n"
        "\n"
        "Sections: prompt, aliases, env, history, general\n"
        "No args on a TTY opens the raw ANSI TUI (q quit).\n",
        stdout);
}

enum class Action { Help, Dump, Check, Tui };

struct Options {
    Action action = Action::Tui;
    const char *section = nullptr;
};

int parse_args(int argc, char **argv, Options *opt)
{
    if (!opt) {
        return -1;
    }
    *opt = Options{};
    for (int i = 1; i < argc; ++i) {
        const char *a = argv[i];
        if (!a) {
            continue;
        }
        if (std::strcmp(a, "--help") == 0 || std::strcmp(a, "-h") == 0) {
            opt->action = Action::Help;
            continue;
        }
        if (std::strcmp(a, "--dump") == 0) {
            opt->action = Action::Dump;
            continue;
        }
        if (std::strcmp(a, "--check") == 0) {
            opt->action = Action::Check;
            continue;
        }
        if (std::strcmp(a, "--section") == 0) {
            if (i + 1 >= argc || !argv[i + 1] || argv[i + 1][0] == '-') {
                std::fputs("configsh: --section requires NAME\n", stderr);
                return -1;
            }
            opt->section = argv[++i];
            continue;
        }
        if (std::strncmp(a, "--section=", 10) == 0) {
            const char *name = a + 10;
            if (!*name) {
                std::fputs("configsh: --section= requires NAME\n", stderr);
                return -1;
            }
            opt->section = name;
            continue;
        }
        std::fprintf(stderr, "configsh: unknown option '%s'\n", a);
        return -1;
    }
    return 0;
}

} // namespace

int main(int argc, char **argv)
{
    Options opt;
    if (parse_args(argc, argv, &opt) != 0) {
        print_help();
        return 2;
    }

    if (opt.action == Action::Help) {
        print_help();
        return 0;
    }

    /* No flags: help on non-TTY (keeps CXX-00 smoke), TUI on TTY. */
    if (opt.action == Action::Tui && argc == 1) {
        if (!::isatty(STDIN_FILENO) || !::isatty(STDOUT_FILENO)) {
            print_help();
            return 0;
        }
    }

    char path[configsh::kMaxPath];
    if (configsh::resolve_config_path(path, sizeof(path)) != 0) {
        std::fputs("configsh: cannot resolve config path\n", stderr);
        return 1;
    }

    configsh::Config cfg;
    if (configsh::load_config(&cfg, path) != 0) {
        return 1;
    }

    if (opt.section && !configsh::is_known_section(opt.section)) {
        std::fprintf(stderr, "configsh: unknown section [%s]\n", opt.section);
        return 1;
    }

    switch (opt.action) {
    case Action::Help:
        print_help();
        return 0;
    case Action::Dump:
        return configsh::dump_config(&cfg, opt.section);
    case Action::Check:
        return configsh::check_config(&cfg, opt.section);
    case Action::Tui:
        return configsh::run_tui(&cfg, opt.section);
    }
    return 0;
}
