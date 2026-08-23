# Log de ações — sessão autônoma petrush

Regra: bullets em tempo real. Orquestrador só despacha/julga. C-level = grok-4.6. Workers = grok-4.5, teto 2. Escrita só neste cwd.

## 2026-08-23 - OSH-2

- OSH-2: `petrush_positional_shift` + builtin `shift` [n]; TDD red (link undefined) → green; `test_expand` 5 + `test_info` 7; smoke osh2 6/6 + osh1 5/5; Docker fedora:44 clang PASS; TODO 🔍. Sem push. Sem 4755.

## 2026-08-23 - XDG-1

- XDG-1: `xdg_paths.c` rc/history XDG + compat legacy; mkdir -p 0700 no save; main+builtin_history; TDD `test_xdg_paths` 9/9 + smoke `xdg-paths.sh` 7/7; Docker fedora:44 clang PASS; TODO 🔍. Sem push. Sem 4755.

## 2026-08-23 - OSH-1

- OSH-1: posicionais `$0`/`$1`../`$#`/`$@`/`$*` em struct (expand.c); main seta script/interativo; TDD red 9 falhas → green 26/26; smoke osh1 5/5 + osh0 9/9; Docker fedora:44 clang PASS; TODO 🔍. Sem push. Sem 4755. Sem install.

## 2026-08-23 - CI-OPENSSL

- CI-OPENSSL: GHA run 32629155268 12/12 vermelho; causa configure = `find_package(OpenSSL REQUIRED)` sem devel; CachyOS/Arch Build = undefined `petrush_wai_scan_impl`/`petrush_netcom_scan_impl` em `test_process`. Fix: openssl-devel/libssl-dev/openssl em ci.yml (matrix+ASan+F45); `PETRUSH_ASM_WAI_NET` no CMake. Smoke fedora:44 + CachyOS clang PASS. TODO 🔍. Sem push. Sem 4755. `-Werror` mantido.

## 2026-08-23 - TST-ASM

- TST-ASM: harness `tests/asm/` via `ctest -R asm_` Docker fedora:44 clang Sanitize 10/10 + Release 10/10; ASan/UBSan static OK; mode 755 sem 4755; seccomp=unconfined (unshare sem CAP_NET_ADMIN); relatorio `docs/memory/tst-asm.md`; TODO 🔍. Sem push.

## 2026-08-23 - TST-CXX

- TST-CXX: `add_test(NAME configsh)` → `tests/smoke/cxx-tui.sh`; `ctest -R configsh` host PASS + Docker fedora:44 `-t` PASS (PTY python; sem `script`); mode 755 sem ncurses; sem display `:0`; relatório `docs/memory/tst-cxx.md`; TODO 🔍. Sem push. Sem 4755.

## 2026-08-23 - ASM-NET

- ASM-NET: builtin `netcom` + `petrush_netcom_scan` (`netcom_scan.S` → impl C); `-wifi -eth -bt`; sysfs+netlink GET; `-up/-down` helpers C timeout; EPERM sem CAP exit 1 sem hang; ctest asm_netcom+eperm; Docker fedora:44; TODO 🔍. Sem push. Sem 4755. plugin_load intocado.
- ASM-NET (QA fix): `netcom.c` RTA loop sem `RTA_NEXT` (int vs `RTA_ALIGNTO=4U`); sem `-Wno-sign-conversion` no produto; gcc host + Docker fedora:44 gcc 16 `-Wconversion -Werror` petrush PASS; ctest asm_netcom PASS; plugin_load intocado; sem 4755; sem push.

## 2026-08-23 - PLG-LOAD

