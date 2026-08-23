# src/front - Camada de Apresentação (UI / REPL)

**Porte:** early (Pipeline-Sprint; `.bigtech-porte`). A palavra “Solo” em prosa antiga é informal.

Código de interface com o usuário nesta pasta (e o composition root em `src/main.c`):

| Unidade | Papel |
|---------|-------|
| `../main.c` (raiz de `src/`) | Loop REPL, sinais de UI, load de `~/.petrushrc`, history linenoise |
| `complete.c` | Tab-complete + history autosuggest |
| `highlight.c` | Colorize mínimo do slice visível (UX-21) |

Integração linenoise (history, prompt, completion hooks) vive no Front; atribuição BSD-2 em `NOTICE`.

**Não confundir com Mid/Foundation:** parse/builtins/`pudo` cliente ficam em `src/mid/`. Jobs `&` e checagem uid/mode do rc (`rc_trust.c`, `petrush_rc_stat_ok`) ficam em Foundation (`src/foundation/`; ARCH-02 fechou F2 / R-I9). Exceção de `fork` no Mid documentada em `docs/architecture.md`. Helper privilegiado: `src/pudod/` (binário separado).

Quando a separação física for feita (após decisão NEW-03 / porte maior), mover `main.c` para cá e atualizar CMake/includes.

Ver `docs/architecture.md` (mapa canônico, exceções F3/F4; F2 fechado) e `CLAUDE.md`.
