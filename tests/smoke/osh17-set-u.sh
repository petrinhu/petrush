#!/bin/bash
# OSH-17: set -u / nounset
# Usage: ./osh17-set-u.sh /path/to/petrush
# Expects exit 0 se todos passarem.

set -euo pipefail

PETRUSH="${1:-./build/petrush}"
PASS=0
FAIL=0
TMPROOT=$(mktemp -d /var/tmp/petrush-osh17-XXXXXX)
cleanup() { rm -rf "$TMPROOT"; }
trap cleanup EXIT

pass() { echo "PASS: $1"; PASS=$((PASS+1)); }
fail() { echo "FAIL: $1"; echo "  detail: $2"; FAIL=$((FAIL+1)); }

echo "=== OSH-17 set -u nounset smoke ($PETRUSH) ==="

# --- 1) $UNSET aborta script ≠0 ---
script1="$TMPROOT/unset.sh"
cat > "$script1" <<'SCRIPT'
set -u
echo $OSH17_UNSET_VAR
echo should-not-run
SCRIPT
rc1=0
out1=$("$PETRUSH" "$script1" 2>&1) || rc1=$?
if [ "$rc1" -ne 0 ] && ! printf '%s\n' "$out1" | grep -q 'should-not-run'; then
    pass "unset-aborts-script"
else
    fail "unset-aborts-script" "rc=$rc1 out=[$out1]"
fi

# --- 2) ${UNSET:-ok} segue ---
script2="$TMPROOT/default.sh"
cat > "$script2" <<'SCRIPT'
set -u
echo ${OSH17_UNSET_VAR:-ok}
echo after
SCRIPT
rc2=0
out2=$("$PETRUSH" "$script2" 2>&1) || rc2=$?
if [ "$rc2" -eq 0 ] && [ "$out2" = $'ok\nafter' ]; then
    pass "default-op-continues"
else
    fail "default-op-continues" "rc=$rc2 out=[$out2]"
fi

# --- 3) var set-vazia nao aborta (export NAME=; -u nao e -z) ---
script3="$TMPROOT/empty.sh"
cat > "$script3" <<'SCRIPT'
set -u
export OSH17_EMPTY=
echo "x${OSH17_EMPTY}y"
echo after
SCRIPT
rc3=0
out3=$("$PETRUSH" "$script3" 2>&1) || rc3=$?
if [ "$rc3" -eq 0 ] && [ "$out3" = $'xy\nafter' ]; then
    pass "set-empty-ok"
else
    fail "set-empty-ok" "rc=$rc3 out=[$out3]"
fi

# --- 4) $@ vazio nao aborta ---
script4="$TMPROOT/at.sh"
cat > "$script4" <<'SCRIPT'
set -u
set --
echo "n=$#"
echo after
SCRIPT
rc4=0
out4=$("$PETRUSH" "$script4" 2>&1) || rc4=$?
if [ "$rc4" -eq 0 ] && printf '%s\n' "$out4" | grep -q 'n=0' \
   && printf '%s\n' "$out4" | grep -q 'after'; then
    pass "at-empty-ok"
else
    fail "at-empty-ok" "rc=$rc4 out=[$out4]"
fi

# --- 5) quoted here-doc $UNSET literal ---
script5="$TMPROOT/hq.sh"
cat > "$script5" <<'SCRIPT'
set -u
cat <<'EOF'
val=$OSH17_UNSET_VAR
EOF
echo after
SCRIPT
rc5=0
out5=$("$PETRUSH" "$script5" 2>&1) || rc5=$?
if [ "$rc5" -eq 0 ] && [ "$out5" = $'val=$OSH17_UNSET_VAR\nafter' ]; then
    pass "quoted-heredoc-literal"
else
    fail "quoted-heredoc-literal" "rc=$rc5 out=[$out5]"
fi

# --- 6) unquoted here-doc aborta ---
script6="$TMPROOT/hu.sh"
cat > "$script6" <<'SCRIPT'
set -u
cat <<EOF
val=$OSH17_UNSET_VAR
EOF
echo should-not-run
SCRIPT
rc6=0
out6=$("$PETRUSH" "$script6" 2>&1) || rc6=$?
if [ "$rc6" -ne 0 ] && ! printf '%s\n' "$out6" | grep -q 'should-not-run'; then
    pass "unquoted-heredoc-aborts"
else
    fail "unquoted-heredoc-aborts" "rc=$rc6 out=[$out6]"
fi

# --- 7) set +u restaura vazio ---
script7="$TMPROOT/plusu.sh"
cat > "$script7" <<'SCRIPT'
set -u
set +u
echo "x${OSH17_UNSET_VAR}y"
echo after
SCRIPT
rc7=0
out7=$("$PETRUSH" "$script7" 2>&1) || rc7=$?
if [ "$rc7" -eq 0 ] && [ "$out7" = $'xy\nafter' ]; then
    pass "plus-u-restores-empty"
else
    fail "plus-u-restores-empty" "rc=$rc7 out=[$out7]"
fi

echo "=== Result: $PASS passed, $FAIL failed ==="
if [ "$FAIL" -ne 0 ]; then
    exit 1
fi
if [ "$PASS" -lt 6 ]; then
    echo "FAIL: expected at least 6 cases, got $PASS"
    exit 1
fi
exit 0
