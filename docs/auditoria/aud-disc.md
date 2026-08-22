# AUD-DISC - Descoberta e modelagem de ameaça (petrush)

| Campo | Valor |
|-------|-------|
| **ID** | AUD-DISC |
| **Data** | 2026-08-22 (`22/08/26 - 20:17:19`) |
| **SHA HEAD** | `e4cdc7b` (`e4cdc7b2c90bb1e8f58c0585ae274b538ffc06d1`) |
| **Auditor** | security-engineer (defensivo-only) |
| **Escopo** | REPL, parser, process, `pudo`/`pudod`, history, rc |
| **Manuais** | `AUDITORIAS.md` (projeto) § AUD-DISC; vault `AUDITORIAS.md` (índice); cruzamento `docs/memory/sudo-pudo-riscos.md` + `docs/memory/shells-seguranca.md` |
| **Método** | revisão manual de superfície + DFD textual + STRIDE por componente; sem Threat Dragon nesta passada |
| **PoC / payload** | não |
| **Push** | não |
| **Setuid 4755** | **não endossado** (gate humano; ver AUD-SEC) |

---

## 1. Objetivo

Mapear **ativos**, **atores**, **trust boundaries**, **fluxo de dados** e **ameças STRIDE** do petrush como shell REPL unprivileged + helper privilegiado opcional (`pudod`). Esta descoberta alimenta AUD-SEC (já em `🔍`), AUD-ARCH e AUD-REPORT. Não fecha gates de código; classifica superfície e risco residual.

### Fontes cruzadas (com aviso de drift)

| Fonte | Papel | Nota |
|-------|-------|------|
| [`docs/memory/sudo-pudo-riscos.md`](../memory/sudo-pudo-riscos.md) | Mapa de 20 classes sudo → boundary A (`pudod`) / B (fallback `sudo`) | Snapshot 2026-08-22 pré-fechamento de vários SEC-*; **classes** ainda válidas; **status** de SEC-* preferir código vivo + [`aud-sec.md`](aud-sec.md) |
| [`docs/memory/shells-seguranca.md`](../memory/shells-seguranca.md) | Superfície REPL unpriv vs 10 shells | Drift: marca noclobber/SEC-09 e SEC-10 como abertos; **código atual** já tem `O_EXCL` em `>`/`2>` e `petrush_rc_stat_ok` |
| [`docs/security/pudo-audit.md`](../security/pudo-audit.md) | Threat model inicial `pudo` (2026-07-01) | Histórico; vários achados já mitigados (SEC-01..10) |
| [`docs/auditoria/aud-sec.md`](aud-sec.md) | Controles vivos SEC-01..10 | Fonte de verdade para **mitigações presentes** nesta sessão |

---

## 2. Ativos protegidos

| Ativo | Onde vive | Classificação | Nota |
|-------|-----------|---------------|------|
| **Integridade do host (root)** | só se `pudod` tiver setuid/setcap | Crítico | Latente enquanto binário for `755` sem bit setuid |
| **Allow-list privilegiada** | `/etc/petrush/pudo.allow` | Crítico | Root-owned; decide o que roda como euid 0 |
| **Política sudoers do host** | Boundary B (`/usr/bin/sudo`) | Crítico (externo) | Reabre classes do sudo instalado |
| **Sessão do usuário (uid real)** | processo `petrush` | Alto | Qualquer comando do REPL = poder do caller |
| **Histórico de comandos** | `~/.petrush_history` | Médio (segredo operacional) | Pode conter paths, tokens colados, PII do operador |
| **Rc / scripts sourcados** | `~/.petrushrc`, paths de `source`/`.` | Médio | Execução no processo do shell |
| **Config client pudo** | `~/.config/petrush/pudo.conf` | Médio | Filtro client-side; **não** é autoridade se `pudod` existir |
| **Ambiente do processo** | `environ` do REPL | Médio | Não deve ser mutado pelo caminho `pudo` (SEC-01) |
| **Binários petrush / pudod** | install path / build | Alto | Spoof do helper = spoof de elevação (SEC-02) |
| **Audit trail** | syslog `LOG_AUTH` (`pudo`/`pudod`) | Médio | Repúdio se truncado ou ausente |

**Fora de escopo de produto:** PII de terceiros, DB, rede autenticada, UI web. CLI local AGPL; LGPD = higiene de history/logs do operador.

