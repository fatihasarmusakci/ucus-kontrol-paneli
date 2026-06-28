#!/usr/bin/env bash
# ArduPilot kaynağını hazırlar: klon, submodule, özel firmware overlay
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
OVERLAY_DIR="$PROJECT_ROOT/firmware/overlay"
ARDUPILOT_URL="${ARDUPILOT_URL:-https://github.com/ArduPilot/ardupilot.git}"
ARDUPILOT_REF="${ARDUPILOT_REF:-}"

# Türkçe karakterli yol varsa ASCII önbellek dizini kullan
_use_cache() {
    python3 -c "import sys; sys.exit(0 if any(ord(c)>127 for c in sys.argv[1]) else 1)" "$PROJECT_ROOT"
}

if _use_cache; then
    ARDUPILOT_DIR="${ARDUPILOT_DIR:-$HOME/.cache/ucus-kontrol-paneli/ardupilot}"
else
    ARDUPILOT_DIR="${ARDUPILOT_DIR:-$PROJECT_ROOT/ardupilot}"
fi

export ARDUPILOT_DIR

if [ ! -f "$ARDUPILOT_DIR/waf" ]; then
    echo "ArduPilot kaynağı indiriliyor..." >&2
    mkdir -p "$(dirname "$ARDUPILOT_DIR")"
    git clone --depth 1 "$ARDUPILOT_URL" "$ARDUPILOT_DIR"
fi

cd "$ARDUPILOT_DIR"
git submodule update --init --recursive

if [ -d "$OVERLAY_DIR" ]; then
    echo "Özel firmware dosyaları uygulanıyor..." >&2
    rsync -a "$OVERLAY_DIR/" "$ARDUPILOT_DIR/"
fi

# Proje içinde symlink (ASCII yol dışındaki geliştirme için)
if _use_cache; then
    ln -sfn "$ARDUPILOT_DIR" "$PROJECT_ROOT/ardupilot"
fi

echo "$ARDUPILOT_DIR"
