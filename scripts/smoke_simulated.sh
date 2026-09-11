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
  # 异常路径只清理本脚本创建的进程；不能把清理成功当作正常关闭通过。
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

"$BUILD/userspace/sensor_subscribe" -n 5 -c "$TMP/sensorhub.conf"
"$BUILD/userspace/sensorctl" stats -c "$TMP/sensorhub.conf"
test -s "$TMP/logs/sensorhub.log"

REPLY="$("$BUILD/userspace/sensorctl" shutdown -c "$TMP/sensorhub.conf")"
[[ "$REPLY" == "ok" ]]
wait "$PID"
unset PID
echo "smoke test passed"
