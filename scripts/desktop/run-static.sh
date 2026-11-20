#!/usr/bin/env bash
set -euo pipefail
project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
game_data=${CSVR_GAME_DATA:-"$project_dir/game-data/SteamCS16"}
stage_dir="$project_dir/stage-desktop/linux-x86_64"
[[ -f "$game_data/cstrike/maps/de_dust2.bsp" ]] || { echo "Set CSVR_GAME_DATA to your owned CS 1.6 installation." >&2; exit 1; }
[[ -x "$stage_dir/csvr-xr" ]] || { echo "Build and stage the Linux target first." >&2; exit 1; }
python3 - "$stage_dir/cstrike/codex-static.cfg" <<'PY'
import sys
from pathlib import Path
Path(sys.argv[1]).write_text('wait\n' * 180 + 'jointeam 2\n' + 'wait\n' * 60 + 'joinclass 1\n' + 'wait\n' * 180 + 'developer 0\nsv_cheats 1\npausable 1\npause\n')
PY
exec "$stage_dir/csvr-xr" -rodir "$game_data" -dev 1 -nosound -ip 127.0.0.1 -noip6 -port 27017 +map de_dust2 +exec codex-static.cfg "$@"
