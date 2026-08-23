# `configsh`

**Tipo Diátaxis:** reference  
**Audiência:** utilizador e contribuidores  
**Item:** DOC-DIA-PT · **Owner:** technical-writer · **Last-reviewed:** 2026-08-23

## Sinopse

```text
configsh [--help] [--dump] [--check] [--section NAME]
configsh --section=NAME …
```

Binário separado (C++23, `-fno-exceptions -fno-rtti`). TUI raw ANSI (sem ncurses, sem Qt).

## Descrição

Lê e valida um INI sob paths XDG (ver [xdg](xdg.md)). Sem ficheiro: carrega defaults em memória. Sem argumentos em TTY: abre TUI (`q` sai). Sem argumentos fora de TTY: imprime help e exit 0.

## Opções

| Flag | Default | Descrição |
|------|---------|-----------|
| `--help`, `-h` | | Ajuda; exit 0 |
| `--dump` | | Imprime INI (ou secção) em stdout |
| `--check` | | Valida; exit 0 se ok |
| `--section NAME` | todas | Limita dump/check/TUI à secção |

## Secções conhecidas

`prompt`, `aliases`, `env`, `history`, `general`

Defaults se o ficheiro faltar:

```ini
[prompt]
ps1=petrush> 

[history]
max=1000
```

## Path de config (ordem)

1. `$PETRUSH_CONFIG` (se definido e não vazio)
2. `$XDG_CONFIG_HOME/petrush/config.ini`
3. `$HOME/.config/petrush/config.ini`
4. Fallback: `./.config/petrush/config.ini` (só se `HOME` faltar)

## Códigos de saída

| Code | Significado |
|------|-------------|
| 0 | Sucesso / help (incl. no-TTY sem args) |
| 1 | Path inválido, load malformado, secção desconhecida/ausente, check falhou |
| 2 | Opção desconhecida ou `--section` sem NAME |

## Limites do modelo

| Constante | Valor |
|-----------|-------|
| path | 512 |
| nome de secção | 32 |
| chave | 64 |
| valor | 256 |
| entradas / secção | 64 |
| secções | 8 |
| linha | 512 |

## Notas

- Não partilha ABI de `plugins/abi.h`.
- Pode usar `petrush_tty_mode` / `petrush_utf8_width` via `extern "C"`.
- Smoke: `tests/smoke/cxx-tui.sh`.

## Ver também

- How-to: [Usar configsh](../how-to/usar-configsh.md)
- [Paths XDG](xdg.md)
