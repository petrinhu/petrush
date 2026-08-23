# Threat Model: plugins `.so` de terceiro (PLG-NARC)

| Campo | Valor |
|-------|-------|
| **ID** | PLG-NARC |
| **Tipo Diátaxis** | Explanation (threat model) + Reference de controles obrigatórios |
| **Audience** | CISO, security-engineer, Foundation (PLG-LOAD), revisor de ABI |
| **Last-reviewed** | 2026-08-23 |
| **Owner** | security-engineer (Narciso / CISO) |
| **Pré-requisito** | PLG-ABI (`plugins/abi.h`, major=1) |
| **Sucessor** | PLG-LOAD (loader XDG + SHA-256); TST-PLG (ww/hash/ABI) |
| **ADR** | [`docs/adr/001-c23-cxx-asm-plugins.md`](../adr/001-c23-cxx-asm-plugins.md) D4 |
| **Classificação** | Alta (código de terceiro no mesmo processo do shell unpriv) |
| **SHA baseline** | `756969c` (header ABI + ADR; **sem** `dlopen` no tree) |

**Escopo desta fatia:** ameaças, trust boundaries e controles **obrigatórios** para o futuro loader.  
**Fora de escopo:** implementação de `dlopen`/`dlsym` (PLG-LOAD), PoC de exploit, setuid/`4755`, mudança de `pudod`.

> **Gate:** nenhum `dlopen` no processo `petrush` até PLG-LOAD consumir **este** documento. CRC-32 / FNV **não** autenticam `.so`.

---

## 1. Assets protegidos

| Asset | Por quê |
|-------|---------|
| Integridade do processo `petrush` (unpriv) | Plugin roda **no mesmo address space**; RCE no `.so` = RCE no shell do utilizador |
| Confiança do utilizador no REPL | Histórico, rc, env, cwd, fds abertos ficam visíveis ao plugin |
| Segredos em memória / env do shell | Tokens, `SSH_AUTH_SOCK`, variáveis de sessão |
| Isolamento do helper privilegiado `pudod` | Elevação **nunca** deve carregar código de terceiro |
| Contrato ABI C11 (`plugins/abi.h`) | Major estável; layout POD; sem C++ na fronteira |
| Disco sob path de plugin (XDG / `PETRUSH_PLUGIN_PATH`) | Substituição de `.so` = tampering |

## 2. Atores

| Ator | Capacidade modelada |
|------|---------------------|
| Utilizador legítimo | Instala/ativa plugin de confiança; edita allow-list local |
| Autor de plugin honesto | Compila `.so` ABI C11; erra em permissões ou path |
| Atacante local (mesmo uid) | Escreve em dirs world-writable; troca `.so` após hash; race TOCTOU |
| Atacante com escrita no home | Planta `.so` em `~/.local/...` se a allow-list for permissiva |
| Insider / supply chain | Entrega `.so` malicioso com hash “oficial” se a allow-list for atualizada sem review |
| Processo `pudod` (se setuid um dia) | **Não** é consumidor de plugin; qualquer carga aí seria Elevação crítica |

## 3. Trust boundaries

```text
[terceiro / disco]  --.so-->  [loader Foundation, unpriv]  --dlopen-->  [petrush process]
                              ^ allow-list + realpath + mode + SHA-256
[petrush unpriv]  --argv absoluto-->  [pudod]     *** pudod NÃO carrega plugin ***
[configsh C++23]  (binário separado; NÃO partilha ABI de plugin com o REPL)
```

| Boundary | Regra |
|----------|-------|
| Disco → loader | Só path canónico (`realpath`), allow-list hit, não world-writable (ficheiro **e** dirs), SHA-256 ok, ABI major compatível |
| Loader → plugin | Só símbolos / vtable de `plugins/abi.h`; sem passar ponteiros privilegiados; `opaque` do host é read-only para o plugin |
| petrush → pudod | Protocolo mínimo argv; **zero** `dlopen` no helper; plugin **nunca** é argumento nem path implícito do pudod |
| configsh ↔ petrush | Binários distintos; plugin ABI **não** é a ABI do `configsh` |

