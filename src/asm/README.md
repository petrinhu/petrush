# src/asm

Assembly System V AMD64 em sintaxe GAS (`.S`), montado com Clang ou GNU `as`.
Sem NASM. Sem ASan/UBSan nestes TUs (sanitizer so em C/C++).

- `abi.inc` - macros PIC (`PETRUSH_ASM_FUNC` / `END` / `LEA_RIP` / `NOTE_GNU_STACK`).
- `empty.S` - stub de link (ASM-00); inclui `abi.inc`.
- Contrato C: `include/petrush/asm.h` (ASM-ABI). Corpos nas fatias ASM-*.
