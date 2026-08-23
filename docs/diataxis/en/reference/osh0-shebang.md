# OSH-0: script mode and shebang

**Diátaxis type:** reference  
**Audience:** users and contributors  
**Item:** DOC-DIA-EN · **Owner:** technical-writer · **Last-reviewed:** 2026-08-23

## Synopsis

```text
petrush <file>
#!/usr/bin/env petrush
```

## Description

When `argc >= 2` and `argv[1]` is non-empty, `main` does **not** enter the REPL: it calls `petrush_run_script(argv[1])` and exits. That covers shebang (`env` passes the script path as `argv[1]`).

OSH-0 is **not** the full OSH dialect and is **not** a claim of 100% POSIX conformance. It is the minimum slice: regular file, line by line, exit = last status.

## Guaranteed behavior in this slice

| Behavior | Detail |
|----------|--------|
| No banner | Does not print `C23 shell` / prompt |
| No `~/.petrushrc` | REPL rc does not run |
| Exit = last command | e.g. `/bin/false` → exit 1 |
| `exit N` in the script | Ends with N; later lines do not run |
| Missing file | Exit 127 |
| Not regular (e.g. directory) | Refuses; exit ≠ 0 and ≠ 127 |
| Comments / blank lines | Ignored (`#` …) |
| `argv[2+]` | Ignored (no `$1` in this slice) |
| Group-writable mode | Accepted in script mode (no SEC-10 `mode&0022` rule from `source`) |

## Recommended shebang

```text
#!/usr/bin/env petrush
```

Requires `petrush` on the `PATH` of the process that runs the file. Alternative: absolute path in the shebang, if installed (system install is **not** a goal on this machine).

## What OSH-0 still does **not** do

Honestly incomplete relative to IEEE 1003.1-2017 XCU ch. 2:

- positional parameters (`$1` … `$n`, `$@`)
- `if` / `while` / `for` / `case` / functions
- `$( )`, `$(( ))`, here-doc
- strict `--posix` mode
- loading rc in script mode

Those items sit on the OSH-1+ track of the plan; do not invent them in documentation as if they already existed.

## Smoke

`tests/smoke/osh0-script.sh` (Docker Fedora 44 / clang in the TODO DoD).

## See also

- Tutorial: [First script](../tutorial/first-osh0-script.md)
- Explanation: [OSH-0 is not full POSIX](../explanation/osh0-is-not-full-posix.md)
- Plan: [`docs/plano-shell-avancado.md`](../../../plano-shell-avancado.md)
