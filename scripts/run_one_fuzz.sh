#!/usr/bin/env bash
# scripts/run_one_fuzz.sh - Run a single fuzz harness under afl-fuzz with a
# time bound, or fall back to corpus replay when afl-fuzz is not installed.
#
# Usage:
#   run_one_fuzz.sh <harness-path> <corpus-dir> [seconds]
#
# Behavior:
#   * If afl-fuzz is on PATH and the corpus dir contains files, launches
#     afl-fuzz in deterministic-explore mode under `timeout` for the given
#     seconds. Exits non-zero if unique_crashes > 0 or unique_hangs > 0.
#   * Otherwise iterates every file in the corpus dir through the harness
#     (stdin mode) as a smoke test. Useful for CI runners that have only
#     the afl++ *compiler* wrapper installed but not the `afl-fuzz`
#     driver, or for local dev without afl-fuzz.
#
# Output artifacts (crashes/, hangs/) land in $TMPDIR/baldr-fuzz-<harness>-$$
# and are copied to ./out/ in the current working directory when crashes
# are detected.

set -euo pipefail

if [[ $# -lt 2 ]]; then
    echo "usage: $0 <harness> <corpus-dir> [seconds]" >&2
    exit 2
fi

HARNESS="$1"
CORPUS="$2"
SECONDS_BOUND="${3:-90}"

if [[ ! -x "$HARNESS" ]]; then
    echo "run_one_fuzz: harness not found or not executable: $HARNESS" >&2
    exit 2
fi

if [[ ! -d "$CORPUS" ]]; then
    echo "run_one_fuzz: corpus dir not found: $CORPUS" >&2
    exit 2
fi

OUT_DIR="${TMPDIR:-/tmp}/baldr-fuzz-$(basename "$HARNESS")-$$"
mkdir -p "$OUT_DIR"

if command -v afl-fuzz >/dev/null 2>&1; then
    # Use a single AFL master process. The harness was compiled with the
    # afl++ wrapper, so it understands __AFL_LOOP and persistent mode.
    echo "run_one_fuzz: launching afl-fuzz for ${SECONDS_BOUND}s against $CORPUS"
    # AFL env that makes it CI-friendly (no affinity dance, skip CPUFREQ).
    export AFL_SKIP_CPUFREQ=1
    export AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1
    export AFL_NO_AFFINITY=1
    export AFL_MAP_SIZE=65536

    # `timeout` sends SIGTERM, then SIGKILL after the kill delay. We give
    # the fuzzer 10s after the SIGTERM to flush state and write stats.
    timeout --kill-after=10s "${SECONDS_BOUND}s" \
        afl-fuzz \
            -i "$CORPUS" \
            -o "$OUT_DIR" \
            -M "$(basename "$HARNESS")" \
            -- "$HARNESS" \
            || RC=$?
    RC="${RC:-0}"

    STATS="$OUT_DIR/$(basename "$HARNESS")/fuzzer_stats"
    CRASHES=0
    HANGS=0
    if [[ -f "$STATS" ]]; then
        CRASHES=$(awk '/^unique_crashes/ {print $3}' "$STATS" || echo 0)
        HANGS=$(awk '/^unique_hangs/ {print $3}' "$STATS" || echo 0)
    fi

    echo "run_one_fuzz: rc=$RC crashes=$CRASHES hangs=$HANGS"

    if [[ "${CRASHES:-0}" -gt 0 || "${HANGS:-0}" -gt 0 ]]; then
        # Propagate artifacts to ./out/ in CWD for ctest's working dir.
        mkdir -p ./out/$(basename "$HARNESS")
        cp -r "$OUT_DIR/$(basename "$HARNESS")/crashes" ./out/$(basename "$HARNESS")/ 2>/dev/null || true
        cp -r "$OUT_DIR/$(basename "$HARNESS")/hangs"    ./out/$(basename "$HARNESS")/ 2>/dev/null || true
        echo "run_one_fuzz: crashes/hangs copied to ./out/$(basename "$HARNESS")/"
        exit 1
    fi
    exit 0
fi

# afl-fuzz not installed - replay corpus as a smoke test.
echo "run_one_fuzz: afl-fuzz not on PATH; replaying corpus as smoke test"
SMOKE_RC=0
while IFS= read -r -d '' seed; do
    if ! "$HARNESS" "$seed" >/dev/null 2>&1; then
        echo "run_one_fuzz: harness returned non-zero for $seed" >&2
        cp "$seed" "$OUT_DIR/"
        SMOKE_RC=1
    fi
done < <(find "$CORPUS" -type f -print0)

if [[ "$SMOKE_RC" -ne 0 ]]; then
    mkdir -p ./out/$(basename "$HARNESS")/smoke
    cp -r "$OUT_DIR"/. ./out/$(basename "$HARNESS")/smoke/ 2>/dev/null || true
    exit 1
fi
exit 0