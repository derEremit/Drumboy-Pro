#!/bin/bash
# Opens the simulator window on the SD card image (builds first if needed).
#
#   run_sim.sh [card image] [simulator options...]
#
# Default card: $DRUMBOY_SIM_CARD, else $DRUMBOY_SIM_DATA/drumboy.img
# (~/Library/Application Support/DrumboySim/drumboy.img), see sim_env.sh.
# Changes the firmware makes (presets, saved files) are kept on the image; add
# --card-copy to start from a fresh copy each time. A control socket
# (build/Simulator/sim.sock) is open for Simulator/tools/simctl.

set -eu
source "$(dirname "$0")/sim_env.sh"
CARD="$SIM_CARD"
if [ $# -gt 0 ] && [ "${1#-}" = "$1" ]; then CARD="$1"; shift; fi
sim_require_card "$CARD"
CARD="$(cd "$(dirname "$CARD")" && pwd)/$(basename "$CARD")"

sim_find_build_tools
cd "$FW_DIR"
[ -d "build/$SIM_PRESET" ] || cmake --preset "$SIM_PRESET" >/dev/null
cmake --build --preset "$SIM_PRESET" | grep -E 'error|FAILED' || true

cd "$BUILD_DIR"
exec Simulator/DrumboySim --card "$CARD" --control sim.sock "$@"
