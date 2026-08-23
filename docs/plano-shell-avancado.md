# Plano: petrush avançado (OSH primeiro)

**Status:** decisões do líder 2026-08-23. Papel. Sem implementação nesta revisão. Sem produção nesta máquina. Testes sempre em Docker.

**Tipo:** explanation + plano de ondas.
**Audience:** líder + C-levels.
**Last-reviewed:** 2026-08-23.

---

## Decisões travadas (não reabrir sem o líder)

| Tema | Decisão |
|------|---------|
| Porte | **scale** agora (Pipeline-Padrão). Gatilho **bigtech** depois. |
| POSIX | Dois dialetos estilo Oils: **OSH** + **YSH** (nome recomendado; petrush = o binário). |
| Ordem | OSH primeiro → linguagem rica → UX Fish no AST → pipes estruturados **só no YSH**. |
| Barra OSH agora | POSIX.1-2017 + bash 4/5 cotidiano. |
| Barra OSH depois | bash 5.2 máximo (onda OSH-BASH52). |
| Pipes | OSH = bytes. YSH = estruturado. `grep\|awk` POSIX não muda. |
| Comandos | Não clonar Unix. Builtins de conforto + PATH + completion dos ~100 mais usados. |
| Papel | Ambicionar `/bin/sh` via OSH. **Não** instalar nesta máquina sem pedido. |
| Testes | Sempre Docker. Sem poll GHA (github-gossips). |
| Produção | Proibida aqui até o líder pedir. Sem 4755. |

Norte de produto (você escolheu “tudo”): Unix-rich + UX Fish + estruturado. **Construção** é sequencial (OSH primeiro), senão viram três shells pela metade.

---

## O que não é POSIX e o que é extra

- **OSH** é o contrato POSIX (IEEE 1003.1-2017 XCU cap. 2) **mais** o bash que as pessoas realmente escrevem (`[[ ]]`, `local`, `source`, arrays indexados).
- **YSH** não é POSIX. É a linguagem máxima.
- **Extras de usuário** (não são POSIX, não clonam `ls`): `pushd`/`popd`/`dirs`, `help`, `pudo`, `which`/`type`, highlight, autosuggest, completion no AST. Externos (`ls`, `git`, `docker`, `ssh`, `grep`, …) continuam no PATH.

---

## Ondas (resumo)

0. **OSH-0** shebang + script + exit status (destrava tudo).
1. **AST-0/1** runner compartilhado + stmt composto.
2. **XDG-1** `~/.config/petrush/` (rc, aliases, env); history em state dir.
3. **OSH-1..9** posicionais, set/trap, funções, if/while/for/case, `$( )`, `$(( ))`, here-doc, `[[ ]]`, arrays, word-eval POSIX só em `--posix`.
4. **JOB-1..3** process group, fg/bg/Ctrl-Z, pipeline = um job (PTY no container).
5. **LANG/STRUCT** YSH + tabelas só nesse dialeto.
6. **AST-2/3 + UX-FISH** complete/highlight/autosuggest no AST.
7. **PUDO-CAP** capability, sem 4755.
8. **OSH-BASH52** depois.

Detalhe de IDs, DoD OSH-0 e lista PATH vs builtin: Caetano 2026-08-23 (sessão). Primeira fatia de código = **OSH-0**, só quando o líder mandar implementar.

---

## Cósimo (scale)

C-levels ativos: Celso, Capitolino, Caetano, Camilo light, Cosmo, Narciso.
Dormentes: Cândido, Caio, Confúcio, Cícero, Cláudio.
Cerimônia: Kanban, sem teatro Scrum de 1 humano.
Não acordar GTM/CLO/CFO.

---

## Fora do OSH-v1

coproc, process subst, `<<<`, associativos, `export -f`, glob `**`, instalar `/bin/sh`, clonar coreutils.
