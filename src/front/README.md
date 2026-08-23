# src/front - Camada de Apresentação (UI / REPL)

**Porte:** early (Pipeline-Sprint; `.bigtech-porte`). A palavra “Solo” em prosa antiga é informal.

Código de interface com o usuário nesta pasta (e o composition root em `src/main.c`):

| Unidade | Papel |
|---------|-------|
| `../main.c` (raiz de `src/`) | Loop REPL, sinais de UI, load de `~/.petrushrc`, history linenoise |
| `complete.c` | Tab-complete + history autosuggest |
| `highlight.c` | Colorize mínimo do slice visível (UX-21) |
| `rc_trust.c` | Checagem uid/mode do rc (`petrush_rc_stat_ok`). Uso também pelo Mid (`source.c`); ver nota F2 em `docs/architecture.md` |

Integração linenoise (history, prompt, completion hooks) vive no Front; atribuição BSD-2 em `NOTICE`.

**Não confundir com Mid:** parse/builtins/`pudo` cliente ficam em `src/mid/`. Jobs `&` em Foundation (`job.c`) + exceção de `fork` no Mid (documentada). Helper privilegiado: `src/pudod/` (binário separado).

Quando a separação física for feita (após decisão NEW-03 / porte maior), mover `main.c` para cá e atualizar CMake/includes.

Ver `docs/architecture.md` (mapa canônico, exceções F3/F4) e `CLAUDE.md`.
