# petrush

**Shell interativo leve em C23** — "petrush" (PetRush) para fluxo pessoal Linux + IA local.

> **Estado atual (S0 - Onda 1 Gate)**: ✅ COMPLETA. 
> Shell REPL clássico com builtins, history, rc, sinais, execução externa + `pudo` (helper setuid seguro).
> Gate automatizado: `cmake --build build --target verify`
> Ver CHANGELOG.md para detalhes.

## Propósito

Criar um **shell interativo leve em C23** (REPL), de alta qualidade, zero dependências de runtime, com:

- Execução de comandos externos via PATH
- Builtins essenciais (cd, pwd, echo, export, history, help, exit...)
- History persistente, ~/.petrushrc, tratamento correto de sinais
- Futuro: builtins diagnósticos e de automação inspirados no ecossistema do autor (my_comp, suporte-linux, IA local)

É um shell real, não uma CLI de subcomandos nem uma ferramenta de diagnóstico exclusiva.

## Status atual (S0 - Onda 1 Gate)

- [x] Build C23 + CMake + hardening + lint
- [x] Estrutura pragmática 4 camadas + TDD parcial
- [x] Smoke + Sanitize + valgrind automatizados
- [x] CI básico (GitHub Actions)
- [x] Gate S0 via `verify` (build + lint + smoke + pudod-valgrind)
- [x] Tag S0 (v0.1.0) + CHANGELOG + status updates

Ver CHANGELOG.md para histórico completo.

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
pudo id
pudo --help
cd /tmp
echo "hello" | cat
history
```

Build testado no CI (clang/gcc, Release/Debug, smoke com `pudo`).

Ver `TODO.md`, `CLAUDE.md`, `docs/beginner-guide.md` (iniciantes), `docs/security/pudod-install.md` (pudo).

## AGPL-3.0

Este projeto é distribuído sob a **GNU Affero General Public License v3.0 (AGPL-3.0)**.
Uso comercial é permitido, desde que o código-fonte de versões modificadas seja disponibilizado.

→ [Leia o texto completo da licença](LICENSE.md)

## Contribuição

Projeto pessoal. Issues e PRs só após conversa.

---

**Repositório oficial**: https://github.com/petrinhu/petrush
