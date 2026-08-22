# AUD-SEC  -  Auditoria de Segurança (petrush)

**Data:** 2026-08-22  
**SHA HEAD (pré-commit do relatório):** `eaf37e1`  
**Agent:** security-engineer (defensivo-only)  
**Item:** AUD-SEC (W15)  
**Classificação de severidade:** CRÍTICO / IMPORTANTE / COSMÉTICO (vault `AUDITORIAS.md`)  
**Veredicto:** **APROVAR COM RESSALVAS**  
**Setuid `4755`:** **NÃO ENDOSSADO** (permanece gate humano / `DEPLOY_CHECKLIST`)

---

## 1. Escopo e método

### In

- Memory safety dinâmica (TST-T4) e estática residual (TST-T2/T15)
- Secrets (TST-T8 + re-scan gitleaks nesta passada)
- Hardening de binário Release (TST-T7 + re-checksec)
- CVE/deps (TST-T5 / TST-T12), com foco linenoise **CVE-2025-9810**
- Helper `pudod` + frontend `pudo` (`src/pudod/*`, `src/mid/pudo.c`)
- SEC-01..SEC-10 (código vivo vs TODO `🔍`)
- Docs: `docs/security/`, `docs/memory/sudo-pudo-riscos.md`, `docs/memory/shells-seguranca.md`, `docs/memory/tst-t*.md`

### Out

- PoC / payload / receita ofensiva
- Aplicar setuid/setcap no host
- Push remoto
- AUD-DISC / AUD-ARCH / AUD-REPORT (ondas irmãs)

### Cruzamento de manuais

| Fonte | Uso nesta auditoria |
|---|---|
| `Projects/petrush/AUDITORIAS.md` § AUD-SEC | Escopo: memory safety, secrets, binário, LGPD/privacidade, helper setuid |
| Vault / `Resources/Standards/AUDITORIAS.md` | Escala CRÍTICO / IMPORTANTE / COSMÉTICO; índice I3 Segurança (cópia canônica no vault está **só como índice** nesta máquina; checklist operacional efetivo = poda do projeto) |
| `TESTES.md` (projeto) + vault T4/T7/T8/T12 | Evidências de gates já em `🔍` |
| `docs/security/pudod-install.md` + `DEPLOY_CHECKLIST` | Postura: install sem privilégio automático; 4755 só manual |

**LGPD/privacidade:** CLI local sem PII de terceiros no produto; history/rc são do operador. Sem base legal/DPIA aplicável além de higiene (não logar segredo; history `0600`).

---

## 2. Evidências revalidadas (2026-08-22)

| Gate | Fonte | Resultado nesta AUD-SEC |
|---|---|---|
| **TST-T4** memory | `docs/memory/tst-t4-asan.md` | Sanitize check 18/18 + smoke 53/53; valgrind unit+REPL+pudod deny; **0 leak/UB** observável |
| **TST-T7** binário | `docs/memory/tst-t7-binario.md` + re-`checksec` | `petrush`/`pudod` Release: **Full RELRO + PIE + NX + canary**; Fortify **parcial** (6/16 e 4/6) |
| **TST-T8** secrets | `docs/memory/tst-t8-secrets.md` + re-`gitleaks detect` | **no leaks found** (78 commits / ~861 KB nesta passada) |
| **TST-T12** CVE | `docs/memory/tst-t12-cves.md` | **0 CRITICAL** no vendor; **CVE-2025-9810 MEDIUM 6.8** mitigado (SEC-08) |
| Host setuid | `ls /usr/local/libexec/petrush-pudod` | **Ausente** (nenhum 4755 instalado neste host) |
| Artefato Release | `build-preci-rel/pudod` | mode `755` root:root (sem setuid bit) |

---

## 3. Matriz SEC-01..SEC-10 (código vivo)

Status do TODO permanece `🔍` (impl entregue; ✅ só pós TST/AUD de fechamento). Esta auditoria **confirma presença** dos controles no tree `eaf37e1`.

