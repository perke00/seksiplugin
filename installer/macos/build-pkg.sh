#!/usr/bin/env bash
# Builds a macOS installer package (.pkg) with the standard install locations:
#   VST3 -> /Library/Audio/Plug-Ins/VST3      AU -> /Library/Audio/Plug-Ins/Components
# usage: build-pkg.sh <version> <artefacts dir (…/KitsuneEQ_artefacts/Release)> <output dir>
set -euo pipefail
VERSION="$1"; ART="$2"; OUT="$3"
HERE="$(cd "$(dirname "$0")" && pwd)"
WORK="$(mktemp -d)"
mkdir -p "$OUT" "$WORK/vst3/Library/Audio/Plug-Ins/VST3" "$WORK/au/Library/Audio/Plug-Ins/Components"

cp -R "$ART/VST3/KITSUNE EQ.vst3"      "$WORK/vst3/Library/Audio/Plug-Ins/VST3/"
cp -R "$ART/AU/KITSUNE EQ.component"   "$WORK/au/Library/Audio/Plug-Ins/Components/"

pkgbuild --root "$WORK/vst3" --identifier com.prodbyperke.kitsuneeq.vst3 --version "$VERSION" --install-location / "$WORK/KitsuneEQ-VST3.pkg"
pkgbuild --root "$WORK/au"   --identifier com.prodbyperke.kitsuneeq.au   --version "$VERSION" --install-location / "$WORK/KitsuneEQ-AU.pkg"

sed "s/@VERSION@/$VERSION/g" "$HERE/distribution.xml" > "$WORK/distribution.xml"
productbuild --distribution "$WORK/distribution.xml" --resources "$HERE/resources" --package-path "$WORK" \
    "$OUT/KitsuneEQ-$VERSION-macOS.pkg"
echo "Built $OUT/KitsuneEQ-$VERSION-macOS.pkg"
