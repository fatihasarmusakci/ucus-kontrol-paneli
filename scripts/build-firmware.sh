#!/usr/bin/env bash
# Özgün uçuş kontrol kartı firmware derlemesi
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
ARDUPILOT_DIR="$PROJECT_ROOT/ardupilot"
TOOLCHAIN_DIR="$PROJECT_ROOT/tools/gcc-arm-none-eabi-10-2020-q4-major"

export PATH="$HOME/.pyenv/shims:$HOME/.pyenv/bin:/opt/homebrew/bin:$TOOLCHAIN_DIR/bin:$PATH"

cd "$ARDUPILOT_DIR"
./waf configure --board LOP-FC --disable-tests
./waf copter

echo ""
echo "=== Firmware derlemesi tamamlandı ==="
echo "APJ: $ARDUPILOT_DIR/build/LOP-FC/bin/arducopter.apj"
echo "BIN: $ARDUPILOT_DIR/build/LOP-FC/bin/arducopter.bin"
