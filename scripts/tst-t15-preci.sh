#!/usr/bin/env bash
# TST-T15: pre-CI local no Fedora 44 (espelha .github/workflows/ci.yml).
# Nao engole falha de cppcheck/clang-tidy. Nao aplica setuid.
set -u
set -o pipefail

REPO=/src
LOG=/src/docs/memory/tst-t15-preci-run.log
BD_REL=/src/build-preci-rel
BD_DBG=/src/build-preci-dbg
TARGETS="petrush pudod test_parser test_process test_job test_env test_info test_pudo test_alias test_complete test_dirstack test_hist_expand test_linenoise_history test_expand test_glob test_prompt test_rc_trust test_highlight test_source test_pipeline_builtin"

: > "$LOG"
log() { printf '%s\n' "$*" | tee -a "$LOG"; }
run_step() {
  local name="$1"
  shift
  log "=== STEP $name ==="
  log "CMD: $*"
  local start end rc
  start=$(date +%s)
  set +e
  "$@" >>"$LOG" 2>&1
  rc=$?
  set -e
  end=$(date +%s)
  log "EXIT_$name=$rc ELAPSED=$((end-start))s"
  printf '%s\n' "$rc" > "/tmp/exit_$name"
  return 0
}

cd "$REPO"
log "HOST=$(uname -a)"
log "DATE=$(date -Iseconds)"

run_step dnf dnf -y install \
  gcc gcc-c++ clang clang-tools-extra \
  cmake make \
  valgrind cppcheck \
  glibc-devel \
  diffutils findutils which

log "cmake=$(command -v cmake)"
gcc --version | head -1 | tee -a "$LOG"
clang --version | head -1 | tee -a "$LOG"

# --- Release gcc (minimo obrigatorio) ---
run_step cmake_rel cmake -B "$BD_REL" -S "$REPO" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=gcc \
  -DCMAKE_CXX_COMPILER=g++ \
  -DENABLE_COVERAGE=OFF

# shellcheck disable=SC2086
run_step build_rel cmake --build "$BD_REL" -j "$(nproc)" --target $TARGETS

run_step check_rel cmake --build "$BD_REL" --target check

run_step smoke_rel bash "$REPO/tests/smoke/pudo-smoke.sh" "$BD_REL/petrush"

run_step cppcheck_rel cmake --build "$BD_REL" --target cppcheck

run_step clang_tidy_rel cmake --build "$BD_REL" --target clang-tidy

# Valgrind direto (exit honesto; target cmake engole com || echo)
if [[ -x "$BD_REL/pudod" ]] && command -v valgrind >/dev/null; then
  run_step valgrind_rel valgrind --leak-check=full --error-exitcode=1 --quiet \
    "$BD_REL/pudod" /usr/bin/true
else
  log "EXIT_valgrind_rel=SKIP"
  printf 'SKIP\n' > /tmp/exit_valgrind_rel
fi

# --- Debug gcc (se Release passou nos gates duros) ---
RC_CHECK=$(cat /tmp/exit_check_rel)
RC_SMOKE=$(cat /tmp/exit_smoke_rel)
RC_CPP=$(cat /tmp/exit_cppcheck_rel)
RC_TIDY=$(cat /tmp/exit_clang_tidy_rel)
DO_DEBUG=1
if [[ "$RC_CHECK" != 0 || "$RC_SMOKE" != 0 || "$RC_CPP" != 0 || "$RC_TIDY" != 0 ]]; then
  DO_DEBUG=0
  log "SKIP Debug: Release gates falharam"
fi

if [[ "$DO_DEBUG" == 1 ]]; then
  run_step cmake_dbg cmake -B "$BD_DBG" -S "$REPO" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_C_COMPILER=gcc \
    -DCMAKE_CXX_COMPILER=g++ \
    -DENABLE_COVERAGE=OFF
  # shellcheck disable=SC2086
  run_step build_dbg cmake --build "$BD_DBG" -j "$(nproc)" --target $TARGETS
  run_step check_dbg cmake --build "$BD_DBG" --target check
  run_step smoke_dbg bash "$REPO/tests/smoke/pudo-smoke.sh" "$BD_DBG/petrush"
fi

log "=== SUMMARY ==="
for f in /tmp/exit_*; do
  log "$(basename "$f")=$(cat "$f")"
done

FAIL=0
for key in dnf cmake_rel build_rel check_rel smoke_rel cppcheck_rel clang_tidy_rel; do
  v=$(cat "/tmp/exit_$key" 2>/dev/null || echo MISSING)
  if [[ "$v" != 0 ]]; then FAIL=1; fi
done
VG=$(cat /tmp/exit_valgrind_rel 2>/dev/null || echo SKIP)
log "valgrind_note=$VG (pudod sem setuid: recusa pode ser exit!=0)"

if [[ "$DO_DEBUG" == 1 ]]; then
  for key in cmake_dbg build_dbg check_dbg smoke_dbg; do
    v=$(cat "/tmp/exit_$key" 2>/dev/null || echo MISSING)
    if [[ "$v" != 0 ]]; then FAIL=1; fi
  done
fi

log "OVERALL_FAIL=$FAIL"
exit "$FAIL"
