# How to use `configsh`

**Diátaxis type:** how-to  
**Audience:** intermediate user  
**Item:** DOC-DIA-EN · **Owner:** technical-writer · **Last-reviewed:** 2026-08-23

## When to use

When you want to view, validate, or edit (TUI) the petrush INI configuration under XDG. `configsh` is a **separate binary** (C++23). The `petrush` REPL does not link `libstdc++`.

## Prerequisites

- Built `configsh` binary (CMake target `configsh`)
- Optional environment variables: `PETRUSH_CONFIG`, `XDG_CONFIG_HOME`, `HOME`

## Steps

1. Discover the resolved path (XDG order):

```bash
# force a test file
export PETRUSH_CONFIG=/var/tmp/petrush-demo.ini
```

Or leave the default: `$XDG_CONFIG_HOME/petrush/config.ini`, else `~/.config/petrush/config.ini`.

2. Create a minimal INI (optional; without a file, dump uses defaults):

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

4. Validate:

```bash
configsh --check
echo "exit=$?"
```

5. Raw TUI (real TTY only; `q` quits):

```bash
configsh
# or
configsh --section history
```

With no arguments and **no** TTY, `configsh` prints help and exits 0 (CXX-00 smoke behavior).

## Verification

- `--dump` shows sections `[prompt]`, `[history]`, and so on
- `--check` exit 0 if the INI (or defaults) is ok
- Unknown section in `--section` → exit 1
- Unknown option → exit 2 + help

## Variations

- Known sections: `prompt`, `aliases`, `env`, `history`, `general`
- Defaults if the file is missing: `prompt.ps1=petrush> `, `history.max=1000`

## Related

- Reference: [`configsh`](../reference/configsh.md)
- XDG how-to: [Locate paths](locate-xdg-paths.md)
- Explanation: [XDG layout](../explanation/petrush-xdg-layout.md)
