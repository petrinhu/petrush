# Plano C++23 + ASM + plugins

Papel 2026-08-23. Sem implementação nesta revisão de docs. Sem produção nesta máquina.

Ver `TODO.md` secção **Trilho W20+** (WSJF). Detalhe técnico: Caetano (ASM ABI, DoD das 10 funções, pastas `src/asm` `src/cxx` `plugins/abi.h`).

## Dez funções ASM (System V AMD64, GAS, Clang)

| # | Símbolo / comando | Papel |
|---|-------------------|--------|
| 1 | `wai` / `petrush_wai_scan` | Inventário sysfs: -disk -video -mem -audio -camera -keyboard -usb -pci -battery -thermal -cpu -board |
| 2 | `netcom` / `petrush_netcom_scan` | -wifi -eth -bt; -up/-down via ip/iw/iwd/bluetoothctl; EPERM sem CAP |
| 3 | `petrush_glob_match` | matcher `*` `?` (substitui C) |
| 4 | `petrush_utf8_width` | colunas UAX#11 subset |
| 5 | `petrush_parse_i64` | inteiro sem overflow UB |
| 6 | `petrush_crc32` | CRC-32 IEEE (rc), não autentica .so |
| 7 | `petrush_memeq_ct` | comparação tempo constante |
| 8 | `petrush_tty_mode` | RAW/COOKED ioctl |
| 9 | `petrush_hash_path` | FNV-1a da string |
| 10 | `petrush_job_setpgid` | syscall setpgid |

Núcleo eval OSH permanece C23. C++23 só em `configsh`. Plugins: ABI C, SHA-256, recusa world-writable.
