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

pkgbuild --analyze --root "$PAY" "$WORK/components.plist" >/dev/null
python3 - "$WORK/components.plist" <<'PY'
import plistlib, sys
p = sys.argv[1]
d = plistlib.load(open(p, 'rb'))
for c in d:
    c['BundleIsRelocatable'] = False
    c['BundleHasStrictIdentifier'] = False
    c['BundleOverwriteAction'] = 'upgrade'
plistlib.dump(d, open(p, 'wb'))
PY

pkgbuild --root "$PAY" --component-plist "$WORK/components.plist" \
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
