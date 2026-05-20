set -eu

CONVERT=${ANA_CONVERT:-build/tools/ana-convert/ana-convert}
PROBE=${ANA_CONVERT_PROBE:-build/tests/ana_convert_image_test}
WORK_DIR=${ANA_TEST_WORK_DIR:-build/tests}
SOURCE="$WORK_DIR/ana_convert_sheet.ppm"
OUTPUT="$WORK_DIR/ana_convert_sheet.anaimg"

mkdir -p "$WORK_DIR"

cat > "$SOURCE" <<'PPM'
P3
6 2
255
255 0 0    0 255 0    255 0 255    0 0 255    255 255 255    0 0 0
255 0 255  255 255 0  0 255 255    255 0 255  0 0 255      255 0 0
PPM

"$CONVERT" image "$SOURCE" \
    --out "$OUTPUT" \
    --colors 16 \
    --frame-width 3 \
    --frame-height 2 \
    --transparent 255,0,255

"$PROBE" "$OUTPUT"

rm -f "$SOURCE" "$OUTPUT"
