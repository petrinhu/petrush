# TST-T5 Scanning de Dependências

**Data:** 2026-08-22  
**SHA HEAD (pré-commit do relatório):** `cbb14fe5d364db8aa3b7ead5d7bab4583c252424`  
**Agent:** qa-engineer  
**Item:** TST-T5 (W14)  
**Veredicto:** **SEM CRITICAL no vendor próprio** (scanners verdes; CVE linenoise classificados e mitigados)

## Escopo

Detectar dependências vulneráveis ou desatualizadas (vendor linenoise + toolchain), conforme `TESTES.md` (projeto) e T5/T12 do vault (`trivy` / `grype` / OSV).

**In:** árvore do repo (exceto dirs de build), `vendor/linenoise/`, binários `build/petrush` e `build/pudod`, consulta OSV/NVD para linenoise, inventário de toolchain local.  
**Out:** CVEs do sistema operacional Fedora (glibc/gcc de distro) como gate de release do petrush; imagem de CI container (fica para TST-T15/T12 se aplicável); `AUD-DEPS` (acoplamento).

## Inventário de dependências

| Artefato | Tipo | Versão / origem | Notas |
|---|---|---|---|
| `vendor/linenoise/` | C embutido (BSD-2) | linenoise **1.0** (`linenoise.h` VERSION 1.0; antirez/linenoise) | Única dep de terceiro no binário; patches locais (SEC-08, UX-20/21, HistoryGet) |
| libc | runtime dinâmico | glibc **2.43** (Fedora 44) | `ldd build/petrush` → só `libc.so.6` |
| Toolchain build | host | gcc **16.1.1**, clang **22.1.8**, cmake **4.3.0** | Não vendored |
| CI Actions (syft) | workflow | `actions/checkout@v4`, `actions/upload-artifact@v4` | Únicos “packages” que o SBOM viu |

Sem lockfile, sem `package.json`/`Cargo.toml`/`go.mod`/`requirements.txt`. SCA de pacote gerenciado não aplica ao vendor C cru.

## Ferramentas

| Ferramenta | Status na máquina | Uso nesta fatia |
|---|---|---|
| `grype` 0.112.0 (`~/.local/bin/grype`) | já instalado | primária: `dir:.`, `dir:./vendor`, binários |
| `trivy` 0.74.0 | **instalado userland** nesta fatia | `trivy fs .` / `vendor` (DB baixada) |
| `osv-scanner` 2.5.1 | **instalado userland** nesta fatia | `-r .` (sem package sources; esperado) |
| OSV/NVD HTTP | online | classificação manual de CVEs linenoise |
| `syft` | instalado | SBOM auxiliar (2 artifacts GHA) |

Artefatos brutos em `/var/tmp/petrush-tst-t5/` (`grype_*.txt`, `trivy_*.txt`, `osv_*.txt`, JSON).

## Execução

```text
grype version                    # 0.112.0
grype dir:. -o table --exclude './build/**' --exclude './build-sanitize/**' --exclude './Testing/**'
# → No vulnerabilities found  EXIT 0

grype dir:./vendor -o table
# → No vulnerabilities found  EXIT 0

grype build/petrush -o table
grype build/pudod -o table
# → No vulnerabilities found  EXIT 0 (ambos)

# trivy instalado de release oficial v0.74.0 → ~/.local/bin/trivy
trivy fs . --scanners vuln --severity UNKNOWN,LOW,MEDIUM,HIGH,CRITICAL \
  --skip-dirs build,build-sanitize,build-preci-rel,Testing
# → Number of language-specific files num=0; Supported files not found; EXIT 0

trivy fs vendor --scanners vuln
# → idem, EXIT 0

# osv-scanner instalado de release oficial v2.5.1 → ~/.local/bin/osv-scanner
osv-scanner -r .
# → Scanned vendor/linenoise, 0 packages; No package sources found; EXIT 0

syft dir:. (excl. build*)
# → artifacts: actions/checkout@v4, actions/upload-artifact@v4
```

Scanners de SCA **não enxergam** C vendored sem manifesto: resultado “limpo” dos tools **não basta**. Classificação manual OSV/NVD abaixo é a prova do vendor.

## Classificação de CVEs do vendor (linenoise)

Fonte: NVD keyword `linenoise` (3 hits) + OSV `CVE-2025-9810`.