---

## 3. Atores

| Ator | Capacidade | Motivação modelada |
|------|------------|--------------------|
| **Operador legítimo** | uid do dono do home; TTY | Usar shell + eventualmente `pudo` |
| **Atacante local mesmo uid** | escreve home, cwd, args, env, symlinks sob controle do user | Abusar rc/history/redir/alias; tentar elevar via `pudod` se 4755 |
| **Atacante com home compartilhado / NFS frágil** | pode plantar rc/history se perms frouxas | Executar código no boot do REPL |
| **Insider / operador descuido** | alarga allow-list; aplica 4755 sem revisão | Transforma helper mínimo em shell root |
| **Processo pai / script** | invoca `petrush` ou `pudod` com argv/env | Env poisoning; PATH spoof do helper |
| **Dependência host (sudo)** | Boundary B | Bugs históricos sudoers/PAM/tickets (mapa sudo-pudo-riscos) |

**Não modelado como adversário remoto de rede:** sem listener; superfície = local/TTY/argv/arquivos.

---

## 4. Trust boundaries

```
┌─────────────────────────────────────────────────────────────┐
│ TB0  Sistema / kernel (execve, setuid, fexecve, open)       │
└─────────────────────────────────────────────────────────────┘
        ▲                              ▲
        │                              │
┌───────┴────────┐            ┌────────┴──────────────────────┐
│ TB1  petrush   │  argv+env  │ TB2  pudod (euid 0 se setuid) │
│ REPL unpriv    │ ─────────► │ allow-list + fexecve          │
│ (nunca root)   │            │ NÃO confia no petrush         │
└───────┬────────┘            └────────┬──────────────────────┘
        │                              │
        │ fallback se helper some      │
        ▼                              │
┌──────────────────┐                   │
│ TB3  sudo host   │◄──────────────────┘ (só via frontend pudo, Boundary B)
│ sudoers/PAM/tty  │
└──────────────────┘

Arquivos sob home do user (TB-home): ~/.petrushrc, ~/.petrush_history,
~/.config/petrush/pudo.conf  - confiáveis só após checagem de uid/mode
onde implementado (rc/source: SEC-10).

Arquivo de política root (TB-root-fs): /etc/petrush/pudo.allow
```

| Boundary | Quem confia em quem | Regra |
|----------|---------------------|-------|
| **TB1 → TB2** | `pudod` **não** confia no frontend | Revalida path absoluto, allow-list, `fstat`, argc |
| **TB1 → TB3** | petrush delega ao sudo do host | `--` bloqueia flags; sudoers decide o resto |
| **TB-home → TB1** | REPL executa linhas do rc/`source` | `petrush_rc_stat_ok` (regular, uid==self, sem write group/other) |
| **Vendor linenoise** | history I/O | SEC-08: `O_NOFOLLOW` + `fchmod(0600)` no fd |

---

## 5. Data flow (DFD textual)

### 5.1 REPL interativo (caminho unpriv)

```
Operador (TTY)
  → linenoise (prompt_render; sem cmdsubst)
  → hist_expand_line (!! / !n / …)
  → linenoiseHistoryAdd
  → alias_expand_line
  → petrush_parse_list (parser)
  → expand ($VAR / ~ / glob *?)
  → dispatch_list / dispatcher
       ├─ builtins (cd, export, pudo, source, …)
       └─ execute_external / pipeline (foundation/process + job)
            → fork → apply_redirs → execvp/execve
  → (ao sair / SIGTERM) linenoiseHistorySave(~/.petrush_history)
```

Boot: `linenoiseHistoryLoad` → `petrush_source_file(~/.petrushrc, missing_ok)` antes do loop.

### 5.2 Elevação `pudo` (Boundary A preferido)

```
dispatcher → builtin_pudo
  → (opcional) filtro client ~/.config/petrush/pudo.conf
  → find_pudod_binary (SEC-02: Release prefere install absoluto)
  → execve(pudod, argv absoluto + args, clean_envp frontend)
       → pudod main (euid==0 exigido)
            → load_allow_list (/etc/petrush/pudo.allow; fail closed)
            → unset LD_* / ENV / BASH_ENV / …
            → realpath(cmd) + is_allowed
            → open O_NOFOLLOW → fstat (root+exec) → build_child_argv (fail closed)
            → fexecve(fd, argv, envp mínimo hardcoded)   [Linux]
            → syslog(LOG_AUTH) com getuid() real
```