## 4. Data flow (DFD resumido)

1. Utilizador (ou rc futuro) pede carregar plugin `nome`.
2. Loader resolve candidatos sob XDG + `PETRUSH_PLUGIN_PATH` (PLG-LOAD).
3. Para cada candidato: `realpath` → walk de dirs + ficheiro → allow-list → SHA-256 → `dlopen` → `dlsym` query/init.
4. Comandos do plugin via `petrush_plugin_cmd` no processo unpriv.
5. `fini` + `dlclose` no unload.

**Não há** fluxo plugin → pudod. Se um plugin precisar de elevação, o desenho correcto é o **utilizador** invocar `pudo` (allow-list do helper), nunca o plugin pedir ao loader para elevar.

---

## 5. STRIDE por componente

### 5.1 Componente: Loader Foundation (PLG-LOAD, futuro)

| STRIDE | Ameaça | Mitigação obrigatória (PLG-NARC → PLG-LOAD) | Status |
|--------|--------|-----------------------------------------------|--------|
| Spoofing | `.so` com nome “oficial” noutro path | Allow-list por **path canónico** pós-`realpath` (não basename solto) | especificado |
| Spoofing | Symlink para ficheiro controlado pelo atacante | `realpath` + recusa se qualquer componente falhar (fail-closed, espelho SEC-05) | especificado |
| Tampering | Troca do `.so` entre hash e `dlopen` (TOCTOU) | Abrir o ficheiro, hashear o **fd**/conteúdo lido, `dlopen` do path só após checks; preferir padrão open→fstat→hash→load documentado em PLG-LOAD | especificado |
| Tampering | Ficheiro ou dir world-writable (`o+w`) | **Recusar** se o `.so` **ou qualquer diretório** do `realpath` tiver write para other (e preferir recusar group-write em dirs não sticky, ver §6) | especificado |
| Tampering | Usar `petrush_crc32` / FNV como “assinatura” | **Proibido.** Autenticação = **SHA-256** (digest completo) comparado à allow-list; CRC/FNV são checksums de dados, não autenticadores | especificado |
| Repudiation | Negar que um plugin foi carregado | Log estruturado (path canónico, SHA-256 truncado seguro, uid, resultado allow/deny) sem vazar segredos | especificado |
| Information disclosure | Plugin lê env/hist/fds | Assumido: plugin = código do mesmo trust que o utilizador; documentar; não passar secrets de pudod | residual aceite (unpriv) |
| Information disclosure | Erro de load vaza path interno / stack | Mensagem genérica ao utilizador; detalhe só em log local | especificado |
| DoS | `.so` enorme / init infinito | Limite de tamanho; timeout de init (PLG-LOAD); fail-closed | a detalhar em PLG-LOAD |
| Elevation | Plugin chama API que eleva | Loader **não** expõe handle do pudod; sem `setuid` no petrush; sem `4755` nesta fatia | especificado |

### 5.2 Componente: ABI / `.so` de terceiro (`plugins/abi.h`)

| STRIDE | Ameaça | Mitigação | Status |
|--------|--------|-----------|--------|
| Spoofing | Plugin mente major/minor em `query` | Host compara com `PETRUSH_PLUGIN_ABI_MAJOR`; major ≠ host → `PETRUSH_PLUGIN_ERR_ABI` e **não** `init` | PLG-ABI |
| Tampering | Vtable com ponteiros lixo / parcial | Validar ponteiros não-NULL antes de chamar; major/minor da vtable = do plugin | PLG-LOAD |
| Tampering | ABI C++ disfarçada (exceções, STL) | Header só POD C11; review PLG-ABI; ADR D4 MUST NOT | PLG-ABI / ADR |
| Repudiation | Plugin não identifica nome/versão | `query` obrigatório antes de `init`; logar `name`/`version` | especificado |
| Information disclosure | `cmd` ecoa argv sensível | Responsabilidade do autor; host não loga argv completo por default | residual |
| DoS | `cmd` bloqueia o REPL | Documentar; futuro: job/timeout (fora de PLG-NARC) | residual |
| Elevation | Plugin assume euid≠uid | petrush permanece unpriv; **proibido** carregar plugin sob euid 0 | especificado |

