#!/usr/bin/env bash
#
# One command to check a chunk. Run from anywhere:
#
#     ./scripts/check.sh          build + test          (the inner loop)
#     ./scripts/check.sh --all    also ASan/UBSan+TSan  (before calling a chunk done)
#
# Run it BEFORE every commit. Unit 0 learned that the hard way: a chunk was
# committed with a test file that did not compile, because the check had run
# and its failure was read past.
#
# Build directories are gitignored and persist, so the fast path rebuilds
# whatever changed rather than reconfiguring.
set -euo pipefail

cd "$(dirname "$0")/.."

# Inherited from unit 0's decision 12: steady_clock only. Enforced here rather
# than in review, with comment lines dropped so a file explaining the ban does
# not trip it. src/ may not exist yet — that is not a failure.
if [[ -d src ]] && grep -rn 'system_clock' src/ | grep -v '//'; then
    echo "FAIL: system_clock is banned in src/ — a latency measured across an" >&2
    echo "      NTP correction is negative or enormous, and reported either way" >&2
    exit 1
fi

run_suite() {
    local dir="$1" label="$2"
    shift 2
    cmake -B "$dir" "$@" >/dev/null

    # Build output is captured rather than discarded. Unit 0 sent it to
    # /dev/null and let a warning survive two chunks unnoticed; warnings are
    # the gate, not advice.
    local log="$dir/.check-build.log"
    if ! cmake --build "$dir" -j >"$log" 2>&1; then
        cat "$log" >&2
        echo "FAIL: $label build failed" >&2
        exit 1
    fi
    # The rig is vendored and compiled here, so its warnings would fail this
    # build too. Scope the check to our own sources: unit 0 polices itself.
    if grep -E 'warning:' "$log" | grep -v 'vendor/dariyanaap'; then
        echo "FAIL: $label built with warnings" >&2
        exit 1
    fi

    echo "--- $label ---"
    ctest --test-dir "$dir" --output-on-failure
}

run_suite build "plain"

if [[ "${1:-}" == "--all" ]]; then
    run_suite build-asan "ASan/UBSan" -DDARIYARAAH_ASAN=ON
    run_suite build-tsan "TSan"       -DDARIYARAAH_TSAN=ON
fi
