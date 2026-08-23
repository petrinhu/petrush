#!/usr/bin/env bash
# PLG-ABI: gate de header C11 (major=1, query/init/cmd/fini).
# So compilacao (-c). Sem dlopen. Sem 4755. Nao reabre UX-25.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
WORKDIR="${TMPDIR:-/var/tmp}/petrush-plg-abi-$$"
mkdir -p "$WORKDIR"
cleanup() { rm -rf "$WORKDIR"; }
trap cleanup EXIT

CC="${CC:-clang}"

echo "=== PLG-ABI: artefactos presentes ==="
test -f "$ROOT/plugins/abi.h"
echo "OK: plugins/abi.h"

ABI_H="$ROOT/plugins/abi.h"

echo "=== PLG-ABI: major=1 e minor definido ==="
if ! grep -E -q '^[[:space:]]*#define[[:space:]]+PETRUSH_PLUGIN_ABI_MAJOR[[:space:]]+1([[:space:]]|$)' "$ABI_H"; then
  echo "FAIL: PETRUSH_PLUGIN_ABI_MAJOR != 1 em plugins/abi.h" >&2
  grep -n 'PETRUSH_PLUGIN_ABI_MAJOR' "$ABI_H" >&2 || true
  exit 1
fi
if ! grep -E -q '^[[:space:]]*#define[[:space:]]+PETRUSH_PLUGIN_ABI_MINOR[[:space:]]+[0-9]+' "$ABI_H"; then
  echo "FAIL: PETRUSH_PLUGIN_ABI_MINOR ausente em plugins/abi.h" >&2
  exit 1
fi
echo "OK: ABI major=1 + minor"

echo "=== PLG-ABI: entry points query/init/cmd/fini ==="
for sym in \
  petrush_plugin_query \
  petrush_plugin_init \
  petrush_plugin_cmd \
  petrush_plugin_fini
do
  if ! grep -E -q "\\b${sym}\\b" "$ABI_H"; then
    echo "FAIL: simbolo ausente em plugins/abi.h: $sym" >&2
    exit 1
  fi
done
echo "OK: query/init/cmd/fini listados"

echo "=== PLG-ABI: tipos info/host/abi ==="
for ty in petrush_plugin_info_t petrush_plugin_host_t petrush_plugin_abi_t; do
  if ! grep -E -q "\\b${ty}\\b" "$ABI_H"; then
    echo "FAIL: tipo ausente em plugins/abi.h: $ty" >&2
    exit 1
  fi
done
echo "OK: tipos presentes"

echo "=== PLG-ABI: main sem dlopen directo (loader = plugin_load / PLG-LOAD) ==="
MAIN="$ROOT/src/main.c"
if grep -E -n '\b(dlopen|dlsym|dlclose|dlerror)\b' "$MAIN"; then
  echo "FAIL: src/main.c referencia dlopen/dlsym (deve viver em plugin_load.c)" >&2
  exit 1
fi
# libdl no petrush e permitido desde PLG-LOAD (OpenSSL+dl no unpriv; pudod continua sem).
echo "OK: main sem dlopen"

echo "=== PLG-ABI: clang -std=c11 -c TU com #include abi.h ==="
"$CC" -std=c11 -c \
  -I"$ROOT/plugins" \
  -Wall -Wextra -Wpedantic -Werror \
  -o "$WORKDIR/plugin_abi_include.o" \
  "$ROOT/tests/smoke/plugin_abi_include.c"
echo "OK: TU C11 compilou (major=1)"

echo "=== PLG-ABI: PASS ==="
