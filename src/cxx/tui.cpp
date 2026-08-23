/*
 * configsh raw ANSI TUI (CXX-TUI).
 * Draws section list; j/k navigate, Enter open, q quit.
 */

#include "tui.hpp"

#include "petrush/asm.h"

#include <cstdio>
#include <cstring>
#include <termios.h>
#include <unistd.h>

namespace configsh {
namespace {

constexpr const char *kClear = "\033[2J\033[H";
constexpr const char *kBold = "\033[1m";
constexpr const char *kReset = "\033[0m";
constexpr const char *kInvert = "\033[7m";

struct TtyGuard {
    int fd = -1;
    bool armed = false;

    explicit TtyGuard(int f) : fd(f)
    {
#ifdef PETRUSH_HAVE_ASM
        if (petrush_tty_mode(fd, PETRUSH_TTY_RAW) == 0) {
            armed = true;
        }
#else
        struct termios t;
        if (tcgetattr(fd, &t) == 0) {
            t.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO | ISIG));
            t.c_cc[VMIN] = 1;
            t.c_cc[VTIME] = 0;
            if (tcsetattr(fd, TCSAFLUSH, &t) == 0) {
                armed = true;
            }
        }
#endif
    }

    TtyGuard(const TtyGuard &) = delete;
    TtyGuard &operator=(const TtyGuard &) = delete;

    ~TtyGuard()
    {
        if (!armed) {
            return;
        }
#ifdef PETRUSH_HAVE_ASM
        (void)petrush_tty_mode(fd, PETRUSH_TTY_COOKED);
#else
        struct termios t;
        if (tcgetattr(fd, &t) == 0) {
            t.c_lflag |= static_cast<tcflag_t>(ICANON | ECHO | ISIG);
            (void)tcsetattr(fd, TCSAFLUSH, &t);
        }
#endif
    }
};

int display_width(const char *s)
{
    if (!s) {
        return 0;
    }
#ifdef PETRUSH_HAVE_ASM
    int w = petrush_utf8_width(s, std::strlen(s));
    return w < 0 ? static_cast<int>(std::strlen(s)) : w;
#else
    return static_cast<int>(std::strlen(s));
#endif
}

int read_key(int fd)
{
    unsigned char c = 0;
    ssize_t n = ::read(fd, &c, 1);
    if (n != 1) {
        return -1;
    }
    if (c == 0x1b) {
        unsigned char seq[2] = {0, 0};
        if (::read(fd, &seq[0], 1) != 1) {
            return 0x1b;
        }
        if (seq[0] == '[') {
            if (::read(fd, &seq[1], 1) != 1) {
                return 0x1b;
            }
            if (seq[1] == 'A') {
                return 'k'; /* up */
            }
            if (seq[1] == 'B') {
                return 'j'; /* down */
            }
        }
        return 0x1b;
    }
    return static_cast<int>(c);
}

void draw_list(const Config *cfg, int selected, const char *filter)
{
    std::fputs(kClear, stdout);
    std::fputs(kBold, stdout);
    std::fputs("configsh", stdout);
    std::fputs(kReset, stdout);
    std::fputs("  (raw ANSI TUI, no ncurses)\n", stdout);
    if (cfg->path[0]) {
        std::printf("file: %s%s\n\n", cfg->path, cfg->loaded_from_disk ? "" : " (defaults)");
    } else {
        std::fputs("\n", stdout);
    }
    std::fputs("Sections  [j/k move, Enter open, q quit]\n", stdout);
    std::fputs("----------------------------------------\n", stdout);

    int idx = 0;
    for (std::size_t i = 0; i < cfg->n; ++i) {
        const Section *sec = &cfg->sections[i];
        if (filter && *filter && std::strcmp(sec->name, filter) != 0) {
            continue;
        }
        const bool on = (idx == selected);
        if (on) {
            std::fputs(kInvert, stdout);
        }
        std::printf("  [%s]", sec->name);
        int pad = 16 - display_width(sec->name);
        for (int p = 0; p < pad; ++p) {
            std::fputc(' ', stdout);
        }
        std::printf("  %zu keys", sec->n);
        if (on) {
            std::fputs(kReset, stdout);
        }
        std::fputc('\n', stdout);
        ++idx;
    }
    if (idx == 0) {
        std::fputs("  (no sections)\n", stdout);
    }
    std::fflush(stdout);
}

