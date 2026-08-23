# Diátaxis documentation (en)

> **Type:** navigation hub (not one of the four Diátaxis types).
> **Audience:** petrush users (novice to intermediate) and contributors.
> **Canonical language of this tree:** English (msgid).
> **Item:** DOC-DIA-EN · **Owner:** technical-writer · **Last-reviewed:** 2026-08-23
> **Product version:** post OSH-0 / CXX-TUI / ASM-WAI / ASM-NET (no release tag in this slice).

English Diátaxis map for petrush. Each page has **one type**, **one audience**, and covers only what the code does today. Sibling trees: [pt-BR](../pt-BR/README.md) (DOC-DIA-PT) and DOC-DIA-ES (next wave).

## Scope notice

- OSH-0 is **not** full POSIX. It is script mode + shebang + exit of the last command. Positionals (`$1`), `if`/`while`, and the rest of the OSH track come later.
- No production on this machine: tests in Docker; no `4755`; `netcom -up`/`-down` needs `CAP_NET_ADMIN` (otherwise a clear EPERM).
- Sources of truth: code under `src/`, contract in [`docs/architecture.md`](../../architecture.md) and ADR-001.

## Tutorial (learn by doing)

| Page | What you build |
|------|----------------|
| [First OSH-0 script](tutorial/first-osh0-script.md) | Executable script with shebang, no REPL |

## How-to (solve one goal)

| Page | Goal |
|------|------|
| [Inventory hardware with `wai`](how-to/inventory-with-wai.md) | List disk, CPU, memory, and more without root |
| [Inspect the network with `netcom`](how-to/inspect-network-with-netcom.md) | Scan wifi/eth/bt; understand EPERM on `-up` |
| [Use `configsh`](how-to/use-configsh.md) | Dump, check, and TUI for the INI config |
| [Locate XDG paths](how-to/locate-xdg-paths.md) | Find `config.ini`, plugins, and allow-list |

## Reference (look up)

| Page | Surface |
|------|---------|
| [`wai`](reference/wai.md) | Builtin for sysfs/proc inventory |
| [`netcom`](reference/netcom.md) | Builtin for scan / link up-down |
| [`configsh`](reference/configsh.md) | C++23 configuration binary |
| [OSH-0 / shebang](reference/osh0-shebang.md) | `petrush file` mode |
| [XDG paths](reference/xdg.md) | Variables and files under XDG |

## Explanation (understand)

| Page | Question |
|------|----------|
| [OSH-0 is not full POSIX](explanation/osh0-is-not-full-posix.md) | Why the track starts with shebang |
| [Why `wai` and `netcom` read sysfs](explanation/why-sysfs-wai-netcom.md) | Mental model and privilege limits |
| [Petrush XDG layout](explanation/petrush-xdg-layout.md) | Why config and plugins live in different places |

## Related (outside this tree)

- Beginner guide (quick mix): [`docs/beginner-guide.md`](../../beginner-guide.md)
- Architecture / triple stack: [`docs/architecture.md`](../../architecture.md)
- Plugin threat model: [`docs/security/plugins-threat.md`](../../security/plugins-threat.md)
- OSH plan: [`docs/plano-shell-avancado.md`](../../plano-shell-avancado.md)
