# `configsh`

**Tipo Diátaxis:** reference  
**Audiencia:** usuario y contribuidores  
**Item:** DOC-DIA-ES · **Owner:** technical-writer · **Last-reviewed:** 2026-08-23

## Sinopsis

```text
configsh [--help] [--dump] [--check] [--section NAME]
configsh --section=NAME …
```

Binario separado (C++23, `-fno-exceptions -fno-rtti`). TUI raw ANSI (sin ncurses, sin Qt).

## Descripción

Lee y valida un INI bajo paths XDG (ver [xdg](xdg.md)). Sin archivo: carga defaults en memoria. Sin argumentos en TTY: abre TUI (`q` sale). Sin argumentos fuera de TTY: imprime help y exit 0.

## Opciones

| Flag | Default | Descripción |
|------|---------|-------------|
| `--help`, `-h` | | Ayuda; exit 0 |
| `--dump` | | Imprime INI (o sección) en stdout |
| `--check` | | Valida; exit 0 si ok |
| `--section NAME` | todas | Limita dump/check/TUI a la sección |

## Secciones conocidas

`prompt`, `aliases`, `env`, `history`, `general`

Defaults si falta el archivo:

```ini
[prompt]
ps1=petrush> 

[history]
max=1000
```

## Path de config (orden)

1. `$PETRUSH_CONFIG` (si está definido y no vacío)
2. `$XDG_CONFIG_HOME/petrush/config.ini`
3. `$HOME/.config/petrush/config.ini`
4. Fallback: `./.config/petrush/config.ini` (solo si falta `HOME`)

## Códigos de salida

| Code | Significado |
|------|-------------|
| 0 | Éxito / help (incl. no-TTY sin args) |
| 1 | Path inválido, load malformado, sección desconocida/ausente, check falló |
| 2 | Opción desconocida o `--section` sin NAME |

## Límites del modelo

| Constante | Valor |
|-----------|-------|
| path | 512 |
| nombre de sección | 32 |
| clave | 64 |
| valor | 256 |
| entradas / sección | 64 |
| secciones | 8 |
| línea | 512 |

## Notas

- No comparte ABI de `plugins/abi.h`.
- Puede usar `petrush_tty_mode` / `petrush_utf8_width` vía `extern "C"`.
- Smoke: `tests/smoke/cxx-tui.sh`.

## Ver también

- How-to: [Usar configsh](../how-to/usar-configsh.md)
- [Paths XDG](xdg.md)
