#!/bin/bash
# OSH-2: builtin shift [n] (default 1; n>$# erro + intactos; shift 0 no-op)
# Usage: ./osh2-shift.sh /path/to/petrush
# Expects exit 0 se todos passarem.

set -euo pipefail

PETRUSH="${1:-./build/petrush}"
PASS=0
FAIL=0
TMPROOT=$(mktemp -d /var/tmp/petrush-osh2-XXXXXX)
cleanup() { rm -rf "$TMPROOT"; }
trap cleanup EXIT

pass() { echo "PASS: $1"; PASS=$((PASS+1)); }
fail() { echo "FAIL: $1"; echo "  detail: $2"; FAIL=$((FAIL+1)); }

echo "=== OSH-2 shift smoke ($PETRUSH) ==="

# --- 1) shift (default 1): a b c → b c; $0 intacto ---
script1="$TMPROOT/shift1.sh"
printf '%s\n' \
  'shift' \
  'echo H=$#' \
  'echo P0=$0' \
  'echo P1=$1' \
  'echo P2=$2' \
  > "$script1"
chmod 0644 "$script1"
out1=$("$PETRUSH" "$script1" a b c 2>&1) || true
ec1=0
"$PETRUSH" "$script1" a b c >/dev/null 2>&1 || ec1=$?
if [[ "$ec1" -ne 0 ]]; then
    fail "shift-default-1" "exit=$ec1 out=$out1"
elif ! echo "$out1" | grep -Fq 'H=2'; then
    fail "shift-default-1" "\$# errado: $out1"
elif ! echo "$out1" | grep -Fq "P0=$script1"; then
    fail "shift-default-1" "\$0 mudou: $out1"
elif ! echo "$out1" | grep -Fq 'P1=b'; then
    fail "shift-default-1" "\$1 errado: $out1"
elif ! echo "$out1" | grep -Fq 'P2=c'; then
    fail "shift-default-1" "\$2 errado: $out1"
else
    pass "shift-default-1"
fi

# --- 2) shift 2: a b c → c ---
script2="$TMPROOT/shift2.sh"
printf '%s\n' \
  'shift 2' \
  'echo H=$#' \
  'echo P1=$1' \
  > "$script2"
out2=$("$PETRUSH" "$script2" a b c 2>&1) || true
ec2=0
"$PETRUSH" "$script2" a b c >/dev/null 2>&1 || ec2=$?
if [[ "$ec2" -ne 0 ]]; then
    fail "shift-2" "exit=$ec2 out=$out2"
elif ! echo "$out2" | grep -Fq 'H=1'; then
    fail "shift-2" "\$#: $out2"
elif ! echo "$out2" | grep -Fq 'P1=c'; then
    fail "shift-2" "\$1: $out2"
else
    pass "shift-2"
fi

# --- 3) shift 0: no-op ---
script3="$TMPROOT/shift0.sh"
printf '%s\n' \
  'shift 0' \
  'echo H=$#' \
  'echo P1=$1' \
  > "$script3"
out3=$("$PETRUSH" "$script3" a b c 2>&1) || true
ec3=0
"$PETRUSH" "$script3" a b c >/dev/null 2>&1 || ec3=$?
if [[ "$ec3" -ne 0 ]]; then
    fail "shift-0-noop" "exit=$ec3 out=$out3"
elif ! echo "$out3" | grep -Fq 'H=3'; then
    fail "shift-0-noop" "\$#: $out3"
elif ! echo "$out3" | grep -Fq 'P1=a'; then
    fail "shift-0-noop" "\$1: $out3"
else
    pass "shift-0-noop"
fi

# --- 4a) shift 4 como ultimo comando → exit != 0 ---
script4a="$TMPROOT/shift-over-exit.sh"
printf '%s\n' 'shift 4' > "$script4a"
ec4a=0
"$PETRUSH" "$script4a" a b c >/dev/null 2>&1 || ec4a=$?
if [[ "$ec4a" -eq 0 ]]; then
    fail "shift-too-many-status" "exit=0 (esperava !=0)"
else
    pass "shift-too-many-status"
fi

# --- 4b) apos shift 4 falho, posicionais intactos (script segue; sem $?) ---
script4b="$TMPROOT/shift-over-intact.sh"
printf '%s\n' \
  'shift 4 || echo FAIL' \
  'echo H=$#' \
  'echo P1=$1' \
  'echo P2=$2' \
  'echo P3=$3' \
  > "$script4b"
out4b=$("$PETRUSH" "$script4b" a b c 2>&1) || true
if ! echo "$out4b" | grep -Fq 'FAIL'; then
    fail "shift-too-many-intact" "sem FAIL (||): $out4b"
elif ! echo "$out4b" | grep -Fq 'H=3'; then
    fail "shift-too-many-intact" "\$# alterado: $out4b"
elif ! echo "$out4b" | grep -Fq 'P1=a' \
   || ! echo "$out4b" | grep -Fq 'P2=b' \
   || ! echo "$out4b" | grep -Fq 'P3=c'; then
    fail "shift-too-many-intact" "posicionais alterados: $out4b"
else
    pass "shift-too-many-intact"
fi

# --- 5) apos shift, $@ e $* refletem o novo conjunto ---
script5="$TMPROOT/shift-at.sh"
printf 'shift\nprintf "A=%%s\\n" "$@"\n' > "$script5"
out5=$("$PETRUSH" "$script5" a b c 2>&1) || true
qa=$(echo "$out5" | grep -c '^A=' || true)
if [[ "$qa" -eq 2 ]] && echo "$out5" | grep -Fq 'A=b' \
   && echo "$out5" | grep -Fq 'A=c' \
   && ! echo "$out5" | grep -Fq 'A=a'; then
    pass "shift-then-at"
else
    fail "shift-then-at" "qa=$qa out=$out5"
fi

echo
echo "OSH-2 smoke: $PASS pass, $FAIL fail"
if [[ "$FAIL" -ne 0 ]]; then
    exit 1
fi
exit 0
