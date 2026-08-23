# Cómo localizar paths XDG del petrush

**Tipo Diátaxis:** how-to  
**Audiencia:** usuario intermedio / ops local  
**Item:** DOC-DIA-ES · **Owner:** technical-writer · **Last-reviewed:** 2026-08-23

## Cuándo usar

Cuando necesites hallar el INI del `configsh`, la allow-list de plugins o la carpeta de `.so`, sin adivinar rutas.

## Prerrequisitos

- Shell POSIX cualquiera para `echo`/`printf`
- Conocer el valor de `HOME` (y opcionalmente `XDG_*`)

## Pasos

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

2. Allow-list de plugins:

```bash
if [ -n "${PETRUSH_PLUGIN_ALLOW:-}" ]; then
  printf 'allow: %s\n' "$PETRUSH_PLUGIN_ALLOW"
elif [ -n "${XDG_CONFIG_HOME:-}" ]; then
  printf 'allow: %s/petrush/plugins.allow\n' "$XDG_CONFIG_HOME"
else
  printf 'allow: %s/.config/petrush/plugins.allow\n' "$HOME"
fi
```

3. Carpeta de plugins (datos):

```bash
if [ -n "${PETRUSH_PLUGIN_PATH:-}" ]; then
  printf 'plugin path (env, solo absolutos): %s\n' "$PETRUSH_PLUGIN_PATH"
fi
if [ -n "${XDG_DATA_HOME:-}" ]; then
  printf 'plugins: %s/petrush/plugins\n' "$XDG_DATA_HOME"
else
  printf 'plugins: %s/.local/share/petrush/plugins\n' "$HOME"
fi
```

4. Confirma con las herramientas:

```bash
configsh --dump >/dev/null && echo 'configsh resolvió el path'
```

## Verificación

- Paths de config usan **config home** (`XDG_CONFIG_HOME` o `~/.config`).
- Plugins usan **data home** (`XDG_DATA_HOME` o `~/.local/share`) más `PETRUSH_PLUGIN_PATH`.
- Elementos **relativos** en `PETRUSH_PLUGIN_PATH` son ignorados por el loader.

## Relacionados

- Reference: [Paths XDG](../reference/xdg.md)
- Explanation: [Layout XDG](../explanation/layout-xdg-petrush.md)
- Threat model: [`docs/security/plugins-threat.md`](../../../security/plugins-threat.md)
