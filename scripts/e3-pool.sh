#!/usr/bin/env bash
#
# E3: a bounded pool, closed-loop, with connections ramped past the workers.
#
#     ./scripts/e3-pool.sh [out-dir]
#
# WORKERS is held fixed and connections vary, which is the opposite of E1 and
# the whole point: there, thread count tracked connection count and throughput
# rose with it. Here thread count is pinned, so throughput should stop at
# workers / service_time and stay there however many connections arrive.
#
# Read the error counters, not just the throughput column. A worker holds a
# keep-alive connection for that connection's whole life, so connections past
# the worker count are not served slowly — they are not served at all, and they
# appear as timeouts and refused connects rather than as latency.
set -euo pipefail

cd "$(dirname "$0")/.."
# shellcheck source=scripts/rig.sh
source scripts/rig.sh
ensure_rig
OUT="${1:-runs/e3}"
WORKERS="${WORKERS:-32}"
DURATION="${DURATION:-3000}"
STEPS="${STEPS:-1 8 16 32 64 128 256 500}"

mkdir -p "$OUT"
rm -f "$OUT/summary.csv"

build/dariyaraah --port 0 --workers "$WORKERS" > "$OUT/server.log" 2>&1 &
SERVER=$!
trap 'kill $SERVER 2>/dev/null || true' EXIT

for _ in $(seq 50); do [[ -s "$OUT/server.log" ]] && break; sleep 0.1; done
PORT="$(awk 'NR==1 {split($2, a, ":"); print a[length(a)]}' "$OUT/server.log")"
echo "$(cat "$OUT/server.log")"
echo "closed-loop, ${DURATION}ms measured after 500ms warm-up per step"
echo

for conns in $STEPS; do
    "$RIG" --target "127.0.0.1:$PORT" --protocol http --path / \
        --connections "$conns" --duration "$DURATION" --warmup 500 --csv-dir "$OUT" || true
    echo
done
