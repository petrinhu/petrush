# petrush

**Ferramenta de diagnóstico e automação em C** — "PetRush" custom para o fluxo pessoal Linux + IA local.

> **Estado atual (2026-05-27)**: Projeto recém-iniciado. Escolha de arquitetura: **Opção A (Shell Interativo REPL clássico)** confirmada.  
> Agora é um shell interativo em C23 (prompt, loop, execução de comandos externos + builtins). Builtins diagnósticos/AI podem ser adicionados em iterações futuras.

## Propósito

Criar um **shell interativo leve em C23** (REPL), de alta qualidade, zero dependências de runtime, com:

- Execução de comandos externos via PATH
- Builtins essenciais (cd, pwd, echo, export, history, help, exit...)
- History persistente, ~/.petrushrc, tratamento correto de sinais
- Futuro: builtins diagnósticos e de automação inspirados no ecossistema do autor (my_comp, suporte-linux, IA local)

É um shell real, não uma CLI de subcomandos nem uma ferramenta de diagnóstico exclusiva.

## Status do MVP

- [ ] Comandos assinatura definidos (PR-01)
- [ ] Build C23 + CMake + hardening + lint (PR-02)
- [ ] Estrutura 4 camadas + primeiros testes TDD (PR-03/06)
- [ ] Smoke + Sanitize limpo (PR-10)
- [ ] CI básico Forgejo Actions (PR-05)

## Build (quando existir)

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
./build/petrush --help
```

Ver `TODO.md` para pendências atuais, `CLAUDE.md` para regras do projeto e `LICENSE.md` para os termos de uso.

## AGPL-3.0

Este projeto é distribuído sob a **GNU Affero General Public License v3.0 (AGPL-3.0)**.
Uso comercial é permitido, desde que o código-fonte de versões modificadas seja disponibilizado.

→ [Leia o texto completo da licença](LICENSE.md)

## Contribuição

Projeto pessoal. Issues e PRs só após conversa.

---

**Repositório oficial**: https://codeberg.org/petrinhu/petrush