### 5.3 Componente: Allow-list de plugins

| STRIDE | Ameaça | Mitigação | Status |
|--------|--------|-----------|--------|
| Spoofing | Entrada por basename (`evil.so`) | Só path absoluto já `realpath`-ável; basename sozinho = reject | especificado |
| Tampering | Allow-list world-writable | Mesma disciplina do `pudo.allow`: dono = utilizador (ou root se system-wide), sem `o+w`; fail-closed se mode inseguro | especificado |
| Tampering | Hash fraco / truncado | SHA-256 hex completo (64 nibbles); comparação **constant-time** (`petrush_memeq_ct` quando disponível) | especificado |
| Repudiation | Edição silenciosa da lista | Log de deny/allow; opcional mtime/inode no log | especificado |
| DoS | Lista gigante | Limite de entradas (PLG-LOAD) | a detalhar |
| Elevation | Entrada aponta para path sob controlo alheio | Checks de mode no path completo (§6) + hash | especificado |

### 5.4 Componente: `pudod` (helper; **não** carrega plugin)

| STRIDE | Ameaça | Mitigação | Status |
|--------|--------|-----------|--------|
| Elevation | `pudod` faz `dlopen` de plugin “para o utilizador” | **MUST NOT.** `pudod` não liga libdl para plugins; não interpreta `PETRUSH_PLUGIN_PATH`; não recebe path de `.so` no protocolo | **obrigatório PLG-NARC** |
| Elevation | Plugin na allow-list do pudod (`/etc/petrush/pudo.allow`) | Operador: não listar loaders genéricos nem `petrush` setuid; SEC-12 já barra shells; documentar que `.so`/loaders não são alvos válidos | política |
| Tampering | Setuid `4755` + plugin no mesmo processo | petrush **não** é setuid; pudod **não** carrega plugin; este doc **não** endossa `4755` | gate humano intacto |
| Information disclosure | Plugin no petrush lê allow-list do pudod | Ficheiro `/etc` root-owned; plugin unpriv só lê se permissões permitirem (644 típico: paths, não secrets) | residual baixo |

### 5.5 Componente: Paths XDG / `PETRUSH_PLUGIN_PATH`

| STRIDE | Ameaça | Mitigação | Status |
|--------|--------|-----------|--------|
| Spoofing | `PETRUSH_PLUGIN_PATH` com `../` e dirs alheios | Cada elemento: absoluto ou resolvido; após `realpath` deve bater allow-list; relativo ambíguo = reject | especificado |
| Tampering | Dir de plugin `0777` | Recusa world-writable em **cada** dir do realpath (§6) | especificado |
| DoS | PATH com milhares de dirs | Cap de entradas / profundidade (PLG-LOAD) | a detalhar |
| Elevation | Path aponta para `/tmp` world-writable | Bloqueado pelo check `o+w` na cadeia | especificado |

---

## 6. Controlo: recusa world-writable (ficheiro e dirs do realpath)

Antes de qualquer `dlopen`, o loader **MUST**:

1. Resolver o path candidato com `realpath` (falha → **deny**, nunca aceitar o literal).
2. `lstat`/`fstat` do ficheiro final: recusar se não for ficheiro regular; recusar se `mode & S_IWOTH` (other writable).
3. Para **cada** diretório prefixo do path canónico (da raiz até ao parent do `.so`):
   - `stat` do dir;
   - recusar se `mode & S_IWOTH`;
   - recomendado: recusar `S_IWGRP` salvo sticky bit (`S_ISVTX`) ao estilo diretórios seguros (ex.: restrição tipo `secure_path` / auditoria de home partilhado).
