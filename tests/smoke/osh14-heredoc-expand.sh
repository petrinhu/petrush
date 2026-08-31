#!/bin/bash
# OSH-14: <<DELIM unquoted — expand $ / $( ) / $(( )) no corpo; quoted literal
# Usage: ./osh14-heredoc-expand.sh /path/to/petrush
# Expects exit 0 se todos passarem.

set -euo pipefail

PETRUSH="${1:-./build/petrush}"
PASS=0
FAIL=0
TMPROOT=$(mktemp -d /var/tmp/petrush-osh14-XXXXXX)
cleanup() { rm -rf "$TMPROOT"; }
trap cleanup EXIT

pass() { echo "PASS: $1"; PASS=$((PASS+1)); }
fail() { echo "FAIL: $1"; echo "  detail: $2"; FAIL=$((FAIL+1)); }

echo "=== OSH-14 heredoc unquoted expand smoke ($PETRUSH) ==="

# --- 1) $VAR expande ---
script1="$TMPROOT/var.sh"
cat > "$script1" <<'SCRIPT'
export OSH14_V=hello
cat <<EOF
val=$OSH14_V
EOF
SCRIPT
out1=$("$PETRUSH" "$script1" 2>&1) || true
if [ "$out1" = 'val=hello' ]; then
    pass "unquoted-var"
else
    fail "unquoted-var" "out=[$out1]"
fi

# --- 2) $(cmd) / builtin ---
script2="$TMPROOT/cmdsubst.sh"
cat > "$script2" <<'SCRIPT'
cat <<EOF
x=$(echo hi)
EOF
SCRIPT
out2=$("$PETRUSH" "$script2" 2>&1) || true
if [ "$out2" = 'x=hi' ]; then
    pass "unquoted-cmdsubst"
else
    fail "unquoted-cmdsubst" "out=[$out2]"
fi

# --- 3) $(( )) ---
script3="$TMPROOT/arith.sh"
cat > "$script3" <<'SCRIPT'
cat <<EOF
n=$((1+1))
EOF
SCRIPT
out3=$("$PETRUSH" "$script3" 2>&1) || true
if [ "$out3" = 'n=2' ]; then
    pass "unquoted-arith"
else
    fail "unquoted-arith" "out=[$out3]"
fi

# --- 4) quoted <<'E' permanece literal ---
script4="$TMPROOT/quoted.sh"
cat > "$script4" <<'SCRIPT'
export OSH14_V=hello
cat <<'EOF'
val=$OSH14_V
EOF
SCRIPT
out4=$("$PETRUSH" "$script4" 2>&1) || true
if [ "$out4" = 'val=$OSH14_V' ]; then
    pass "quoted-still-literal"
else
    fail "quoted-still-literal" "out=[$out4]"
fi

# --- 5) concat pre$VAR post ---
script5="$TMPROOT/concat.sh"
cat > "$script5" <<'SCRIPT'
export OSH14_V=mid
cat <<EOF
pre${OSH14_V}post
EOF
SCRIPT
out5=$("$PETRUSH" "$script5" 2>&1) || true
if [ "$out5" = 'premidpost' ]; then
    pass "unquoted-concat"
else
    fail "unquoted-concat" "out=[$out5]"
fi

# --- 6) * no corpo NAO globa ---
touch "$TMPROOT/a.txt" "$TMPROOT/b.txt"
script6="$TMPROOT/noglob.sh"
cat > "$script6" <<SCRIPT
cd $TMPROOT
cat <<EOF
pat=*
EOF
SCRIPT
out6=$("$PETRUSH" "$script6" 2>&1) || true
if [ "$out6" = 'pat=*' ]; then
    pass "no-glob-in-body"
else
    fail "no-glob-in-body" "out=[$out6]"
fi

# --- 7) ~/x no corpo NAO vira \$HOME/x ---
script7="$TMPROOT/notilde.sh"
cat > "$script7" <<'SCRIPT'
cat <<EOF
path=~/x
EOF
SCRIPT
out7=$("$PETRUSH" "$script7" 2>&1) || true
if [ "$out7" = 'path=~/x' ]; then
    pass "no-tilde-in-body"
else
    fail "no-tilde-in-body" "out=[$out7]"
fi

# --- 8) regressao OSH-13: duas linhas quoted ---
script8="$TMPROOT/osh13reg.sh"
cat > "$script8" <<'SCRIPT'
cat <<'E'
line-a
line-b
E
SCRIPT
out8=$("$PETRUSH" "$script8" 2>&1) || true
if [ "$out8" = $'line-a\nline-b' ]; then
    pass "osh13-quoted-regression"
else
    fail "osh13-quoted-regression" "out=[$out8]"
fi

echo "=== OSH-14: $PASS passed, $FAIL failed ==="
if [ "$FAIL" -ne 0 ]; then
    exit 1
fi
exit 0