- PLG-LOAD: `plugin_load.c` XDG + `PETRUSH_PLUGIN_PATH`, allow-list SHA-256 (OpenSSL), recusa world-writable (ficheiro+dirs), dlopen so apos checks; pudod sem .so (`plugin_pudod_no_dl`); TDD red stub → green; ctest -R plugin_ PASS; Docker fedora:44 clang PASS; TODO 🔍. Sem push. Sem 4755. Sem mexer netcom.
- PLG-LOAD (QA fix): `dlsym` via `memcpy`/`union` (`petrush_dlsym_fn`); sem cast object→fn; `-Wpedantic -Werror` mantido; gcc host + Docker fedora:44 gcc 16 PASS; ctest plugin_load PASS; netcom intocado; sem 4755; sem push.

## 2026-08-23 - CXX-TUI

- CXX-TUI: configsh TUI raw ANSI (sem ncurses); --section/--dump/--check; XDG `$XDG_CONFIG_HOME/petrush/config.ini`; src/cxx/{main,config,tui}.cpp; smoke cxx-tui.sh + target cxx_tui; PTY fedora:44 PASS (python pty + docker -t); TODO 🔍. Sem push. Sem 4755.

## 2026-08-21 — início AFK

- Início modo autônomo (líder AFK). Escopo: (1) sudo CVEs vs pudo (2) falhas de segurança de 10 shells (3) inventário de funções dos mesmos shells (4) commit+push pesquisa (5) WSJF na tabela (6) depois, ondas uma a uma.
- Shells alvo: bash, zsh, fish, dash, ksh, mksh, tcsh, busybox ash, yash, osh/oil.
- Memórias: só neste cwd (`docs/memory/`). Fora do cwd = read-only. Sem clone/cópia de git.
- Despacho C-level: Narciso (plano pesquisa segurança) + Caetano (plano inventário de funções + encaixe TODO/WSJF).
- Schema TODO: 9 colunas (ID, Onda, Grupo, Descrição Técnica, Prioridade, Pré-requisito, Dificuldade, Status, Estado Auditado). WSJF early qualitativo. Teto 2 workers 4.5.
- Planos C-level recebidos: Narciso (20 classes sudo vs boundary A pudod / B fallback sudo; 10 shells vs REPL unpriv). Caetano (matriz ~32 classes Feature×shell; cap 8 FEAT; UX-24/25/26 skip; UX-19..23 intactos).
- Despacho 2 workers grok-4.5: security-engineer A = docs/memory/sudo-pudo-riscos.md; security-engineer B = docs/memory/shells-seguranca.md. technical-writer do inventário de funções espera slot.
- Artefatos A/B gravados (HEAD 1fdbfbf). Orquestrador: CVE-2025-32462/32463 confirmados em sudo.ws 2025-06-30. Mapping A NÃO para flags -h/-R bate com `sudo --`. SEC-04/05/03 vivos em A se 4755.
- Slot livre: technical-writer shells-funcoes.md + Narciso audita os dois md de segurança (C-level, não conta no teto de 2 workers de execução).
- Narciso: APROVAR COM RESSALVAS; prosa corrigida (ordem find_pudod, CWE, UAF CHROOT=, noclobber SIM). Residuais SEC-06..10 sugeridos.
- Caetano: cap FEAT = BANG TRUE UMASK NOCLOBBER READ PARAM TEST (rejeitou CMDSUB/FUNCS/FLOW e printf/glob[]).
- Cosmo: TODO reordenado W1..W17; TST após FEAT-TEST; primeira ⏳ = W2 SEC-02/05/04/03 + DOC-01.
- Próximo: commit+push da pesquisa; depois onda W2 uma fatia por vez (teto 2 workers).
- Commit `3269896` push origin/main (`1fdbfbf..3269896`). Pesquisa + WSJF no remoto.
- Onda W2 ⏳: despacho 2 workers (teto) SEC-02 (`pudo.c` find_pudod) + SEC-05 (`pudod.c` realpath fail-closed). NEW-22 não puxar (líder AFK).
- SEC-02 `ae73665` e SEC-05 `307e891` no main; `test_pudo` 13/13; check 12/12; push até `dada643`.
- W2 continua: SEC-04 (fail closed argc) + SEC-03 (example allow).
- SEC-03/04/06/07/08 no remoto. SEC-09 O_EXCL trunc + SEC-10 rc uid/mode: commits locais `587b045` / `877db3c`.
- NEW-22 permanece ⏳ (líder AFK). Próxima fatia ROI ⏳: UX-19 builtins no pipe.
- UX-19 `5d55236` no main: check 15/15, smoke 47/47 (`echo | cat` builtin). TODO UX-19 🔍.
- Próxima ⏳: UX-20 Ctrl-R. NEW-22 segue bloqueado no líder.
- UX-20 Opção A (`75fa64e`): Ctrl-R reverse-i-search no linenoise; check 15/15. TODO 🔍.
- Próxima ⏳: UX-21 highlight mínimo. Log segue neste arquivo.
- UX-21 `d5ccfa4` no main: highlight mínimo aspas/tokens; check 16/16. TODO 🔍.
- Próxima ⏳ na fila: UX-22 `source` / `.`. NEW-22 ainda espera o líder.
- UX-22 `5a6c097` no main: source/. teto 8, rc_trust; check 17/17. TODO 🔍.
- Próxima ⏳: UX-23 background `&` + job control mínimo. NEW-22 segue AFK.
- UX-23 `053e3b5` no main: `&` + jobs + reaper; check 18/18. TODO 🔍. Sem fg/bg.
- Onda W10: despacho FEAT-BANG + FEAT-TRUE (teto 2, paralelos, pré-req UX-23).
- W10/W11/W12/W13 FEAT fechadas em 🔍: BANG TRUE UMASK NOCLOBBER READ PARAM TEST. HEAD inclui `dd93dda` FEAT-TEST.
- Próxima onda W14 TST-* (T2 estática, T8 secrets, T15 pré-CI…). NEW-22 segue ⏳ (líder AFK). Log canônico: este arquivo.
- TST-T8 LIMPO (`693e66f`) → 🔍. TST-T2 gate FALHOU (cppcheck/clang-tidy); críticos dispatcher/parser remediados (`ebcafb3`, `3983fff`). Residual: complexidade cognitiva (pipeline/tokenize/find_pudod). TST-T2 permanece ⏳ até lint completo.
- check 18/18. Próximo: re-rodar lint TST-T2 ou TST-T15 pré-CI.
- 2026-08-22 líder voltou: ordem de seguir até o fim da tabela, mesmo modo (C-level 4.6 planeja, workers 4.5 teto 2). NEW-22 tratado como confirmação (Onda 3 = NEW-20 já 🔍). Retoma W14: fechar TST-T2 (lint residual) e drenar TST restantes, depois AUD, depois NEW-23.
- TST-T15 pré-CI Fedora 44 (docker; podman ausente): Debug gcc PASS (check 18/18, smoke 53/53, cppcheck+clang-tidy 0). Release smoke FAIL (SEC-02, espelha GHA); Release clang-tidy ArrayBound FAIL. Com install sem setuid, smoke Release 53/53. Relatório `docs/memory/tst-t15-preci.md`. Status permanece ⏳ (não 🔍). Sem push.
- TST-T15 remediação CI/preci: Release smoke via `cmake --install --prefix /usr/local` (sem setuid) em `ci.yml` + `scripts/tst-t15-preci.sh`. Re-prova container: tree 51/53 FAIL (SEC-02), installed 53/53 PASS. TODO → 🔍. ArrayBound residual. Sem push.
- TST-T14 smoke 53/0 + check 18/18 → 🔍 (`521f7b1`). NEW-22 → 🔍 (confirmação do líder).
- Despacho BE-1 tokenize + BE-2 pipeline/find_pudod (complexidade <50, sem NOLINT).
- TST-T2 gate verde: cppcheck 0, clang-tidy 0 (após double-free exe_path + struct try_abs_candidate). TODO → 🔍. check 18/18.
- W14 fatia 1: TST-T15 (container Fedora 44) + TST-T5 (trivy/grype).
- TST-T5 deps: grype+trivy+osv-scanner EXIT 0; linenoise CVE-2025-9810 MEDIUM mitigado (SEC-08); 0 CRITICAL vendor → 🔍. Relatório `docs/memory/tst-t5-deps.md`. Sem push.
- TST-T12 CVEs: grype+trivy+osv EXIT 0; cruzamento CVE-2025-9810↔SEC-08 (MEDIUM mitigado); fedora:44 grype 0; Actions SBOM 0; 0 CRITICAL vendor → 🔍. Relatório `docs/memory/tst-t12-cves.md`. Sem push.
- UX-21 Opção A: highlight mínimo (aspas/CMD/OP); `test_highlight` + check 16/16; TODO 🔍. Sem push.
- UX-22 impl: Mid source.h/c + test_source; load_rc_file=missing_ok; check 17/17 smoke 50; TODO 🔍. Sem push.
- AUD-REPORT (W16): livro `docs/auditoria/aud-report.md` + índice `docs/auditoria/README.md`. Score 73/100, APROVADO COM RESSALVAS (unpriv early). Setuid 4755 NÃO endossado. 3 vermelhos P0 (gate 4755, fallback sudo, allow-list). `src/` intocado. TODO AUD-REPORT → 🔍. Sem push.
- W18 docs P1: DOC-02/03/04 commits `7f50edc` `5b271b5` `1e773ae`. QA APROVAR PUSH. Push origin. CI via github-gossips (sem poll). Próximo após sinal verde: P2 fronteira (R-I8/I9/I13).
- W19 ARCH-01/02 push `096a968` (QA APROVAR). ARCH-03 (porta ui_port DIP) planejado pelo CTO; implementação espera github-gossips verde. Sem poll GHA.

