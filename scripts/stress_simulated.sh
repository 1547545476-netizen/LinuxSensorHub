#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build"
TMP="$BUILD/stress"
CLIENTS="${CLIENTS:-8}"
LINES="${LINES:-50}"
CLIENT_PIDS=""

cmake -S "$ROOT" -B "$BUILD"
cmake --build "$BUILD" -j

rm -rf "$TMP"
mkdir -p "$TMP"

cat > "$TMP/sensorhub.conf" <<EOF
device_path=/dev/sensorhub0
log_file=$TMP/logs/sensorhub.log
control_socket=$TMP/run/sensorhub.sock
tcp_listen_port=29090
udp_export_host=127.0.0.1
udp_export_port=29091
worker_threads=4
log_rotate_bytes=1048576
queue_capacity=4096
simulate_device=true
sensor_interval_ms=5
EOF

"$BUILD/userspace/sensorhubd" -c "$TMP/sensorhub.conf" > "$TMP/stdout.log" 2> "$TMP/stderr.log" &
PID="$!"

cleanup() {
  if [[ -n "${PID:-}" ]]; then
    kill -KILL "$PID" >/dev/null 2>&1 || true
    wait "$PID" >/dev/null 2>&1 || true
  fi
}
trap cleanup EXIT

for _ in {1..50}; do
  [[ -S "$TMP/run/sensorhub.sock" ]] && break
  sleep 0.1
done

for i in $(seq 1 "$CLIENTS"); do
  "$BUILD/userspace/sensor_subscribe" -n "$LINES" -c "$TMP/sensorhub.conf" > "$TMP/client_$i.log" &
  CLIENT_PIDS="$CLIENT_PIDS $!"
done

for client_pid in $CLIENT_PIDS; do
  wait "$client_pid"
done
"$BUILD/userspace/sensorctl" stats -c "$TMP/sensorhub.conf"
wc -l "$TMP"/client_*.log
REPLY="$("$BUILD/userspace/sensorctl" shutdown -c "$TMP/sensorhub.conf")"
[[ "$REPLY" == "ok" ]]
wait "$PID"
unset PID
echo "stress test passed"
