# TST-T12 Busca de CVEs (deps + toolchain)

**Data:** 2026-08-22  
**SHA HEAD:** `994322c3ae57f7d7f400cbd8ab40f113b9df7437`  
**Agent:** qa-engineer  
**Item:** TST-T12 (W14)  
**Veredicto:** **SEM CRITICAL no vendor próprio**; CVE-2025-9810 (linenoise) **MEDIUM 6.8**, já mitigado (SEC-08)

## Escopo

Cruzar CVEs conhecidos nas dependências e no toolchain da matriz de CI, conforme `TESTES.md` (projeto § TST-T12) e vault `TESTES.md` § T12 (`trivy` + `grype` + NVD/OSV).

**In:** árvore do repo (exceto dirs de build), `vendor/linenoise/`, binários `build/petrush` e `build/pudod`, imagem CI `registry.fedoraproject.org/fedora:44`, SBOM das Actions GHA, consulta NVD/OSV para linenoise, inventário de toolchain (host = Fedora 44, espelho da matriz).  
**Out:** gate de release sobre CVEs de RPM do host como bloqueio do petrush; `AUD-DEPS` (acoplamento); mutação adversarial.

Relação com **TST-T5**: T5 cobriu SCA de deps e classificação linenoise; T12 reexecuta o cruzamento CVE com foco em **toolchain/CI** (imagem Fedora 44 + Actions) e reconfirma o cruzamento **CVE-2025-9810 ↔ SEC-08**.

## Inventário

| Artefato | Tipo | Versão / origem | Nota CVE |
|---|---|---|---|
| `vendor/linenoise/` | C embutido (BSD-2) | linenoise **1.0** (antirez; patches locais) | Única dep de terceiro no binário |
| libc | runtime dinâmico | glibc **2.43-8.fc44** | `ldd build/petrush` → só `libc.so.6` |
| Toolchain host/CI | distro | gcc **16.1.1**, clang **22.1.8**, cmake **4.3.0** | Não vendored; matriz CI usa a mesma imagem Fedora 44 |
| CI container | imagem | `registry.fedoraproject.org/fedora:44` (`f2d7418fa4ad`) | Job `build-and-test` |
| GHA | workflow | `actions/checkout@v4`, `actions/upload-artifact@v4` | Únicos artifacts no SBOM syft |

Sem lockfile / manifesto de ecossistema. SCA de pacote gerenciado **não vê** o C vendored; classificação NVD/OSV manual é obrigatória para linenoise.

## Ferramentas

| Ferramenta | Versão | Uso nesta fatia |
|---|---|---|
| `grype` | 0.112.0 (`~/.local/bin`) | `dir:.`, `dir:./vendor`, binários, imagem fedora:44, SBOM syft |
| `trivy` | 0.74.0 | `fs .` / `vendor`; tentativa `image fedora:44` (OS Fedora **unsupported** no trivy) |
| `osv-scanner` | 2.5.1 | `-r .` (0 packages; esperado) |
| OSV/NVD HTTP | online | `CVE-2025-9810` + keyword `linenoise` (3 hits) |
| `syft` | instalado | SBOM auxiliar (2 Actions) |

Artefatos brutos: `/var/tmp/petrush-tst-t12/` (`grype_*.txt`, `trivy_*.txt`, `osv*.txt`, `nvd_*.txt`, JSON).

## Execução (scanners)

```text
grype dir:. -o table --exclude './build/**' ...
# → No vulnerabilities found  EXIT 0

grype dir:./vendor -o table
# → No vulnerabilities found  EXIT 0

grype build/petrush / build/pudod
# → No vulnerabilities found  EXIT 0 (ambos)

trivy fs . --scanners vuln --severity UNKNOWN,LOW,MEDIUM,HIGH,CRITICAL
# → language-specific files num=0; Supported files not found; EXIT 0

trivy fs vendor --scanners vuln
# → idem EXIT 0

osv-scanner -r .
# → Scanned vendor/linenoise, 0 packages; No package sources found; EXIT 0

grype registry.fedoraproject.org/fedora:44
# → No vulnerabilities found  EXIT 0  (matches=0)

trivy image registry.fedoraproject.org/fedora:44 --severity CRITICAL,HIGH
# → Detected OS fedora 44; WARN Unsupported os family=fedora; EXIT 0 (sem DB OS)

syft dir:. → actions/checkout@v4, actions/upload-artifact@v4
grype sbom:<syft.json>
# → No vulnerabilities found  EXIT 0
```

## Cruzamento CVE-2025-9810 (linenoise) ↔ SEC-08

Fonte: NVD `cveId=CVE-2025-9810` + OSV `api.osv.dev/v1/vulns/CVE-2025-9810` + keyword NVD `linenoise` (3 CVEs).

