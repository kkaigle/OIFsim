#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CPP_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
ROOT_DIR="$(cd "$CPP_DIR/.." && pwd)"

APP_NAME="Iraq Theater Command"
BUNDLE_ID="com.oifsim.iraqtheatercommand"
VERSION="${ITC_VERSION:-0.7.0}"
BUILD_DIR="$CPP_DIR/build"
DIST_DIR="$CPP_DIR/dist"
APP_BUNDLE="$DIST_DIR/${APP_NAME}.app"

echo "[ITC Mac Packager] Ensuring Release binary is built..."
if [ ! -f "$CPP_DIR/assets/map/theater.bin" ]; then
    python3 "$CPP_DIR/tools/extract_assets.py"
fi

cmake -B "$BUILD_DIR" -S "$CPP_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" --config Release -j

rm -rf "$DIST_DIR"
mkdir -p "$APP_BUNDLE/Contents/MacOS"
mkdir -p "$APP_BUNDLE/Contents/Resources"

echo "[ITC Mac Packager] Populating ${APP_NAME}.app bundle..."
cp "$BUILD_DIR/IraqTheaterCommand" "$APP_BUNDLE/Contents/MacOS/IraqTheaterCommand"
chmod +x "$APP_BUNDLE/Contents/MacOS/IraqTheaterCommand"
cp -R "$CPP_DIR/assets" "$APP_BUNDLE/Contents/Resources/assets"

# Generate macOS AppIcon.icns from docs/icon-512.png
ICON_SRC="$ROOT_DIR/docs/icon-512.png"
if [ -f "$ICON_SRC" ]; then
    ICONSET_DIR="$DIST_DIR/AppIcon.iconset"
    mkdir -p "$ICONSET_DIR"
    sips -z 16 16     "$ICON_SRC" --out "$ICONSET_DIR/icon_16x16.png"      >/dev/null
    sips -z 32 32     "$ICON_SRC" --out "$ICONSET_DIR/icon_16x16@2x.png"   >/dev/null
    sips -z 32 32     "$ICON_SRC" --out "$ICONSET_DIR/icon_32x32.png"      >/dev/null
    sips -z 64 64     "$ICON_SRC" --out "$ICONSET_DIR/icon_32x32@2x.png"   >/dev/null
    sips -z 128 128   "$ICON_SRC" --out "$ICONSET_DIR/icon_128x128.png"    >/dev/null
    sips -z 256 256   "$ICON_SRC" --out "$ICONSET_DIR/icon_128x128@2x.png" >/dev/null
    sips -z 256 256   "$ICON_SRC" --out "$ICONSET_DIR/icon_256x256.png"    >/dev/null
    sips -z 512 512   "$ICON_SRC" --out "$ICONSET_DIR/icon_256x256@2x.png" >/dev/null
    sips -z 512 512   "$ICON_SRC" --out "$ICONSET_DIR/icon_512x512.png"    >/dev/null
    cp "$ICON_SRC" "$ICONSET_DIR/icon_512x512@2x.png"
    iconutil -c icns "$ICONSET_DIR" -o "$APP_BUNDLE/Contents/Resources/AppIcon.icns" || true
    rm -rf "$ICONSET_DIR"
fi

cat > "$APP_BUNDLE/Contents/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleDevelopmentRegion</key>
    <string>en</string>
    <key>CFBundleDisplayName</key>
    <string>${APP_NAME}</string>
    <key>CFBundleExecutable</key>
    <string>IraqTheaterCommand</string>
    <key>CFBundleIconFile</key>
    <string>AppIcon</string>
    <key>CFBundleIdentifier</key>
    <string>${BUNDLE_ID}</string>
    <key>CFBundleInfoDictionaryVersion</key>
    <string>6.0</string>
    <key>CFBundleName</key>
    <string>${APP_NAME}</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>${VERSION}</string>
    <key>CFBundleVersion</key>
    <string>155</string>
    <key>LSMinimumSystemVersion</key>
    <string>12.0</string>
    <key>NSHighResolutionCapable</key>
    <true/>
    <key>LSApplicationCategoryType</key>
    <string>public.app-category.strategy-games</string>
</dict>
</plist>
EOF

echo "[ITC Mac Packager] Ad-hoc signing ${APP_NAME}.app..."
codesign --force --deep --sign - "$APP_BUNDLE"

echo "[ITC Mac Packager] Creating macOS .pkg installer..."
PKG_PATH="$DIST_DIR/IraqTheaterCommand-macOS-Installer.pkg"
pkgbuild \
    --component "$APP_BUNDLE" \
    --install-location "/Applications" \
    --identifier "$BUNDLE_ID" \
    --version "$VERSION" \
    "$PKG_PATH"

echo "[ITC Mac Packager] Creating macOS .dmg disk image..."
DMG_STAGE="$DIST_DIR/dmg_stage"
DMG_PATH="$DIST_DIR/IraqTheaterCommand-macOS.dmg"
mkdir -p "$DMG_STAGE"
cp -R "$APP_BUNDLE" "$DMG_STAGE/"
ln -s /Applications "$DMG_STAGE/Applications"
hdiutil create \
    -volname "$APP_NAME" \
    -srcfolder "$DMG_STAGE" \
    -ov -format UDZO \
    "$DMG_PATH" >/dev/null
rm -rf "$DMG_STAGE"

echo "[ITC Mac Packager] Done! Generated artifacts in $DIST_DIR:"
ls -lh "$DIST_DIR"

