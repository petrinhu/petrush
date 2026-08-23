# src/cxx - configsh (C++23)

Binario **separado** do REPL `petrush`. ADR-001 / CXX-00.

- Linguagem: C++23 com `-fno-exceptions -fno-rtti`
- Sem Qt, sem ncurses (TUI raw em CXX-TUI)
- Pode ligar `petrush_tty_mode` e `petrush_utf8_width` via `petrush/asm.h`
- `petrush` **nao** liga `libstdc++` (prova: `tests/smoke/cxx00-ldd.sh`)
