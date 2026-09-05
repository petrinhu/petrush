#!/bin/bash
# OSH-20: trap ACTION EXIT (disparo na saida; source nao dispara)
# Usage: ./osh20-trap-exit.sh /path/to/petrush
# Expects exit 0 se todos passarem.

set -euo pipefail

PETRUSH="${1:-./build/petrush}"
PASS=0
FAIL=0
TMPROOT=$(mktemp -d /var/tmp/petrush-osh20-XXXXXX)
cleanup() { rm -rf "$TMPROOT"; }
trap cleanup EXIT

pass() { echo "PASS: $1"; PASS=$((PASS+1)); }
fail() { echo "FAIL: $1"; echo "  detail: $2"; FAIL=$((FAIL+1)); }

echo "=== OSH-20 trap EXIT smoke ($PETRUSH) ==="

# --- 1) fim normal: hi depois bye, rc 0 ---
script1="$TMPROOT/normal.sh"
cat > "$script1" <<'SCRIPT'
trap 'echo bye' EXIT
echo hi
SCRIPT
rc1=0
out1=$("$PETRUSH" "$script1" 2>&1) || rc1=$?
if [ "$rc1" -eq 0 ] && [ "$out1" = $'hi\nbye' ]; then
    pass "normal-exit-runs-trap"
else
    fail "normal-exit-runs-trap" "rc=$rc1 out=[$out1]"
fi

# --- 2) $? dentro do trap = status previsto (false sem -e) ---
script2="$TMPROOT/status.sh"
cat > "$script2" <<'SCRIPT'
trap 'echo $?' EXIT
false
SCRIPT
rc2=0
out2=$("$PETRUSH" "$script2" 2>&1) || rc2=$?
if [ "$rc2" -eq 1 ] && [ "$out2" = "1" ]; then
    pass "trap-sees-planned-status"
else
    fail "trap-sees-planned-status" "rc=$rc2 out=[$out2]"
fi

# --- 3) set -e abort ainda corre EXIT; nao imprime x ---
script3="$TMPROOT/errexit-abort.sh"
cat > "$script3" <<'SCRIPT'
set -e
trap 'echo e' EXIT
false
echo x
SCRIPT
rc3=0
out3=$("$PETRUSH" "$script3" 2>&1) || rc3=$?
if [ "$rc3" -ne 0 ] && printf '%s\n' "$out3" | grep -qx 'e' \
   && ! printf '%s\n' "$out3" | grep -qx 'x'; then
    pass "errexit-abort-still-runs-exit"
else
    fail "errexit-abort-still-runs-exit" "rc=$rc3 out=[$out3]"
fi

# --- 4) source NAO dispara EXIT; pai dispara no fim ---
other="$TMPROOT/other.sh"
cat > "$other" <<'SCRIPT'
echo sourced
SCRIPT
script4="$TMPROOT/source.sh"
cat > "$script4" <<'SCRIPT'
trap 'echo s' EXIT
source __OTHER__
echo after-source
SCRIPT
# embutir caminho real do other
sed -i "s|__OTHER__|$other|" "$script4"
rc4=0
out4=$("$PETRUSH" "$script4" 2>&1) || rc4=$?
# Esperado: sourced, after-source, s (uma vez no fim do pai). Sem s entre sourced e after-source.
if [ "$rc4" -eq 0 ] && [ "$out4" = $'sourced\nafter-source\ns' ]; then
    pass "source-does-not-fire-exit"
else
    fail "source-does-not-fire-exit" "rc=$rc4 out=[$out4]"
fi

# --- 5) exit dentro do EXIT nao recorre; rc do exit interno ---
script5="$TMPROOT/reenter.sh"
cat > "$script5" <<'SCRIPT'
trap 'exit 7' EXIT
exit 3
SCRIPT
rc5=0
out5=$("$PETRUSH" "$script5" 2>&1) || rc5=$?
if [ "$rc5" -eq 7 ] && [ -z "$out5" ]; then
    pass "exit-in-exit-no-reenter"
else
    fail "exit-in-exit-no-reenter" "rc=$rc5 out=[$out5]"
fi

# --- 6) set -e isento na acao EXIT (false no trap nao aborta a meio) ---
script6="$TMPROOT/errexit-in-trap.sh"
cat > "$script6" <<'SCRIPT'
set -e
trap 'false; echo survived' EXIT
true
SCRIPT
rc6=0
out6=$("$PETRUSH" "$script6" 2>&1) || rc6=$?
if [ "$rc6" -eq 0 ] && [ "$out6" = "survived" ]; then
    pass "errexit-exempt-in-exit-action"
else
    fail "errexit-exempt-in-exit-action" "rc=$rc6 out=[$out6]"
fi

# --- 7) cmdsubst: exit/trap do filho nao imprime no pai ---
script7="$TMPROOT/cmdsubst.sh"
cat > "$script7" <<'SCRIPT'
echo $(trap 'echo inner' EXIT; echo n)
echo done
SCRIPT
rc7=0
out7=$("$PETRUSH" "$script7" 2>&1) || rc7=$?
if [ "$rc7" -eq 0 ] && [ "$out7" = $'n\ndone' ] \
   && ! printf '%s\n' "$out7" | grep -q 'inner'; then
    pass "cmdsubst-child-no-exit-trap"
else
    fail "cmdsubst-child-no-exit-trap" "rc=$rc7 out=[$out7]"
fi

# --- 8) builtin exit n dispara EXIT com $? = n ---
script8="$TMPROOT/exit-n.sh"
cat > "$script8" <<'SCRIPT'
trap 'echo $?' EXIT
exit 42
SCRIPT
rc8=0
out8=$("$PETRUSH" "$script8" 2>&1) || rc8=$?
if [ "$rc8" -eq 42 ] && [ "$out8" = "42" ]; then
    pass "builtin-exit-fires-with-status"
else
    fail "builtin-exit-fires-with-status" "rc=$rc8 out=[$out8]"
fi

echo "=== OSH-20: $PASS passed, $FAIL failed ==="
if [ "$FAIL" -ne 0 ]; then
    exit 1
fi
if [ "$PASS" -lt 6 ]; then
    echo "FAIL: expected >=6 passes, got $PASS"
    exit 1
fi
exit 0
