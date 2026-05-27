# CLAUDE.md — petrush (Shell CLI / Ferramenta Diagnóstica em C)

Preferências específicas deste projeto. Regras universais em `~/.claude/CLAUDE.md` continuam valendo 100%.

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
- TDD obrigatório para mid/back.
- TODO.md canônico no formato tab_pendencias.
- Repo: sempre Codeberg/Forgejo (git@codeberg.org:petrinhu/petrush.git). SSH já configurada.

## Pendências
A tabela de planejamento vive em `TODO.md` na raiz (formato da skill `tab_pendencias`).

## Referências rápidas
- Plano de implementação aprovado: `.grok/sessions/.../plan.md` (atualizado com escolha final Opção A).
- Docs universais: `~/.claude/docs/`.
- Skills proativas: `tab_pendencias`, `memo_persistente`, `forgejo`, `proj_software`, `suporte-linux`.

Qualquer dúvida de design → apresentar opções primeiro.
