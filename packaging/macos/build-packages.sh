#!/bin/bash
# Uso: build-packages.sh <VERSION> <dir_con_RiserLab.vst3_y_RiserLab.component> <dir_salida>
set -euo pipefail
VERSION="$1"; SRC="$2"; OUT="$3"
HERE="$(cd "$(dirname "$0")" && pwd)"
WORK="$(mktemp -d)"
mkdir -p "$OUT"

PAY="$WORK/payload"
mkdir -p "$PAY/Library/Audio/Plug-Ins/VST3" "$PAY/Library/Audio/Plug-Ins/Components"
ditto "$SRC/RiserLab.vst3" "$PAY/Library/Audio/Plug-Ins/VST3/RiserLab.vst3"
ditto "$SRC/RiserLab.component" "$PAY/Library/Audio/Plug-Ins/Components/RiserLab.component"

# Construir el paquete directamente desde el payload con rutas /Library/... .
# Evita el component-plist generado por pkgbuild --analyze, que provoca
# "pkgbuild: error: (null)" en el runner macOS actual.
pkgbuild --root "$PAY" \
  --identifier com.crayldr.riserlab.pkg --version "$VERSION" \
  --install-location / --scripts "$HERE/scripts" "$WORK/RiserLab-component.pkg"

sed "s/@VERSION@/$VERSION/g" "$HERE/distribution.xml.in" > "$WORK/distribution.xml"
productbuild --distribution "$WORK/distribution.xml" --resources "$HERE/resources" \
  --package-path "$WORK" "$WORK/Instalar RiserLab.pkg"

ZP="$WORK/zip/RiserLab-macOS-$VERSION"
mkdir -p "$ZP"
ditto "$SRC/RiserLab.vst3" "$ZP/RiserLab.vst3"
ditto "$SRC/RiserLab.component" "$ZP/RiserLab.component"
cp "$HERE/LEEME-INSTALACION.txt" "$ZP/"
(cd "$WORK/zip" && ditto -c -k --sequesterRsrc --keepParent "RiserLab-macOS-$VERSION" "$OUT/RiserLab-macOS-Universal-$VERSION.zip")

DM="$WORK/dmg"; mkdir -p "$DM"
cp "$WORK/Instalar RiserLab.pkg" "$DM/"
cp "$HERE/LEEME-INSTALACION.txt" "$DM/"
cp "$HERE/Desinstalar-RiserLab.command" "$DM/Desinstalar RiserLab.command"
chmod +x "$DM/Desinstalar RiserLab.command"
hdiutil create -volname "RiserLab $VERSION" -srcfolder "$DM" -ov -format UDZO "$OUT/RiserLab-macOS-Universal-$VERSION.dmg"
cp "$WORK/Instalar RiserLab.pkg" "$OUT/RiserLab-macOS-Universal-$VERSION.pkg"
rm -rf "$WORK"
