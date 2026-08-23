#!/usr/bin/env bash
# PLG-LOAD / PLG-NARC: pudod NAO carrega plugin (.so / dlopen).
# Uso: plugin-pudod-no-dl.sh <pudod_bin> <repo_root>
set -euo pipefail

PUDOD_BIN="${1:?pudod binary}"
ROOT="${2:?repo root}"

echo "=== plugin_pudod_no_dl: fontes pudod sem dlopen ==="
if grep -R -n -E '\b(dlopen|dlsym|dlclose|dlerror)\b' \
    "$ROOT/src/pudod" --include='*.c' --include='*.h'; then
  echo "FAIL: src/pudod referencia dlopen/dlsym" >&2
  exit 1
fi
if grep -R -n -E 'PETRUSH_PLUGIN|plugin_load|libdl' \
    "$ROOT/src/pudod" --include='*.c' --include='*.h'; then
  echo "FAIL: src/pudod referencia plugin/libdl" >&2
  exit 1
fi
echo "OK: fontes limpas"

echo "=== plugin_pudod_no_dl: binario sem simbolos dl* ==="
if ! test -x "$PUDOD_BIN"; then
  echo "FAIL: pudod nao executavel: $PUDOD_BIN" >&2
  exit 1
fi
# dynamic symbols (se houver)
if command -v nm >/dev/null 2>&1; then
  if nm -D "$PUDOD_BIN" 2>/dev/null | grep -E ' (U|T) dl(open|sym|close|error)$'; then
    echo "FAIL: pudod importa/exporta dlopen/dlsym" >&2
    exit 1
  fi
fi
if command -v readelf >/dev/null 2>&1; then
  if readelf -d "$PUDOD_BIN" 2>/dev/null | grep -E 'NEEDED.*libdl\.so'; then
    echo "FAIL: pudod NEEDED libdl" >&2
    exit 1
  fi
fi
echo "OK: binario sem libdl/dlopen"

echo "=== plugin_pudod_no_dl: CMake pudod sem Crypto/dl ==="
if grep -n -E 'target_link_libraries\s*\(\s*pudod[^)]*\b(dl|Crypto|crypto)\b' \
    "$ROOT/CMakeLists.txt"; then
  echo "FAIL: CMake liga dl/crypto ao pudod" >&2
  exit 1
fi
echo "OK: CMake pudod isolado"

echo "=== plugin_pudod_no_dl: PASS ==="
