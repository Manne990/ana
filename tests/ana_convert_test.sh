set -eu

CONVERT=${ANA_CONVERT:-build/tools/ana-convert/ana-convert}
PROBE=${ANA_CONVERT_PROBE:-build/tests/ana_convert_image_test}
WORK_DIR=${ANA_TEST_WORK_DIR:-build/tests}
SOURCE="$WORK_DIR/ana_convert_sheet.ppm"
OUTPUT="$WORK_DIR/ana_convert_sheet.anaimg"
PNG_SOURCE="$WORK_DIR/ana_convert_sheet.png"
PNG_PALETTE="$WORK_DIR/ana_convert_palette.png"
PALETTE="$WORK_DIR/game.anapal"
PNG_OUTPUT="$WORK_DIR/ana_convert_sheet_png.anaimg"
MANIFEST="$WORK_DIR/assets.ana"
MANIFEST_OUT="$WORK_DIR/nested/manifest-assets"
MOD_SOURCE="$WORK_DIR/theme.mod"

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

python3 - "$PNG_SOURCE" "$PNG_PALETTE" <<'PY'
import struct
import sys
import zlib


def chunk(kind, data):
    body = kind + data
    return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xffffffff)


def write_png(path, width, height, rows):
    raw = bytearray()
    for row in rows:
        raw.append(0)
        for pixel in row:
            raw.extend(pixel)

    data = bytearray()
    data.extend(b"\x89PNG\r\n\x1a\n")
    data.extend(chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)))
    data.extend(chunk(b"IDAT", zlib.compress(bytes(raw))))
    data.extend(chunk(b"IEND", b""))
    with open(path, "wb") as handle:
        handle.write(data)


red = (255, 0, 0, 255)
green = (0, 255, 0, 255)
blue = (0, 0, 255, 255)
white = (255, 255, 255, 255)
black = (0, 0, 0, 255)
yellow = (255, 255, 0, 255)
cyan = (0, 255, 255, 255)
magenta = (255, 0, 255, 255)
alpha_clear = (12, 34, 56, 0)

write_png(sys.argv[2], 7, 1, [[red, green, blue, white, black, yellow, cyan]])
write_png(
    sys.argv[1],
    6,
    2,
    [
        [red, green, magenta, blue, white, black],
        [alpha_clear, yellow, cyan, magenta, blue, red],
    ],
)
PY

python3 - "$MOD_SOURCE" <<'PY'
import sys

data = bytearray(1084)
data[950] = 1
data[1080:1084] = b"M.K."
with open(sys.argv[1], "wb") as handle:
    handle.write(data)
PY

"$CONVERT" palette "$PNG_PALETTE" \
    --out "$PALETTE" \
    --colors 7

"$CONVERT" image "$PNG_SOURCE" \
    --out "$PNG_OUTPUT" \
    --palette "$PALETTE" \
    --frame-width 3 \
    --frame-height 2 \
    --transparent "#ff00ff"

"$PROBE" "$PNG_OUTPUT"

cat > "$MANIFEST" <<EOF
ANA_ASSETS 1
palette game ana_convert_palette.png --colors 7
image sheet ana_convert_sheet.png --palette game --frame-width 3 --frame-height 2 --transparent #ff00ff
music theme theme.mod
EOF

"$CONVERT" build "$MANIFEST" --out "$MANIFEST_OUT"
"$PROBE" "$MANIFEST_OUT/sheet.anaimg"
test -f "$MANIFEST_OUT/game.anapal"
cmp "$MOD_SOURCE" "$MANIFEST_OUT/theme.mod"

rm -rf "$SOURCE" "$OUTPUT" "$PNG_SOURCE" "$PNG_PALETTE" "$PALETTE" \
    "$PNG_OUTPUT" "$MANIFEST" "$MANIFEST_OUT" "$MOD_SOURCE"