4. Fail-secure: qualquer erro de `stat` no walk → **deny**.

**Por quê dirs e não só o ficheiro:** um dir `o+w` permite ao atacante local substituir o nome do `.so` (unlink+creat) ou plantar symlink mesmo que o ficheiro actual esteja `0644`.

Isto é independente do SHA-256: hash sem path integrity perde para TOCTOU de diretório.

---

## 7. Controlo: allow-list

Formato alvo (PLG-LOAD materializa; PLG-NARC fixa a política):

```text
# path_canonico_absoluto  sha256_hex64
/home/alice/.local/lib/petrush/plugins/foo.so  <64 hex chars>
```

Regras:

| Regra | Detalhe |
|-------|---------|
| Default deny | Sem ficheiro de allow-list → **nenhum** plugin carrega |
| Match exacto | Só path pós-`realpath` igual à coluna 1 |
| Hash obrigatório | Coluna 2 = SHA-256 do conteúdo; mismatch → deny + log |
| Sem globs | `*.so` na lista é inválido |
| Sem URL / download | Loader não busca rede |
| Dono/mode da lista | Utilizador dono da lista user-local; sem `o+w`; system-wide (se existir) root-owned |
| Comparação de hash | Constant-time; **não** `strcmp` curto-circuitável em produção se `petrush_memeq_ct` estiver disponível |

Espelho conceptual da allow-list do `pudod`, **mas** no domínio unpriv do shell: autoridade ≠ root, superfície ≠ elevação.

---

## 8. Controlo: SHA-256 (não CRC-32)

| Mecanismo | Papel no petrush | Autentica `.so`? |
|-----------|------------------|------------------|
| `petrush_crc32` (ASM-CRC) | Checksum de dados / wire | **Não** (colisão trivial; ADR D3/D4) |
| `petrush_hash_path` (FNV-1a, ASM-HASH) | Hash de path em estruturas internas | **Não** |
| **SHA-256** | Digest do conteúdo do `.so` vs allow-list | **Sim** (obrigatório) |

Motivo CWE-alinhado: usar CRC/FNV como autenticador é a classe **CWE-328** / uso incorrecto de hash fraco para integridade de código. SHA-256 aqui é **integridade + binding allow-list**, não assinatura assimétrica. Assinatura (ed25519/cosign) fica **fora** do MVP; pode ser onda futura se o líder pedir supply-chain mais forte.

Implementação (PLG-LOAD): lib madura do sistema (`EVP_Digest` OpenSSL/LibreSSL, ou API OS), **não** “SHA caseiro”. Liga-se ao petrush unpriv; **não** ao `pudod`.

---

## 9. Controlo: `pudod` NÃO carrega plugin

**Normativo (PLG-NARC):**

1. O binário `pudod` / `petrush-pudod` **não** chama `dlopen`, `dlsym`, `dlclose` para plugins petrush.
2. O protocolo petrush→pudod **não** inclui path de `.so`, nome de plugin, nem `PETRUSH_PLUGIN_*`.
3. Review de `pudod.c` e deps: ausência de loader de plugin é invariante de segurança (cruzar em AUD-SEC / TST-PLG quando o loader existir no petrush).
4. Motivo: código de terceiro no address space **euid 0** (se `4755`/setcap um dia existisse) seria Elevação imediata (STRIDE E). Mesmo **sem** setuid endossado, misturar as superfícies complica o threat model e a auditoria.

Elevação continua só: utilizador → builtin `pudo` → helper + `/etc/petrush/pudo.allow` (ver [`pudo-audit.md`](pudo-audit.md)). Plugin malicioso unpriv pode *tentar* invocar `pudo` como o utilizador; a autoridade continua no helper. Isso **não** justifica carregar o `.so` dentro do pudod.

**Setuid `4755`:** este documento **não** endossa `chmod 4755` nem `setcap` no pudod (alinhado a AUD-SEC / R-C1 / `DEPLOY_CHECKLIST`).

---

## 10. Abuse cases

