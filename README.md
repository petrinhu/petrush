# petrush

**Shell interativo leve em C23** — "petrush" (PetRush) para fluxo pessoal Linux + IA local.

> **Estado atual (v0.3.0)**: UX wave — aliases, `PETRUSH_PS1`, Tab-complete, history hints, `&&`/`||`, which.
> Também: pipes `|`, redirs, `pudo`, history, rc, sinais.
> Gate: `cmake --build build --target check` + smoke · Wiki: https://github.com/petrinhu/petrush/wiki

## Propósito

Criar um **shell interativo leve em C23** (REPL), de alta qualidade, zero dependências de runtime, com:

- Execução de comandos externos via PATH
- Builtins essenciais (cd, pwd, echo, export, history, help, exit...)
- History persistente, ~/.petrushrc, tratamento correto de sinais
- Futuro: builtins diagnósticos e de automação inspirados no ecossistema do autor (my_comp, suporte-linux, IA local)

É um shell real, não uma CLI de subcomandos nem uma ferramenta de diagnóstico exclusiva.

## Status atual (v0.2.0)

- [x] Build C23 + CMake + hardening + lint
- [x] Estrutura pragmática 4 camadas + TDD parcial
- [x] Smoke + Sanitize + valgrind automatizados
- [x] CI (GitHub Actions) + wiki
- [x] Gate S0 via `verify`
- [x] Pipes `|` e redirecionamento `>` `>>` `<` (NEW-20 mínimo)
- [x] Tags `v0.1.0` / `v0.2.0` + CHANGELOG

Ver CHANGELOG.md e a [wiki](https://github.com/petrinhu/petrush/wiki).

## Build & Uso

```bash
# Release (recomendado)
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

# Gate completo automatizado (lint + smoke + valgrind + tudo)
cmake --build build --target verify

# Executar
./build/petrush
```

Exemplos:
```bash
alias ll='ls -la'
which pwd
export PETRUSH_PS1='🚀 > '   # reinicie o shell para ver (lido a cada prompt)
printf 'abc\n' | cat
/bin/true && echo ok
/bin/false || echo recovered
echo out > /tmp/x.txt
# Tab = complete; digite prefixo de history para ghost-text (hints)
```

Build testado no CI em **container Fedora 44** (clang/gcc × Release/Debug + smoke). Job experimental Fedora 45 não bloqueia `main`.

Ver `TODO.md`, `CLAUDE.md`, `docs/beginner-guide.md` (iniciantes), `docs/security/pudod-install.md` (pudo),
`docs/standards.md` (mapas para CONTRACT / AGILE / TESTES / AUDITORIAS / DEPLOY_CHECKLIST do vault).

## AGPL-3.0

Este projeto é distribuído sob a **GNU Affero General Public License v3.0 (AGPL-3.0)**.
Uso comercial é permitido, desde que o código-fonte de versões modificadas seja disponibilizado.

→ [Leia o texto completo da licença](LICENSE.md)

## Contribuição

Projeto pessoal. Issues e PRs só após conversa.

---

**Repositório oficial**: https://github.com/petrinhu/petrush
