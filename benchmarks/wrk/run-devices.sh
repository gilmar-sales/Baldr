#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

URL="${URL:-http://127.0.0.1:8080/api/devices}"
DURATION="${DURATION:-30s}"
THREADS="${THREADS:-14}"
CONNECTIONS="${CONNECTIONS:-400}"

WRK_SCRIPT="$SCRIPT_DIR/devices.lua"

if ! command -v wrk >/dev/null 2>&1; then
    echo "error: 'wrk' is not installed or not in PATH" >&2
    exit 1
fi

if [[ ! -f "$WRK_SCRIPT" ]]; then
    echo "error: wrk script not found: $WRK_SCRIPT" >&2
    exit 1
fi

echo "Benchmarking $URL"
echo "  threads=$THREADS connections=$CONNECTIONS duration=$DURATION"
echo

wrk \
    --threads "$THREADS" \
    --connections "$CONNECTIONS" \
    --duration "$DURATION" \
    --script "$WRK_SCRIPT" \
    "$URL"