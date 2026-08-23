# Como usar o `configsh`

**Tipo Diátaxis:** how-to  
**Audiência:** utilizador intermédio  
**Item:** DOC-DIA-PT · **Owner:** technical-writer · **Last-reviewed:** 2026-08-23

## Quando usar

Quando quiser ver, validar ou editar (TUI) o ficheiro INI de configuração do petrush sob XDG. O `configsh` é um **binário separado** (C++23). O REPL `petrush` não liga `libstdc++`.

## Pré-requisitos

- Binário `configsh` compilado (alvo CMake `configsh`)
- Variáveis de ambiente opcionais: `PETRUSH_CONFIG`, `XDG_CONFIG_HOME`, `HOME`

## Passos

1. Descubra o path resolvido (ordem XDG):

```bash
# força um ficheiro de teste
export PETRUSH_CONFIG=/var/tmp/petrush-demo.ini
```

Ou deixe o default: `$XDG_CONFIG_HOME/petrush/config.ini`, senão `~/.config/petrush/config.ini`.

2. Crie um INI mínimo (opcional; sem ficheiro o dump usa defaults):

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

5. TUI raw (só em TTY real; `q` sai):

```bash
configsh
# ou
configsh --section history
```

Sem argumentos e **sem** TTY, o `configsh` imprime help e sai 0 (comportamento do smoke CXX-00).

## Verificação

- `--dump` mostra secções `[prompt]`, `[history]`, etc.
- `--check` exit 0 se o INI (ou defaults) estiver ok
- Secção desconhecida em `--section` → exit 1
- Opção desconhecida → exit 2 + help

## Variações

- Secções conhecidas: `prompt`, `aliases`, `env`, `history`, `general`
- Defaults se o ficheiro faltar: `prompt.ps1=petrush> `, `history.max=1000`

## Relacionados

- Reference: [`configsh`](../reference/configsh.md)
- How-to XDG: [Localizar paths](localizar-paths-xdg.md)
- Explanation: [Layout XDG](../explanation/layout-xdg-petrush.md)
