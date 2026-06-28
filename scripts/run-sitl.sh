#!/usr/bin/env bash
# SITL simülasyon başlatma (MAVLink telemetri testi)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
ARDUPILOT_DIR="$PROJECT_ROOT/ardupilot"

export PATH="$HOME/.pyenv/shims:$HOME/.pyenv/bin:/opt/homebrew/bin:$PATH"

cd "$ARDUPILOT_DIR"

if [ ! -f build/sitl/bin/arducopter ]; then
    echo "SITL binary yok. Önce scripts/setup-env.sh çalıştırın."
    exit 1
fi

echo "SITL başlatılıyor (Ctrl+C ile durdurun)..."
echo "Mission Planner / QGC bağlantısı: UDP 14550"
Tools/autotest/sim_vehicle.py -v ArduCopter --console --map
