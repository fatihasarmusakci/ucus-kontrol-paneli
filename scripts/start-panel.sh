#!/usr/bin/env bash
# SkyTrace GCS — SITL simülasyonu ve web kontrol panelini tek komutla başlatır
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
PORT="${GCS_PORT:-8080}"

export PATH="$HOME/.pyenv/shims:$HOME/.pyenv/bin:/opt/homebrew/bin:$PATH"

echo "=============================================="
echo "  SkyTrace GCS — Yer Kontrol İstasyonu"
echo "=============================================="

SITL_BIN="$PROJECT_ROOT/ardupilot/build/sitl/bin/arducopter"
if [ ! -f "$SITL_BIN" ]; then
    echo "SITL derleniyor (ilk sefer birkaç dakika sürebilir)..."
    bash "$SCRIPT_DIR/setup-env.sh"
fi

python3 -m pip install -q -r "$PROJECT_ROOT/dashboard/requirements.txt" 2>/dev/null || \
    python3 -m pip install -q pymavlink aiohttp

pkill -f "arducopter --model" 2>/dev/null || true
sleep 1

echo ""
echo "  Panel adresi: http://localhost:$PORT"
echo "  (Tarayıcı birkaç saniye içinde açılacak)"
echo ""

(sleep 3 && open "http://localhost:$PORT" 2>/dev/null || \
 xdg-open "http://localhost:$PORT" 2>/dev/null || true) &

cd "$PROJECT_ROOT"
export GCS_PORT="$PORT"
exec python3 dashboard/server.py
