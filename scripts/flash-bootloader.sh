#!/usr/bin/env bash
# ST-Link ile bootloader yükleme (fiziksel donanım gerekli)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BIN="$PROJECT_ROOT/ardupilot/build/LOP-FC/bin/bootloader.bin"

if [ ! -f "$BIN" ]; then
    echo "Bootloader bulunamadı. Önce scripts/build-bootloader.sh çalıştırın."
    exit 1
fi

if ! command -v st-flash &>/dev/null; then
    echo "st-flash bulunamadı. Kurulum: brew install stlink"
    exit 1
fi

echo "Bootloader yükleniyor: $BIN"
st-flash write "$BIN" 0x08000000
echo "=== Bootloader yüklendi ==="
