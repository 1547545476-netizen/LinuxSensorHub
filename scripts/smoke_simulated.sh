#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build"
TMP="$BUILD/smoke"

cmake -S "$ROOT" -B "$BUILD"
cmake --build "$BUILD" -j

rm -rf "$TMP"
mkdir -p "$TMP"

cat > "$TMP/sensorhub.conf" <<EOF
device_path=/dev/sensorhub0
log_file=$TMP/logs/sensorhub.log
control_socket=$TMP/run/sensorhub.sock
tcp_listen_port=19090
udp_export_host=127.0.0.1
udp_export_port=19091
worker_threads=2
log_rotate_bytes=1048576
queue_capacity=1024
simulate_device=true
sensor_interval_ms=20
EOF

"$BUILD/userspace/sensorhubd" -c "$TMP/sensorhub.conf" > "$TMP/stdout.log" 2> "$TMP/stderr.log" &
PID="$!"

cleanup() {
  "$BUILD/userspace/sensorctl" shutdown -c "$TMP/sensorhub.conf" >/dev/null 2>&1 || true
  wait "$PID" >/dev/null 2>&1 || true
}
trap cleanup EXIT

for _ in {1..50}; do
  [[ -S "$TMP/run/sensorhub.sock" ]] && break
  sleep 0.1
done

"$BUILD/userspace/sensor_subscribe" -n 5 -c "$TMP/sensorhub.conf"
"$BUILD/userspace/sensorctl" stats -c "$TMP/sensorhub.conf"
test -s "$TMP/logs/sensorhub.log"

echo "smoke test passed"

