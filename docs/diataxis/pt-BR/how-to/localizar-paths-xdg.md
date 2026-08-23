# Como localizar paths XDG do petrush

**Tipo Diátaxis:** how-to  
**Audiência:** utilizador intermédio / ops local  
**Item:** DOC-DIA-PT · **Owner:** technical-writer · **Last-reviewed:** 2026-08-23

## Quando usar

Quando precisar achar o INI do `configsh`, a allow-list de plugins ou a pasta de `.so`, sem adivinhar caminhos.

## Pré-requisitos

- Shell POSIX qualquer para `echo`/`printf`
- Conhecer o valor de `HOME` (e opcionalmente `XDG_*`)

## Passos

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

3. Pasta de plugins (dados):

```bash
if [ -n "${PETRUSH_PLUGIN_PATH:-}" ]; then
  printf 'plugin path (env, só absolutos): %s\n' "$PETRUSH_PLUGIN_PATH"
fi
if [ -n "${XDG_DATA_HOME:-}" ]; then
  printf 'plugins: %s/petrush/plugins\n' "$XDG_DATA_HOME"
else
  printf 'plugins: %s/.local/share/petrush/plugins\n' "$HOME"
fi
```

4. Confirme com as ferramentas:

```bash
configsh --dump >/dev/null && echo 'configsh resolveu o path'
```

## Verificação

- Paths de config usam **config home** (`XDG_CONFIG_HOME` ou `~/.config`).
- Plugins usam **data home** (`XDG_DATA_HOME` ou `~/.local/share`) mais `PETRUSH_PLUGIN_PATH`.
- Elementos **relativos** em `PETRUSH_PLUGIN_PATH` são ignorados pelo loader.

## Relacionados

- Reference: [Paths XDG](../reference/xdg.md)
- Explanation: [Layout XDG](../explanation/layout-xdg-petrush.md)
- Threat model: [`docs/security/plugins-threat.md`](../../../security/plugins-threat.md)