1. Atacante local escreve `evil.so` em `/tmp` e pede `PETRUSH_PLUGIN_PATH=/tmp` → dirs `o+w` → **deny** (§6).
2. Utilizador allow-lista path correcto; atacante com write no parent troca o ficheiro → SHA-256 mismatch → **deny**.
3. Plugin ABI major=2 em host major=1 → `ERR_ABI`, sem `init`.
4. Autor usa CRC do release notes em vez de SHA-256 na lista → loader rejeita formato / política (só SHA-256).
5. Operador tenta “otimizar” e `dlopen` no pudod “porque já é root” → **proibido** por este threat model; review deve reprovar.
6. Symlink `~/.local/.../foo.so` → `/mnt/usb/attacker.so` com hash desconhecido → realpath + hash → deny se não allow-listado; se o operador allow-listar o alvo USB world-writable → deny por §6.

## 11. Riscos residuais

| Risco | CVSS-ish | Plano |
|-------|----------|-------|
| Plugin allow-listado e íntegro ainda é código arbitrário no uid do utilizador | Alto (esperado) | Educação; least plugins; review humano antes de allow-listar |
| Sem assinatura assimétrica (só hash pinning) | Médio | Aceite no MVP; onda futura se líder pedir |
| TOCTOU subtil fd vs path no `dlopen` POSIX | Médio | PLG-LOAD deve documentar e testar (TST-PLG) |
| DoS / deadlock em `cmd` | Baixo-Médio | Timeout/jobs futuros |
| Supply chain: utilizador cola hash de gist malicioso | Médio | UX clara; não auto-fetch |

## 12. Premissas

- petrush interativo permanece **unpriv** (Opção A).
- `plugins/abi.h` major=1 é a fronteira C11 (PLG-ABI feito).
- Testes de loader em Docker (política do trilho W20+).
- Não há PoC de exploração neste repositório nem neste documento.
- UX-25 (plugins estilo OMZ) permanece skip; isto é ABI `.so` versionada, não clone de framework de zsh.

## 13. Itens fora de escopo (explícito)

| Item | Onde vive |
|------|-----------|
| `dlopen` / resolução XDG / código do loader | **PLG-LOAD** |
| Testes ww/hash/ABI automatizados | **TST-PLG** |
| Implementação SHA-256 no tree | **PLG-LOAD** (este doc só obriga o algoritmo) |
| Assinatura ed25519 / sigstore | Futuro (não bloqueia PLG-LOAD) |
| Sandbox seccomp/landlock do plugin | Futuro (defense-in-depth extra) |
| Endosso setuid `4755` | **Nunca** por esta fatia |

## 14. Checklist para PLG-LOAD (consumo directo)

- [ ] Allow-list default-deny; path canónico + SHA-256 hex64
- [ ] `realpath` fail-closed
- [ ] Recusa `S_IWOTH` no ficheiro **e** em cada dir do realpath
- [ ] SHA-256 via lib madura; comparação constant-time
- [ ] **Proibido** autenticar com `petrush_crc32` / FNV
- [ ] ABI major check antes de `init`
- [ ] Zero `dlopen` em `pudod`
- [ ] Zero `4755` / setcap aplicado por scripts desta fatia
- [ ] Log de allow/deny sem segredos
- [ ] Testes: world-writable file, world-writable parent dir, hash mismatch, major mismatch (TST-PLG)

## 15. Referências

- CWE-94 (code injection via load), CWE-426 (untrusted search path), CWE-732 (incorrect permission), CWE-367 (TOCTOU), CWE-328 (weak hash misuse)
- OWASP: Unrestricted File Upload / deserialization-adjacent (código nativo carregado)
- ADR-001 D4; `plugins/abi.h`; [`pudo-audit.md`](pudo-audit.md) (padrão allow-list / fail-closed)
- `TODO.md` ID **PLG-NARC**

---

*Documento normativo da fatia PLG-NARC. Alterações de política de plugin exigem actualizar este ficheiro na mesma mudança que o loader.*
