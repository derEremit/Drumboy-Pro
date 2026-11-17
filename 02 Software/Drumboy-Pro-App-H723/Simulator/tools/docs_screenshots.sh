#!/bin/bash
# Takes the screenshots in docs/images (run after changing what they show).
#
#   docs_screenshots.sh [card image]
#
# Works on a throwaway copy of the card ($DRUMBOY_SIM_CARD by default, see
# sim_env.sh): switches the cpu_meter and psychedelic plugins on, then runs
# docs_screenshots.sim and docs_panel.sim, which write the PNGs straight into
# docs/images.

set -eu
source "$(dirname "$0")/sim_env.sh"
CARD="${1:-$SIM_CARD}"
sim_require_card "$CARD"
BIN="$BUILD_DIR/Simulator/DrumboySim"
[ -x "$BIN" ] || { echo "error: simulator not built ($BIN). Build it: cmake --preset Simulator && cmake --build --preset Simulator" >&2; exit 1; }
OUT="$(cd "$FW_DIR/../.." && pwd)/docs/images"
mkdir -p "$OUT"

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
cp "$CARD" "$WORK/card.img"
SIMCTL=("$SIM_DIR/tools/simctl" --socket "$WORK/sim.sock")

# 1. Plugins on (the manager toggles, so only press GLOBAL where the saved state is off)
"$BIN" --card "$WORK/card.img" --headless --delay-scale 0.2 --control "$WORK/sim.sock" --out "$WORK" >/dev/null 2>&1 &
"${SIMCTL[@]}" wait-ready "wait-stable 300" "press FILE" "wait-menu FILE 2000" "wait 300" "press FILE" "wait 500" "expect manager == 1" >/dev/null
cursor=0
for entry in 0:cpu_meter 2:psychedelic; do
    row="${entry%%:*}" id="${entry#*:}"
    while [ "$cursor" -lt "$row" ]; do "${SIMCTL[@]}" "press DOWN" >/dev/null; cursor=$((cursor + 1)); done
    saved="$("${SIMCTL[@]}" "get plugin.$id" | sed 's/^ok //; s/.*,//')"
    [ "$saved" = "on" ] || "${SIMCTL[@]}" "press CENTER" "wait 200" >/dev/null
done
"${SIMCTL[@]}" "press FILE" "wait 800" quit >/dev/null
wait

# 2. Screenshots
"$BIN" --card "$WORK/card.img" --headless --delay-scale 0.2 --script "$SIM_DIR/tools/docs_screenshots.sim" --out "$OUT" 2>&1 | grep -E '^(error|sim: script)' || true
# 3. The whole simulator window (opens it for a few seconds)
"$BIN" --card "$WORK/card.img" --window --delay-scale 0.2 --script "$SIM_DIR/tools/docs_panel.sim" --out "$OUT" 2>&1 | grep -E '^(error|sim: script)' || true
echo "Screenshots in $OUT"
