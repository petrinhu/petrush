# TST-T7 Scanning de Binário (checksec / hardening)

**Data:** 2026-08-22  
**SHA HEAD (pré-commit do relatório):** `e3a45b201392`  
**Agent:** qa-engineer  
**Item:** TST-T7 (W14)  
**Veredicto:** **PASS** (PIE + NX + Full RELRO + stack canary OK; Fortify parcial, aceito)

## Escopo

Conferir flags de hardening dos binários `petrush` e `pudod` em build **Release**, conforme `TESTES.md` (projeto) § TST-T7 e T7 do vault (`checksec` / `hardening-check`: PIE, RELRO, NX, Fortify).

**In:** binários Release `petrush` e `pudod`.  
**Out:** Debug/Sanitize como gate; strip de símbolos como gate; imagem de container; `AUD-SEC` (auditoria completa).

## Artefato sob teste

| Campo | Valor |
|---|---|
| Build dir | `build-preci-rel/` (já existia; **não** recompilado) |
| `CMAKE_BUILD_TYPE` | `Release` (`-O3 -DNDEBUG`) |
| Compilador | `/usr/bin/gcc` |
| `build/` | `Debug` (fora do gate Release; não usado) |
| `petrush` | 138896 B, BuildID `0f65063174587ce2b39426d17230fec22b7e4c7e`, mtime 2026-08-22 19:53 |
| `pudod` | 22352 B, BuildID `9b1fdbccb0d7ae6fd77541952286e0fef49435e1`, mtime 2026-08-22 19:53 |

## Ferramentas

| Ferramenta | Status | Uso |
|---|---|---|
| `checksec` 2.7.1 (`/usr/bin/checksec`) | instalado | primária (`--file`, `--fortify-file`, JSON) |
| `hardening-check` | **ausente** | não executado; checksec cobriu o escopo |
| `readelf` (binutils) | instalado | confirmação GNU_STACK, GNU_RELRO, BIND_NOW, `__stack_chk_fail` |

## Execução

```text
checksec --format=cli --file=build-preci-rel/petrush
checksec --format=cli --file=build-preci-rel/pudod
checksec --fortify-file=build-preci-rel/petrush
checksec --fortify-file=build-preci-rel/pudod
checksec --format=json --file=build-preci-rel/petrush
checksec --format=json --file=build-preci-rel/pudod
readelf -lW / -dW / -sW  (GNU_STACK, GNU_RELRO, BIND_NOW, FLAGS_1 PIE, __stack_chk_fail)
```

## Resultados (matriz)

| Mitigação | petrush | pudod | Gate |
|---|---|---|---|
| **PIE** | enabled (`FLAGS_1: NOW PIE`) | enabled | **OK** |
| **NX** (GNU_STACK RW, sem X) | enabled | enabled | **OK** |
| **RELRO** | Full (`GNU_RELRO` + `BIND_NOW`) | Full | **OK** |
| **Stack protector** (canary / `__stack_chk_fail`) | Canary found | Canary found | **OK** |
| **Fortify** (`_FORTIFY_SOURCE`) | Partial (6/16) | Partial (4/6) | **parcial OK** |
| RPATH / RUNPATH | No / No | No / No | OK (info) |
| Symbols (não stripped) | 367 symbols | 73 symbols | info (não é gate T7) |

JSON checksec:

```json
{ "build-preci-rel/petrush": { "relro":"full","canary":"yes","nx":"yes","pie":"yes","rpath":"no","runpath":"no","symbols":"yes","fortify_source":"partial","fortified":"6","fortify-able":"16" } }
{ "build-preci-rel/pudod": { "relro":"full","canary":"yes","nx":"yes","pie":"yes","rpath":"no","runpath":"no","symbols":"yes","fortify_source":"partial","fortified":"4","fortify-able":"6" } }
```

### Fortify detalhe

- libc oferece FORTIFY: **Yes**; ambos compilados com suporte: **Yes**.
- `petrush`: 6 checked / 16 fortifiable (unchecked típicos: `fgets`, `getcwd`, `memcpy`, `memset`, `read`, `readlink`, `realpath` plain, `snprintf` plain, etc.; checked: `__fprintf_chk`, `__printf_chk`, `__realpath_chk`, `__snprintf_chk`, `__syslog_chk`, `__vsnprintf_chk`).
- `pudod`: 4 checked / 6 fortifiable (unchecked: `fgets`, `memcpy`; checked: `__fprintf_chk`, `__realpath_chk`, `__syslog_chk`, `__vsnprintf_chk`).
- CMake já aplica `-D_FORTIFY_SOURCE=2` em builds não-Debug (`CMakeLists.txt` petrush/pudod). Partial é esperado: o compilador só troca para `*_chk` quando o tamanho do destino é conhecido em tempo de compilação.

### Confirmação readelf

- `GNU_STACK` … **RW** (sem execute) → NX.
- `GNU_RELRO` presente + `FLAGS BIND_NOW` → Full RELRO.
- `FLAGS_1: NOW PIE` → PIE.
- UND `__stack_chk_fail@GLIBC_2.4` em ambos → stack protector ligado.

## Critério de saída TST-T7

- [x] PIE enabled em `petrush` e `pudod` Release  
- [x] NX enabled em ambos  
- [x] Full RELRO em ambos  
- [x] Stack canary / stack protector presente em ambos  
- [x] Fortify presente (parcial aceito pelo brief da fatia)  
- [x] Relatório em `docs/memory/tst-t7-binario.md`  
- [ ] `hardening-check` (ausente; não bloqueia com checksec verde)

**Status no TODO:** `🔍 Pendente verificação` (execução entregue; `✅` só após onda de auditoria/TST).

## Limitações (honesto)

1. `hardening-check` (Debian/Ubuntu) não está no Fedora 44; checksec cobriu o mesmo conjunto.  
2. Binários Release não stripped (símbolos presentes): útil para debug, não falha T7.  
3. Scan feito nos artefatos já presentes em `build-preci-rel/` (Release 2026-08-22 19:53); `build/` local permanece Debug.  
4. Fortify partial não foi tratado como defeito nesta fatia (brief: Fortify pode ser parcial).

## Referências

- Item: `TODO.md` → TST-T7  
- `TESTES.md` (projeto) § TST-T7  
- Vault `TESTES.md` § T7  
- Flags: `CMakeLists.txt` (`-fPIE`, `-pie`, `-Wl,-z,relro,-z,now,-z,noexecstack`, `-fstack-protector-strong`, `-D_FORTIFY_SOURCE=2`)