## 2026-08-23 - ASM-00

- ASM-00 toolchain: CMake C CXX ASM, PETRUSH_ASM, empty.S .note.GNU-stack, ASan so C/C++, Docker fedora:44 PASS, TODO 🔍. Sem push.

## 2026-08-23 - ASM-ABI

- ASM-ABI: asm.h (10 decls) + abi.inc (SysV AMD64 PIC); empty.S usa abi.inc; smoke asm-abi-header.sh + target asm_abi; Docker fedora:44 clang PASS; TODO 🔍. Sem corpos. Sem push. Sem 4755.
- ASM-ABI: smoke passa a travar contrato memeq_ct (verbatim "Retorna 0 se iguais, 1 se diferem" + "1  = diferem (memeq_ct)"; preambulo sem -1 associado a diferente/memeq). HEAD verde; preambulo antigo vermelho; Docker fedora:44 PASS. Sem push.
- ASM-MEMEQ: memeq_ct.S (XOR|OR, sem early-out; 0/1); tests/asm/test_memeq.c; ctest -R asm_memeq 11/11 (RED stub→GREEN); Docker fedora:44 clang PASS; TODO 🔍. Sem push. Sem 4755.

## 2026-08-23 - ASM-GLOB

- ASM-GLOB: glob_match.S (`*` `?` iterativo; `[` literal); expand.c match_pat → petrush_glob_match (PETRUSH_HAVE_ASM); ctest -R test_glob PASS (RED stub→GREEN); Docker fedora:44 clang PASS; TODO 🔍. Sem push. Sem 4755.

