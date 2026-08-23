#!/usr/bin/env bash
# ASM-NET: netcom -up sem CAP_NET_ADMIN → EPERM claro, exit 1, sem hang.
# Roda em netns (unshare user+net) com cap_net_admin dropado.
# Sem root real. Sem 4755. Timeout curto (anti-hang).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="${1:-$ROOT/build}"
PETRUSH="${BUILD}/petrush"

fail() { echo "FAIL: $*" >&2; exit 1; }

[[ -x "$PETRUSH" ]] || fail "petrush missing: $PETRUSH (pass build dir as \$1)"

WORKDIR="${TMPDIR:-/var/tmp}/petrush-netcom-eperm-$$"
mkdir -p "$WORKDIR"
cleanup() { rm -rf "$WORKDIR"; }
trap cleanup EXIT

SCRIPT="$WORKDIR/up.petrush"
printf 'netcom -up lo\n' > "$SCRIPT"
chmod 0644 "$SCRIPT"

echo "=== ASM-NET smoke: netcom -up without CAP_NET_ADMIN (netns) ==="

# Prefer unshare user+net + capsh drop. Fallback: bare timeout (host CapEff=0).
run_eperm() {
  local out rc
  if command -v unshare >/dev/null 2>&1 && command -v capsh >/dev/null 2>&1; then
    set +e
    out="$(timeout 5s unshare --user --map-root-user --net \
      capsh --drop=cap_net_admin -- -c \
      "timeout 3s '$PETRUSH' '$SCRIPT'" 2>&1)"
    rc=$?
    set -e
  else
    set +e
    out="$(timeout 3s "$PETRUSH" "$SCRIPT" 2>&1)"
    rc=$?
    set -e
  fi
  printf '%s\n' "$out"
  # timeout(1) → 124; must not happen (hang)
  if [[ "$rc" -eq 124 ]]; then
    fail "netcom -up hung (timeout); expected fast EPERM"
  fi
  # petrush script exit propagates; expect 1
  if [[ "$rc" -ne 1 ]]; then
    fail "expected exit 1, got $rc; out=$out"
  fi
  echo "$out" | grep -Eqi 'EPERM|CAP_NET_ADMIN' \
    || fail "stderr/out missing EPERM/CAP_NET_ADMIN; out=$out"
}

run_eperm
echo "OK: ASM-NET EPERM sem CAP (netns), exit 1, sem hang"
