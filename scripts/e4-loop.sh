#!/usr/bin/env bash
#
# E4: the event loop, ramped like E1 and E3 so the three models compare.
#
#     ./scripts/e4-loop.sh [out-dir]
#
# MODE selects which loop: 1 is E4a, where the handler still sleeps in the single
# thread, and 2 is E4b, where the database call becomes a kqueue timer and the
# thread goes and does something else.
#
# Read this table beside E1's and E3's. Same client, same ramp, same handler:
# the only thing that differs is who waits during those 20 milliseconds, which
# is the entire content of the phrase "threading model".
set -euo pipefail

cd "$(dirname "$0")/.."
OUT="${1:-runs/e4}"
DURATION="${DURATION:-3000}"
MODE="${MODE:-1}"
STEPS="${STEPS:-1 2 4 8 16 32}"

mkdir -p "$OUT"
rm -f "$OUT/summary.csv"

build/dariyaraah --port 0 --event-loop "$MODE" > "$OUT/server.log" 2>&1 &
SERVER=$!
trap 'kill $SERVER 2>/dev/null || true' EXIT

for _ in $(seq 50); do [[ -s "$OUT/server.log" ]] && break; sleep 0.1; done
PORT="$(awk 'NR==1 {split($2, a, ":"); print a[length(a)]}' "$OUT/server.log")"
echo "$(cat "$OUT/server.log")"
echo "closed-loop, ${DURATION}ms measured after 500ms warm-up per step"
echo

for conns in $STEPS; do
    vendor/dariyanaap/build/dariyanaap --target "127.0.0.1:$PORT" --protocol http --path / \
        --connections "$conns" --duration "$DURATION" --warmup 500 --csv-dir "$OUT" || true
    echo
done