## 2026-08-23 - ASM-CRC

- ASM-CRC: crc32.S IEEE 0xEDB88320 (tabela .rodata PIC); incremental sem XOR final; vetor "123456789" → 0xCBF43926; tests/asm/test_crc32.c; ctest -R asm_crc32 8/8 (RED stub→GREEN); Docker fedora:44 clang PASS; TODO 🔍. Sem push. Sem 4755. Sem tocar glob/memeq.

## 2026-08-23 - ASM-I64

- ASM-I64: parse_i64.S (decimal +/-; mag uint64; INT64_MIN/MAX; overflow/invalido -1 sem tocar *out); tests/asm/test_parse_i64.c; ctest -R asm_parse_i64 16/16 (RED stub→GREEN); Docker fedora:44 clang PASS; TODO 🔍. Sem push. Sem 4755. Sem tocar glob/memeq.

## 2026-08-23 - ASM-HASH

- ASM-HASH: hash_path.S FNV-1a 64 (offset 0xcbf29ce484222325, prime 0x100000001b3; len==0 → semente); tests/asm/test_hash_path.c; ctest -R asm_hash_path 9/9 (RED stub→GREEN); Docker fedora:44 clang PASS; TODO 🔍. Sem push. Sem 4755. Sem tocar crc32/parse_i64.

