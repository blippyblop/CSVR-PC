#!/usr/bin/env bash
set -euo pipefail
project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
game_data=${CSVR_GAME_DATA:-"$project_dir/game-data/SteamCS16"}
stage_dir="$project_dir/stage-desktop/linux-x86_64"
menu_command=chooseteam
if [[ ${1:-} == --command ]]; then
    menu_command='exec touch/cmd/cmd'
    shift
fi
[[ -f "$game_data/cstrike/maps/de_dust2.bsp" ]] || { echo "Set CSVR_GAME_DATA to your owned CS 1.6 installation." >&2; exit 1; }
python3 - "$stage_dir/cstrike/codex-menu-check.cfg" "$menu_command" <<'PY'
import sys
from pathlib import Path
Path(sys.argv[1]).write_text('wait\n' * 420 + sys.argv[2] + '\n')
PY
exec "$stage_dir/csvr-xr" -rodir "$game_data" -dev 1 -nosound -ip 127.0.0.1 -noip6 -port 27017 +map de_dust2 +exec codex-menu-check.cfg "$@"