### 5.3 Fallback Boundary B

```
find_pudod_binary falha
  → aviso
  → execve("/usr/bin/sudo", ["sudo","--", cmd…], clean_envp)
  → se falhar: execvp("sudo", …)   # residual PATH (AUD-SEC-I1)
```

---

## 6. Inventário de superfície (componentes AUD-DISC)

| Componente | Arquivos | Entrada | Saída / efeito | Privilegio |
|------------|----------|---------|----------------|------------|
| **REPL / main** | `src/main.c` | TTY, sinais, env `HOME`/`PETRUSH_PS1` | loop, history, rc boot | unpriv |
| **Parser** | `src/mid/parser.c` | linha de comando | `petrush_list_t` / argv | unpriv |
| **Expand** | `src/mid/expand.c` | tokens | `$VAR`, `~`, glob `*`/`?` (teto 256) | unpriv |
| **Hist bang** | `src/mid/hist_expand.c` | `!!` / `!n` | linha expandida + eco | unpriv |
| **Alias** | `src/mid/alias.c` | 1ª palavra | reescrita | unpriv |
| **Dispatcher** | `src/mid/dispatcher.c` | lista parseada | builtin / fork / redir | unpriv |
| **Process** | `src/foundation/process.c` | cmd + redirs | fork/exec, `O_EXCL` em `>` | unpriv |
| **Job** | `src/foundation/job.c` | background / reap | notificação Done | unpriv |
| **Source / rc** | `src/mid/source.c`, `src/front/rc_trust.c` | path de arquivo | executa no processo | unpriv |
| **Complete** | `src/front/complete.c` | tab | walk PATH/fs | unpriv |
| **Prompt** | `src/mid/prompt.c` | `PETRUSH_PS1` | escapes fixos `\w\u\h\n\$\\` | unpriv |
| **History file** | `vendor/linenoise/linenoise.c` | path history | load/save | unpriv |
| **pudo client** | `src/mid/pudo.c` | argv após `pudo` | exec helper ou sudo | unpriv → eleva |
| **pudod** | `src/pudod/*` | argv absoluto | fexecve root | **privilegiado se setuid** |
| **Allow example** | `src/pudod/pudo.allow.example` | doc | `id`/`whoami`/`true` só | policy |

### Classes **ausentes** (vantagem estrutural vs shells maduros)

Herdado do cruzamento `shells-seguranca.md` (ainda válido no código):

- Sem export de função / Shellshock-class
- Sem `ENV` / `BASH_ENV` mágico no boot non-interactive
- Sem IFS word-split pós-expansão
- Sem `$(( ))` / nameref / `${!x}`
- Sem `PROMPT_COMMAND` / cmdsubst no prompt
- Sem setuid no binário `petrush` (só helper separado)

### Classes **presentes** (REPL cotidiano)

- History bang, alias, redir/pipe, glob limitado, complete PATH walk
- `source`/`.` (path explícito, depth 8, `rc_stat_ok`)
- Jobs / `&` (superfície de TTY/orphan se ligada; reap no prompt)

---

## 7. STRIDE por componente (resumido)

Severidade: **C** crítico · **A** alto · **M** médio · **B** baixo. Status: **mitigado** / **parcial** / **aberto** / **latente** (só com 4755 ou policy ruim).

### 7.1 REPL + parser + expand + dispatcher + process

| STRIDE | Ameaça | Vetor | Sev | Mitigação | Status |
|--------|--------|-------|-----|-----------|--------|
| S | Spoof de “comando inocente” via alias/rc | `~/.petrushrc`, alias table | M | `rc_stat_ok` (SEC-10); alias é feature | parcial |
| T | Tamper argv / OOB parser | realloc argv | A | NEW-01 `finalize_argv` NULL terminator; fuzz T3 | mitigado |
| T | Overwrite de arquivo via `>` | redir | M | SEC-09 `O_EXCL` em `>`/`2>`; `>>` append | mitigado (append esperado) |
| R | Negar comando digitado | sem audit no REPL unpriv | B | history local; syslog só em `pudo` | aceito (unpriv) |
| I | History world-readable / symlink | `~/.petrush_history` | M | SEC-08 `O_NOFOLLOW` + mode 0600 | mitigado |
| I | Leak via complete/PATH walk | tab | B | custo local | aberto (baixo) |
| D | Glob explosion | `*` | M | `PETRUSH_GLOB_MAX` 256 fail-closed | mitigado |
| D | Source recursivo | `. a; . a` | M | depth max 8 | mitigado |
| E | Elevação via shell sozinho | N/A | - | petrush nunca setuid | N/A |