void draw_section(const Section *sec)
{
    std::fputs(kClear, stdout);
    std::fputs(kBold, stdout);
    std::printf("[%s]", sec->name);
    std::fputs(kReset, stdout);
    std::fputs("  [q/Esc back]\n", stdout);
    std::fputs("----------------------------------------\n", stdout);
    if (sec->n == 0) {
        std::fputs("  (empty)\n", stdout);
    }
    for (std::size_t i = 0; i < sec->n; ++i) {
        std::printf("  %s=%s\n", sec->entries[i].key, sec->entries[i].val);
    }
    std::fflush(stdout);
}

const Section *section_at_visible(const Config *cfg, const char *filter, int visible_index)
{
    int idx = 0;
    for (std::size_t i = 0; i < cfg->n; ++i) {
        const Section *sec = &cfg->sections[i];
        if (filter && *filter && std::strcmp(sec->name, filter) != 0) {
            continue;
        }
        if (idx == visible_index) {
            return sec;
        }
        ++idx;
    }
    return nullptr;
}

int count_visible(const Config *cfg, const char *filter)
{
    int n = 0;
    for (std::size_t i = 0; i < cfg->n; ++i) {
        if (filter && *filter && std::strcmp(cfg->sections[i].name, filter) != 0) {
            continue;
        }
        ++n;
    }
    return n;
}

} // namespace

int run_tui(const Config *cfg, const char *section_filter)
{
    if (!cfg) {
        return 1;
    }
    if (!::isatty(STDIN_FILENO) || !::isatty(STDOUT_FILENO)) {
        std::fputs("configsh: TUI requires a TTY (use --dump/--check)\n", stderr);
        return 1;
    }

    TtyGuard guard(STDIN_FILENO);
    if (!guard.armed) {
        std::fputs("configsh: failed to enter raw TTY mode\n", stderr);
        return 1;
    }

    /* --section NAME: jump straight into that section view. */
    if (section_filter && *section_filter) {
        const Section *sec = find_section(cfg, section_filter);
        if (!sec) {
            std::fprintf(stderr, "configsh: section [%s] not found\n", section_filter);
            return 1;
        }
        draw_section(sec);
        for (;;) {
            int k = read_key(STDIN_FILENO);
            if (k < 0 || k == 'q' || k == 'Q' || k == 0x1b || k == 3) {
                break;
            }
        }
        std::fputs(kClear, stdout);
        std::fflush(stdout);
        return 0;
    }

    int selected = 0;
    const int total = count_visible(cfg, nullptr);
    if (total <= 0) {
        selected = 0;
    }

    enum class Mode { List, Detail };
    Mode mode = Mode::List;
    const Section *detail = nullptr;

    draw_list(cfg, selected, nullptr);

    for (;;) {
        int k = read_key(STDIN_FILENO);
        if (k < 0 || k == 3) { /* Ctrl-C */
            break;
        }
        if (mode == Mode::List) {
            if (k == 'q' || k == 'Q') {
                break;
            }
            if (k == 'j' && total > 0) {
                selected = (selected + 1) % total;
                draw_list(cfg, selected, nullptr);
                continue;
            }
            if ((k == '\n' || k == '\r') && total > 0) {
                detail = section_at_visible(cfg, nullptr, selected);
                if (detail) {
                    mode = Mode::Detail;
                    draw_section(detail);
                }
                continue;
            }
            if (k == 'k' && total > 0) {
                selected = (selected - 1 + total) % total;
                draw_list(cfg, selected, nullptr);
                continue;
            }
        } else {
            if (k == 'q' || k == 'Q' || k == 0x1b) {
                mode = Mode::List;
                detail = nullptr;
                draw_list(cfg, selected, nullptr);
            }
        }
    }

    std::fputs(kClear, stdout);
    std::fflush(stdout);
    return 0;
}

} // namespace configsh
