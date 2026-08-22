#!/usr/bin/env bash
# TST-T3 — build libFuzzer harnesses + short runs (parser / expand / prompt)
# Requires: clang with -fsanitize=fuzzer,address
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="${FUZZ_OUT:-/var/tmp/petrush-fuzz}"
SECS="${FUZZ_SECS:-60}"
JOBS="${FUZZ_JOBS:-1}"

mkdir -p "$OUT"/{bin,corpus_work/{parser,expand,prompt},artifacts}
cd "$ROOT"

CC="${CC:-clang}"
# gnu2x: mesmo dialecto do CMake (setenv/PATH_MAX/gethostname)
CFLAGS_COMMON=(-std=gnu2x -O1 -g -fno-omit-frame-pointer
  -fsanitize=fuzzer,address,undefined
  -I"$ROOT/include" -I"$ROOT/src"
  -Wall -Wextra -Wno-unused-parameter)

echo "[TST-T3] building harnesses → $OUT/bin"
"$CC" "${CFLAGS_COMMON[@]}" \
  -o "$OUT/bin/fuzz_parser" \
  tests/fuzz/fuzz_parser.c src/mid/parser.c

"$CC" "${CFLAGS_COMMON[@]}" \
  -o "$OUT/bin/fuzz_expand" \
  tests/fuzz/fuzz_expand.c src/mid/expand.c src/foundation/env.c

"$CC" "${CFLAGS_COMMON[@]}" \
  -o "$OUT/bin/fuzz_prompt" \
  tests/fuzz/fuzz_prompt.c src/mid/prompt.c src/foundation/env.c

cp -a tests/fuzz/corpus/parser/. "$OUT/corpus_work/parser/"
cp -a tests/fuzz/corpus/expand/. "$OUT/corpus_work/expand/"
cp -a tests/fuzz/corpus/prompt/. "$OUT/corpus_work/prompt/"

run_one() {
  local name="$1"
  local bin="$OUT/bin/fuzz_$name"
  local corp="$OUT/corpus_work/$name"
  local art="$OUT/artifacts/$name"
  mkdir -p "$art"
  echo "[TST-T3] fuzzing $name for ${SECS}s ..."
  set +e
  ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1 \
  UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  "$bin" "$corp" \
    -max_total_time="$SECS" \
    -artifact_prefix="$art/" \
    -rss_limit_mb=1024 \
    -timeout=5 \
    -jobs="$JOBS" \
    -workers="$JOBS" \
    >"$OUT/${name}.log" 2>&1
  local rc=$?
  set -e
  echo "[TST-T3] $name exit=$rc (log: $OUT/${name}.log)"
  # libFuzzer returns 0 on clean timeout; non-zero on crash/OOM
  return "$rc"
}

FAIL=0
run_one parser || FAIL=1
run_one expand || FAIL=1
run_one prompt || FAIL=1

echo "[TST-T3] summary FAIL=$FAIL"
# crash artifacts (not seed copies): libFuzzer names crash-* / leak-* / timeout-* / oom-*
find "$OUT/artifacts" -type f \( -name 'crash-*' -o -name 'leak-*' -o -name 'timeout-*' -o -name 'oom-*' \) 2>/dev/null | tee "$OUT/crash_list.txt" || true
CRASH_N=$(wc -l < "$OUT/crash_list.txt" | tr -d ' ')
echo "[TST-T3] crash_artifacts=$CRASH_N"
exit "$FAIL"
