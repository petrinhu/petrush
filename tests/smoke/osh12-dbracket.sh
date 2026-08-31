#!/bin/bash
# OSH-12: [[ ... ]] minimo (FEAT-TEST + && || ! + == glob; sem =~)
# Usage: ./osh12-dbracket.sh /path/to/petrush
# Expects exit 0 se todos passarem.

set -euo pipefail

PETRUSH="${1:-./build/petrush}"
PASS=0
FAIL=0
TMPROOT=$(mktemp -d /var/tmp/petrush-osh12-XXXXXX)
cleanup() { rm -rf "$TMPROOT"; }
trap cleanup EXIT

pass() { echo "PASS: $1"; PASS=$((PASS+1)); }
fail() { echo "FAIL: $1"; echo "  detail: $2"; FAIL=$((FAIL+1)); }

echo "=== OSH-12 dbracket smoke ($PETRUSH) ==="

# --- 1) -f true ---
script1="$TMPROOT/db-f-true.sh"
touch "$TMPROOT/exists.txt"
printf '%s\n' "[[ -f $TMPROOT/exists.txt ]]" > "$script1"
st1=0
"$PETRUSH" "$script1" >/dev/null 2>&1 || st1=$?
if [[ "$st1" -eq 0 ]]; then
    pass "dbracket-f-true"
else
    fail "dbracket-f-true" "status=$st1"
fi

# --- 2) -f false ---
script2="$TMPROOT/db-f-false.sh"
printf '%s\n' "[[ -f $TMPROOT/missing-no-such.txt ]]" > "$script2"
st2=0
"$PETRUSH" "$script2" >/dev/null 2>&1 || st2=$?
if [[ "$st2" -eq 1 ]]; then
    pass "dbracket-f-false"
else
    fail "dbracket-f-false" "status=$st2 (esperado 1)"
fi

# --- 3) && curto-circuito (falso && ...) ---
script3="$TMPROOT/db-and.sh"
printf '%s\n' "[[ -f $TMPROOT/missing-no-such.txt && -d / ]]" > "$script3"
st3=0
"$PETRUSH" "$script3" >/dev/null 2>&1 || st3=$?
if [[ "$st3" -eq 1 ]]; then
    pass "dbracket-and-short"
else
    fail "dbracket-and-short" "status=$st3 (esperado 1)"
fi

# --- 4) || ---
script4="$TMPROOT/db-or.sh"
printf '%s\n' "[[ -f $TMPROOT/missing-no-such.txt || -d / ]]" > "$script4"
st4=0
"$PETRUSH" "$script4" >/dev/null 2>&1 || st4=$?
if [[ "$st4" -eq 0 ]]; then
    pass "dbracket-or"
else
    fail "dbracket-or" "status=$st4"
fi

# --- 5) ! ---
script5="$TMPROOT/db-not.sh"
printf '%s\n' "[[ ! -f $TMPROOT/missing-no-such.txt ]]" > "$script5"
st5=0
"$PETRUSH" "$script5" >/dev/null 2>&1 || st5=$?
if [[ "$st5" -eq 0 ]]; then
    pass "dbracket-not"
else
    fail "dbracket-not" "status=$st5"
fi

# --- 6) == glob unquoted ---
script6="$TMPROOT/db-glob.sh"
printf '%s\n' 'export x=foobar' '[[ $x == foo* ]]' > "$script6"
st6=0
"$PETRUSH" "$script6" >/dev/null 2>&1 || st6=$?
if [[ "$st6" -eq 0 ]]; then
    pass "dbracket-eq-glob"
else
    fail "dbracket-eq-glob" "status=$st6"
fi

# --- 7) "]]" quoted nao fecha; compara literal ---
script7="$TMPROOT/db-quoted-close.sh"
printf '%s\n' '[[ "]]" == "]]" ]]' > "$script7"
st7=0
"$PETRUSH" "$script7" >/dev/null 2>&1 || st7=$?
if [[ "$st7" -eq 0 ]]; then
    pass "dbracket-quoted-close"
else
    fail "dbracket-quoted-close" "status=$st7"
fi

# --- 8) if [[ ... ]]; then ---
script8="$TMPROOT/db-if.sh"
printf '%s\n' "if [[ -f $TMPROOT/exists.txt ]]; then echo YES; else echo NO; fi" > "$script8"
out8=$("$PETRUSH" "$script8" 2>&1) || true
if [[ "$out8" == "YES" ]]; then
    pass "dbracket-if"
else
    fail "dbracket-if" "out=$out8"
fi

# --- 9) operador invalido → status 2 ---
script9="$TMPROOT/db-badop.sh"
printf '%s\n' '[[ a -zz b ]]' > "$script9"
st9=0
"$PETRUSH" "$script9" >/dev/null 2>&1 || st9=$?
if [[ "$st9" -eq 2 ]]; then
    pass "dbracket-bad-op"
else
    fail "dbracket-bad-op" "status=$st9 (esperado 2)"
fi

echo
echo "OSH-12 smoke: $PASS pass, $FAIL fail"
if [[ "$FAIL" -ne 0 ]]; then
    exit 1
fi
exit 0
