#!/bin/bash
# Runs simulator test scripts, each on a fresh clone of the card image.
#
#   run_tests.sh [card image] [test.sim ...]
#
# Defaults: $DRUMBOY_SIM_TEST_CARD, else $DRUMBOY_SIM_DATA/test.img
# (~/Library/Application Support/DrumboySim/test.img, see sim_env.sh), and
# Simulator/tests/*.sim. The test card is kept separate from the card you play
# with; if it is missing it is made once from $DRUMBOY_SD_BACKUP.
# Output (log, screenshots, recordings) goes to build/Simulator/test-output/<test>/.
# Exit status is the number of failed tests, 100 if it could not start.

set -u
source "$(dirname "$0")/sim_env.sh"
BIN="$BUILD_DIR/Simulator/DrumboySim"
CARD="${1:-$SIM_TEST_CARD}"
if [ ! -f "$CARD" ]; then
    BACKUP="${DRUMBOY_SD_BACKUP:-}"
    if [ -z "$BACKUP" ] || [ ! -d "$BACKUP" ]; then
        cat >&2 <<MSG
error: test card not found: $CARD
Set DRUMBOY_SD_BACKUP to a folder with the SD card contents to create it${BACKUP:+ ($BACKUP does not exist)},
or pass a card image as the first argument, or set DRUMBOY_SIM_TEST_CARD / DRUMBOY_SIM_DATA.
MSG
        exit 100
    fi
    echo "Creating test card $CARD from $BACKUP"
    mkdir -p "$(dirname "$CARD")"
    "$SIM_DIR/tools/make_card_image.sh" "$BACKUP" "$CARD" >/dev/null || exit 100
fi
shift $(( $# > 0 ? 1 : 0 ))
TESTS=("$@")
[ ${#TESTS[@]} -eq 0 ] && TESTS=("$SIM_DIR"/tests/*.sim)

if [ ! -x "$BIN" ]; then
    echo "error: simulator not built ($BIN). Build it: cmake --preset $SIM_PRESET && cmake --build --preset $SIM_PRESET" >&2
    exit 100
fi

failed=0
for test in "${TESTS[@]}"; do
    name="$(basename "$test" .sim)"
    out="$BUILD_DIR/test-output/$name"
    rm -rf "$out" && mkdir -p "$out"
    if "$BIN" --card "$CARD" --card-copy --delay-scale 0.2 --out "$out" --script "$test" > "$out/log.txt" 2>&1; then
        echo "PASS $name"
    else
        echo "FAIL $name (see $out/log.txt)"
        tail -5 "$out/log.txt" | sed 's/^/    /'
        failed=$((failed + 1))
    fi
done
exit $failed
