#!/usr/bin/env bash
# Fase 2: Debug gcc + smoke Release apos install sem setuid
set -u
set -o pipefail
REPO=/src
LOG=/src/docs/memory/tst-t15-preci-run.log
BD_REL=/src/build-preci-rel
BD_DBG=/src/build-preci-dbg
TARGETS="petrush pudod test_parser test_process test_job test_env test_info test_pudo test_alias test_complete test_dirstack test_hist_expand test_linenoise_history test_expand test_glob test_prompt test_rc_trust test_highlight test_source test_pipeline_builtin"

log() { printf '%s\n' "$*" | tee -a "$LOG"; }
run_step() {
  local name="$1"; shift
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
log "=== PHASE2 START $(date -Iseconds) ==="

# Debug gcc
if [[ ! -d "$BD_DBG" ]]; then
  run_step cmake_dbg cmake -B "$BD_DBG" -S "$REPO" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_C_COMPILER=gcc \
    -DCMAKE_CXX_COMPILER=g++ \
    -DENABLE_COVERAGE=OFF
else
  log "reuse $BD_DBG"
  printf '0\n' > /tmp/exit_cmake_dbg
fi
# shellcheck disable=SC2086
run_step build_dbg cmake --build "$BD_DBG" -j "$(nproc)" --target $TARGETS
run_step check_dbg cmake --build "$BD_DBG" --target check
run_step smoke_dbg bash "$REPO/tests/smoke/pudo-smoke.sh" "$BD_DBG/petrush"
run_step cppcheck_dbg cmake --build "$BD_DBG" --target cppcheck
run_step clang_tidy_dbg cmake --build "$BD_DBG" --target clang-tidy

# Release smoke apos install SEM setuid (espelha path install SEC-02)
run_step install_rel cmake --install "$BD_REL" --prefix /usr/local
log "ls install:"
ls -la /usr/local/bin/petrush /usr/local/libexec/petrush-pudod /usr/local/bin/pudod 2>&1 | tee -a "$LOG" || true
# Nao aplicar setuid (brief)
run_step smoke_rel_installed bash "$REPO/tests/smoke/pudo-smoke.sh" /usr/local/bin/petrush

# Valgrind no pudod instalado (exit do processo pode ser deny=1)
if [[ -x /usr/local/libexec/petrush-pudod ]]; then
  run_step valgrind_installed valgrind --leak-check=full --error-exitcode=1 --quiet \
    /usr/local/libexec/petrush-pudod /usr/bin/true
elif [[ -x /usr/local/bin/pudod ]]; then
  run_step valgrind_installed valgrind --leak-check=full --error-exitcode=1 --quiet \
    /usr/local/bin/pudod /usr/bin/true
else
  log "EXIT_valgrind_installed=SKIP"
  printf 'SKIP\n' > /tmp/exit_valgrind_installed
fi

log "=== PHASE2 SUMMARY ==="
for f in /tmp/exit_*; do
  log "$(basename "$f")=$(cat "$f")"
done
