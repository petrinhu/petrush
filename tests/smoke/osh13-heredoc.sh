#!/bin/bash
# OSH-13: <<'DELIM' quoted here-doc (literal; sem expand; script/source)
# Usage: ./osh13-heredoc.sh /path/to/petrush
# Expects exit 0 se todos passarem.

set -euo pipefail

PETRUSH="${1:-./build/petrush}"
PASS=0
FAIL=0
TMPROOT=$(mktemp -d /var/tmp/petrush-osh13-XXXXXX)
cleanup() { rm -rf "$TMPROOT"; }
trap cleanup EXIT

pass() { echo "PASS: $1"; PASS=$((PASS+1)); }
fail() { echo "FAIL: $1"; echo "  detail: $2"; FAIL=$((FAIL+1)); }

echo "=== OSH-13 heredoc quoted smoke ($PETRUSH) ==="

# --- 1) quoted: $HOME literal ---
script1="$TMPROOT/literal.sh"
cat > "$script1" <<'SCRIPT'
cat <<'EOF'
hello $HOME
EOF
SCRIPT
out1=$("$PETRUSH" "$script1" 2>&1) || true
if [ "$out1" = 'hello $HOME' ]; then
    pass "quoted-literal-dollar"
else
    fail "quoted-literal-dollar" "out=[$out1]"
fi

# --- 2) duas linhas no corpo ---
script2="$TMPROOT/twolines.sh"
cat > "$script2" <<'SCRIPT'
cat <<'E'
line-a
line-b
E
SCRIPT
out2=$("$PETRUSH" "$script2" 2>&1) || true
if [ "$out2" = $'line-a\nline-b' ]; then
    pass "two-body-lines"
else
    fail "two-body-lines" "out=[$out2]"
fi

# --- 3) unterminated → status != 0 ---
script3="$TMPROOT/unterminated.sh"
printf '%s\n' "cat <<'EOF'" "orphan" > "$script3"
ec3=0
"$PETRUSH" "$script3" >/dev/null 2>&1 || ec3=$?
if [ "$ec3" -ne 0 ]; then
    pass "unterminated-nonzero"
else
    fail "unterminated-nonzero" "exit=$ec3 esperado !=0"
fi

# --- 4) source ganha here-doc ---
src4="$TMPROOT/lib.sh"
cat > "$src4" <<'SCRIPT'
cat <<'EOF'
from-source
EOF
SCRIPT
script4="$TMPROOT/source.sh"
printf 'source %s\n' "$src4" > "$script4"
out4=$("$PETRUSH" "$script4" 2>&1) || true
if [ "$out4" = "from-source" ]; then
    pass "source-heredoc"
else
    fail "source-heredoc" "out=[$out4]"
fi

# --- 5) last-wins: dois << ; stdin = ultimo ---
script5="$TMPROOT/lastwins.sh"
cat > "$script5" <<'SCRIPT'
cat <<'A' <<'B'
skip-me
A
keep-me
B
SCRIPT
out5=$("$PETRUSH" "$script5" 2>&1) || true
if [ "$out5" = "keep-me" ]; then
    pass "two-heredoc-last-wins"
else
    fail "two-heredoc-last-wins" "out=[$out5]"
fi

# --- 6) << + > no mesmo comando ---
out6f="$TMPROOT/redir-out.txt"
script6="$TMPROOT/redir.sh"
cat > "$script6" <<SCRIPT
cat <<'EOF' > $out6f
redir-body
EOF
SCRIPT
"$PETRUSH" "$script6" >/dev/null 2>&1 || true
got6=$(cat "$out6f" 2>/dev/null || true)
if [ "$got6" = "redir-body" ]; then
    pass "heredoc-plus-stdout-redir"
else
    fail "heredoc-plus-stdout-redir" "file=[$got6]"
fi

# --- 7) regressao: < file inalterado ---
infile="$TMPROOT/in.txt"
printf 'from-file\n' > "$infile"
script7="$TMPROOT/lt.sh"
printf 'cat < %s\n' "$infile" > "$script7"
out7=$("$PETRUSH" "$script7" 2>&1) || true
if [ "$out7" = "from-file" ]; then
    pass "redir-in-regression"
else
    fail "redir-in-regression" "out=[$out7]"
fi

# --- 8) # no corpo e literal (nao comentario) ---
script8="$TMPROOT/hash.sh"
cat > "$script8" <<'SCRIPT'
cat <<'EOF'
# hash-line
EOF
SCRIPT
out8=$("$PETRUSH" "$script8" 2>&1) || true
if [ "$out8" = "# hash-line" ]; then
    pass "hash-literal-in-body"
else
    fail "hash-literal-in-body" "out=[$out8]"
fi

# Nota: <<- tokenizado em OSH-13 mas strip = OSH-15 — nao usar <<- como caso verde de strip.

echo "=== OSH-13: $PASS passed, $FAIL failed ==="
if [ "$FAIL" -ne 0 ]; then
    exit 1
fi
exit 0
