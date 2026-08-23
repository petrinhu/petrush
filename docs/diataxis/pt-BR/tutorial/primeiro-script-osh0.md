# Tutorial: seu primeiro script com petrush (OSH-0)

**Tipo Diátaxis:** tutorial  
**Audiência:** novato que já compilou o binário `petrush`  
**Tempo estimado:** 10 min  
**Pré-requisitos:** binário `petrush` no `PATH` ou caminho absoluto conhecido; Linux  
**O que você terá ao fim:** um ficheiro executável com shebang que imprime uma linha e sai com status 0  
**Item:** DOC-DIA-PT · **Owner:** technical-writer · **Last-reviewed:** 2026-08-23

## 1. Confirmar o binário

```bash
which petrush || ls -l ./build/petrush
```

Se ainda não compilou, use o build local (exemplo):

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --target petrush
export PATH="$(pwd)/build:$PATH"
```

Output esperado: o executável `petrush` existe e é executável.

## 2. Criar o script

Crie `ola.sh` (o nome é livre):

```bash
cat > /var/tmp/ola.sh <<'EOF'
#!/usr/bin/env petrush
echo osh0-hello
EOF
chmod 0755 /var/tmp/ola.sh
```

O shebang (`#!/usr/bin/env petrush`) pede ao kernel para arrancar o `petrush` do `PATH` e passar o ficheiro como argumento. Isto é o modo **OSH-0**: script, sem banner, sem `~/.petrushrc`.

## 3. Rodar pelo shebang

```bash
/var/tmp/ola.sh
echo "exit=$?"
```

Output esperado:

```text
osh0-hello
exit=0
```

Não deve aparecer o banner `C23 shell` nem o prompt `petrush>`.

## 4. Rodar passando o ficheiro ao binário

Equivalente sem depender do bit executável:

```bash
petrush /var/tmp/ola.sh
echo "exit=$?"
```

Mesmo output. Internamente o `main` chama `petrush_run_script` quando há `argv[1]`.

## 5. Ver o status do último comando

```bash
cat > /var/tmp/ultimo.sh <<'EOF'
#!/usr/bin/env petrush
/bin/false
EOF
chmod 0755 /var/tmp/ultimo.sh
/var/tmp/ultimo.sh
echo "exit=$?"
```

Output esperado: `exit=1` (status de `/bin/false`).

## 6. (Opcional) Olhar o hardware e a config

Builtins também correm em script OSH-0:

```bash
cat > /var/tmp/hw.sh <<'EOF'
#!/usr/bin/env petrush
wai -cpu
EOF
chmod 0755 /var/tmp/hw.sh
/var/tmp/hw.sh | head
```

A config XDG fica no binário separado `configsh` (não no REPL):

```bash
configsh --dump | head
```

Se `config.ini` não existir, o `configsh` usa defaults em memória (`ps1=petrush> `, `history.max=1000`).

## Verificar

- [ ] Script com shebang imprime e sai 0
- [ ] Não há banner REPL no modo script
- [ ] `/bin/false` no script propaga exit 1
- [ ] (Opcional) `wai` e `configsh --dump` respondem

## Resumo

Você construiu: um script OSH-0 com shebang.  
Você aprendeu:

- `petrush arquivo` entra em modo script (sem rc, sem banner)
- o exit do processo é o do último comando (ou de `exit N`)
- OSH-0 **ainda não** expõe `$1` nem `if` (isso é onda posterior)
- `wai` e `configsh` são superfícies à parte (builtin e binário)

## Próximos passos

- How-to: [Inventariar com `wai`](../how-to/inventariar-com-wai.md)
- How-to: [Usar o `configsh`](../how-to/usar-configsh.md)
- Reference: [OSH-0 / shebang](../reference/osh0-shebang.md)
- Explanation: [OSH-0 não é POSIX completo](../explanation/osh0-nao-e-posix-completo.md)
