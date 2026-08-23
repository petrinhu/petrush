# src/asm

Assembly System V AMD64 em sintaxe GAS (`.S`), montado com Clang ou GNU `as`.
Sem NASM. Sem ASan/UBSan nestes TUs (sanitizer so em C/C++).

- `abi.inc` - macros PIC (`PETRUSH_ASM_FUNC` / `END` / `LEA_RIP` / `NOTE_GNU_STACK`).
- `empty.S` - stub de link (ASM-00); inclui `abi.inc`.
- `memeq_ct.S` - `petrush_memeq_ct` (ASM-MEMEQ; XOR|OR, sem early-out).
- `glob_match.S` - `petrush_glob_match` (ASM-GLOB; `*` `?` iterativo; `[` literal).
- `crc32.S` - `petrush_crc32` (ASM-CRC; IEEE 0xEDB88320 incremental).
- `parse_i64.S` - `petrush_parse_i64` (ASM-I64; decimal signed 64 sem overflow UB).
- `hash_path.S` - `petrush_hash_path` (ASM-HASH; FNV-1a 64).
- `job_setpgid.S` - `petrush_job_setpgid` (ASM-PGID; SYS_setpgid; 0 / -errno).
- `utf8_width.S` - `petrush_utf8_width` (ASM-UTF8; UAX#11 subset ASCII=1/combining=0/CJK=2/invalid=-1).
- `tty_mode.S` - `petrush_tty_mode` (ASM-TTY; RAW/COOKED via TCGETS/TCSETSF; 0 / -errno; nao-TTY=-ENOTTY).
- Contrato C: `include/petrush/asm.h` (ASM-ABI). Corpos nas fatias ASM-*.
