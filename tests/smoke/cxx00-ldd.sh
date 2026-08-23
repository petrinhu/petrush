#!/usr/bin/env bash
# CXX-00: gate ldd - petrush sem libstdc++; configsh C++23 com libstdc++.
# Flags: -fno-exceptions -fno-rtti. Stub imprime help e sai 0.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
WORKDIR="${TMPDIR:-/var/tmp}/petrush-cxx00-$$"
mkdir -p "$WORKDIR"
cleanup() { rm -rf "$WORKDIR"; }
trap cleanup EXIT

echo "=== CXX-00: cmake configure + build petrush + configsh ==="
cmake -B "$WORKDIR/build" -S "$ROOT" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DPETRUSH_ASM=ON \
  -DENABLE_COVERAGE=OFF
cmake --build "$WORKDIR/build" -j"${CMAKE_BUILD_PARALLEL_LEVEL:-2}" --target petrush configsh

PETRUSH_BIN="$WORKDIR/build/petrush"
CONFIGSH_BIN="$WORKDIR/build/configsh"

if [[ ! -x "$PETRUSH_BIN" ]]; then
  echo "FAIL: petrush ausente" >&2
  exit 1
fi
if [[ ! -x "$CONFIGSH_BIN" ]]; then
  echo "FAIL: configsh ausente" >&2
  exit 1
fi

echo "=== CXX-00: ldd petrush NAO deve ter libstdc++ / libc++ ==="
PETRUSH_LDD="$(ldd "$PETRUSH_BIN")"
echo "$PETRUSH_LDD"
if echo "$PETRUSH_LDD" | grep -E 'libstdc\+\+|libc\+\+' >/dev/null; then
  echo "FAIL: petrush ligou libstdc++/libc++ (vazamento C++ no REPL)" >&2
  exit 1
fi
echo "OK: petrush sem libstdc++/libc++"

echo "=== CXX-00: ldd configsh DEVE ter libstdc++ (ou libc++) ==="
CONFIGSH_LDD="$(ldd "$CONFIGSH_BIN")"
echo "$CONFIGSH_LDD"
if ! echo "$CONFIGSH_LDD" | grep -E 'libstdc\+\+|libc\+\+' >/dev/null; then
  echo "FAIL: configsh nao ligou libstdc++/libc++ (nao parece binario C++)" >&2
  exit 1
fi
echo "OK: configsh com runtime C++"

echo "=== CXX-00: flags -fno-exceptions -fno-rtti no alvo configsh ==="
# compile_commands.json (CMAKE_EXPORT_COMPILE_COMMANDS=ON no projeto)
CCDB="$WORKDIR/build/compile_commands.json"
if [[ ! -f "$CCDB" ]]; then
  echo "FAIL: compile_commands.json ausente" >&2
  exit 1
fi
# Uma linha do main.cpp do configsh deve carregar as duas flags
if ! grep -E 'src/cxx/main\.cpp' "$CCDB" | grep -q -- '-fno-exceptions'; then
  echo "FAIL: -fno-exceptions ausente na compilacao de src/cxx/main.cpp" >&2
  exit 1
fi
if ! grep -E 'src/cxx/main\.cpp' "$CCDB" | grep -q -- '-fno-rtti'; then
  echo "FAIL: -fno-rtti ausente na compilacao de src/cxx/main.cpp" >&2
  exit 1
fi
echo "OK: -fno-exceptions -fno-rtti presentes"

echo "=== CXX-00: stub help + exit 0 ==="
set +e
OUT="$("$CONFIGSH_BIN" 2>&1)"
RC=$?
set -e
if [[ "$RC" -ne 0 ]]; then
  echo "FAIL: configsh exit=$RC (esperado 0)" >&2
  printf '%s\n' "$OUT" >&2
  exit 1
fi
if ! printf '%s' "$OUT" | grep -qi 'configsh'; then
  echo "FAIL: help nao menciona configsh" >&2
  printf '%s\n' "$OUT" >&2
  exit 1
fi
echo "OK: help + exit 0"

echo "=== CXX-00: CMakeLists declara alvo configsh ==="
grep -q 'add_executable(configsh' "$ROOT/CMakeLists.txt"
grep -q 'fno-exceptions' "$ROOT/CMakeLists.txt"
grep -q 'fno-rtti' "$ROOT/CMakeLists.txt"
echo "OK: CMakeLists configsh + flags"

echo "=== CXX-00 PASS ==="
