# Riscos sudo → pudo / pudod (mapa defensivo)

**Data:** 2026-08-22T06:52:35-03:00 (`22/08/26 - 06:52:35`)  
**HEAD:** `1fdbfbfe8e9000ef490bf531f898340704fa4210` (`refs/heads/main`)  
**Autor do mapa:** security-engineer (defensivo-only)  
**Escopo de escrita:** somente este arquivo. Sem PoC, sem payload, sem patch de código.

## Aviso defensivo (leia antes)

Este documento **classifica classes de risco** e aponta **onde o petrush já mitiga, mitiga pela metade ou reintroduz** superfície herdada do sudo do host. Serve para hardening, triagem SEC-01..05 e decisão do líder sobre **não** aplicar `chmod 4755` no `pudod` sem revisão.

- **Não** é receita de exploração.
- **Não** endossa instalar o `pudod` setuid. O bit 4755 só existe se o líder aplicar manualmente; o projeto **não** o recomenda como default.
- Boundary **A** = caminho privilegiado do `pudod` (`src/pudod/pudod.c`), só relevante se o binário estiver setuid/setcap.
- Boundary **B** = fallback `execve("/usr/bin/sudo")` em `src/mid/pudo.c` (~L470-491), que **reintroduz** as classes do sudo instalado no host (versão, sudoers, Defaults).
- Vereditos: **SIM** = CWE equivalente vivo naquele boundary; **NÃO** = feature/classe inexistente no `pudod` (ou bloqueada de forma estrutural no fallback); **PARCIAL** = guard incompleto, só no fallback, ou mitigação com lacuna documentada.

### História curta do sudo (contexto, não atribuição errada)