| ID | Controle | Evidência arquivo | Estado código | Risco residual |
|---|---|---|---|---|
| **SEC-01** | Não `unsetenv` no pai após `pudo` | `src/mid/pudo.c` (~L112); filho via `build_clean_envp` | Presente | API `pudo_sanitize_environment()` ainda muta se chamada |
| **SEC-02** | Release: path do helper só install absoluto | `pudo_allow_pudod_candidate` + `find_pudod_binary` (`NDEBUG`) | Presente | Sibling de `/proc/self/exe` sob dirs de install; Debug ainda aceita relativos |
| **SEC-03** | Example allow mínima | `src/pudod/pudo.allow.example` (`id`/`whoami`/`true`) | Presente | Operador pode alargar `/etc/petrush/pudo.allow` (policy humana) |
| **SEC-04** | Fail closed se argc > MAX | `child_argv.c` + `pudo_helper_argv_fits` | Presente |  -  |
| **SEC-05** | Allow-list: `realpath` fail → skip literal | `allow_resolve.c` + `load_allow_list` | Presente | Entradas inexistentes no load somem (fail closed) |
| **SEC-06** | Alvo: regular + `st_uid==0` + exec | `target_check.c` após `fstat(fd)` | Presente |  -  |
| **SEC-07** | TOCTOU: `O_NOFOLLOW` + `fexecve` | `target_open.c` + `pudod.c` Linux | Presente | Fallback não-Linux fecha fd e `execve(path)` (janela residual) |
| **SEC-08** | linenoise history: `O_NOFOLLOW` + `fchmod` 0600 | `vendor/linenoise/linenoise.c` ~2551-2565; `test_linenoise_history` | Presente | CVE-2025-9810 mitigado localmente (MEDIUM) |
| **SEC-09** | `>` / `2>` com `O_EXCL` | `process.c` + `dispatcher.c` | Presente | `>>`/`2>>` continuam append (esperado) |
| **SEC-10** | `~/.petrushrc` uid/mode | `rc_trust.c` + `source.c` + `test_rc_trust` | Presente |  -  |

---

## 4. Achados

### 🔴 CRÍTICO

#### AUD-SEC-C1  -  Setuid `4755` / setcap no `pudod` permanece **não endossado**

**Categoria:** CWE-269 / CWE-250 · OWASP A01 (Broken Access Control) em helper privilegiado  
**Descrição:** Mesmo com SEC-01..10 no código, qualquer bit setuid no helper transforma bug residual + allow-list larga em escalada local. CMake/`pudod-install.md` **não** aplicam 4755 automaticamente; o binário Release local está `755`.  
**Impacto:** Compromisso total da máquina se o operador aplicar 4755 sem revisão linha a linha + allow-list mínima + ambiente isolado.  
**Postura AUD-SEC:** **NÃO endossar** `chmod 4755` nem `setcap` nesta passada. Gate continua humano (`DEPLOY_CHECKLIST` + leitura de `docs/security/`).  
**Verificação:** `stat` do artefato; ausência de `/usr/local/libexec/petrush-pudod` no host.

#### AUD-SEC-C2  -  Boundary B (fallback `sudo`) reabre superfície do host

**Arquivo:** `src/mid/pudo.c` `run_via_pudod` (~L667-696)  
**Categoria:** CWE-829 / CWE-1104  
**Descrição:** Se `find_pudod_binary()` falha, o frontend avisa e faz `execve("/usr/bin/sudo", …)` e, se falhar, `execvp("sudo", …)`. O `--` bloqueia flags do sudo, mas **sudoers / tickets / PAM / Defaults** do host voltam (mapa em `docs/memory/sudo-pudo-riscos.md`).  
**Impacto:** Comportamento privilegiado deixa de ser o helper mínimo auditável e passa a depender da versão/config do sudo instalado.  
**Fix (defensivo, futuro):** falhar fechado sem pudod em Release; ou exigir path absoluto configurado e recusar `execvp("sudo")` (PATH spoof).  
**Defense in depth:** manter pudod install path; documentar que fallback é só emergência de lab.

---

### 🟠 IMPORTANTE

#### AUD-SEC-I1  -  `execvp("sudo")` após falha de `/usr/bin/sudo`

**Arquivo:** `src/mid/pudo.c` ~L694  
**CWE-426 / CWE-427**  
PATH search do segundo fallback permite spoof se `/usr/bin/sudo` sumiu/foi substituído e `PATH` do clean_envp ainda aponta a dirs graváveis. Preferir fail closed sem `execvp`.

#### AUD-SEC-I2  -  Frontend `build_clean_envp` propaga `TZ`/`TMPDIR`/`HOME`/…

