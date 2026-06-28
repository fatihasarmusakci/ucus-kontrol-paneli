#!/usr/bin/env bash
# ST-Link ile firmware yükleme (fiziksel donanım gerekli)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
source "$SCRIPT_DIR/ardupilot-path.sh"
ARDUPILOT_DIR="$(resolve_ardupilot_dir "$PROJECT_ROOT")"
APJ="$ARDUPILOT_DIR/build/LOP-FC/bin/arducopter.apj"

if [ ! -f "$APJ" ]; then
    echo "Firmware bulunamadı. Önce scripts/build-firmware.sh çalıştırın."
    exit 1
fi

UPLOADER="$ARDUPILOT_DIR/Tools/scripts/uploader.py"
if [ ! -f "$UPLOADER" ]; then
    echo "uploader.py bulunamadı."
    exit 1
fi

PORT=""
for p in /dev/tty.usbmodem* /dev/ttyACM*; do
    [ -e "$p" ] || continue
    PORT="$p"
    break
done

echo "Firmware yükleniyor: $APJ"
if [ -n "$PORT" ]; then
    python3 "$UPLOADER" --port "$PORT" "$APJ"
else
    python3 "$UPLOADER" "$APJ"
fi
echo "=== Firmware yüklendi ==="