- Sudo nasceu ~**1980** em SUNY/Buffalo (Bob Coggeshall e Cliff Spencer; depois Phil Betchel et al. no net.sources 1985). Fonte: [sudo.ws/history](https://www.sudo.ws/history.html).
- **Todd C. Miller** mantém o sudo desde o release público “CU sudo” **1994** (não é o autor original). Continua maintainer em sudo.ws.

---

## 1. Resumo executivo

| # | Classe | A (pudod) | B (fallback sudo) | Gap SEC-* | CWE |
|---|--------|-----------|-------------------|-----------|-----|
| 1 | argv unescape overflow (CVE-2021-3156) | NÃO | NÃO* | - | CWE-122 / CWE-787 |
| 2 | prompt heap | NÃO | SIM | - | CWE-122 |
| 3 | env poisoning (LD_*/BASH_ENV/TZ) | NÃO† | PARCIAL | SEC-01 | CWE-426 / CWE-15 |
| 4 | LD_PRELOAD em setuid | NÃO | PARCIAL | SEC-01 | CWE-426 |
| 5 | hostname `-h` (CVE-2025-32462) | NÃO | NÃO* | - | CWE-863 |
| 6 | chroot / nsswitch (CVE-2025-32463) | NÃO | NÃO* | - | CWE-22 / CWE-427 |
| 7 | sudoers parse | NÃO | SIM | - | CWE-20 / CWE-863 |
| 8 | UID -1 / `#-1` (CVE-2019-14287) | NÃO | NÃO* | - | CWE-266 / CWE-269 |
| 9 | sudoedit TOCTOU / editor `--` (CVE-2023-22809) | NÃO | NÃO* | - | CWE-367 / CWE-88 |
| 10 | path TOCTOU (check→exec) | PARCIAL | SIM | - | CWE-367 |
| 11 | TTY `/proc` (CVE-2017-1000367/8) | NÃO | SIM | - | CWE-119 / CWE-59 |
| 12 | pwfeedback (CVE-2019-18634) | NÃO | SIM | - | CWE-121 / CWE-120 |
| 13 | timestamp tickets | NÃO | SIM | - | CWE-613 / CWE-287 |
| 14 | NOEXEC bypass | NÃO | SIM | - | CWE-78 / CWE-426 |
| 15 | trunc argv silencioso | SIM | SIM | **SEC-04** | CWE-20 / CWE-755 |
| 16 | allow-list `realpath` fail-open | SIM | NÃO | **SEC-05** | CWE-59 / CWE-754 |
| 17 | PATH spoof do helper | NÃO‡ | PARCIAL | **SEC-02** | CWE-426 / CWE-427 |
| 18 | comando “inocente” vira shell/admin | SIM | SIM | **SEC-03** | CWE-250 / CWE-78 |
| 19 | UAF | NÃO | SIM | - | CWE-416 |
| 20 | fallback sudo (classe meta-B) | NÃO | SIM | - | CWE-829 / CWE-1104 |

\* No fallback, `pudo` monta `sudo -- <argv do usuário>` (`src/mid/pudo.c:479-484`). Isso **impede** injetar flags do sudo (`-h`, `-R`, `-u`, `-e`/`sudoedit`, `-s`). CVEs **só** ativadas por essas flags **não** entram pelo boundary B via `pudo`. Ainda assim o sudo do host continua parseando sudoers, env, tickets, tty, etc.  
† Filho do `pudod` recebe `build_clean_envp()` hardcoded (`pudod.c:173-189`); LD_* unset no próprio helper (`pudod.c:238-249`).  
‡ Spoof de **qual** helper sobe está no frontend (`find_pudod_binary`, SEC-02), não dentro do `main` do `pudod` já em execução. Sibling de `/proc/self/exe` é o vetor SEC-02 mais relevante.

**Leitura rápida:** sob A (sem 4755) o risco privilegiado do `pudod` é **latente**. Com 4755, os buracos vivos em A hoje são sobretudo **SEC-04/05/03** (+ TOCTOU residual). Sob B, qualquer host com sudo vulnerável ou sudoers largo **reabre** classes históricas mesmo com `--`.

---

## 2. STRIDE delta vs audit 2026-07-01

Baseline: [`docs/security/pudo-audit.md`](../security/pudo-audit.md) (2026-07-01).

| STRIDE | Audit 2026-07-01 | Delta 2026-08-22 (este mapa) |
|--------|------------------|------------------------------|
| **S**poofing | Helper spoof via PATH; exigir path absoluto hardcoded | **Aberto (SEC-02):** ordem real de `find_pudod_binary` (`pudo.c:360-418`): (1) irmão de `/proc/self/exe`, (2) `build/pudod` relativos, (3) libexec/install, (4) `access("pudod")` por último. Sibling é o vetor SEC-02 mais relevante. Fallback B usa `/usr/bin/sudo` depois `execvp("sudo")` (`pudo.c:487-489`) → PATH spoof no 2º exec. |
| **T**ampering | Allow-list root-owned; realpath no load | **Aberto (SEC-05):** se `realpath` da entrada falha, o literal entra na lista (`pudod.c:142-148`). Fail-open = higiene + janela ENOENT no mesmo `main()` (não “literal que depois resolve para outro inode”: o `strcmp` usa `realpath` do `user_cmd`). Exemplo versionado ainda lista apt/dnf/systemctl/passwd (`pudo.allow.example` L12-19) → **SEC-03**. |
| **R**epudiation | syslog + uid real | Sem mudança material. Trunc de log (`LOG_BUF_SIZE` / `buf[512]`) ainda pode cortar argv longo. |
| **I**nfo disclosure | Sanitização; mutação de env no pai era crítica | **SEC-01 em 🔍:** `builtin_pudo` **não** chama mais `unsetenv` no pai (comentário L108); teste `test_pudo_builtin_does_not_mutate_parent_env` cobre. Função `pudo_sanitize_environment` **ainda muta** se chamada (API de teste). Frontend `build_clean_envp` **propaga** `TZ`/`TMPDIR`/`HOME` do pai para o filho (`pudo.c:327-341`) - relevante no B. |
| **D**oS | Limites de args | **SEC-04 aberto:** trunc silencioso em `pudod_argv[128]` / `sudo_argv[128]` / `MAX_ARGS` sem erro ao usuário. |
| **E**levation | Revalidação no pudod; risco setuid | **Sem endosso 4755.** CVEs 2025 host/chroot documentadas; B bloqueia flags via `--`, mas reintroduz sudoers/tickets/env do host. Classe “comando inocente” (apt → shell) permanece se allow-list larga. |

**Novos desde a audit (externos):** CVE-2025-32462 (`-h` host), CVE-2025-32463 (`-R` chroot/nsswitch). Relevância petrush: **A NÃO** (sem essas features); **B NÃO*** para ativação via flags do `pudo`, **SIM** se o operador chamar sudo fora do `pudo` ou se o host sudo tiver outros bugs sem flag.

---

## 3. Classes (uma subseção cada)

### 3.1 argv unescape overflow (CVE-2021-3156 Baron Samedit)

| | |
|--|--|
| **CWE** | CWE-122 / CWE-787 |
| **Fonte** | [sudo unescape_overflow](https://www.sudo.ws/security/advisories/unescape_overflow/), Qualys Baron Samedit |
| **Mecanismo (defensivo)** | Modo shell/`sudoedit -s` + unescape de argv em heap além do tamanho alocado. |
| **A** | **NÃO** - `pudod` não tem shell mode, sudoedit nem unescape. Protocolo: argv cru + `execve`/`fexecve`. |
| **B** | **NÃO*** - fallback não invoca `sudoedit` nem passa `-s` como opção do sudo (`--` antes dos args). Host sudo vulnerável ainda é risco **fora** deste caminho. |
| **SEC-*** | - |

### 3.2 prompt heap

| | |
|--|--|
| **CWE** | CWE-122 |
| **Fonte** | Família de bugs de heap no caminho de autenticação/prompt do sudo (adjacente a overflows de entrada de senha; ver também pwfeedback §3.12). |
| **A** | **NÃO** - `pudod` não pede senha, não tem prompt, não usa PAM. |
| **B** | **SIM** - fallback delega auth ao sudo do host (prompt/PAM/ticket). Qualquer bug de heap no prompt do sudo volta. |
| **SEC-*** | - |

### 3.3 env poisoning (LD_*, BASH_ENV, TZ, …)

| | |
|--|--|
| **CWE** | CWE-426 / CWE-15 |
| **Fonte** | Design clássico sudo/setuid; audit 2026-07-01; SEC-01 |
| **A** | **NÃO** (filho) - `build_clean_envp()` fixo sem LD_*/BASH_ENV/TZ (`pudod.c:180-186`); unset de lista perigosa no helper (`pudod.c:238-249`). Decisões de policy **não** leem env. |
| **B** | **PARCIAL** - `build_clean_envp` do frontend **copia** `TZ`, `TMPDIR`, `HOME`, `PATH`, etc. do processo pai (`pudo.c:327-341`) para o `execve` do sudo. Sudo aplica `env_reset`/`env_check` conforme sudoers; não é envp mínimo absoluto. |
| **SEC-*** | **SEC-01** (mutação no pai: mitigada no fluxo `builtin_pudo`; API `pudo_sanitize_environment` ainda unsetenv se usada). |

### 3.4 LD_PRELOAD em binário setuid

| | |
|--|--|
| **CWE** | CWE-426 |
| **Fonte** | Comportamento do dynamic linker em executáveis setuid; lista LD_* no pudod |
| **A** | **NÃO** - kernel ignora LD_PRELOAD em setuid; helper ainda faz unset + envp limpo no filho. |
| **B** | **PARCIAL** - depende do sudo/sudoers (`env_reset`, `env_delete`). Frontend não coloca LD_PRELOAD no clean_envp (só keep-list), o que ajuda; `execvp("sudo")` herda complexidade do host. |
| **SEC-*** | SEC-01 (higiene de env no pai/filho). |

### 3.5 hostname `-h` (CVE-2025-32462)

| | |
|--|--|
| **CWE** | CWE-863 |
| **Fonte** | [host_any](https://www.sudo.ws/security/advisories/host_any/), openwall oss-security 2025-06-30, Stratascale |
| **A** | **NÃO** - sem opção `-h` / host matching estilo sudoers. |
| **B** | **NÃO*** - `--` faz com que `-h` do usuário seja **comando**, não opção do sudo. |
| **SEC-*** | - |

### 3.6 chroot / nsswitch (CVE-2025-32463)

| | |
|--|--|
| **CWE** | CWE-22 / CWE-427 |
| **Fonte** | [chroot_bug](https://www.sudo.ws/security/advisories/chroot_bug/) (sudo 1.9.14-1.9.17; fix 1.9.17p1) |
| **A** | **NÃO** - sem `-R`/chroot; sem carregar nsswitch do usuário. |
| **B** | **NÃO*** - `--` bloqueia `-R` como opção. |
| **SEC-*** | - |

### 3.7 sudoers parse

| | |
|--|--|
| **CWE** | CWE-20 / CWE-863 |
| **Fonte** | Superfície histórica do parser sudoers (gramática, includes, Defaults, LDAP) |
| **A** | **NÃO** - policy = lista de paths absolutos + `strcmp` após `realpath` (`pudod.c:158-168`). Sem gramática sudoers. |
| **B** | **SIM** - 100% da decisão privilegiada no fallback é sudoers do host. |
| **SEC-*** | - (policy A coberta por SEC-03/05 na allow-list). |

### 3.8 UID -1 / `#-1` (CVE-2019-14287)

| | |
|--|--|
| **CWE** | CWE-266 / CWE-269 |
| **Fonte** | [minus_1_uid](https://www.sudo.ws/security/advisories/minus_1_uid/) |
| **A** | **NÃO** - sem `-u` / Runas; sempre euid root do setuid para o filho allow-listed. |
| **B** | **NÃO*** - `--` impede `-u#-1` como opção do sudo. |
| **SEC-*** | - |

### 3.9 sudoedit TOCTOU / editor com `--` (CVE-2023-22809)

| | |
|--|--|
| **CWE** | CWE-367 / CWE-88 |
| **Fonte** | [sudoedit_any](https://www.sudo.ws/security/advisories/sudoedit_any/) |
| **A** | **NÃO** - sem modo edit; não escolhe editor via `SUDO_EDITOR`/`VISUAL`. |
| **B** | **NÃO*** - não chama `sudoedit`; `-e` após `--` não é flag do sudo. |
| **SEC-*** | - |

### 3.10 path TOCTOU (check → exec)

| | |
|--|--|
| **CWE** | CWE-367 |
| **Fonte** | Audit 2026-07-01; padrão clássico realpath/stat/exec |
| **A** | **PARCIAL** - Linux: `realpath` → `open` → `fstat` → `fexecve(fd, …)` (`pudod.c:266-315`) reduz janela. Fallback não-Linux fecha fd e `execve(path)`. Entradas da allow-list canonicalizadas **no load** (tempo ≠ exec). |
| **B** | **SIM** - superfície TOCTOU do sudo do host (e `access`→`exec` no discovery de paths no frontend). |
| **SEC-*** | Relaciona-se a SEC-05 (canonicalização inconsistente). |

### 3.11 TTY `/proc` (CVE-2017-1000367 / 1000368)

| | |
|--|--|
| **CWE** | CWE-119 / CWE-59 |
| **Fonte** | [linux_tty](https://www.sudo.ws/security/advisories/linux_tty/) |
| **A** | **NÃO** - não parseia `/proc/self/stat` para tty device. |
| **B** | **SIM** - se o sudo do host for antigo/vulnerável e SELinux path aplicável. |
| **SEC-*** | - |

### 3.12 pwfeedback (CVE-2019-18634)

| | |
|--|--|
| **CWE** | CWE-121 / CWE-120 |
| **Fonte** | [pwfeedback](https://www.sudo.ws/security/advisories/pwfeedback/) |
| **A** | **NÃO** - sem leitura de senha / pwfeedback. |
| **B** | **SIM** - se `Defaults pwfeedback` no sudoers do host (comum em alguns spins). |
| **SEC-*** | - |

### 3.13 timestamp tickets

| | |
|--|--|
| **CWE** | CWE-613 / CWE-287 |
| **Fonte** | Modelo de credencial cached do sudo (`~/.sudo_as_admin_successful` / tty tickets / timestamp dir) |
| **A** | **NÃO** - sem cache de autenticação; cada exec revalida allow-list. |
| **B** | **SIM** - tickets do sudo do host aplicam-se ao fallback. |
| **SEC-*** | - |

### 3.14 NOEXEC bypass

| | |
|--|--|
| **CWE** | CWE-78 / CWE-426 |
| **Fonte** | [noexec_bypass](https://www.sudo.ws/security/advisories/noexec_bypass/) (CVE-2016-7032 e sucessores) |
| **A** | **NÃO** - sem tag NOEXEC; política é “executar ou negar”, não “executar sem exec filhos”. |
| **B** | **SIM** - se sudoers usar `NOEXEC` / `noexec`, bugs históricos dessa feature aplicam. |
| **SEC-*** | - |

### 3.15 trunc argv silencioso (SEC-04)

| | |
|--|--|
| **CWE** | CWE-20 / CWE-755 |
| **Fonte** | TODO SEC-04; código |
| **A** | **SIM** - `build_child_argv` para em `MAX_ARGS` sem falhar (`pudod.c:204-206`). Args extras somem. |
| **B** | **SIM** - `sudo_argv[128]` / loop `sidx < 126` (`pudo.c:477-484`); mesmo padrão no path pudod frontend (`pudod_argv[128]`, `idx < 126`, L501-509). |
| **SEC-*** | **SEC-04** (fail closed se exceder). |

### 3.16 allow-list `realpath` fail-open (SEC-05)

| | |
|--|--|
| **CWE** | CWE-59 / CWE-754 |
| **Fonte** | TODO SEC-05; `pudod.c:142-148` |
| **A** | **SIM** - se `realpath` da entrada falha no load, o literal entra na lista (`pudod.c:142-148`). Fail-open = higiene + janela ENOENT no mesmo `main()` (entrada que ainda não existe no load). **Não** é “literal que depois resolve para outro inode”: na checagem, `is_allowed` compara contra o `realpath` do `user_cmd` (`pudod.c:158-168`, `266-276`). Veredito permanece **SIM**. |
| **B** | **NÃO** - sudo não usa este loader; classe é do pudod. |
| **SEC-*** | **SEC-05** |

### 3.17 PATH spoof do helper (SEC-02)

| | |
|--|--|
| **CWE** | CWE-426 / CWE-427 |
| **Fonte** | TODO SEC-02; `find_pudod_binary` |
| **A** | **NÃO** (dentro do pudod) - o helper não se localiza via PATH. |
| **B** | **PARCIAL** - 1º `execve("/usr/bin/sudo")`; se falhar, `execvp("sudo")` (`pudo.c:487-489`). Antes de A ou B, `find_pudod_binary` tenta nesta ordem (`pudo.c:360-418`): (1) irmão de `/proc/self/exe`, (2) `build/pudod` relativos, (3) libexec/install, (4) `access("pudod")` por último. Sibling é o vetor SEC-02 mais relevante → **SEC-02**. |
| **SEC-*** | **SEC-02** |

### 3.18 comando “inocente” vira shell / admin (SEC-03)

| | |
|--|--|
| **CWE** | CWE-250 / CWE-78 |
| **Fonte** | TODO SEC-03; design `docs/design/pudo.md`; `pudo.allow.example` |
| **A** | **SIM** (se allow-list larga) - exemplo versionado inclui `/usr/bin/apt` (L12), `apt-get`, `dnf`, `systemctl`, `/bin/cat`, `passwd` (`src/pudod/pudo.allow.example` L12-19). Vários desses permitem escape para shell ou alteração de trust. |
| **B** | **SIM** - equivalente via sudoers `ALL` ou comandos poderosos. |
| **SEC-*** | **SEC-03** (encolher exemplo a id/whoami/true). |

### 3.19 UAF

| | |
|--|--|
| **CWE** | CWE-416 |
| **Fonte** | Ex.: CVE-2023-27320 double-free com regra `CHROOT=` no sudoers ([double_free](https://www.sudo.ws/security/advisories/double_free/)); histórico sudo |
| **A** | **NÃO** (pelo desenho atual) - buffers estáticos, sem free de policy objects no hot path; não há feature chroot. (UAF futuro exigiria regressão de código; não há evidência no `pudod.c` atual.) |
| **B** | **SIM** - CVE-2023-27320 dispara por regra `CHROOT=` no sudoers (não por `-R` na linha de comando). O separador `--` do fallback **não** bloqueia essa classe. Demais UAF/double-free do sudo do host também aplicam. |
| **SEC-*** | - |

### 3.20 fallback sudo (classe meta do boundary B)

| | |
|--|--|
| **CWE** | CWE-829 / CWE-1104 |
| **Fonte** | `src/mid/pudo.c:470-491`; design Fase 0 |
| **A** | **NÃO** - se `pudod` foi encontrado, este branch não corre. |
| **B** | **SIM** - ausência do helper **reintroduz** deliberadamente o sudo do sistema (aviso em stderr). Toda a matriz de CVEs/Defaults/sudoers do host volta ao jogo, filtrada só pelo `--` e pelo clean_envp parcial. |
| **SEC-*** | Transversal; endurecer discovery (SEC-02) reduz queda acidental neste branch. |

---

## 4. Matriz CVE / classe → SEC-01..05

| CVE / classe | SEC-01 env pai | SEC-02 path helper | SEC-03 allow example | SEC-04 trunc argv | SEC-05 realpath fail-open |
|--------------|----------------|--------------------|----------------------|-------------------|---------------------------|
| CVE-2021-3156 unescape | - | - | - | - | - |
| prompt heap | - | - | - | - | - |
| env LD_*/BASH_ENV/TZ | **mapa** | - | - | - | - |
| LD_PRELOAD setuid | **mapa** | - | - | - | - |
| CVE-2025-32462 host `-h` | - | - | - | - | - |
| CVE-2025-32463 chroot | - | - | - | - | - |
| sudoers parse | - | - | **mapa** (política) | - | **mapa** (policy load) |
| CVE-2019-14287 UID -1 | - | - | - | - | - |
| CVE-2023-22809 sudoedit | - | - | - | - | - |
| path TOCTOU | - | parcial (access→exec) | - | - | **mapa** |
| CVE-2017-1000367 tty | - | - | - | - | - |
| CVE-2019-18634 pwfeedback | - | - | - | - | - |
| timestamp tickets | - | - | - | - | - |
| NOEXEC bypass | - | - | **mapa** | - | - |
| trunc argv | - | - | - | **dono** | - |
| realpath fail-open | - | - | - | - | **dono** |
| PATH spoof helper | - | **dono** | - | - | - |
| comando→shell | - | - | **dono** | - | - |
| UAF (sudo) | - | - | - | - | - |
| fallback sudo (B) | parcial | **mapa** | - | **mapa** | - |

**Nota:** SEC-01..05 **já existem** no `TODO.md`. Este mapa **não edita** o `TODO.md`. Residuais sugeridos (ainda sem linha na tabela): ver §Fecho (SEC-06..10).

Status lido em 2026-08-22 (não editado):

| ID | Status na tabela | Papel neste mapa |
|----|------------------|------------------|
| SEC-01 | 🔍 Pendente verificação | Env: pai não deve mutar; filho clean |
| SEC-02 | ⏳ Pendente | Path absoluto do pudod em release |
| SEC-03 | ⏳ Pendente | Exemplo allow-list mínimo |
| SEC-04 | ⏳ Pendente | Fail closed em overflow de argc |
| SEC-05 | ⏳ Pendente | Negar entrada se realpath falhar |

---

## 5. Classes N/A por design (no pudod)

Features do sudo **ausentes de propósito** no boundary A (por isso NÃO na tabela executiva):

- Parser sudoers / LDAP / SSSD  
- `sudoedit` / `-e`  
- Runas `-u` / `-g` / `#uid`  
- Host `-h` / list `-l` remoto  
- Chroot `-R` / per-command chroot  
- Shell mode `-s` / login shell `-i`  
- Prompt de senha / PAM / askpass  
- Timestamp tickets / lecture  
- `NOEXEC` / `sudo_noexec.so`  
- SELinux role/type via `sesh`  
- Defaults (`pwfeedback`, `env_keep` amplo, etc.)

Isso é **redução de superfície**, não prova de ausência de bug no pouco que existe (SEC-04/05/03 + TOCTOU residual).

---

## 6. Fontes + crawl log

### Fontes semente (hop 0)

| URL | Uso |
|-----|-----|
| https://www.sudo.ws/security/advisories/ | Índice de advisories |
| https://www.sudo.ws/history.html | Origem 1980 SUNY/Buffalo; Todd Miller desde 1994 |
| https://www.sudo.ws/security/advisories/unescape_overflow/ | CVE-2021-3156 |
| https://www.sudo.ws/security/advisories/host_any/ | CVE-2025-32462 |
| https://www.sudo.ws/security/advisories/chroot_bug/ | CVE-2025-32463 |
| https://www.sudo.ws/security/advisories/sudoedit_any/ | CVE-2023-22809 |
| https://www.sudo.ws/security/advisories/minus_1_uid/ | CVE-2019-14287 |
| https://www.sudo.ws/security/advisories/pwfeedback/ | CVE-2019-18634 |
| https://www.sudo.ws/security/advisories/linux_tty/ | CVE-2017-1000367/8 |
| https://www.sudo.ws/security/advisories/noexec_bypass/ | CVE-2016-7032 |
| https://www.sudo.ws/security/advisories/double_free/ | CVE-2023-27320 (UAF/double-free) |
| Repo local | `docs/security/pudo-audit.md`, `docs/design/pudo.md`, `src/mid/pudo.c`, `src/pudod/pudod.c`, `src/pudod/pudo.allow.example`, `include/petrush/pudo.h`, `tests/test_pudo.c`, linhas SEC-01..05 do `TODO.md` (leitura) |

### Crawl (≤5 hops a partir de advisories)

| Hop | De | Para | Resultado |
|-----|----|------|-----------|
| 1 | unescape_overflow | https://blog.qualys.com/.../cve-2021-3156-...baron-samedit | Mecânica heap/unescape confirmada (sem PoC reproduzido aqui) |
| 1 | host_any / chroot_bug | Stratascale advisory URLs (citadas no sudo.ws) | Confirma LPE host/chroot 2025; fetch parcial HTML |
| 1 | linux_tty | openwall lists (citado no advisory) | Referência Qualys 2017 |
| 1 | índice advisories | `/security/advisories/page/2/` | Continuação do índice (double_free etc.) |
| 1-2 | CVE-2025-32462 | https://www.openwall.com/lists/oss-security/2025/06/30/2 | Título/thread LPE via host option |
| 2 | NVD search “sudo” | nvd.nist.gov search | UI sem extrato útil nesta sessão; CVEs âncora ficaram no sudo.ws |

**Limite:** não se avançou além de 2 hops efetivos com conteúdo útil; teto 5 não foi estourado.

### Código âncora (arquivo:linha)

| Trecho | Onde |
|--------|------|
| Fallback sudo + `--` | `src/mid/pudo.c:470-491` |
| `find_pudod_binary` / SEC-02 | `src/mid/pudo.c:360-418` |
| `build_clean_envp` frontend (keep TZ/TMPDIR) | `src/mid/pudo.c:320-351` |
| Trunc argv frontend | `src/mid/pudo.c:477-484`, `501-509` |
| Allow-list realpath fail-open / SEC-05 | `src/pudod/pudod.c:142-148` |
| Env limpo + fexecve | `src/pudod/pudod.c:173-189`, `238-249`, `307-315` |
| Trunc child argv / SEC-04 | `src/pudod/pudod.c:204-206` (`MAX_ARGS` 128) |
| Exemplo allow largo / SEC-03 | `src/pudod/pudo.allow.example` L12-19 (apt na L12) |
| Alvo sem `st_uid==0` (residual) | `src/pudod/pudod.c:295-297` (comentário; check ausente) |

---

## 7. O que não foi feito

- **Não** se editou `TODO.md`, `src/`, configs, CMake, testes.  
- **Não** se aplicou nem sugeriu comando de `chmod 4755` / `setcap` como passo a executar.  
- **Não** se escreveu PoC, payload, exploit, nem passos de reprodução ofensiva.  
- **Não** se clonou repositórios alheios nem se copiou árvores de código de terceiros.  
- **Não** se auditou binário setuid instalado no sistema (fora do cwd / estado de deploy).  
- **Não** se fez SCA/SAST automatizado (semgrep/gitleaks) nesta fatia: mapa é revisão dirigida + advisories.  
- NVD UI não devolveu extrato estruturado nesta sessão; CVEs 2024-2026 usadas vieram de sudo.ws + oss-security.

---

## Fecho (para o líder)

1. **Sem 4755:** boundary A é código morto privilegiado; o risco operacional dominante é **B** (sudo do host) + discovery frouxo do helper (**SEC-02**, com sibling de `/proc/self/exe` como vetor principal).  
2. **Com 4755 (só se você autorizar depois):** fechar **SEC-04**, **SEC-05**, **SEC-03** antes; reler `pudod.c` linha a linha; manter allow-list mínima root-owned.  
3. O separador `--` no fallback é **controle real** contra a família de CVEs por **flag** (32462/32463/14287/22809/3156 via sudoedit). **Não** cobre sudoers largo, tickets, pwfeedback, tty, UAF por regra `CHROOT=` (CVE-2023-27320), nem trunc/fail-open do próprio pudo/pudod.

### Residuais sugeridos (não gravados no TODO nesta fatia)

| ID sugerido | Classe | Âncora |
|-------------|--------|--------|
| **SEC-06** | Alvo sem exigir `st_uid==0` (aceita regular allow-listed) | `pudod.c:295-297` |
| **SEC-07** | TOCTOU `realpath` → `open` (mesmo com `fexecve` do fd) | `pudod.c:266-279` |
| **SEC-08** | History `fopen` segue symlink | `linenoiseHistorySave` / `shells-seguranca.md` |
| **SEC-09** | noclobber ausente em dois sítios | `process.c:161` + `dispatcher.c:108` |
| **SEC-10** | `~/.petrushrc` sem checar uid/mode | `main.c` `load_rc_file` |

*Fim do mapa. Próxima ação de produto (se houver) fica a cargo do orquestrador / TODO existente. Este mapa aponta residuals SEC-06..10; **não** edita o `TODO.md`.*