**Arquivo:** `src/mid/pudo.c` ~L325-356  
**CWE-15 / CWE-426**  
No caminho **pudod**, o helper descarta e reconstrói env mínimo (bom). No caminho **sudo**, o keep-list do frontend chega ao host sudo (mitigação parcial via `env_reset` do sudoers, não sob controle do petrush).

#### AUD-SEC-I3  -  Allow-list client-side permissiva quando vazia + fallback sudo

**Arquivo:** `src/mid/pudo.c` `is_command_allowed` (~L219-223)  
Se `~/.config/petrush/pudo.conf` não existe, o frontend **não** filtra; a autoridade é o `pudod`. Com fallback sudo e sem conf do usuário, a filtragem client some e sobra só sudoers.

#### AUD-SEC-I4  -  Docs de auditoria desatualizados vs código

| Doc | Drift |
|---|---|
| `docs/security/pudo-audit.md` (2026-07-01) | Ainda descreve trunc silencioso / realpath fail-open / example amplo; **código já tem SEC-03/04/05** |
| `docs/memory/shells-seguranca.md` | Ainda marca noclobber como superfície aberta; **SEC-09 já usa `O_EXCL`** |
| `docs/memory/sudo-pudo-riscos.md` | Snapshot pré-fechamento de vários SEC-*; útil como mapa de classes, não como status atual |

**Risco:** operador/auditor lê doc velho e subestima ou sobrestima o risco. Atualizar na onda DOC / AUD-REPORT.

#### AUD-SEC-I5  -  Superfície REPL unpriv residual (não setuid)

Classes ainda presentes (ver `shells-seguranca.md`, ajustado ao código atual):

- History bang (`!!` / `!n`)
- Alias + completion PATH
- Redir/pipe (overwrite mitigado em `>` via SEC-09; append permanece)
- Glob limitado (`*`/`?`)
- `source` / `.` (depth 8 + `rc_stat_ok`; ainda executa no processo)

Não são escalada root por si; são abuso local do shell do usuário.

#### AUD-SEC-I6  -  Fortify parcial + símbolos não stripped (Release)

**Evidência:** checksec Fortify partial; 367 / 73 symbols.  
Não falha T7; aumenta superfície de info para atacante local com o binário. Strip em pacote de produção é recomendação, não bloqueio desta AUD-SEC.

---

### 🟢 COSMÉTICO

#### AUD-SEC-O1  -  `pudo_sanitize_environment` API pública ainda muta `environ`

Útil a testes; risco se algum caller futuro usar no REPL. Preferir deprecar ou `#ifdef` de teste.

#### AUD-SEC-O2  -  Truncamento de log (`LOG_BUF_SIZE` 512 / buffers de log)

Pode cortar argv longo na trilha de auditoria (repudiation parcial).

#### AUD-SEC-O3  -  Ferramentas ausentes no host

`trufflehog` e `hardening-check` ausentes; cobertos por gitleaks + checksec. Registrar no inventário de tooling.

#### AUD-SEC-O4  -  Vault `AUDITORIAS.md` só índice

A cópia canônica na raiz do vault / `Resources/Standards` nesta máquina não traz o corpo I3; a poda do projeto carrega o escopo operacional. Risco de processo (não de código).

---

## 5. Memory safety (síntese)

| Camada | Resultado | Nota |
|---|---|---|
| ASan+UBSan (check+smoke) | PASS | TST-T4 |
| Valgrind (unit + REPL + pudod deny) | PASS (definite/indirect 0) | TST-T4 |
| Fuzz parser/expand/prompt | crashes=0 (TST-T3) | Consumir em AUD-QUALITY/COV |
| clang-tidy ArrayBound residual | citado em T15 (`pudo.c` find path) | Não é PoC; acompanhar em T2 |

**Conclusão memory safety:** sem achado dinâmico aberto nesta passada. Risco residual = complexidade de `find_pudod_binary` / buffers fixos (já fail-closed em overflow de argv).

---

## 6. Secrets (síntese)

- gitleaks histórico + working tree: limpo (T8 + re-scan AUD-SEC)
- Sem `.env` / PEM / `BEGIN PRIVATE KEY` / tokens óbvios no tree
- History file mode 0600 via `fchmod` (SEC-08)
- **Não** há vault de segredo no produto; risco = history do operador com senhas digitadas no REPL (educação / `maskmode` linenoise opcional)

