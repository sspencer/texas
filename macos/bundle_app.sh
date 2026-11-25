#!/bin/sh
set -eu

BINARY="$1"
APP_BUNDLE="$2"
ICON_ICNS="$3"
INFO_PLIST="$4"

rm -rf "$APP_BUNDLE"
mkdir -p "$APP_BUNDLE/Contents/MacOS" \
         "$APP_BUNDLE/Contents/Resources" \
         "$APP_BUNDLE/Contents/Frameworks"

cp "$BINARY" "$APP_BUNDLE/Contents/MacOS/texas"
chmod +x "$APP_BUNDLE/Contents/MacOS/texas"
cp "$INFO_PLIST" "$APP_BUNDLE/Contents/Info.plist"
cp "$ICON_ICNS" "$APP_BUNDLE/Contents/Resources/Texas.icns"

# Bundle non-system dynamic libraries (e.g., Homebrew libraylib) so Texas.app is self-contained.
EXE="$APP_BUNDLE/Contents/MacOS/texas"
LIBS=$(otool -L "$EXE" | tail -n +2 | awk '{print $1}' | grep -v '^/System/' | grep -v '^/usr/lib/' | grep -v '^@' || true)
for lib in $LIBS; do
	if [ -f "$lib" ]; then
		base=$(basename "$lib")
		dest="$APP_BUNDLE/Contents/Frameworks/$base"
		cp -L "$lib" "$dest"
		chmod 755 "$dest"
		install_name_tool -id "@executable_path/../Frameworks/$base" "$dest"
		install_name_tool -change "$lib" "@executable_path/../Frameworks/$base" "$EXE"
	fi
done

# Ad-hoc sign so Apple Silicon macOS launches the bundle cleanly on double-click.
if command -v codesign >/dev/null 2>&1; then
	codesign --force --deep --sign - "$APP_BUNDLE" >/dev/null
fi
