#!/usr/bin/env bash
# XDG-1: rc/history sob XDG + compat legacy; script mode sem rc; HOME fake.
# Usage: ./xdg-paths.sh /path/to/petrush
set -euo pipefail

PETRUSH="${1:-./build/petrush}"
PASS=0
FAIL=0
TMPROOT=$(mktemp -d /var/tmp/petrush-xdg-XXXXXX)
cleanup() { rm -rf "$TMPROOT"; }
trap cleanup EXIT

pass() { echo "PASS: $1"; PASS=$((PASS+1)); }
fail() { echo "FAIL: $1"; echo "  detail: $2"; FAIL=$((FAIL+1)); }

run_repl() {
    # stdin = commands; env vars already exported by caller
    printf '%s' "$1" | "$PETRUSH" 2>&1 || true
}

echo "=== XDG-1 smoke ($PETRUSH) ==="
[[ -x "$PETRUSH" ]] || { echo "FAIL: petrush missing: $PETRUSH"; exit 1; }

# --- 1) default XDG rc (~/.config/petrush/rc) no REPL ---
home1="$TMPROOT/home1"
mkdir -p "$home1/.config/petrush"
printf 'echo XDG_RC_LOADED\n' > "$home1/.config/petrush/rc"
chmod 0600 "$home1/.config/petrush/rc"
out1=$(HOME="$home1" env -u XDG_CONFIG_HOME -u XDG_STATE_HOME \
  bash -c 'printf "exit\n" | "$1"' bash "$PETRUSH") || true
if echo "$out1" | grep -Fq 'XDG_RC_LOADED'; then
    pass "repl-loads-xdg-rc"
else
    fail "repl-loads-xdg-rc" "$out1"
fi

# --- 2) XDG_CONFIG_HOME override ---
home2="$TMPROOT/home2"
xdg2="$TMPROOT/xdgcfg2"
mkdir -p "$home2" "$xdg2/petrush"
printf 'echo XDG_CFG_HOME_RC\n' > "$xdg2/petrush/rc"
chmod 0600 "$xdg2/petrush/rc"
out2=$(HOME="$home2" XDG_CONFIG_HOME="$xdg2" env -u XDG_STATE_HOME \
  bash -c 'printf "exit\n" | "$1"' bash "$PETRUSH") || true
if echo "$out2" | grep -Fq 'XDG_CFG_HOME_RC'; then
    pass "xdg-config-home-rc"
else
    fail "xdg-config-home-rc" "$out2"
fi

# --- 3) compat: ~/.petrushrc se XDG rc ausente ---
home3="$TMPROOT/home3"
mkdir -p "$home3"
printf 'echo LEGACY_RC\n' > "$home3/.petrushrc"
chmod 0600 "$home3/.petrushrc"
out3=$(HOME="$home3" env -u XDG_CONFIG_HOME -u XDG_STATE_HOME \
  bash -c 'printf "exit\n" | "$1"' bash "$PETRUSH") || true
if echo "$out3" | grep -Fq 'LEGACY_RC'; then
    pass "compat-legacy-rc"
else
    fail "compat-legacy-rc" "$out3"
fi

# --- 4) history grava em ~/.local/state/petrush/history + dir 0700 ---
home4="$TMPROOT/home4"
mkdir -p "$home4"
out4=$(HOME="$home4" env -u XDG_CONFIG_HOME -u XDG_STATE_HOME \
  bash -c 'printf "echo hist-xdg-probe\nexit\n" | "$1"' bash "$PETRUSH") || true
hist4="$home4/.local/state/petrush/history"
dir4="$home4/.local/state/petrush"
if [[ -f "$hist4" ]] && grep -Fq 'hist-xdg-probe' "$hist4"; then
    mode4=$(stat -c '%a' "$dir4")
    if [[ "$mode4" == "700" ]]; then
        pass "history-xdg-write-0700"
    else
        fail "history-xdg-write-0700" "dir mode=$mode4 (want 700); out=$out4"
    fi
else
    fail "history-xdg-write-0700" "missing hist or probe; out=$out4 ls=$(ls -laR "$home4" 2>&1)"
fi

# --- 5) compat history: le ~/.petrush_history se XDG history ausente ---
home5="$TMPROOT/home5"
mkdir -p "$home5"
printf 'legacy-hist-line\n' > "$home5/.petrush_history"
chmod 0600 "$home5/.petrush_history"
out5=$(HOME="$home5" env -u XDG_CONFIG_HOME -u XDG_STATE_HOME \
  bash -c 'printf "history\nexit\n" | "$1"' bash "$PETRUSH") || true
if echo "$out5" | grep -Fq 'legacy-hist-line'; then
    pass "compat-legacy-history"
else
    fail "compat-legacy-history" "$out5"
fi

# --- 6) script mode NAO carrega XDG rc (OSH-0) ---
home6="$TMPROOT/home6"
mkdir -p "$home6/.config/petrush"
printf 'echo RC_SHOULD_NOT_RUN\n' > "$home6/.config/petrush/rc"
chmod 0600 "$home6/.config/petrush/rc"
script6="$TMPROOT/script6.sh"
printf 'echo script-body-xdg\n' > "$script6"
out6=$(HOME="$home6" env -u XDG_CONFIG_HOME -u XDG_STATE_HOME \
  "$PETRUSH" "$script6" 2>&1) || true
if echo "$out6" | grep -Fq 'RC_SHOULD_NOT_RUN'; then
    fail "script-no-xdg-rc" "rc vazou: $out6"
elif ! echo "$out6" | grep -Fq 'script-body-xdg'; then
    fail "script-no-xdg-rc" "script nao rodou: $out6"
else
    pass "script-no-xdg-rc"
fi

# --- 7) XDG_STATE_HOME override na gravacao ---
home7="$TMPROOT/home7"
state7="$TMPROOT/xdgstate7"
mkdir -p "$home7"
out7=$(HOME="$home7" XDG_STATE_HOME="$state7" env -u XDG_CONFIG_HOME \
  bash -c 'printf "echo state-home-probe\nexit\n" | "$1"' bash "$PETRUSH") || true
hist7="$state7/petrush/history"
if [[ -f "$hist7" ]] && grep -Fq 'state-home-probe' "$hist7"; then
    pass "xdg-state-home-write"
else
    fail "xdg-state-home-write" "out=$out7 ls=$(ls -laR "$state7" 2>&1)"
fi

echo "=== XDG-1 summary: PASS=$PASS FAIL=$FAIL ==="
[[ "$FAIL" -eq 0 ]]
