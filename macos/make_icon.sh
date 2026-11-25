#!/bin/sh
set -eu

GEN_BIN="$1"
OUT_ICNS="$2"
WORK_DIR="$3"

TGA="$WORK_DIR/icon_1024.tga"
PNG="$WORK_DIR/icon_1024.png"
ICONSET="$WORK_DIR/Texas.iconset"

mkdir -p "$WORK_DIR"
rm -rf "$ICONSET"
mkdir -p "$ICONSET"

"$GEN_BIN" "$TGA"
sips -s format png "$TGA" --out "$PNG" >/dev/null

# Generate all standard macOS App Icon sizes (16..512 at 1x and 2x Retina)
sips -z 16 16     "$PNG" --out "$ICONSET/icon_16x16.png"      >/dev/null
sips -z 32 32     "$PNG" --out "$ICONSET/icon_16x16@2x.png"   >/dev/null
sips -z 32 32     "$PNG" --out "$ICONSET/icon_32x32.png"      >/dev/null
sips -z 64 64     "$PNG" --out "$ICONSET/icon_32x32@2x.png"   >/dev/null
sips -z 128 128   "$PNG" --out "$ICONSET/icon_128x128.png"    >/dev/null
sips -z 256 256   "$PNG" --out "$ICONSET/icon_128x128@2x.png" >/dev/null
sips -z 256 256   "$PNG" --out "$ICONSET/icon_256x256.png"    >/dev/null
sips -z 512 512   "$PNG" --out "$ICONSET/icon_256x256@2x.png" >/dev/null
sips -z 512 512   "$PNG" --out "$ICONSET/icon_512x512.png"    >/dev/null
cp "$PNG"                       "$ICONSET/icon_512x512@2x.png"

# Pack Apple .icns container with all standard 1x & 2x Retina PNG OSType chunks
python3 - "$ICONSET" "$OUT_ICNS" <<'PY'
import os, struct, sys

iconset_dir, out_path = sys.argv[1], sys.argv[2]
entries = [
    (b"icp4", "icon_16x16.png"),
    (b"icp5", "icon_32x32.png"),
    (b"icp6", "icon_32x32@2x.png"),
    (b"ic07", "icon_128x128.png"),
    (b"ic08", "icon_256x256.png"),
    (b"ic09", "icon_512x512.png"),
    (b"ic10", "icon_512x512@2x.png"),
    (b"ic11", "icon_16x16@2x.png"),
    (b"ic12", "icon_32x32@2x.png"),
    (b"ic13", "icon_128x128@2x.png"),
    (b"ic14", "icon_256x256@2x.png"),
]
body = bytearray()
for tag, name in entries:
    with open(os.path.join(iconset_dir, name), "rb") as f:
        data = f.read()
    body += tag + struct.pack(">I", len(data) + 8) + data

with open(out_path, "wb") as f:
    f.write(b"icns" + struct.pack(">I", len(body) + 8) + body)
PY
