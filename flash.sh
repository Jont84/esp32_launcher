#!/usr/bin/env bash
# Writes the launcher into the factory slot and nothing else, after checking
# that the board has the shared partition table v2.
#
#   . ~/esp/esp-idf/export.sh
#   idf.py build && ./flash.sh [port] [boot]
#
# "boot" also clears the boot choice, so the board starts the launcher next
# (do this once, after all the games are written). Settings in NVS are kept.
# Never use `idf.py flash` on the shared board.
set -euo pipefail
PORT=/dev/ttyACM0
BOOT=no
for a in "$@"; do
    case "$a" in
        boot) BOOT=yes ;;
        *) PORT=$a ;;
    esac
done
SLOT=factory
SLOT_SIZE=$((0x100000))
BIN=build/launcher.bin
cd "$(dirname "$0")"

[ -n "${IDF_PATH:-}" ] || { echo "Run '. ~/esp/esp-idf/export.sh' first."; exit 1; }
[ -f "$BIN" ] || { echo "No $BIN: run 'idf.py build' first."; exit 1; }
GEN="python $IDF_PATH/components/partition_table/gen_esp32part.py"
PT="python $IDF_PATH/components/partition_table/parttool.py"
OTA="python $IDF_PATH/components/app_update/otatool.py"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT

size=$(stat -c %s "$BIN")
if [ "$size" -gt "$SLOT_SIZE" ]; then
    echo "$BIN is $size bytes, too big for $SLOT ($SLOT_SIZE). Nothing written."; exit 1
fi

echo "Checking the board's partition table..."
$GEN --flash-size 16MB partitions.csv "$TMP/want.bin" >/dev/null
esptool.py -p "$PORT" read_flash 0x8000 0xC00 "$TMP/board.bin" >/dev/null
if ! cmp -s "$TMP/want.bin" "$TMP/board.bin"; then
    echo "The board doesn't have partition table v2. Nothing written."
    echo "Write it first: see 'Moving the board onto the launcher' in README.md."
    exit 1
fi

echo "Writing $BIN ($((size / 1024)) KB) into $SLOT..."
$PT -p "$PORT" write_partition --partition-name "$SLOT" --input "$BIN"
if [ "$BOOT" = yes ]; then
    echo "Clearing the boot choice: the board starts the launcher next."
    $OTA -p "$PORT" erase_otadata
fi
echo "Done."
