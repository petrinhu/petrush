# TST-T2 Análise Estática

**Data:** 2026-08-22T09:01:33-03:00  
**SHA HEAD (pré-commit do relatório):** `693e66febce8f8d4281a7e9f4dfa95541adc522b`  
**Agent:** qa-engineer  
**Item:** TST-T2 (W14)  
**Veredicto:** **GATE FALHOU** (targets `cppcheck` e `clang-tidy` exit ≠ 0; há achados críticos)

## Escopo

Detectar bugs, má prática e problemas de segurança sem executar o código, conforme `TESTES.md` (projeto) § TST-T2 e vault `TESTES.md` § T2.

**In:** sources em `src/` via targets CMake `cppcheck` e `clang-tidy` (lista explícita no `CMakeLists.txt`; vendor excluído do clang-tidy).  
**Out:** mutação de código de produção, fix dos achados (handoff a backend-engineer), CI remoto.

## Ferramentas / ambiente

| Peça | Valor |
|---|---|
| cppcheck | 2.21.1 (`/usr/bin/cppcheck`) |
| clang-tidy | LLVM 22.1.8 (`/usr/bin/clang-tidy`) |
| Build dir | `build/` (já configurado; `compile_commands.json` presente) |
| `.clang-tidy` | `WarningsAsErrors: '*'` + threshold cognitive complexity = 50 |
| CI local note | workflow trata lint como best-effort (`\|\| true`); aqui o gate da fatia é o exit dos targets |

Logs brutos em `/var/tmp/petrush-tst-t2/` (`cppcheck.log`, `clang-tidy.log`, `meta.txt`).

## Execução

```text
cmake --build build --target cppcheck
# EXIT 2 (cppcheck --error-exitcode=1 + 1 warning)

cmake --build build --target clang-tidy
# EXIT 2 (7 diagnostics sob WarningsAsErrors)
```

## Achados

### cppcheck (1 warning → falha do target)

| Sev | ID | Local | Notas |
|---|---|---|---|
| **Crítico (ordem de check)** | `nullPointerRedundantCheck` | `src/mid/dispatcher.c:575` (`feat_test_parse_long`) | `strtol(s, …)` **antes** de `if (!s \|\| …)`. Se `s == NULL`, UB. Caller atual (`feat_test_binary`) passa `argv` (não-NULL na prática), mas o contrato da função promete tratar NULL e faz isso tarde demais. **Novo com FEAT-TEST.** |

### clang-tidy (7 errors sob `WarningsAsErrors: '*'`)

| Sev | Check | Local | Notas |
|---|---|---|---|
| **Crítico (SAST / CWE-119)** | `clang-analyzer-security.insecureAPI.strcpy` | `dispatcher.c:835` e `:836` | `strcat` no builtin `alias` ao concatenar valor. Buffer foi `malloc(total+1)` com `total` somado dos `strlen`, então o tamanho cabe; mesmo assim o check exige API com bound (`strlcat` / `memcpy` + restante). |
| **Crítico (analyzer)** | `clang-analyzer-core.CallAndMessage` | `parser.c:50` (`cmd_clear` → `free(cmd->argv[i])`) | Analyzer marca argumento de `free` como valor não inicializado num caminho a partir do parse de lista. Exige revisão do fluxo `argv`/`argc` (falso positivo possível; não descartar sem prova). |
| Qualidade | `readability-function-cognitive-complexity` | `process.c:321` `execute_pipeline_with_hook` = **89** (limiar 50) | Dívida conhecida de pipeline; sem `NOLINT` no sítio. |
| Qualidade | idem | `parser.c:86` `tokenize` = **91** | Comentário NEW-02 aponta NOLINT em `parser.c`, mas o `NOLINTNEXTLINE` atual está em `petrush_parse_pipeline` (linha 331) e **não** cobre `tokenize`. |
| Qualidade | idem | `pudo.c:481` `find_pudod_binary` = **52** | Pouco acima do limiar 50. |
| Qualidade / API | `bugprone-easily-swappable-parameters` | `dispatcher.c:583` `feat_test_unary(const char *op, const char *arg)` | Dois `const char *` adjacentes. **Novo com FEAT-TEST.** Estilo; não é crash. |

