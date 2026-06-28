#!/usr/bin/env bash
# Uçuş Kontrol Paneli — SITL simülasyonu ve web arayüzünü tek komutla başlatır
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
source "$SCRIPT_DIR/ardupilot-path.sh"
ARDUPILOT_DIR="$(resolve_ardupilot_dir "$PROJECT_ROOT")"
PORT="${GCS_PORT:-8080}"

export PATH="$HOME/.pyenv/shims:$HOME/.pyenv/bin:/opt/homebrew/bin:$PATH"
export ARDUPILOT_DIR

echo "=============================================="
echo "  Uçuş Kontrol Paneli"
echo "=============================================="

SITL_BIN="$ARDUPILOT_DIR/build/sitl/bin/arducopter"
if [ ! -f "$SITL_BIN" ]; then
    echo "SITL derleniyor (ilk sefer birkaç dakika sürebilir)..."
    bash "$SCRIPT_DIR/setup-env.sh"
    ARDUPILOT_DIR="$(resolve_ardupilot_dir "$PROJECT_ROOT")"
    export ARDUPILOT_DIR
    SITL_BIN="$ARDUPILOT_DIR/build/sitl/bin/arducopter"
fi

python3 -m pip install -q -r "$PROJECT_ROOT/dashboard/requirements.txt" 2>/dev/null || \
    python3 -m pip install -q pymavlink aiohttp

pkill -f "arducopter --model" 2>/dev/null || true
lsof -ti:"$PORT" 2>/dev/null | xargs kill -9 2>/dev/null || true
sleep 1

PANEL_URL="http://localhost:$PORT/?t=$(date +%s)"

echo ""
echo "  Panel adresi: $PANEL_URL"
echo "  Tarayıcı otomatik açılacak."
echo ""

(sleep 2 && open "$PANEL_URL" 2>/dev/null || \
 xdg-open "$PANEL_URL" 2>/dev/null || true) &

cd "$PROJECT_ROOT"
export GCS_PORT="$PORT"
exec python3 dashboard/server.py
