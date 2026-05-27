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

## Licença

Este projeto é distribuído sob a **PolyForm Noncommercial License 1.0.0**.

### O que é considerado "Noncommercial"?

De acordo com a licença, uso **não-comercial** significa qualquer propósito que **não** seja primariamente voltado para:

- Vantagem comercial ou compensação monetária;
- Venda de produtos ou serviços que contenham o código;
- Oferecimento gratuito de produtos/serviços cujo objetivo principal seja gerar receita, tráfego pago, dados para monetização ou qualquer outro benefício comercial.

**Exemplos de uso permitido**:
- Uso pessoal
- Pesquisa e estudo
- Projetos open source sem modelo de monetização
- Ensino e workshops sem fins lucrativos

**Exemplos de uso não permitido**:
- Incorporar o código em um produto ou serviço pago
- Usar o código em uma ferramenta gratuita que tenha como objetivo principal gerar receita (ex: freemium, ads, dados para venda, etc.)
- Revender ou oferecer o código como parte de uma solução comercial

→ [Leia o texto completo da licença](LICENSE.md)  
→ [Versão oficial](https://polyformproject.org/licenses/noncommercial/1.0.0/)

## Contribuição

Projeto pessoal. Issues e PRs só após conversa.

---

**Repositório oficial**: https://codeberg.org/petrinhu/petrush