**Contagem por check:** complexity×3, insecure strcat×2, CallAndMessage×1, easily-swappable×1.

## Classificação do gate

Critério da fatia (brief): *zero erro novo crítico*.

- Erros **novos críticos** presentes: sim (`nullPointerRedundantCheck` em FEAT-TEST; `strcat` inseguro; analyzer `CallAndMessage`).  
- Targets CMake: **vermelhos**.  
- Portanto **não** se promove TST-T2 a `🔍`.

## O que NÃO cobre

1. Fix / refactor (fora do mandato QA desta fatia).  
2. ASan/UBSan/valgrind (TST-T4).  
3. Semgrep/CodeQL (AUD-SEC).  
4. Mutation testing da suíte.  
5. Prova de que o `CallAndMessage` em `parser.c:50` é verdadeiro positivo (só o path do analyzer).

## Critério de saída TST-T2

- [x] `cppcheck` target executado e log capturado  
- [x] `clang-tidy` target executado e log capturado  
- [x] Achados classificados (crítico vs qualidade)  
- [x] Relatório em `docs/memory/tst-t2-estatica.md`  
- [ ] Gate limpo (zero erro novo crítico) — **FALHOU**  
- [ ] Status `🔍` — **bloqueado** até remediação + re-run verde

## Handoff sugerido (backend-engineer)

1. Em `feat_test_parse_long`: checar `!s` / vazio **antes** de `strtol`.  
2. Em `alias`: trocar `strcat` por cópia com bound (ex. `memcpy` + cursor, ou `strlcat` se disponível).  
3. Investigar path `cmd_clear` / `argv[i]` não inicializado (`parser.c:50`).  
4. `NOLINT` ou extrair átomos: `tokenize`, `execute_pipeline_with_hook`, `find_pudod_binary`; opcional tipar/`NOLINT` em `feat_test_unary`.  
5. Re-rodar `cmake --build build --target lint` até exit 0.

## Referências

- Item: `TODO.md` → TST-T2  
- `TESTES.md` (projeto) § TST-T2  
- Vault `TESTES.md` § T2  
- Targets: `CMakeLists.txt` (`cppcheck`, `clang-tidy`, `lint`)  
- Config: `.clang-tidy` (NEW-02)

## Remediação parcial (backend-engineer, dispatcher.c)

**Escopo:** só achados críticos/apontados em `src/mid/dispatcher.c`. Sem mexer em `execute_pipeline` / `parser.c` / complexity.

| Achado | Fix |
|---|---|
| `nullPointerRedundantCheck` `feat_test_parse_long` | Guard `!s \|\| !out \|\| s[0]=='\\0'` **antes** de `strtol`. |
| `clang-analyzer-security.insecureAPI.strcpy` (`strcat` em `builtin_alias`) | Concat com `snprintf` + `off`/`cap`; aborta se truncar. |
| `bugprone-easily-swappable-parameters` `feat_test_unary` | Struct `feat_test_unary_args` (`unary_op`, `operand`) + designated init no call site. |

**Prova local:**
- `cmake --build build --target petrush test_info` OK
- `./build/test_info` SUCCESS (incl. `builtin_test_*`)
- `cppcheck` só em `src/mid/dispatcher.c` → exit 0
- `clang-tidy -p=build src/mid/dispatcher.c` → sem `nullPointer` / `strcat`/`strcpy` / `easily-swappable`

**Fora deste commit (ainda no gate TST-T2):** `parser.c:50` CallAndMessage; complexity em `process.c`/`parser.c`/`pudo.c`. Orquestrador re-roda lint completo; **não** marcar TODO TST-T2 `🔍` aqui.
