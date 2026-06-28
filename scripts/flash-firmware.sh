#!/usr/bin/env bash
# ST-Link ile firmware yükleme (fiziksel donanım gerekli)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
APJ="$PROJECT_ROOT/ardupilot/build/LOP-FC/bin/arducopter.apj"

if [ ! -f "$APJ" ]; then
    echo "Firmware bulunamadı. Önce scripts/build-firmware.sh çalıştırın."
    exit 1
fi

if ! command -v uploader.py &>/dev/null && [ ! -f "$PROJECT_ROOT/ardupilot/Tools/scripts/uploader.py" ]; then
    echo "uploader.py bulunamadı."
    exit 1
fi

UPLOADER="$PROJECT_ROOT/ardupilot/Tools/scripts/uploader.py"
echo "Firmware yükleniyor: $APJ"
python3 "$UPLOADER" --port /dev/tty.usbmodem* "$APJ" 2>/dev/null || \
python3 "$UPLOADER" "$APJ"
echo "=== Firmware yüklendi ==="
