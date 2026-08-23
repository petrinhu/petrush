# Cómo usar el `configsh`

**Tipo Diátaxis:** how-to  
**Audiencia:** usuario intermedio  
**Item:** DOC-DIA-ES · **Owner:** technical-writer · **Last-reviewed:** 2026-08-23

## Cuándo usar

Cuando quieras ver, validar o editar (TUI) el archivo INI de configuración del petrush bajo XDG. El `configsh` es un **binario separado** (C++23). El REPL `petrush` no enlaza `libstdc++`.

## Prerrequisitos

- Binario `configsh` compilado (target CMake `configsh`)
- Variables de entorno opcionales: `PETRUSH_CONFIG`, `XDG_CONFIG_HOME`, `HOME`

## Pasos

1. Descubre el path resuelto (orden XDG):

```bash
# fuerza un archivo de prueba
export PETRUSH_CONFIG=/var/tmp/petrush-demo.ini
```

O deja el default: `$XDG_CONFIG_HOME/petrush/config.ini`, si no `~/.config/petrush/config.ini`.

2. Crea un INI mínimo (opcional; sin archivo el dump usa defaults):

```bash
mkdir -p "$(dirname "$PETRUSH_CONFIG")"
cat > "$PETRUSH_CONFIG" <<'EOF'
[prompt]
ps1=demo> 

[history]
max=500
EOF
```

3. Dump:

```bash
configsh --dump
configsh --section prompt --dump
```

4. Validar:

```bash
configsh --check
echo "exit=$?"
```

5. TUI raw (solo en TTY real; `q` sale):

```bash
configsh
# o
configsh --section history
```

Sin argumentos y **sin** TTY, el `configsh` imprime help y sale 0 (comportamiento del smoke CXX-00).

## Verificación

- `--dump` muestra secciones `[prompt]`, `[history]`, etc.
- `--check` exit 0 si el INI (o defaults) está ok
- Sección desconocida en `--section` → exit 1
- Opción desconocida → exit 2 + help

## Variaciones

- Secciones conocidas: `prompt`, `aliases`, `env`, `history`, `general`
- Defaults si falta el archivo: `prompt.ps1=petrush> `, `history.max=1000`

## Relacionados

- Reference: [`configsh`](../reference/configsh.md)
- How-to XDG: [Localizar paths](localizar-paths-xdg.md)
- Explanation: [Layout XDG](../explanation/layout-xdg-petrush.md)
