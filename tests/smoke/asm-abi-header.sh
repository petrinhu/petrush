#!/usr/bin/env bash
# ASM-ABI: gate de header + macros (TU C inclui asm.h; .S inclui abi.inc).
# So compilacao (-c). Sem corpos de producao nesta fatia.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
WORKDIR="${TMPDIR:-/var/tmp}/petrush-asm-abi-$$"
mkdir -p "$WORKDIR"
cleanup() { rm -rf "$WORKDIR"; }
trap cleanup EXIT

CC="${CC:-clang}"

echo "=== ASM-ABI: artefactos presentes ==="
test -f "$ROOT/include/petrush/asm.h"
test -f "$ROOT/src/asm/abi.inc"
echo "OK: asm.h + abi.inc"

ASM_H="$ROOT/include/petrush/asm.h"

echo "=== ASM-ABI: asm.h declara os 10 simbolos ==="
for sym in \
  petrush_glob_match \
  petrush_utf8_width \
  petrush_parse_i64 \
  petrush_crc32 \
  petrush_memeq_ct \
  petrush_tty_mode \
  petrush_hash_path \
  petrush_job_setpgid \
  petrush_wai_scan \
  petrush_netcom_scan
do
  if ! grep -E -q "\\b${sym}\\b" "$ASM_H"; then
    echo "FAIL: simbolo ausente em asm.h: $sym" >&2
    exit 1
  fi
done
echo "OK: 10 simbolos listados"

echo "=== ASM-ABI: contrato memeq_ct (0 iguais / 1 diferem) ==="
# Verbatim: comentario do simbolo + linha do preambulo. Regressao f483c32:
# preambulo dizia -1 para "diferente" em memeq_ct.
if ! grep -F -q 'Retorna 0 se iguais, 1 se diferem' "$ASM_H"; then
  echo "FAIL: asm.h sem 'Retorna 0 se iguais, 1 se diferem'" >&2
  exit 1
fi
if ! grep -F -q '1  = diferem (memeq_ct)' "$ASM_H"; then
  echo "FAIL: asm.h sem '1  = diferem (memeq_ct)' no preambulo" >&2
  exit 1
fi
# Preambulo (primeiras ~15 linhas): -1 nao pode estar associado a diferente/memeq.
if head -n 15 "$ASM_H" | grep -Eiq -- '-1.*(diferem|diferente|memeq)|(diferem|diferente|memeq).*-1'; then
  echo "FAIL: preambulo de asm.h associa -1 a diferente/memeq (contrato e 1=diferem)" >&2
  head -n 15 "$ASM_H" >&2
  exit 1
fi
echo "OK: memeq_ct 0=iguais / 1=diferem; preambulo sem -1=diferente"

echo "=== ASM-ABI: abi.inc macros PIC / SysV ==="
for mac in PETRUSH_ASM_FUNC PETRUSH_ASM_END PETRUSH_LEA_RIP PETRUSH_NOTE_GNU_STACK; do
  if ! grep -q "$mac" "$ROOT/src/asm/abi.inc"; then
    echo "FAIL: macro ausente em abi.inc: $mac" >&2
    exit 1
  fi
done
# PIC: endereco via %rip (nao absoluto)
if ! grep -q '%rip' "$ROOT/src/asm/abi.inc"; then
  echo "FAIL: abi.inc sem enderecamento PIC (%rip)" >&2
  exit 1
fi
echo "OK: macros SysV PIC"

echo "=== ASM-ABI: clang -c TU C com #include petrush/asm.h ==="
"$CC" -std=c23 -c \
  -I"$ROOT/include" \
  -Wall -Wextra -Wpedantic -Werror \
  -o "$WORKDIR/asm_abi_include.o" \
  "$ROOT/tests/smoke/asm_abi_include.c"
echo "OK: TU C compilou"

echo "=== ASM-ABI: clang -c .S com #include asm/abi.inc ==="
"$CC" -c \
  -I"$ROOT/src" \
  -o "$WORKDIR/asm_abi_macros.o" \
  "$ROOT/tests/smoke/asm_abi_macros.S"
if command -v readelf >/dev/null 2>&1; then
  readelf -S "$WORKDIR/asm_abi_macros.o" | grep -q 'note.GNU-stack'
  echo "OK: .note.GNU-stack no objeto das macros"
elif command -v llvm-readelf >/dev/null 2>&1; then
  llvm-readelf -S "$WORKDIR/asm_abi_macros.o" | grep -q 'note.GNU-stack'
  echo "OK: .note.GNU-stack no objeto das macros"
fi
echo "OK: .S com abi.inc montou"

echo "=== ASM-ABI PASS ==="
