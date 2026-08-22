# TST-T14 Integração fim-a-fim (smoke)

**Data:** 2026-08-22T19:35:31-03:00  
**SHA HEAD (pré-commit do relatório):** `680e0191ed85b60bc2ec03193508125eb08b441d`  
**Agent:** qa-engineer  
**Item:** TST-T14 (W14)  
**Veredicto:** **PASS**

## Escopo

Validação de integração fim-a-fim do binário `petrush` (builtins, externos, pudo sem setuid, pipes, redirs, source, background/jobs), conforme `TESTES.md` (projeto) e vault `TESTES.md` § T14 (Integração / Sandbox).

**In:** target CMake `smoke` → `tests/smoke/pudo-smoke.sh` contra `build/petrush`; target `check` (ctest unitário) como suíte rápida de regressão.  
**Out:** setuid/`pudod` privilegiado, ASan/valgrind (TST-T4), pré-CI container (TST-T15), CI remoto.

## Ambiente

| Peça | Valor |
|---|---|
| Build dir | `build/` (já configurado) |
| Binário | `build/petrush` (relinkado antes do smoke) |
| Script | `tests/smoke/pudo-smoke.sh` |
| Host | Linux local (sem setuid no pudod; paths de recusa esperados) |
| Sandbox | smoke não abre UI; sem sessão gráfica |

## Execução

```text
cmake --build build --target smoke
# ELAPSED=1.10s  EXIT=0
# SMOKE SUMMARY: Passed: 53  Failed: 0
# SMOKE PASSED (includes pudo integrated paths + sanitization-relevant commands)

cmake --build build --target check
# ELAPSED=0.83s  EXIT=0
# 100% tests passed, 0 tests failed out of 18
# Total Test time (real) = 0.33 sec
```

Logs brutos: `/var/tmp/petrush-tst-t14-smoke.log`, `/var/tmp/petrush-tst-t14-check.log`.

## Contagem

| Suite | Comando | Passed | Failed | Tempo | EXIT |
|---|---|---:|---:|---:|---:|
| smoke (integração) | `cmake --build build --target smoke` | 53 | 0 | 1.10 s | 0 |
| check (unitário ctest) | `cmake --build build --target check` | 18/18 | 0 | 0.83 s (ctest 0.33 s) | 0 |

## Cobertura do smoke (amostra do que passou)

Builtins: `pwd`, `echo`, `help`, `export`/`unset`, `history`, `clear`, `info`, `alias`, `which`, `pushd`/`popd`, `cd -`, `source`/`.`.  
Externos e erro: comando inexistente (127), `ls` path inválido.  
pudo: `--help`; execução sem setuid (recusa allow-list/euid).  
Expand: tilde, `$VAR`, glob unquoted vs quoted.  
Listas: `&&`, `||`, `;` (SEQ), aspas com `;` literal, leading `;` parse error.  
Pipes/redir: `printf|cat`, builtin no pipe, `>`/`2>`/`2>>`/`2>&1`/`&>`, SEC-09 noclobber.  
Jobs: background `&`, `jobs`, `&>` não é background.

## Achados

Nenhum. Smoke 53/53 e check 18/18 verdes.

## Limitações (honesto)

1. Smoke espera paths de **recusa** do pudo sem setuid; não prova o caminho privilegiado real.  
2. Não cobre ASan/UBSan/valgrind (TST-T4) nem pré-CI em container Fedora (TST-T15).  
3. Não substitui CI remoto (fonte de verdade pós-push).  
4. `check` é unitário (ctest); a prova T14 de integração é o target `smoke`.

## Critério de saída TST-T14

- [x] `cmake --build build --target smoke` EXIT 0  
- [x] Contagem smoke: Failed = 0 (53 PASS)  
- [x] `check` rápido também verde (18/18)  
- [x] Relatório em `docs/memory/tst-t14-smoke.md`  
- [ ] ✅ na tabela só após onda de auditoria/TST se o orquestrador assim marcar

**Status sugerido no TODO:** `🔍 Pendente verificação` (execução entregue; ✅ só pós auditoria/TST da onda).

## Referências

- Item: `TODO.md` → TST-T14  
- `tests/smoke/pudo-smoke.sh`  
- `CMakeLists.txt` targets `smoke` / `check`  
- Vault `TESTES.md` § T14  
