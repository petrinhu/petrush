# Testes do Projeto

> Tipos de teste aplicáveis a este projeto (stack: **C23 / binário CLI**).
> T1 unitário fica sob o hook de TDD (acutest em `tests/test_*.c`), não listado aqui.
> Cada tipo vira um item `TST-*` na tabela de pendências (`TODO.md`).
>
> Este arquivo **inclui** (não substitui) o manual canônico do vault
> [`TESTES.md`](/home/petrus/IDrive/Documentos/projetos_claudebrain/TESTES.md).
> Poda: sem T6/T9 (rede/API HTTP), T10 (DB SQL), T11 (protocolo de rede custom).

## TST-T2 Análise Estática

Detectar bugs, má prática e problemas de segurança sem executar o código.

**Ferramentas:** cppcheck + clang-tidy (targets CMake `lint` / `cppcheck` / `clang-tidy`).

## TST-T3 Fuzzing de Inputs

Exercitar parsing de input não-confiável (linha de comando, PS1, expansão, glob).

**Ferramentas:** libFuzzer / AFL++ sobre `parser` / `expand` / `prompt`.

## TST-T4 Análise Dinâmica de Memória

Detectar vazamentos, acessos inválidos e comportamento indefinido em runtime.

**Ferramentas:** ASan + UBSan (preset sanitize do CMake) + valgrind nos caminhos críticos (`pudod-valgrind`).

## TST-T5 Scanning de Dependências

Detectar dependências vulneráveis ou desatualizadas (vendor linenoise + toolchain).

**Ferramentas:** trivy / grype / OSV sobre a árvore e a imagem de CI.

## TST-T7 Scanning de Binário

Conferir flags de hardening do binário (`petrush`, `pudod`).

**Ferramentas:** checksec / hardening-check (PIE, RELRO, NX, Fortify).

## TST-T8 Verificação de Secrets

Detectar credencial ou segredo commitado no histórico.

**Ferramentas:** gitleaks / trufflehog.

## TST-T12 Busca de CVEs

Cruzar CVEs conhecidos nas dependências e no toolchain da matriz de CI.

**Ferramentas:** trivy, grype, OSV/NVD.

## TST-T14 Integração (fim-a-fim)

Sistema integrado contra fontes de verdade (builtins, externos, erros 126-127, sinais, `pudo`).

**Ferramentas:** `tests/smoke/pudo-smoke.sh` + target CMake `smoke`.

## TST-T15 Pré-CI

Rodar a suíte do CI no **container local** (mesma imagem do workflow) antes do push. GitHub Actions é o espelho remoto; o caminho pesado (sanitize, valgrind, `verify`) não depende do runner da nuvem.

**Ferramentas:** `podman`/`docker` com `registry.fedoraproject.org/fedora:44` + target CMake `verify` (build + lint + smoke + valgrind). Referência de matriz: `.github/workflows/ci.yml`.