| CVE | Severidade (NVD) | Escopo | Afeta petrush vendor? | Status |
|---|---|---|---|---|
| **CVE-2025-9810** | **MEDIUM 6.8** (CVSS:3.1/AV:L/AC:L/PR:N/UI:N/S:U/C:N/I:H/A:L) | TOCTOU em `linenoiseHistorySave`: `fopen("w")` + `chmod(path)` permite symlink race (overwrite + perms) | Upstream 1.0 **sim**; **mitigado localmente** | **Não CRITICAL.** Patch petrush: `open(O_NOFOLLOW\|O_CREAT\|O_TRUNC\|O_CLOEXEC)` + `fchmod(fd, 0600)` (`vendor/linenoise/linenoise.c` ~2554). Teste: `test_linenoise_history` (SEC-08). Fix upstream ref: `f2558e1e…` |
| **CVE-2013-7458** | (legado; perms world-readable do history no Redis CLI) | History file legível por outros usuários | Mesma classe de risco de perms | **Mitigado:** `umask` restritivo + `fchmod(fd, S_IRUSR\|S_IWUSR)` no save |
| **CVE-2021-46481** | MEDIUM 5.5 | Memory leak em **Jsish** v3.5.0 via cópia própria de linenoise | **Não aplicável** ao vendor antirez embutido no petrush | Fora de escopo (fork alheio) |

### CRITICAL no vendor próprio?

**Não.** Nenhum CVE CRITICAL listado para linenoise nesta passada. O único CVE diretamente do antirez linenoise em uso é **CVE-2025-9810 (MEDIUM)**, já mitigado e coberto por teste.

## Toolchain (informativo)

| Componente | Versão local | Nota SCA |
|---|---|---|
| gcc | 16.1.1 (Red Hat 16.1.1-2) | Pacote distro; não vendored |
| clang | 22.1.8 (Fedora) | idem |
| glibc | 2.43 | única lib dinâmica do binário |
| cmake | 4.3.0 | build-only |

Grype/trivy sobre o ELF não reportaram vulns de pacote embutido (binário sem deps empacotadas no SBOM do scanner). Inventário de CVEs do **RPM** do host fica fora do gate T5 do vendor; cruzar na matriz CI/T12 se necessário.

## Achados

1. **SCA automatizado:** 0 vulnerabilidades reportadas (grype/trivy/osv-scanner) porque não há ecossistema de pacotes.  
2. **Vendor linenoise:** CVE-2025-9810 MEDIUM mitigado (SEC-08); CVE-2013-7458 classe de perms mitigada; CVE-2021-46481 N/A.  
3. **Zero CRITICAL** no vendor próprio → critério de saída da fatia atendido para marcar `🔍`.

## Limitações (honesto)

1. Scanners SCA clássicos **não analisam** source C vendored linha a linha; a classificação OSV/NVD + inspeção do patch é obrigatória.  
2. `trivy`/`osv-scanner` foram instalados em userland nesta sessão (`~/.local/bin`); não estavam no PATH antes.  
3. Não houve scan da **imagem** `fedora:44` do CI (TST-T15 / T12).  
4. Actions GHA (`checkout`/`upload-artifact` v4) aparecem no SBOM syft; não houve advisory CRITICAL levantado nesta passada (não bloqueiam T5 do vendor).  
5. Toolchain/glibc do host: inventário apenas; CVEs de distro não viraram gate desta fatia.

## Critério de saída TST-T5

- [x] Pelo menos um de trivy/grype/osv rodou com EXIT 0 sobre a árvore  
- [x] Segunda ferramenta tentada (trivy + osv-scanner após grype)  
- [x] CVEs do vendor linenoise classificados (severidade + mitigação)  
- [x] **Sem CRITICAL** no vendor próprio  
- [x] Relatório em `docs/memory/tst-t5-deps.md`  
- [ ] Imagem CI / T12 profundo (fora desta fatia)

**Status sugerido no TODO:** `🔍 Pendente verificação` (scan executado; ✅ só após onda de auditoria/TST se o orquestrador assim marcar).

## Referências

- Item: `TODO.md` → TST-T5  
- `TESTES.md` (projeto) § TST-T5  
- Vault `TESTES.md` § T5 / T12  
- `vendor/linenoise/README.md` (patches SEC-08 / UX)  
- `tests/test_linenoise_history.c`  
- OSV/NVD: `CVE-2025-9810`, `CVE-2013-7458`, `CVE-2021-46481`  
