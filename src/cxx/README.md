# src/cxx - configsh (C++23, CXX-TUI)

Binario **separado** do REPL `petrush`. ADR-001 / CXX-00 / CXX-TUI.

- Linguagem: C++23 com `-fno-exceptions -fno-rtti`
- Sem Qt, sem ncurses (TUI raw ANSI)
- Flags: `--help`, `--dump`, `--check`, `--section NAME`
- XDG: `$PETRUSH_CONFIG` ou `$XDG_CONFIG_HOME/petrush/config.ini` ou `~/.config/petrush/config.ini`
- Secoes: `prompt`, `aliases`, `env`, `history`, `general`
- Liga `petrush_tty_mode` e `petrush_utf8_width` via `petrush/asm.h` quando `PETRUSH_ASM`
- `petrush` **nao** liga `libstdc++` (prova: `tests/smoke/cxx00-ldd.sh`)
- Smoke: `tests/smoke/cxx-tui.sh` / target `cxx_tui`
