#!/usr/bin/env bash
set -euo pipefail

DAEMON="$1"
CTL="$2"
ROOT="$3"
SUBSCRIBE="$(dirname "$CTL")/sensor_subscribe"

TMP_DIR="$(mktemp -d)"
cleanup() {
  if [[ -n "${PID:-}" ]]; then
    "$CTL" shutdown -c "$TMP_DIR/sensorhub.conf" >/dev/null 2>&1 || true
    wait "$PID" >/dev/null 2>&1 || true
  fi
  rm -rf "$TMP_DIR"
}
trap cleanup EXIT

cat > "$TMP_DIR/sensorhub.conf" <<EOF
device_path=/dev/sensorhub0
log_file=$TMP_DIR/logs/sensorhub.log
control_socket=$TMP_DIR/run/sensorhub.sock
tcp_listen_port=9090
udp_export_host=127.0.0.1
udp_export_port=9091
worker_threads=1
log_rotate_bytes=1048576
queue_capacity=128
simulate_device=true
sensor_interval_ms=20
EOF

"$DAEMON" -c "$TMP_DIR/sensorhub.conf" > "$TMP_DIR/stdout.log" 2> "$TMP_DIR/stderr.log" &
PID="$!"

for _ in {1..50}; do
  if [[ -S "$TMP_DIR/run/sensorhub.sock" ]]; then
    break
  fi
  sleep 0.1
done

sleep 0.2
TCP_LINES="$("$SUBSCRIBE" -n 3 -c "$TMP_DIR/sensorhub.conf")"
echo "$TCP_LINES"
grep -q '^seq=' <<< "$TCP_LINES"

STATS="$("$CTL" stats -c "$TMP_DIR/sensorhub.conf")"
echo "$STATS"

grep -q '^mode=simulate' <<< "$STATS"
grep -q '^received=' <<< "$STATS"
grep -q '^tcp_clients=' <<< "$STATS"

"$CTL" shutdown -c "$TMP_DIR/sensorhub.conf" >/dev/null
wait "$PID"
unset PID

test -s "$TMP_DIR/logs/sensorhub.log"
