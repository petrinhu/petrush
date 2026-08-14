# Auditorias do Projeto

> Auditorias aplicáveis a este projeto (stack: **C23 / binário CLI**).
> Cada uma vira um item `AUD-*` na tabela de pendências, nas ondas finais
> (downstream de código + teste).
>
> Este arquivo **inclui** (não substitui) o manual canônico do vault
> [`AUDITORIAS.md`](/home/petrus/IDrive/Documentos/projetos_claudebrain/AUDITORIAS.md).
> Poda: sem AUD-DB (SQL), AUD-API (HTTP), AUD-UI (widgets/HTML), AUD-FRAMEWORK (Qt/web).

## AUD-DISC Descoberta e Modelagem

Mapear superfície, ativos e modelo de ameaça (REPL, parser, process, `pudo`/`pudod`, history, rc).

**Ferramentas:** OWASP Threat Dragon, diagramas C4/DFD, revisão manual de superfície e ativos.

## AUD-ARCH Arquitetura e Camadas

Conferir 4 camadas (Front/Mid/Back/Foundation), SOLID, DRY, sem violação de dependência.

**Ferramentas:** revisão de `docs/architecture.md` + includes; clang-tidy; checagem de fronteira SDL-equivalente (aqui: só `main`/platform chama libc de processo).

## AUD-SEC Segurança

Memory safety, secrets, binário, LGPD/privacidade, helper setuid (`pudod`).

**Ferramentas:** SAST (semgrep, CodeQL, cppcheck), gitleaks/trufflehog, trivy/grype, revisão de `docs/security/`.

## AUD-QUALITY Qualidade de Código

God functions, complexidade, dead code, duplicação (Rule of 3 no parser/cmd).

**Ferramentas:** clang-tidy, lizard, revisão manual.

## AUD-COV Cobertura de Testes

Cobertura significativa nos módulos críticos (parser, expand, process, dispatcher, pudo, prompt).

**Ferramentas:** lcov/gcov ou llvm-cov.

## AUD-DEPS Dependências e Acoplamento

Grafo de deps, acoplamento, ciclos (vendor linenoise + camadas internas).

**Ferramentas:** revisão do grafo de includes; license-checker do vendor.

## AUD-LANG Idiomas Modernos da Linguagem

Idioms C23 (tipos, `_BitInt` se couber, atributos, sem VLA perigoso, headers próprios).

**Ferramentas:** clang-tidy (`modernize`, equivalentes C), revisão de idioms.

## AUD-REPORT Relatório Final de Auditoria

Score 0-100, sumário de problemas, patches. Consolida as auditorias anteriores.

**Ferramentas:** consolidação manual + relatório markdown.
