#!/usr/bin/env bash
# ArduPilot dizinini çözümler (Türkçe karakterli yol sorununa karşı)
set -euo pipefail

_needs_ardupilot_fallback() {
    local project_root="$1"
    python3 - "$project_root" <<'PY'
import os
import subprocess
import sys

root = sys.argv[1]
ap = os.path.join(root, "ardupilot")
fallback = os.path.expanduser("~/Desktop/lop-fc/ardupilot")

# Türkçe / Unicode yol
if any(ord(c) > 127 for c in root):
    sys.exit(0)

if not os.path.isfile(os.path.join(ap, "waf")):
    sys.exit(0)

try:
    subprocess.run(
        ["git", "submodule", "status", "modules/littlefs"],
        cwd=ap,
        capture_output=True,
        check=True,
        text=True,
    )
except (subprocess.CalledProcessError, FileNotFoundError):
    sys.exit(0)

sys.exit(1)
PY
}

resolve_ardupilot_dir() {
    local project_root="$1"
    local local_ap="$project_root/ardupilot"
    local fallback="$HOME/Desktop/lop-fc/ardupilot"

    if _needs_ardupilot_fallback "$project_root"; then
        if [ -f "$fallback/waf" ]; then
            echo "Not: Bu klasör yolu ArduPilot derlemesini bozuyor (Türkçe karakter / git yolu)." >&2
            echo "Çalışan ArduPilot kopyasına bağlanılıyor: $fallback" >&2
            rm -rf "$local_ap"
            ln -sf "$fallback" "$local_ap"
        else
            echo "HATA: ArduPilot bu konumda derlenemiyor." >&2
            echo "Projeyi ASCII isimli bir klasöre taşıyın:" >&2
            echo "  ~/Desktop/ucus-kontrol-paneli" >&2
            exit 1
        fi
    elif [ ! -f "$local_ap/waf" ]; then
        if [ -f "$fallback/waf" ]; then
            echo "ArduPilot eksik, lop-fc kopyasına bağlanılıyor..." >&2
            ln -sf "$fallback" "$local_ap"
        else
            echo "HATA: ardupilot klasörü bulunamadı." >&2
            exit 1
        fi
    fi

    cd "$local_ap" && pwd -P
}
