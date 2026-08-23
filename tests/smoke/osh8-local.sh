#!/bin/bash
# OSH-8: builtin local name[=value] so dentro de funcao; restaura ao sair
# Usage: ./osh8-local.sh /path/to/petrush
# Expects exit 0 se todos passarem.

set -euo pipefail

PETRUSH="${1:-./build/petrush}"
PASS=0
FAIL=0
TMPROOT=$(mktemp -d /var/tmp/petrush-osh8-XXXXXX)
cleanup() { rm -rf "$TMPROOT"; }
trap cleanup EXIT

pass() { echo "PASS: $1"; PASS=$((PASS+1)); }
fail() { echo "FAIL: $1"; echo "  detail: $2"; FAIL=$((FAIL+1)); }

echo "=== OSH-8 local smoke ($PETRUSH) ==="

# --- 1) local x=1: valor visivel no body ---
script1="$TMPROOT/local-vis.sh"
printf '%s\n' 'f() { local x=1; echo in:$x; }; f' > "$script1"
out1=$("$PETRUSH" "$script1" 2>&1) || true
if echo "$out1" | grep -Fq 'in:1'; then
    pass "local-visible-in-body"
else
    fail "local-visible-in-body" "out=$out1"
fi

# --- 2) valor externo restaurado ao sair ---
script2="$TMPROOT/local-restore.sh"
printf '%s\n' 'export x=outer; f() { local x=inner; echo in:$x; }; f; echo out:$x' > "$script2"
out2=$("$PETRUSH" "$script2" 2>&1) || true
if echo "$out2" | grep -Fq 'in:inner' && echo "$out2" | grep -Fq 'out:outer'; then
    pass "local-restores-outer"
else
    fail "local-restores-outer" "out=$out2"
fi

# --- 3) var inexistente: local no body; unset ao sair ---
script3="$TMPROOT/local-unset-exit.sh"
printf '%s\n' 'unset y; f() { local y=1; echo in:$y; }; f; echo out:${y:-UNSET}' > "$script3"
out3=$("$PETRUSH" "$script3" 2>&1) || true
if echo "$out3" | grep -Fq 'in:1' && echo "$out3" | grep -Fq 'out:UNSET'; then
    pass "local-unsets-on-exit"
else
    fail "local-unsets-on-exit" "out=$out3"
fi

# --- 4) local x sem =: unset local (mascara outer) ---
script4="$TMPROOT/local-bare.sh"
printf '%s\n' 'export z=outer; f() { local z; echo in:${z:-UNSET}; }; f; echo out:$z' > "$script4"
out4=$("$PETRUSH" "$script4" 2>&1) || true
if echo "$out4" | grep -Fq 'in:UNSET' && echo "$out4" | grep -Fq 'out:outer'; then
    pass "local-bare-unsets"
else
    fail "local-bare-unsets" "out=$out4"
fi

# --- 5) fora de funcao: status != 0 e script segue ---
script5="$TMPROOT/local-outside.sh"
printf '%s\n' 'local x=1; echo SURVIVED' > "$script5"
out5=$("$PETRUSH" "$script5" 2>&1) || true
st5=0
"$PETRUSH" "$script5" >/dev/null 2>&1 || st5=$?
# script com local+echo: exit e o do echo se local nao matar; checar SURVIVED
if echo "$out5" | grep -Fq 'SURVIVED'; then
    pass "local-outside-continues"
else
    fail "local-outside-continues" "out=$out5"
fi

script5b="$TMPROOT/local-outside-st.sh"
printf '%s\n' 'local x=1' > "$script5b"
st5b=0
"$PETRUSH" "$script5b" >/dev/null 2>&1 || st5b=$?
if [[ "$st5b" -ne 0 ]]; then
    pass "local-outside-status"
else
    fail "local-outside-status" "st=0 (esperava !=0)"
fi

# --- 6) sem flags local -* (status da fn != 0; script segue) ---
script6="$TMPROOT/local-flag.sh"
printf '%s\n' 'f() { local -a x; }; f || echo REJECTED; echo AFTER' > "$script6"
out6=$("$PETRUSH" "$script6" 2>&1) || true
if echo "$out6" | grep -Fq 'options not supported' \
   && echo "$out6" | grep -Fq 'REJECTED' \
   && echo "$out6" | grep -Fq 'AFTER'; then
    pass "local-rejects-flags"
else
    fail "local-rejects-flags" "out=$out6"
fi

# --- 7) return ainda restaura local ---
script7="$TMPROOT/local-return.sh"
printf '%s\n' 'export r=outer; f() { local r=inner; return 0; }; f; echo out:$r' > "$script7"
out7=$("$PETRUSH" "$script7" 2>&1) || true
if echo "$out7" | grep -Fq 'out:outer'; then
    pass "local-restore-on-return"
else
    fail "local-restore-on-return" "out=$out7"
fi

echo
echo "OSH-8 smoke: $PASS pass, $FAIL fail"
if [[ "$FAIL" -ne 0 ]]; then
    exit 1
fi
exit 0
