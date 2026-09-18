#!/usr/bin/env bash
#
# E2: thread-per-connection, open-loop, rate swept through the knee.
#
#     ./scripts/e2-rate.sh [out-dir]
#
# Connections are held fixed, so the server's thread count is fixed, and --rate
# is what varies. That makes the offered load an input rather than an output,
# which is the one question closed-loop cannot answer.
#
# The knee is at connections / service_time. The handler asks for a 20ms sleep
# and macOS delivers about 23.4ms (E1), so 32 connections carry roughly
# 32 / 0.0244s = 1,310 rps and not one more, whatever --rate says.
#
# Past the knee, read `connection_wait` rather than the VOID banner: unit 0's
# M6 exists so that a pool too small for a target reads differently from a rig
# that could not keep up.
set -euo pipefail

cd "$(dirname "$0")/.."
# shellcheck source=scripts/rig.sh
source scripts/rig.sh
ensure_rig
OUT="${1:-runs/e2}"
CONNECTIONS="${CONNECTIONS:-32}"
DURATION="${DURATION:-3000}"
RATES="${RATES:-400 800 1200 1300 2600}"

mkdir -p "$OUT"
rm -f "$OUT/summary.csv"

build/dariyaraah --port 0 > "$OUT/server.log" 2>&1 &
SERVER=$!
trap 'kill $SERVER 2>/dev/null || true' EXIT

for _ in $(seq 50); do [[ -s "$OUT/server.log" ]] && break; sleep 0.1; done
PORT="$(awk 'NR==1 {split($2, a, ":"); print a[length(a)]}' "$OUT/server.log")"
echo "target 127.0.0.1:$PORT, $CONNECTIONS connections held open, knee near 1,310 rps"
echo

for rate in $RATES; do
    echo "--- offered $rate rps ---"
    "$RIG" --target "127.0.0.1:$PORT" --protocol http --path / \
        --connections "$CONNECTIONS" --rate "$rate" --duration "$DURATION" --warmup 500 \
        --csv-dir "$OUT" || true
    echo
done
