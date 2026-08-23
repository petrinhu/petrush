# How to locate petrush XDG paths

**Diátaxis type:** how-to  
**Audience:** intermediate user / local ops  
**Item:** DOC-DIA-EN · **Owner:** technical-writer · **Last-reviewed:** 2026-08-23

## When to use

When you need to find the `configsh` INI, the plugin allow-list, or the `.so` folder, without guessing paths.

## Prerequisites

- Any POSIX shell for `echo`/`printf`
- Knowing the value of `HOME` (and optionally `XDG_*`)

## Steps

1. Config INI (`configsh`):

```bash
if [ -n "${PETRUSH_CONFIG:-}" ]; then
  printf 'config: %s\n' "$PETRUSH_CONFIG"
elif [ -n "${XDG_CONFIG_HOME:-}" ]; then
  printf 'config: %s/petrush/config.ini\n' "$XDG_CONFIG_HOME"
else
  printf 'config: %s/.config/petrush/config.ini\n' "$HOME"
fi
```

2. Plugin allow-list:

```bash
if [ -n "${PETRUSH_PLUGIN_ALLOW:-}" ]; then
  printf 'allow: %s\n' "$PETRUSH_PLUGIN_ALLOW"
elif [ -n "${XDG_CONFIG_HOME:-}" ]; then
  printf 'allow: %s/petrush/plugins.allow\n' "$XDG_CONFIG_HOME"
else
  printf 'allow: %s/.config/petrush/plugins.allow\n' "$HOME"
fi
```

3. Plugin directory (data):

```bash
if [ -n "${PETRUSH_PLUGIN_PATH:-}" ]; then
  printf 'plugin path (env, absolute only): %s\n' "$PETRUSH_PLUGIN_PATH"
fi
if [ -n "${XDG_DATA_HOME:-}" ]; then
  printf 'plugins: %s/petrush/plugins\n' "$XDG_DATA_HOME"
else
  printf 'plugins: %s/.local/share/petrush/plugins\n' "$HOME"
fi
```

4. Confirm with the tools:

```bash
configsh --dump >/dev/null && echo 'configsh resolved the path'
```

## Verification

- Config paths use **config home** (`XDG_CONFIG_HOME` or `~/.config`).
- Plugins use **data home** (`XDG_DATA_HOME` or `~/.local/share`) plus `PETRUSH_PLUGIN_PATH`.
- **Relative** entries in `PETRUSH_PLUGIN_PATH` are ignored by the loader.

## Related

- Reference: [XDG paths](../reference/xdg.md)
- Explanation: [XDG layout](../explanation/petrush-xdg-layout.md)
- Threat model: [`docs/security/plugins-threat.md`](../../../security/plugins-threat.md)
