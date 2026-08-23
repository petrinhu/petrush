# `configsh`

**Diátaxis type:** reference  
**Audience:** users and contributors  
**Item:** DOC-DIA-EN · **Owner:** technical-writer · **Last-reviewed:** 2026-08-23

## Synopsis

```text
configsh [--help] [--dump] [--check] [--section NAME]
configsh --section=NAME …
```

Separate binary (C++23, `-fno-exceptions -fno-rtti`). Raw ANSI TUI (no ncurses, no Qt).

## Description

Reads and validates an INI under XDG paths (see [xdg](xdg.md)). Without a file: loads in-memory defaults. No arguments on a TTY: opens the TUI (`q` quits). No arguments off a TTY: prints help and exit 0.

## Options

| Flag | Default | Description |
|------|---------|-------------|
| `--help`, `-h` | | Help; exit 0 |
| `--dump` | | Print INI (or section) to stdout |
| `--check` | | Validate; exit 0 if ok |
| `--section NAME` | all | Limit dump/check/TUI to the section |

## Known sections

`prompt`, `aliases`, `env`, `history`, `general`

Defaults if the file is missing:

```ini
[prompt]
ps1=petrush> 

[history]
max=1000
```

## Config path (order)

1. `$PETRUSH_CONFIG` (if set and non-empty)
2. `$XDG_CONFIG_HOME/petrush/config.ini`
3. `$HOME/.config/petrush/config.ini`
4. Fallback: `./.config/petrush/config.ini` (only if `HOME` is missing)

## Exit codes

| Code | Meaning |
|------|---------|
| 0 | Success / help (incl. no-TTY with no args) |
| 1 | Invalid path, malformed load, unknown/missing section, check failed |
| 2 | Unknown option or `--section` without NAME |

## Model limits

| Constant | Value |
|----------|-------|
| path | 512 |
| section name | 32 |
| key | 64 |
| value | 256 |
| entries / section | 64 |
| sections | 8 |
| line | 512 |

## Notes

- Does not share the `plugins/abi.h` ABI.
- May use `petrush_tty_mode` / `petrush_utf8_width` via `extern "C"`.
- Smoke: `tests/smoke/cxx-tui.sh`.

## See also

- How-to: [Use configsh](../how-to/use-configsh.md)
- [XDG paths](xdg.md)
