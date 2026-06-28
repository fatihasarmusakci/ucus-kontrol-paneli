#!/usr/bin/env bash
# Bootloader yükleme (ST-Link)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
source "$SCRIPT_DIR/ardupilot-path.sh"
ARDUPILOT_DIR="$(resolve_ardupilot_dir "$PROJECT_ROOT")"
BIN="$ARDUPILOT_DIR/build/LOP-FC/bin/bootloader.bin"

if [ ! -f "$BIN" ]; then
    echo "Bootloader bulunamadı. Önce scripts/build-bootloader.sh çalıştırın."
    exit 1
fi

echo "Bootloader: $BIN"
echo "ST-Link ile 0x08000000 adresine yükleyin (st-flash veya OpenOCD)."
