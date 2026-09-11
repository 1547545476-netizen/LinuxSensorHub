#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [[ "$(id -u)" -ne 0 ]]; then
  echo "please run with sudo: sudo bash scripts/verify_driver.sh"
  exit 1
fi

make -C "$ROOT/kernel"

if lsmod | grep -q '^sensorhub_drv'; then
  rmmod sensorhub_drv
fi

insmod "$ROOT/kernel/sensorhub_drv.ko"
trap 'rmmod sensorhub_drv >/dev/null 2>&1 || true' EXIT

ls -l /dev/sensorhub0
cat /sys/class/misc/sensorhub0/interval_ms
echo 100 > /sys/class/misc/sensorhub0/interval_ms
cat /sys/class/misc/sensorhub0/stats

if mountpoint -q /sys/kernel/debug; then
  cat /sys/kernel/debug/sensorhub/stats
else
  echo "debugfs is not mounted; run: mount -t debugfs none /sys/kernel/debug"
fi

dmesg | tail -20
echo "driver verification passed"

