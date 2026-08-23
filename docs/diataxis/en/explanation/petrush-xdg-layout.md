# Petrush XDG layout

**Diátaxis type:** explanation  
**Audience:** contributors and people configuring the environment  
**Item:** DOC-DIA-EN · **Owner:** technical-writer · **Last-reviewed:** 2026-08-23

## Context

Petrush separates **editable configuration** (INI, allow-list) from **data artifacts** (plugin `.so` files). That mirrors the XDG idea of `CONFIG` versus `DATA`, without claiming to implement the whole specification (cache/state dirs from the XDG-1 track are still in the plan).

## Mental model

| File type | Home | Examples |
|-----------|------|----------|
| Preferences / policy | config home | `config.ini`, `plugins.allow` |
| Loadable binaries | data home (+ env) | `…/petrush/plugins/*.so` |
| One-off override | project env | `PETRUSH_CONFIG`, `PETRUSH_PLUGIN_PATH`, `PETRUSH_PLUGIN_ALLOW` |

`configsh` only talks to the INI. The plugin loader (`plugin_load.c`) talks to data dirs + allow-list. The interactive REPL still uses `~/.petrushrc` on classic startup; the **OSH-0 script** does not load that rc. Unifying rc under XDG is a future slice (XDG-1 in the plan), not current state.

## Trade-offs

- **Pros:** users know where to look; CI injects `XDG_*` under `/var/tmp` without touching real `$HOME`; the plugin threat model closes on predictable paths.
- **Accepted cons:** two trees (`config` and `data`); people who only know `~/.petrushrc` need this note; the `configsh` `./.config/...` fallback is a rare last resort.

## When to apply / not apply

- Apply `XDG_CONFIG_HOME` / `PETRUSH_CONFIG` overrides in tests and sandboxes.
- Do not place world-writable `.so` files "just to try": the loader refuses.
- Do not document `~/.cache/petrush` as official until code exists.

## References

- Reference: [XDG paths](../reference/xdg.md)
- How-to: [Locate paths](../how-to/locate-xdg-paths.md)
- Plan (future XDG-1): [`docs/plano-shell-avancado.md`](../../../plano-shell-avancado.md)
