#!/bin/bash
# wait-for-idle.sh — wait until system load drops below threshold before compiling
# Usage: ./scripts/wait-for-idle.sh [threshold] [timeout_seconds]
# Default: threshold=1.0 (10% of 8 cores ≈ 0.8, rounded up), timeout=600s
THRESHOLD="${1:-1.0}"
TIMEOUT="${2:-600}"
ELAPSED=0
while true; do
    LOAD=$(awk '{print $1}' /proc/loadavg)
    # bc-style float compare via awk
    OK=$(awk -v l="$LOAD" -v t="$THRESHOLD" 'BEGIN{print (l <= t) ? 1 : 0}')
    if [ "$OK" = "1" ]; then
        echo "[idle] load $LOAD <= $THRESHOLD — proceeding"
        exit 0
    fi
    if [ "$ELAPSED" -ge "$TIMEOUT" ]; then
        echo "[idle] timeout ${TIMEOUT}s, load still $LOAD — proceeding anyway"
        exit 0
    fi
    echo "[idle] load $LOAD > $THRESHOLD, waiting 10s... (${ELAPSED}s elapsed)"
    sleep 10
    ELAPSED=$((ELAPSED + 10))
done
