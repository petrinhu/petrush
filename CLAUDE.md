# CLAUDE.md — petrush (Shell CLI / Ferramenta Diagnóstica em C)

Preferências específicas deste projeto. Regras universais em `~/.claude/CLAUDE.md` continuam valendo 100%.

## Manuais canônicos do vault (inclusão obrigatória)

O petrush **inclui** (não substitui) os manuais da raiz do vault. Mapa + adaptação C23:
[`docs/standards.md`](docs/standards.md).

| Manual | Caminho (absoluto nesta máquina) |
|--------|----------------------------------|
| CONTRACT | `/home/petrus/IDrive/Documentos/projetos_claudebrain/CONTRACT.md` |
| AGILE | `/home/petrus/IDrive/Documentos/projetos_claudebrain/AGILE.md` |
| TESTES | `/home/petrus/IDrive/Documentos/projetos_claudebrain/TESTES.md` |
| AUDITORIAS | `/home/petrus/IDrive/Documentos/projetos_claudebrain/AUDITORIAS.md` |
| DEPLOY_CHECKLIST | `/home/petrus/IDrive/Documentos/projetos_claudebrain/DEPLOY_CHECKLIST.md` |

**Antes de escrever código:** CONTRACT (camadas, clean code, §11) + TESTES (T1/T2/T4/T15) + TDD em tudo (abaixo).  
**Antes de planejar entrega:** AGILE (histórias, DoD).  
**Antes de auditoria/release pesada:** AUDITORIAS + gates do CONTRACT §11.  
**Antes de setuid/`pudod` em sistema:** DEPLOY_CHECKLIST + `docs/security/`.

Hierarquia de autoridade: líder → CONTRACT → TESTES/AUDITORIAS → AGILE → DEPLOY_CHECKLIST → este CLAUDE.md → `docs/*`.

## Regra de Processo (cross-project, 2026-05-27)

**NUNCA tomar decisões de design ou arquitetura sem consultar o usuário antes.**

- Sempre apresentar **opções claras** (2–3 alternativas) com prós, contras, impacto em camadas, esforço e trade-offs.
- O usuário decide. Só então codificar ou estruturar.
- Esta regra foi registrada também via memo_persistente (feedback + apêndice universal).

## Natureza do petrush (decidido 2026-05-27)

- **Opção A escolhida (final)**: Shell Interativo REPL clássico.
- Prompt persistente (`petrush> ` ou customizável via PETRUSH_PS1), loop de leitura, execução de comandos externos do PATH + builtins.
- Características MVP: history persistente, ~/.petrushrc, tratamento de SIGINT, variáveis de ambiente, builtins essenciais (cd, pwd, echo, export, history, help, exit...).
- Não é ferramenta de diagnóstico custom pesada nem CLI de subcomandos pura. É um shell interativo leve em C23.
- Futuro: builtins diagnósticos/AI podem ser adicionados depois (sem inflar o MVP de shell básico).

## Stack confirmada
- C23 + CMake + hardening (ASan/UBSan, PIE, FORTIFY, relro etc.) + cppcheck/clang-tidy.
- 4 camadas (Front/Mid/Back/Foundation) mesmo em ferramenta pequena.
- TODO.md canônico no formato tab_pendencias.
- Repo: **somente GitHub** (`git@github.com:petrinhu/petrush.git`). Codeberg/Forgejo deprecado (política 2026-07-25).

## TDD em tudo (regra fixa — 2026-08-03)

**Sempre que for escrever ou alterar comportamento de código**, o ciclo é **red → green → refactor**. Não é “TDD só mid/back”: vale para **qualquer** camada (front/mid/back/foundation), parser, dispatcher, process, pudo, main/REPL (via teste + smoke), e bugfix.

### Ordem obrigatória

1. **Red** — escrever ou estender o teste **primeiro** (acutest em `tests/`, smoke em `tests/smoke/` quando for integração ponta-a-ponta). Rodar e **provar vermelho** (falha esperada). Sem evidência de vermelho, **não** há licença para implementar produção.
2. **Green** — mínimo de código de produção para o teste passar. Sem refatoração paralela, sem “já deixo genérico”.
3. **Refactor** — limpar com a suíte **verde**. Se quebrar, voltar ao green antes de seguir.

### O que conta como “escrever código”

- Feature nova, bugfix, hardening de comportamento, mudança de contrato de API interna.
- Ajuste de parser/dispatcher/process/builtins/pudo/pudod/env/REPL.

### Exceções (declarar no commit/mensagem, nunca default)

- Doc-only, formatação de comentário sem mudar comportamento, renomeação mecânica coberta por testes já existentes (ainda assim rodar a suíte).
- Exploração throwaway em `/var/tmp` **não** vira commit sem antes ter teste.

### Evidência na sessão

Ao relatar trabalho de código: dizer explicitamente **qual teste ficou vermelho**, **qual comando mostrou isso**, depois **o green**, depois refactor se houver. “Implementei e os testes passaram” sem red prévio = processo errado.

### Frameworks deste repo

- Unitário: **acutest** (`tests/test_*.c`).
- Integração: **smoke** (`tests/smoke/pudo-smoke.sh`) + target CMake `check` / `smoke` / `verify`.

## Licença

- **GNU Affero General Public License v3.0 (AGPL-3.0)**
- Arquivo oficial: `LICENSE.md` na raiz do projeto.
- Uso comercial é permitido, desde que o código-fonte de versões modificadas (incluindo as oferecidas em rede) seja disponibilizado sob a mesma licença.
- Link oficial: https://www.gnu.org/licenses/agpl-3.0.html

## Pendências
A tabela de pendências e planejamento do projeto está em `TODO.md` na raiz (ordenada por execução, coluna Onda marca passos paralelizáveis).

## Referências rápidas
- Standards vault (5 manuais): [`docs/standards.md`](docs/standards.md).
- Plano de implementação aprovado: `.grok/sessions/.../plan.md` (atualizado com escolha final Opção A).
- Docs universais: `~/.claude/docs/`.
- Skills proativas: `tab_pendencias`, `memo_persistente`, `proj_software`, `suporte-linux`.

Qualquer dúvida de design → apresentar opções primeiro.
