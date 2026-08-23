#!/usr/bin/env bash
# ASM-00: gate de toolchain (GAS/Clang, sem NASM).
# Prova: clang -c empty.S, cmake LANGUAGES C CXX ASM + PETRUSH_ASM, ASan nao em ASM.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
WORKDIR="${TMPDIR:-/var/tmp}/petrush-asm00-$$"
mkdir -p "$WORKDIR"
cleanup() { rm -rf "$WORKDIR"; }
trap cleanup EXIT

echo "=== ASM-00: clang -c src/asm/empty.S ==="
clang -c -I"$ROOT/src" "$ROOT/src/asm/empty.S" -o "$WORKDIR/empty.o"
if command -v readelf >/dev/null 2>&1; then
  readelf -S "$WORKDIR/empty.o" | grep -q 'note.GNU-stack'
  echo "OK: .note.GNU-stack presente no objeto"
elif command -v llvm-readelf >/dev/null 2>&1; then
  llvm-readelf -S "$WORKDIR/empty.o" | grep -q 'note.GNU-stack'
  echo "OK: .note.GNU-stack presente no objeto"
else
  echo "WARN: readelf ausente; objeto gerado mesmo assim"
fi

echo "=== ASM-00: cmake configure (PETRUSH_ASM default / ON) ==="
cmake -B "$WORKDIR/build" -S "$ROOT" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DPETRUSH_ASM=ON \
  -DENABLE_COVERAGE=OFF
grep -q 'project(petrush LANGUAGES C CXX ASM)' "$ROOT/CMakeLists.txt"
# ASan so em C/C++: CMAKE_ASM_FLAGS do cache nao deve carregar fsanitize
if grep -E 'CMAKE_ASM_FLAGS.*fsanitize' "$WORKDIR/build/CMakeCache.txt" >/dev/null 2>&1; then
  echo "FAIL: sanitizer vazou para CMAKE_ASM_FLAGS" >&2
  exit 1
fi
echo "OK: configure com PETRUSH_ASM=ON"

echo "=== ASM-00: build petrush (liga empty.S) ==="
cmake --build "$WORKDIR/build" -j"${CMAKE_BUILD_PARALLEL_LEVEL:-2}" --target petrush
echo "OK: petrush linkou com stub ASM"

echo "=== ASM-00: PETRUSH_ASM=ON em arch falso deve falhar configure ==="
FAKE_TC="$WORKDIR/fake-aarch64.cmake"
cat > "$FAKE_TC" <<'EOF'
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_ASM_COMPILER clang)
EOF
set +e
cmake -B "$WORKDIR/build-fail" -S "$ROOT" \
  -DCMAKE_TOOLCHAIN_FILE="$FAKE_TC" \
  -DPETRUSH_ASM=ON \
  -DENABLE_COVERAGE=OFF \
  >"$WORKDIR/fail.log" 2>&1
rc=$?
set -e
if [[ "$rc" -eq 0 ]]; then
  echo "FAIL: configure deveria ter abortado em aarch64 com PETRUSH_ASM=ON" >&2
  cat "$WORKDIR/fail.log" >&2
  exit 1
fi
if ! grep -q 'PETRUSH_ASM=ON requires x86_64' "$WORKDIR/fail.log"; then
  echo "FAIL: FATAL_ERROR esperado nao apareceu" >&2
  cat "$WORKDIR/fail.log" >&2
  exit 1
fi
echo "OK: configure falhou como esperado em arch nao-x86_64"

echo "=== ASM-00 PASS ==="
