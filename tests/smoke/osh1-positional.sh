#!/bin/bash
# OSH-1: posicionais $0 $1.. $# $@ $* (script/shebang + interativo $0)
# Usage: ./osh1-positional.sh /path/to/petrush
# Expects exit 0 se todos passarem.

set -euo pipefail

PETRUSH="${1:-./build/petrush}"
PASS=0
FAIL=0
TMPROOT=$(mktemp -d /var/tmp/petrush-osh1-XXXXXX)
cleanup() { rm -rf "$TMPROOT"; }
trap cleanup EXIT

pass() { echo "PASS: $1"; PASS=$((PASS+1)); }
fail() { echo "FAIL: $1"; echo "  detail: $2"; FAIL=$((FAIL+1)); }

echo "=== OSH-1 positional smoke ($PETRUSH) ==="

# --- 1) petrush script.sh a b c: $0=path $1=a $2=b $#=3 ---
script1="$TMPROOT/pos.sh"
printf 'echo P0=$0\necho P1=$1\necho P2=$2\necho PH=$#\n' > "$script1"
chmod 0644 "$script1"
out1=$("$PETRUSH" "$script1" a b c 2>&1) || true
ec1=0
"$PETRUSH" "$script1" a b c >/dev/null 2>&1 || ec1=$?
if [[ "$ec1" -ne 0 ]]; then
    fail "args-012-hash" "exit=$ec1 out=$out1"
elif ! echo "$out1" | grep -Fq "P0=$script1"; then
    fail "args-012-hash" "\$0 errado: $out1"
elif ! echo "$out1" | grep -Fq 'P1=a'; then
    fail "args-012-hash" "\$1 errado: $out1"
elif ! echo "$out1" | grep -Fq 'P2=b'; then
    fail "args-012-hash" "\$2 errado: $out1"
elif ! echo "$out1" | grep -Fq 'PH=3'; then
    fail "args-012-hash" "\$# errado: $out1"
else
    pass "args-012-hash"
fi

# --- 2) $@ e $* sem aspas → palavras ---
script2="$TMPROOT/at.sh"
printf 'printf "N=%%s\\n" $#\nprintf "A=%%s\\n" $@\nprintf "S=%%s\\n" $*\n' > "$script2"
out2=$("$PETRUSH" "$script2" a b c 2>&1) || true
if echo "$out2" | grep -Fq 'N=3' \
   && echo "$out2" | grep -Fq 'A=a' \
   && echo "$out2" | grep -Fq 'A=b' \
   && echo "$out2" | grep -Fq 'A=c' \
   && echo "$out2" | grep -Fq 'S=a' \
   && echo "$out2" | grep -Fq 'S=b' \
   && echo "$out2" | grep -Fq 'S=c'; then
    pass "at-star-unquoted-words"
else
    fail "at-star-unquoted-words" "$out2"
fi

# --- 3) "$@" → N palavras; "$*" → uma (IFS espaco) ---
script3="$TMPROOT/quoted.sh"
printf 'printf "QA=%%s\\n" "$@"\nprintf "QS=%%s\\n" "$*"\n' > "$script3"
out3=$("$PETRUSH" "$script3" a b c 2>&1) || true
qa=$(echo "$out3" | grep -c '^QA=' || true)
qs=$(echo "$out3" | grep -c '^QS=' || true)
if [[ "$qa" -eq 3 ]] && echo "$out3" | grep -Fq 'QA=a' \
   && echo "$out3" | grep -Fq 'QA=b' \
   && echo "$out3" | grep -Fq 'QA=c' \
   && [[ "$qs" -eq 1 ]] && echo "$out3" | grep -Fq 'QS=a b c'; then
    pass "at-star-quoted"
else
    fail "at-star-quoted" "qa=$qa qs=$qs out=$out3"
fi

# --- 4) shebang: args depois do script via exec ---
bindir=$(cd "$(dirname "$PETRUSH")" && pwd)
base=$(basename "$PETRUSH")
script4="$TMPROOT/shebang-pos.sh"
printf '#!/usr/bin/env %s\necho S0=$0\necho S1=$1\necho SH=$#\n' "$base" > "$script4"
chmod 0755 "$script4"
out4=$(PATH="$bindir:$PATH" "$script4" hello 2>&1) || true
ec4=0
PATH="$bindir:$PATH" "$script4" hello >/dev/null 2>&1 || ec4=$?
if [[ "$ec4" -ne 0 ]]; then
    fail "shebang-args" "exit=$ec4 out=$out4"
elif ! echo "$out4" | grep -Fq "S0=$script4"; then
    fail "shebang-args" "\$0: $out4"
elif ! echo "$out4" | grep -Fq 'S1=hello'; then
    fail "shebang-args" "\$1: $out4"
elif ! echo "$out4" | grep -Fq 'SH=1'; then
    fail "shebang-args" "\$#: $out4"
else
    pass "shebang-args"
fi

# --- 5) interativo: $0=argv[0] do petrush; $1 vazio ---
# Use basename path as invoked
out5=$(printf 'echo I0=$0\necho I1=[$1]\nexit\n' | "$PETRUSH" 2>&1) || true
# $0 should be how petrush was invoked (path we passed)
if echo "$out5" | grep -Fq "I0=$PETRUSH" && echo "$out5" | grep -Fq 'I1=[]'; then
    pass "interactive-dollar0"
else
    # also accept basename-only if argv[0] was relative differently
    basep=$(basename "$PETRUSH")
    if echo "$out5" | grep -Eq "I0=.*/$basep|I0=$basep|I0=$PETRUSH" \
       && echo "$out5" | grep -Fq 'I1=[]'; then
        pass "interactive-dollar0"
    else
        fail "interactive-dollar0" "$out5"
    fi
fi

echo
echo "OSH-1 smoke: $PASS pass, $FAIL fail"
if [[ "$FAIL" -ne 0 ]]; then
    exit 1
fi
exit 0