| CVE | Severidade (NVD) | Escopo | Afeta petrush vendor? | Status |
|---|---|---|---|---|
| **CVE-2025-9810** | **MEDIUM 6.8** (CVSS:3.1/AV:L/AC:L/PR:N/UI:N/S:U/C:N/I:H/A:L) | TOCTOU em `linenoiseHistorySave`: `fopen("w")` + `chmod(path)` permite symlink race (overwrite + perms) | Upstream 1.0 **sim**; **mitigado localmente** | **Não CRITICAL.** Patch petrush: `open(O_WRONLY\|O_CREAT\|O_TRUNC\|O_NOFOLLOW\|O_CLOEXEC)` + `fchmod(fd, S_IRUSR\|S_IWUSR)` em `vendor/linenoise/linenoise.c` (~2551-2565). Teste: `test_linenoise_history` (SEC-08). Fix upstream ref: `f2558e1e588b1ba384ec73a2cf5c9a46409753db` (OSV GIT fixed). Publicado NVD: 2025-09-01. |
| **CVE-2013-7458** | LOW 3.3 | History world-readable (Redis CLI / linenoise) | Mesma classe de perms | **Mitigado:** umask restritivo + `fchmod` 0600 no save |
| **CVE-2021-46481** | MEDIUM 5.5 | Memory leak em **Jsish** v3.5.0 (cópia própria) | **Não aplicável** ao vendor antirez do petrush | Fora de escopo |

### Evidência de mitigação (SEC-08) nesta árvore

- Comentário + flags em `vendor/linenoise/linenoise.c` (`O_NOFOLLOW`, `fchmod` no fd aberto).
- Doc de patch: `vendor/linenoise/README.md` item 1 (SEC-08 / CVE-2025-9810).
- Teste: `tests/test_linenoise_history.c` (binário `build/test_linenoise_history` presente).
- Item TODO `SEC-08` já em `🔍 Pendente verificação`.

### CRITICAL no vendor próprio?

**Não.** Nenhum CVE CRITICAL listado para linenoise nesta passada. O único CVE direto do antirez linenoise em uso é **CVE-2025-9810 (MEDIUM)**, mitigado e coberto por teste.

## Toolchain / matriz CI

| Componente | Versão | Achado scanner |
|---|---|---|
| gcc (host/CI Fedora 44) | 16.1.1-2.fc44 | Não vendored; grype imagem 0 matches |
| clang | 22.1.8-4.fc44 | idem |
| glibc | 2.43-8.fc44 | única lib dinâmica do ELF |
| cmake | 4.3.0-1.fc44 | build-only |
| Imagem `fedora:44` | local `f2d7418fa4ad` | grype: **0** vulns; trivy: OS unsupported (sem DB Fedora) |
| `actions/checkout@v4` | SBOM | grype: 0 |
| `actions/upload-artifact@v4` | SBOM | grype: 0 |

**Nota honesta:** ausência de matches na imagem Fedora pode refletir cobertura incompleta das DBs dos scanners para RPM Fedora (trivy declara `Unsupported os`), não prova matemática de “zero CVE no distro”. Gate desta fatia = **vendor próprio + Actions no SBOM**, não “Fedora distro limpa”.

## Achados

1. **SCA automatizado (árvore/vendor/ELF):** 0 vulnerabilidades (grype + trivy + osv-scanner), EXIT 0.  
2. **Vendor linenoise:** CVE-2025-9810 MEDIUM mitigado (SEC-08); CVE-2013-7458 classe perms mitigada; CVE-2021-46481 N/A.  
3. **CI imagem Fedora 44:** grype 0 matches; trivy sem suporte OS Fedora.  
4. **Actions GHA (syft+grype):** 0 vulns.  
5. **Zero CRITICAL** no vendor próprio → critério para marcar `🔍` atendido.

## Limitações

1. SCA clássico **não analisa** source C vendored linha a linha; NVD/OSV + inspeção do patch são a prova do vendor.  
2. `trivy image` **não** cobre advisory OS Fedora.  
3. CVEs de RPM do host/imagem não são gate de release do binário petrush nesta fatia.  
4. Job experimental `fedora:45` (`continue-on-error`) não foi escaneado (fora do path crítico de main).

## Critério de saída TST-T12

- [x] trivy + grype rodaram (EXIT 0) sobre árvore/vendor  
- [x] osv-scanner tentado  
- [x] Consulta NVD/OSV para linenoise (incl. CVE-2025-9810)  
- [x] Cruzamento explícito CVE-2025-9810 ↔ mitigação SEC-08  
- [x] Toolchain/CI inventariados (imagem fedora:44 + Actions)  
- [x] **Sem CRITICAL** no vendor próprio  
- [x] Relatório em `docs/memory/tst-t12-cves.md`

**Status sugerido no TODO:** `🔍 Pendente verificação` (scan executado; `✅` só após onda de auditoria).

## Referências

- Item: `TODO.md` → TST-T12  
- `TESTES.md` (projeto) § TST-T12  
- Vault `TESTES.md` § T12  
- Irmão: [`docs/memory/tst-t5-deps.md`](tst-t5-deps.md)  
- `vendor/linenoise/README.md` (patch SEC-08)  
- `tests/test_linenoise_history.c`  
- OSV/NVD: `CVE-2025-9810`, `CVE-2013-7458`, `CVE-2021-46481`  
- `.github/workflows/ci.yml` (imagem fedora:44)
