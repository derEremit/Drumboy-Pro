# Shared paths for the simulator scripts (sourced by run_sim.sh and run_tests.sh).
#
#   DRUMBOY_SIM_DATA        card images folder, default ~/Library/Application Support/DrumboySim
#                           on macOS, $XDG_DATA_HOME/DrumboySim (~/.local/share/DrumboySim) on Linux
#   DRUMBOY_SIM_CARD        playing card, default $DRUMBOY_SIM_DATA/drumboy.img
#   DRUMBOY_SIM_TEST_CARD   test card, default $DRUMBOY_SIM_DATA/test.img
#   DRUMBOY_SD_BACKUP       folder with SD card contents, used to create the test card
#
# CMake and Ninja come from PATH, else from the STM32 VS Code extension bundles.

SIM_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FW_DIR="$(dirname "$SIM_DIR")"
case "$(uname -s)" in
    Darwin) SIM_PRESET="Simulator"; SIM_DATA_DEFAULT="$HOME/Library/Application Support/DrumboySim" ;;
    *)      SIM_PRESET="SimulatorLinux"; SIM_DATA_DEFAULT="${XDG_DATA_HOME:-$HOME/.local/share}/DrumboySim" ;;
esac
BUILD_DIR="$FW_DIR/build/$SIM_PRESET"

SIM_DATA="${DRUMBOY_SIM_DATA:-$SIM_DATA_DEFAULT}"
SIM_CARD="${DRUMBOY_SIM_CARD:-$SIM_DATA/drumboy.img}"
SIM_TEST_CARD="${DRUMBOY_SIM_TEST_CARD:-$SIM_DATA/test.img}"

# Adds CMake and Ninja to PATH if they are not there (newest STM32 bundle wins)
sim_find_build_tools() {
    local bundles="$HOME/Library/Application Support/stm32cube/bundles" dir
    if ! command -v cmake >/dev/null; then
        dir="$(ls -d "$bundles"/cmake/*/CMake.app/Contents/bin 2>/dev/null | sort -V | tail -1)"
        [ -n "$dir" ] && PATH="$dir:$PATH"
    fi
    if ! command -v ninja >/dev/null; then
        dir="$(ls -d "$bundles"/ninja/*/bin 2>/dev/null | sort -V | tail -1)"
        [ -n "$dir" ] && PATH="$dir:$PATH"
    fi
    export PATH
    local tool
    for tool in cmake ninja; do
        if ! command -v "$tool" >/dev/null; then
            echo "error: $tool not found. Put it on PATH (e.g. brew install cmake ninja) or install the STM32 VS Code extension." >&2
            return 1
        fi
    done
}

# Fails with a hint when a card image is missing
sim_require_card() {
    [ -f "$1" ] && return 0
    cat >&2 <<MSG
error: card image not found: $1
Make one from a folder with the SD card contents:
    Simulator/tools/make_card_image.sh <SD card folder> "$1"
or pass another image as the first argument, or set DRUMBOY_SIM_CARD / DRUMBOY_SIM_DATA.
MSG
    return 1
}