### 7.2 History bang + prompt

| STRIDE | Ameaça | Vetor | Sev | Mitigação | Status |
|--------|--------|-------|-----|-----------|--------|
| T/E | `!!` executa linha anterior indesejada (paste) | `hist_expand.c` | B | ecoa expansão (como bash); sem `!$` legado amplo | aberto (UX) |
| E | RCE via prompt | PS1 | A | só escapes fixos; sem cmdsubst | mitigado (ausência) |

### 7.3 Rc / source

| STRIDE | Ameaça | Vetor | Sev | Mitigação | Status |
|--------|--------|-------|-----|-----------|--------|
| T | Plantar rc gravável por outros | mode/uid | A | `petrush_rc_stat_ok` | mitigado |
| E | Código arbitrário no boot | linhas do rc | M | confiança no dono do home (esperado) | residual |
| S | `source` via PATH | busca PATH | A | path explícito; sem PATH | mitigado |

### 7.4 `pudo` frontend (Boundary A discovery + B)

| STRIDE | Ameaça | Vetor | Sev | Ref mapa | Status |
|--------|--------|-------|-----|----------|--------|
| S | Spoof do binário `pudod` | PATH / cwd / sibling | A | sudo-pudo §3.17 / SEC-02 | mitigado em Release (install abs); Debug ainda relativos |
| S | `execvp("sudo")` PATH spoof | fallback B | A | AUD-SEC-I1 | aberto (residual) |
| T | Mutar env do pai | `unsetenv` | A | SEC-01 no `builtin_pudo` | mitigado; API teste ainda muta |
| I | Propagar `TZ`/`TMPDIR`/`HOME` ao sudo | clean_envp keep-list | M | pudod descarta; B parcial | parcial em B |
| D | Trunc argv silencioso | buffers 128 | M | SEC-04 fail closed | mitigado |
| E | Fallback reabre sudoers/tickets/PAM | Boundary B | C | sudo-pudo classes 2,7,11-14,20 | **latente/aberto** se helper some |
| E | Client allow vazia = não filtra | `pudo.conf` ausente | M | autoridade = pudod; B piora | parcial |

### 7.5 `pudod` (Boundary A; privilégio latente)

| STRIDE | Ameaça | Vetor | Sev | Ref | Status código | Nota 4755 |
|--------|--------|-------|-----|-----|---------------|-----------|
| S | Caller forja identidade | N/A | B | getuid no log | N/A | euid root ≠ identidade |
| T | Allow-list adulterada | `/etc/petrush/pudo.allow` | C | root-owned + fstat no load | mitigado se perms corretas | |
| T | TOCTOU path check→exec | symlink race | A | SEC-07 `O_NOFOLLOW` + `fexecve` | mitigado Linux; residual non-Linux | |
| T | Literal na allow se `realpath` falha | load | A | SEC-05 skip literal | mitigado | |
| R | Negar exec privilegiado | sem log | A | syslog + uid real | mitigado; trunc `LOG_BUF_SIZE` | |
| I | Env poisoning LD_*/BASH_ENV | envp | C | unset + `build_clean_envp` hardcoded | mitigado | |
| D | argc overflow / DoS | argv longo | M | SEC-04 | mitigado | |
| E | Comando “inocente” vira shell admin | allow larga (`/bin/sh`, apt…) | C | SEC-03 example mínimo; policy humana | **latente** | **pior com 4755** |
| E | Alvo não-root / não-exec | binário user | A | SEC-06 `st_uid==0` + exec | mitigado | |
| E | Features estilo sudo `-h`/`-R`/`-u`/`sudoedit` | N/A em A | - | mapa: NÃO em A; B bloqueia flags via `--` | N/A em A | |

**Composição crítica (shells-seguranca §12 + sudo-pudo §3.18):** allow-listar um shell genérico (`/bin/sh`, bash, busybox) no `pudod` **reabre o modelo POSIX completo já como root**. Isso não é bug do parser do petrush; é falha de política.

---

