# pudod — Helper setuid mínimo para `pudo`

**ATENÇÃO DE SEGURANÇA**: Este é o único binário do projeto que pode rodar com privilégios elevados (root via setuid ou capabilities).

## Princípios (NEW-05)
- Tamanho mínimo para facilitar auditoria manual (< 300 LOC).
- Revalida **tudo** no lado privilegiado (allow-list, realpath, tipo de arquivo, etc.).
- Nunca confia em dados vindos do petrush além do argv[1..].
- Fail secure: qualquer dúvida → _exit(1) sem executar o comando.
- Logging com UID real do caller (`getuid()`).

## Protocolo
```
pudod <comando-absoluto> [arg ...]
```

Exemplo de argv que petrush passa:
`pudod /usr/bin/id -u`

## Build
O CMake constrói automaticamente `build/pudod` junto com `petrush`.

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --target pudod
```

## Instalação (SÓ APÓS AUDITORIA E APROVAÇÃO EXPLÍCITA)

O CMake já instala o binário (veja `cmake --install`).

Passos recomendados:

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --target pudod
cmake --install build --prefix /usr/local
```

Depois aplique privilégios **manualmente**:

```bash
sudo chown root:root /usr/local/libexec/petrush-pudod
sudo chmod 4755 /usr/local/libexec/petrush-pudod
```

Alternativa mais moderna (capabilities, reduz privilégios permanentes):

```bash
sudo setcap cap_setuid,cap_setgid+ep /usr/local/libexec/petrush-pudod
```

**REGRAS OBRIGATÓRIAS**:
- Só faça isso após ler `docs/security/pudo-audit.md` completamente.
- O bit setuid expande a superfície de ataque do sistema inteiro.
- Mantenha a allow-list mínima e revisada.
- Nunca rode `cmake --install` + chmod em scripts automatizados sem gate humano.

**Nunca instale com setuid sem revisão completa + aprovação do líder.**

## Allow-list
O pudod carrega de `/etc/petrush/pudo.allow` (deve ser root:root, sem write para outros/grupo).

Durante `cmake --install`, é instalado `pudo.allow.example`.

Copie e proteja:
```bash
sudo mkdir -p /etc/petrush
sudo cp /usr/local/etc/petrush/pudo.allow.example /etc/petrush/pudo.allow
sudo chown root:root /etc/petrush/pudo.allow
sudo chmod 644 /etc/petrush/pudo.allow
```

O arquivo é verificado rigorosamente no pudod (dono root + perms).

## Testes
- `make test_pudo` (testes do frontend + recusa do pudod sem root)
- `cmake --build build --target pudod-valgrind` (valgrind no helper)
- Testes completos com setuid exigem setup manual + revisão.

Use `pudod-valgrind` para validar ausência de leaks nos caminhos de negação.

## Ver também
- `docs/security/pudo-audit.md`
- `docs/design/pudo.md`
- `src/mid/pudo.c` (frontend no petrush)
