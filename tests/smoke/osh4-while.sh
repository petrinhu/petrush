#!/bin/bash
# OSH-4: while/do/done (status do ultimo comando; sem for/until)
# Teto anti-loop: shift (posicionais) ou ficheiro bandeira.
# Usage: ./osh4-while.sh /path/to/petrush
# Expects exit 0 se todos passarem.

set -euo pipefail

PETRUSH="${1:-./build/petrush}"
PASS=0
FAIL=0
TMPROOT=$(mktemp -d /var/tmp/petrush-osh4-XXXXXX)
cleanup() { rm -rf "$TMPROOT"; }
trap cleanup EXIT

pass() { echo "PASS: $1"; PASS=$((PASS+1)); }
fail() { echo "FAIL: $1"; echo "  detail: $2"; FAIL=$((FAIL+1)); }

echo "=== OSH-4 while smoke ($PETRUSH) ==="

# --- 1) while false; do body nao roda ---
script1="$TMPROOT/while-false.sh"
printf '%s\n' 'while false; do echo NO; done' 'echo AFTER' > "$script1"
out1=$("$PETRUSH" "$script1" 2>&1) || true
if echo "$out1" | grep -Fq 'NO'; then
    fail "while-false-skip" "body rodou: $out1"
elif ! echo "$out1" | grep -Fq 'AFTER'; then
    fail "while-false-skip" "sem AFTER: $out1"
else
    pass "while-false-skip"
fi

# --- 2) teto via shift: 3 posicionais → 3 iteracoes ---
script2="$TMPROOT/while-shift.sh"
printf '%s\n' 'while shift; do echo X; done' > "$script2"
out2=$("$PETRUSH" "$script2" a b c 2>&1) || true
n2=$(printf '%s\n' "$out2" | grep -c '^X$' || true)
if [[ "$n2" -eq 3 ]]; then
    pass "while-shift-ceiling"
else
    fail "while-shift-ceiling" "esperava 3 X, got n=$n2 out=$out2"
fi

# --- 3) teto via ficheiro bandeira (1 iteracao) ---
flag3="$TMPROOT/flag3"
: > "$flag3"
script3="$TMPROOT/while-file.sh"
printf '%s\n' \
  "while /bin/test -e $flag3; do /bin/rm -f $flag3; echo ONCE; done" \
  > "$script3"
out3=$("$PETRUSH" "$script3" 2>&1) || true
n3=$(printf '%s\n' "$out3" | grep -c '^ONCE$' || true)
if [[ "$n3" -eq 1 ]] && [[ ! -e "$flag3" ]]; then
    pass "while-file-ceiling"
else
    fail "while-file-ceiling" "n=$n3 flag_exists=$([[ -e $flag3 ]] && echo 1 || echo 0) out=$out3"
fi

# --- 4) /bin/true e /bin/false como condicao (file ceiling) ---
flag4="$TMPROOT/flag4"
: > "$flag4"
script4="$TMPROOT/while-bin.sh"
printf '%s\n' \
  "while /bin/test -e $flag4; do /bin/rm -f $flag4; echo BIN; done" \
  'while /bin/false; do echo NOBIN; done' \
  'echo AFTERBIN' \
  > "$script4"
out4=$("$PETRUSH" "$script4" 2>&1) || true
if echo "$out4" | grep -Fq 'BIN' \
   && ! echo "$out4" | grep -Fq 'NOBIN' \
   && echo "$out4" | grep -Fq 'AFTERBIN'; then
    pass "while-bin-true-false"
else
    fail "while-bin-true-false" "out=$out4"
fi

# --- 5) done quoted nao e keyword (stdout only; shift final em stderr e ok) ---
script5="$TMPROOT/while-quoted-done.sh"
printf '%s\n' 'while shift; do echo "done"; done' > "$script5"
out5=$("$PETRUSH" "$script5" one 2>/dev/null) || true
if [[ "$out5" == "done" ]]; then
    pass "while-done-quoted"
else
    fail "while-done-quoted" "out=$out5"
fi

# --- 6) while apos ; ---
script6="$TMPROOT/while-seq.sh"
printf '%s\n' 'echo A; while false; do echo B; done; echo C' > "$script6"
out6=$("$PETRUSH" "$script6" 2>&1) || true
if echo "$out6" | grep -Fq 'A' \
   && echo "$out6" | grep -Fq 'C' \
   && ! echo "$out6" | grep -Fq 'B'; then
    pass "while-after-seq"
else
    fail "while-after-seq" "out=$out6"
fi

echo
echo "OSH-4 smoke: $PASS pass, $FAIL fail"
if [[ "$FAIL" -ne 0 ]]; then
    exit 1
fi
exit 0