## 2026-08-23 - ASM-PGID

- ASM-PGID: job_setpgid.S via SYS_setpgid=109; retorno 0 / -errno (ESRCH/EPERM); sem tocar errno TLS; process.c + dispatcher.c (fallback libc se ASM OFF); tests/asm/test_job_setpgid.c; ctest -R asm_job_setpgid 6/6 + test_job (RED stub→GREEN); Docker fedora:44 clang PASS; TODO 🔍. Sem push. Sem 4755. Sem tocar crc32.

## 2026-08-23 - PLG-ABI

- PLG-ABI: plugins/abi.h C11 (MAJOR=1 MINOR=0; query/init/cmd/fini + vtable); smoke plg-abi-header.sh + target plugin_abi; TU -std=c11 prova major=1; main sem dlopen; Docker fedora:44 clang PASS; TODO 🔍. Sem push. Sem 4755. UX-25 intocado.

## 2026-08-23 - ASM-UTF8

- ASM-UTF8: utf8_width.S UAX#11 subset (ASCII=1, combining Mn=0, CJK/wide=2, invalid=-1; UTF-8 estrito); tests/asm/test_utf8.c; ctest -R asm_utf8 17/17 (RED stub→GREEN); Docker fedora:44 clang PASS; TODO 🔍. Sem push. Sem 4755. tty_mode.S intocado.

## 2026-08-23 - ASM-TTY

- ASM-TTY: tty_mode.S via ioctl TCGETS/TCSETSF; RAW/COOKED; retorno 0 / -errno (nao-TTY=-ENOTTY, modo invalido=-EINVAL); sem tocar errno TLS; tests/asm/test_tty_mode.c (PTY openpty, sem display :0); ctest -R asm_tty 5/5 (RED stub→GREEN); Docker fedora:44 clang PASS; TODO 🔍. Sem push. Sem 4755.

## 2026-08-23 - CXX-00

- CXX-00: alvo configsh C++23 (src/cxx/main.cpp stub help+exit 0; -fno-exceptions -fno-rtti); liga tty_mode.S + utf8_width.S se PETRUSH_ASM; ldd: libstdc++ so em configsh (petrush limpo); smoke cxx00-ldd.sh + target cxx00 (RED sem alvo → GREEN); Docker fedora:44 clang PASS; TODO 🔍. Sem push. Sem 4755.

## 2026-08-23 - I18N-GETTEXT

- I18N-GETTEXT: po/ en+pt_BR+es_419 (msgid=en); i18n.h/i18n.c bindtextdomain; msgfmt no CMake (petrush_mo); smoke i18n-gettext.sh + target i18n_gettext (RED missing po → GREEN); Docker fedora:44 clang+gettext PASS; ASM sem gettext; TODO 🔍. Sem push. Sem 4755.

## 2026-08-23 - ASM-WAI

- ASM-WAI: builtin `wai` + `petrush_wai_scan` (entrada `wai_scan.S` → `petrush_wai_scan_impl` em `mid/wai.c`); flags `-disk -video -mem -audio -camera -keyboard -usb -pci -battery -thermal -cpu -board`; overlay via `petrush_wai_set_root`; sem serial/uuid; ctest -R asm_wai 11/11; Docker fedora:44 clang PASS; TODO 🔍. Sem push. Sem 4755. configsh/CXX-TUI intocado.