## 8. Abuse cases (defensivos, sem PoC)

1. **Atacante same-uid planta `~/.petrushrc` gravável por group** → SEC-10 recusa; residual se o próprio user for comprometido (esperado).
2. **Symlink no path do history** → SEC-08 `O_NOFOLLOW` falha o save em vez de seguir.
3. **`>` sobre arquivo existente** → SEC-09 `O_EXCL` falha (noclobber default).
4. **Operador aplica `chmod 4755` com allow-list contendo `/usr/bin/apt`** → elevação ampla (classe “comando inocente”); **não endossado**.
5. **`pudod` ausente no PATH de install** → fallback Boundary B; superfície passa a ser a do sudo do host (tickets, pwfeedback, env_reset…).
6. **Paste de `!!` em sessão compartilhada** → reexecuta última linha do history (abuso UX, não priv-esc).
7. **Allow-list com `/bin/sh`** → filho root herda shell completo (IFS, scripts, redirecionamentos).

---

## 9. Riscos residuais (top, para AUD-REPORT)

| # | Risco | Sev | Fronteira | Ligação TODO / AUD |
|---|-------|-----|-----------|--------------------|
| 1 | Setuid `4755` sem endosso | C | A | AUD-SEC-C1; `DEPLOY_CHECKLIST` |
| 2 | Fallback `sudo` + `execvp` | C/A | B | AUD-SEC-C2 / I1; sudo-pudo meta-B |
| 3 | Allow-list larga / shell na lista | C | A policy | SEC-03 posture; shells §12 |
| 4 | Docs de memória desatualizados vs SEC-* | M | processo | AUD-SEC-I4 |
| 5 | History bang / alias / source no uid do user | M/B | TB1 | shells-seguranca classes SIM |
| 6 | Jobs `&` / TTY races | M | TB1 | UX-23; monitorar orphan/SIGTSTP |
| 7 | Fortify parcial / symbols | B | binário | AUD-SEC-I6 |

---

## 10. Premissas

1. O binário interativo **nunca** é instalado setuid.
2. `pudod` só ganha 4755 por ação **manual** do líder após revisão (hoje: **não endossado**).
3. `/etc/petrush/pudo.allow` permanece root-owned, sem write group/other, com entradas mínimas (espelho do example).
4. O operador controla o próprio home; ameaça principal same-uid / home frágil, não RCE remoto.
5. Host sudo, se usado no fallback, está sob responsabilidade da distro/admin (fora do contrato mínimo do helper).

## 11. Fora de escopo desta AUD-DISC

- PoC, fuzz ofensivo, bypass recipes
- Aplicar setuid/setcap neste host
- AUD-QUALITY / COV / DEPS / LANG (ondas irmãs)
- Atualizar o texto drift de `shells-seguranca.md` / `sudo-pudo-riscos.md` (recomendado em DOC / AUD-REPORT)
- Desenho Threat Dragon binário (DFD textual basta no porte early)

---

## 12. Veredito de descoberta

| Pergunta | Resposta |
|----------|----------|
| Superfície mapeada? | **Sim** (REPL unpriv + Boundary A/B) |
| Ativos e boundaries claros? | **Sim** |
| STRIDE cobrindo parser/process/pudo/history/rc? | **Sim** (resumido) |
| Pronto para consolidar em AUD-REPORT? | **Sim**, após fechar as outras AUD-* |
| Endosso setuid? | **Não** |
| Drift documental? | **Sim** (memórias W2 vs código SEC-*); não invalida o mapa de **classes** |

**Score de maturidade de modelagem (descoberta):** **82/100**  
(desconto: Threat Dragon não gerado; drift das memórias W2; jobs/`&` ainda em evolução de superfície).

---

## 13. Entregáveis e próximos passos

1. Este arquivo: `docs/auditoria/aud-disc.md` (AUD-DISC).
2. TODO: status `🔍 Pendente verificação` (impl do artefato; ✅ só pós verificação humana/AUD-REPORT).
3. Recomendações de follow-up (não feitas aqui):
   - Sincronizar `shells-seguranca.md` / `sudo-pudo-riscos.md` com SEC-03..10 (AUD-SEC-I4).
   - Decidir policy de Boundary B: fail closed sem pudod em Release vs manter sudo lab.
   - Manter allow-list mínima; nunca shells genéricos.

**Citar:** AUD-DISC.
