#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

cmake -S "$ROOT" -B "$ROOT/build"
cmake --build "$ROOT/build" -j

CONFIG="$ROOT/build/simulated.conf"
cat > "$CONFIG" <<EOF
device_path=/dev/sensorhub0
log_file=$ROOT/build/logs/sensorhub.log
control_socket=$ROOT/build/run/sensorhub.sock
tcp_listen_port=9090
udp_export_host=127.0.0.1
udp_export_port=9091
worker_threads=2
log_rotate_bytes=1048576
queue_capacity=1024
simulate_device=true
sensor_interval_ms=100
EOF

"$ROOT/build/userspace/sensorhubd" -c "$CONFIG"

