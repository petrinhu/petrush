# Log de ações — sessão autônoma petrush

Regra: bullets em tempo real. Orquestrador só despacha/julga. C-level = grok-4.6. Workers = grok-4.5, teto 2. Escrita só neste cwd.

## 2026-08-21 — início AFK

- Início modo autônomo (líder AFK). Escopo: (1) sudo CVEs vs pudo (2) falhas de segurança de 10 shells (3) inventário de funções dos mesmos shells (4) commit+push pesquisa (5) WSJF na tabela (6) depois, ondas uma a uma.
- Shells alvo: bash, zsh, fish, dash, ksh, mksh, tcsh, busybox ash, yash, osh/oil.
- Memórias: só neste cwd (`docs/memory/`). Fora do cwd = read-only. Sem clone/cópia de git.
- Despacho C-level: Narciso (plano pesquisa segurança) + Caetano (plano inventário de funções + encaixe TODO/WSJF).
- Schema TODO: 9 colunas (ID, Onda, Grupo, Descrição Técnica, Prioridade, Pré-requisito, Dificuldade, Status, Estado Auditado). WSJF early qualitativo. Teto 2 workers 4.5.
- Planos C-level recebidos: Narciso (20 classes sudo vs boundary A pudod / B fallback sudo; 10 shells vs REPL unpriv). Caetano (matriz ~32 classes Feature×shell; cap 8 FEAT; UX-24/25/26 skip; UX-19..23 intactos).
- Despacho 2 workers grok-4.5: security-engineer A = docs/memory/sudo-pudo-riscos.md; security-engineer B = docs/memory/shells-seguranca.md. technical-writer do inventário de funções espera slot.
- Artefatos A/B gravados (HEAD 1fdbfbf). Orquestrador: CVE-2025-32462/32463 confirmados em sudo.ws 2025-06-30. Mapping A NÃO para flags -h/-R bate com `sudo --`. SEC-04/05/03 vivos em A se 4755.
- Slot livre: technical-writer shells-funcoes.md + Narciso audita os dois md de segurança (C-level, não conta no teto de 2 workers de execução).
- Narciso: APROVAR COM RESSALVAS; prosa corrigida (ordem find_pudod, CWE, UAF CHROOT=, noclobber SIM). Residuais SEC-06..10 sugeridos.
- Caetano: cap FEAT = BANG TRUE UMASK NOCLOBBER READ PARAM TEST (rejeitou CMDSUB/FUNCS/FLOW e printf/glob[]).
- Cosmo: TODO reordenado W1..W17; TST após FEAT-TEST; primeira ⏳ = W2 SEC-02/05/04/03 + DOC-01.
- Próximo: commit+push da pesquisa; depois onda W2 uma fatia por vez (teto 2 workers).
- Commit `3269896` push origin/main (`1fdbfbf..3269896`). Pesquisa + WSJF no remoto.
- Onda W2 ⏳: despacho 2 workers (teto) SEC-02 (`pudo.c` find_pudod) + SEC-05 (`pudod.c` realpath fail-closed). NEW-22 não puxar (líder AFK).
- SEC-02 `ae73665` e SEC-05 `307e891` no main; `test_pudo` 13/13; check 12/12; push até `dada643`.
- W2 continua: SEC-04 (fail closed argc) + SEC-03 (example allow).
- SEC-03/04/06/07/08 no remoto. SEC-09 O_EXCL trunc + SEC-10 rc uid/mode: commits locais `587b045` / `877db3c`.
- NEW-22 permanece ⏳ (líder AFK). Próxima fatia ROI ⏳: UX-19 builtins no pipe.
- UX-19 `5d55236` no main: check 15/15, smoke 47/47 (`echo | cat` builtin). TODO UX-19 🔍.
- Próxima ⏳: UX-20 Ctrl-R. NEW-22 segue bloqueado no líder.
- UX-20 Opção A (`75fa64e`): Ctrl-R reverse-i-search no linenoise; check 15/15. TODO 🔍.
- Próxima ⏳: UX-21 highlight mínimo. Log segue neste arquivo.
- UX-21 `d5ccfa4` no main: highlight mínimo aspas/tokens; check 16/16. TODO 🔍.
- Próxima ⏳ na fila: UX-22 `source` / `.`. NEW-22 ainda espera o líder.
- UX-22 `5a6c097` no main: source/. teto 8, rc_trust; check 17/17. TODO 🔍.
- Próxima ⏳: UX-23 background `&` + job control mínimo. NEW-22 segue AFK.
- UX-23 `053e3b5` no main: `&` + jobs + reaper; check 18/18. TODO 🔍. Sem fg/bg.
- Onda W10: despacho FEAT-BANG + FEAT-TRUE (teto 2, paralelos, pré-req UX-23).
- W10/W11/W12/W13 FEAT fechadas em 🔍: BANG TRUE UMASK NOCLOBBER READ PARAM TEST. HEAD inclui `dd93dda` FEAT-TEST.
- Próxima onda W14 TST-* (T2 estática, T8 secrets, T15 pré-CI…). NEW-22 segue ⏳ (líder AFK). Log canônico: este arquivo.
- UX-21 Opção A: highlight mínimo (aspas/CMD/OP); `test_highlight` + check 16/16; TODO 🔍. Sem push.
- UX-22 impl: Mid source.h/c + test_source; load_rc_file=missing_ok; check 17/17 smoke 50; TODO 🔍. Sem push.
