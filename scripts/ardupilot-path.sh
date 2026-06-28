#!/usr/bin/env bash
# ArduPilot dizinini çözümler
set -euo pipefail

resolve_ardupilot_dir() {
    local project_root="$1"

    if [ -n "${ARDUPILOT_DIR:-}" ] && [ -f "${ARDUPILOT_DIR}/waf" ]; then
        cd "$ARDUPILOT_DIR" && pwd -P
        return
    fi

    local local_ap="$project_root/ardupilot"
    local cache_ap="$HOME/.cache/ucus-kontrol-paneli/ardupilot"
    local legacy_ap="$HOME/Desktop/lop-fc/ardupilot"

    if [ -f "$local_ap/waf" ]; then
        cd "$local_ap" && pwd -P
        return
    fi

    if [ -f "$cache_ap/waf" ]; then
        ln -sfn "$cache_ap" "$local_ap" 2>/dev/null || true
        cd "$cache_ap" && pwd -P
        return
    fi

    if [ -f "$legacy_ap/waf" ]; then
        echo "Not: Yerel ArduPilot kopyası kullanılıyor." >&2
        ln -sfn "$legacy_ap" "$local_ap" 2>/dev/null || true
        cd "$legacy_ap" && pwd -P
        return
    fi

    # Son çare: klonla ve overlay uygula
    local script_dir
    script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    "$script_dir/ensure-ardupilot.sh" >/dev/null
    cd "$(ARDUPILOT_DIR="${ARDUPILOT_DIR:-}" "$script_dir/ensure-ardupilot.sh")" && pwd -P
}