---

## 7. Binário (síntese)

| Mitigação | petrush | pudod |
|---|---|---|
| PIE | sim | sim |
| NX | sim | sim |
| Full RELRO | sim | sim |
| Stack canary | sim | sim |
| Fortify | parcial | parcial |
| RPATH/RUNPATH | não | não |
| setuid no artefato local | **não** | **não** (`755`) |

---

## 8. pudod / setuid (postura final)

```
petrush (unpriv) ──argv absoluto──► pudod (só se euid==0)
                                      ├─ allow-list root-owned
                                      ├─ realpath + O_NOFOLLOW + fstat
                                      ├─ root-owned regular + exec
                                      ├─ envp mínimo hardcoded
                                      └─ fexecve(fd)  [Linux]
                 └──(se pudod ausente)──► /usr/bin/sudo -- …   ← residual CRÍTICO de processo
```

**Regras desta auditoria (não negociáveis nesta passada):**

1. **Não** aplicar `chmod 4755` / `setcap` sem ordem explícita do líder após revisão do `pudod.c` e allow-list mínima.
2. Install CMake continua **sem** setuid automático (já documentado).
3. Smoke/CI devem continuar cobrindo o caminho **sem** privilégio (deny + install prefix sem 4755), como em TST-T15.

---

## 9. linenoise CVE-2025-9810

| Campo | Valor |
|---|---|
| CVE | CVE-2025-9810 |
| Severidade NVD | MEDIUM 6.8 |
| Classe | TOCTOU history save (`fopen`+`chmod` path) |
| Mitigação petrush | `open(O_NOFOLLOW\|…)` + `fchmod(fd, 0600)` |
| Teste | `test_linenoise_history` |
| Item | SEC-08 `🔍` |
| Veredito AUD-SEC | **Mitigado** no vendor local; não CRITICAL |

---

## 10. Veredicto e próximos passos

**APROVAR COM RESSALVAS** para seguir a onda W15 (AUD-* irmãs / AUD-REPORT), **sem** endosso de setuid.

### Checklist de saída AUD-SEC

- [x] Memory safety cruzada com TST-T4
- [x] Secrets cruzados com TST-T8 (+ re-gitleaks)
- [x] Binário cruzado com TST-T7 (+ re-checksec)
- [x] CVE linenoise cruzado com TST-T12 / SEC-08
- [x] pudod/pudo revisados (SEC-01..10 presentes)
- [x] Setuid 4755 **não** endossado
- [x] Achados classificados CRÍTICO / IMPORTANTE / COSMÉTICO
- [x] Relatório em `docs/auditoria/aud-sec.md`
- [ ] Status TODO `🔍` (este commit)
- [ ] ✅ só após julgamento do orquestrador / TST-AUD de fechamento

### Ações recomendadas (não implementadas nesta fatia)

1. **CRÍTICO-processo:** manter 4755 bloqueado; se um dia for considerado, AskUserQuestion + revisão Narciso/CISO + allow-list ≤3 inocentes.
2. **I1/I2/C2:** endurecer fallback sudo (fail closed Release; remover `execvp("sudo")`; envp mínimo no B).
3. **I4:** atualizar `pudo-audit.md` e `shells-seguranca.md` para bater com SEC-03/04/05/09.
4. **O1:** isolar `pudo_sanitize_environment` de builds de produto.

---

## 11. Referências

- `AUDITORIAS.md` (projeto) § AUD-SEC  
- Vault / `Resources/Standards/AUDITORIAS.md` (escala + índice I3)  
- `docs/security/pudo-audit.md`, `docs/security/pudod-install.md`  
- `docs/memory/sudo-pudo-riscos.md`, `docs/memory/shells-seguranca.md`  
- `docs/memory/tst-t4-asan.md`, `tst-t7-binario.md`, `tst-t8-secrets.md`, `tst-t12-cves.md`  
- Código: `src/pudod/*`, `src/mid/pudo.c`, `src/front/rc_trust.c`, `vendor/linenoise/linenoise.c`  
- TODO: SEC-01..10, TST-T4/T7/T8/T12, **AUD-SEC**

*Relatório defensivo. Sem PoC. Sem push. Sem endosso de setuid.*
