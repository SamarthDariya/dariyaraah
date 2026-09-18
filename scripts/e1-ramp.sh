#!/usr/bin/env bash
#
# E1: thread-per-connection, closed-loop, ramped.
#
#     ./scripts/e1-ramp.sh [out-dir]
#
# One server for the whole sweep, so no step pays another's start-up. Each step
# holds N connections open and sends as fast as replies allow — which, against a
# thread-per-connection server, means N server threads each serialised behind a
# 20ms sleep. Throughput should therefore be N/0.0205s and the latency should
# barely move.
#
# Read the connections column before the throughput column. If a step started
# fewer connections than it asked for, the rig hit a limit (somaxconn is 128 on
# this machine) and that row describes a different experiment from the others.
set -euo pipefail

cd "$(dirname "$0")/.."
OUT="${1:-runs/e1}"
DURATION="${DURATION:-3000}"
STEPS="${STEPS:-1 2 4 8 16 32 64 128 256 500}"

mkdir -p "$OUT"
rm -f "$OUT/summary.csv"

build/dariyaraah --port 0 > "$OUT/server.log" 2>&1 &
SERVER=$!
trap 'kill $SERVER 2>/dev/null || true' EXIT

for _ in $(seq 50); do [[ -s "$OUT/server.log" ]] && break; sleep 0.1; done
PORT="$(awk 'NR==1 {split($2, a, ":"); print a[length(a)]}' "$OUT/server.log")"
echo "target 127.0.0.1:$PORT, 20ms handler, thread per connection"
echo "closed-loop, ${DURATION}ms measured after 500ms warm-up per step"
echo

for conns in $STEPS; do
    vendor/dariyanaap/build/dariyanaap --target "127.0.0.1:$PORT" --protocol http --path / \
        --connections "$conns" --duration "$DURATION" --warmup 500 --csv-dir "$OUT" || true
    echo
done

echo "summary: $OUT/summary.csv"
