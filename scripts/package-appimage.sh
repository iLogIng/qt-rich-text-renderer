#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

LINUXDEPLOY="${LINUXDEPLOY:-linuxdeploy}"
APPIMAGETOOL="${APPIMAGETOOL:-appimagetool}"

if ! command -v "$LINUXDEPLOY" >/dev/null 2>&1; then
  echo "linuxdeploy not found: $LINUXDEPLOY" >&2
  echo "Download the AppImage from https://github.com/linuxdeploy/linuxdeploy/releases" >&2
  echo "or set LINUXDEPLOY=/path/to/linuxdeploy" >&2
  exit 1
fi
if ! command -v "$APPIMAGETOOL" >/dev/null 2>&1; then
  echo "appimagetool not found: $APPIMAGETOOL" >&2
  echo "Download the AppImage from https://github.com/AppImage/appimagetool/releases" >&2
  echo "or set APPIMAGETOOL=/path/to/appimagetool" >&2
  exit 1
fi

rm -rf AppDir dist
"$LINUXDEPLOY" --appimage-extract-and-run \
  --appdir AppDir \
  --executable build/rich-text-render \
  --desktop-file packaging/rich-text-render.desktop \
  --icon-file packaging/rich-text-render.png \
  --plugin qt || true

if [[ ! -x AppDir/usr/bin/rich-text-render ]]; then
  echo "linuxdeploy did not populate AppDir/usr/bin/rich-text-render" >&2
  exit 1
fi

mkdir -p \
  AppDir/usr/share/rich-text-render/renderer/dist \
  AppDir/usr/share/rich-text-render/config \
  AppDir/usr/share/applications \
  AppDir/usr/share/icons/hicolor/256x256/apps

cp third_party/quickjs-ng/build/qjs AppDir/usr/bin/qjs
chmod +x AppDir/usr/bin/qjs
cp renderer/dist/*.mjs AppDir/usr/share/rich-text-render/renderer/dist/
cp config/settings.ini AppDir/usr/share/rich-text-render/config/settings.ini
cp packaging/rich-text-render.desktop AppDir/rich-text-render.desktop
cp packaging/rich-text-render.png AppDir/rich-text-render.png
cp packaging/rich-text-render.desktop AppDir/usr/share/applications/
cp packaging/rich-text-render.png AppDir/usr/share/icons/hicolor/256x256/apps/rich-text-render.png

cat > AppDir/AppRun <<'EOF'
#!/bin/sh
HERE=$(dirname "$(readlink -f "$0")")
export LD_LIBRARY_PATH="$HERE/usr/lib:$LD_LIBRARY_PATH"
export QT_PLUGIN_PATH="$HERE/usr/plugins"
export QT_QPA_PLATFORM_PLUGIN_PATH="$HERE/usr/plugins/platforms"
exec "$HERE/usr/bin/rich-text-render" "$@"
EOF
chmod +x AppDir/AppRun

mkdir -p dist
ARCH=x86_64 "$APPIMAGETOOL" --appimage-extract-and-run AppDir dist/rich-text-render-x86_64.AppImage
ls -lh dist/rich-text-render-x86_64.AppImage
