#!/bin/bash
APPIMAGE_SOURCE_ROOT="$MESON_SOURCE_ROOT/packages/appimage_bundle"
APPDIR="$APPIMAGE_SOURCE_ROOT/AppDir"
AEGISUB=${DESTDIR}/"${MESON_INSTALL_PREFIX}"/"$1"//aegisub

for f in `ls "${DESTDIR}/${MESON_INSTALL_PREFIX}/"Aegisub-*.AppImage 2> /dev/null`; do
    rm "$f"
done

linuxdeploy --appdir "$APPDIR" --executable "$MESON_BUILD_ROOT/aegisub" \
    --desktop-file "$APPDIR"/aegisub.desktop --icon-file "$APPDIR"/aegisub.svg \
    --output appimage

(hash upx && (upx -l "$AEGISUB")) || \
    (hash upx && upx "$AEGISUB")

appimagetool "$APPDIR" -g -s --comp xz && \

mkdir -p "$MESON_BUILD_ROOT"/usr/bin && \
for f in "$MESON_BUILD_ROOT"/*.AppImage; do
    cp "$f" ${DESTDIR}/${MESON_INSTALL_PREFIX}/$1/
done
