#!/usr/bin/env bash
# Geliştirme ortamı kurulumu: ARM toolchain + SITL derlemesi
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
source "$SCRIPT_DIR/ardupilot-path.sh"
ARDUPILOT_DIR="$(resolve_ardupilot_dir "$PROJECT_ROOT")"
TOOLCHAIN_DIR="$PROJECT_ROOT/tools/gcc-arm-none-eabi-10-2020-q4-major"

export PATH="$HOME/.pyenv/shims:$HOME/.pyenv/bin:/opt/homebrew/bin:$PATH"

echo "=== Uçuş Kontrol Paneli — Ortam Kurulumu ==="
echo "ArduPilot: $ARDUPILOT_DIR"

if ! command -v pyenv &>/dev/null; then
    echo "pyenv bulunamadı. Önce Python 3.10+ kurun."
    exit 1
fi

pyenv global 3.10.18 2>/dev/null || true

python3 -m pip install -q empy==3.3.4 pexpect future lxml pymavlink

if [ -d "$TOOLCHAIN_DIR/bin" ]; then
    export PATH="$TOOLCHAIN_DIR/bin:$PATH"
    echo "ARM toolchain: $TOOLCHAIN_DIR"
else
    echo "ARM toolchain indiriliyor..."
    mkdir -p "$PROJECT_ROOT/tools"
    curl -L -o /tmp/gcc-arm-none-eabi.tar.bz2 \
        "https://firmware.ardupilot.org/Tools/STM32-tools/gcc-arm-none-eabi-10-2020-q4-major-mac.tar.bz2"
    tar xjf /tmp/gcc-arm-none-eabi.tar.bz2 -C "$PROJECT_ROOT/tools"
    export PATH="$TOOLCHAIN_DIR/bin:$PATH"
fi

arm-none-eabi-gcc --version | head -1

cd "$ARDUPILOT_DIR"
if [ -f .gitmodules ]; then
    git submodule update --init --recursive
fi

if [ -f build/sitl/bin/arducopter ]; then
    echo ""
    echo "=== SITL zaten derlenmiş ==="
    echo "Binary: $ARDUPILOT_DIR/build/sitl/bin/arducopter"
else
    ./waf configure --board sitl --disable-tests
    ./waf copter
    echo ""
    echo "=== Kurulum tamamlandı — SITL derlemesi başarılı ==="
    echo "Binary: $ARDUPILOT_DIR/build/sitl/bin/arducopter"
fi

echo ""
echo "Paneli başlatmak için: bash scripts/start-panel.sh"
