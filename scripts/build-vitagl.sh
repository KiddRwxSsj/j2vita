#!/usr/bin/env bash
# Builds a pinned vitaGL without the boot splash. The splash opens a second
# sceGxm context, which costs boot time and breaks Vita3K.
# Usage: build-vitagl.sh <dest-dir>   (produces <dest-dir>/libvitaGL.a)
set -euo pipefail
dest="${1:?destination dir}"
rev=3dddc43eff782a7ae7cc4b26498aba58fcf875c2
[ -f "$dest/libvitaGL.a" ] && [ "$(cat "$dest/rev" 2>/dev/null)" = "$rev" ] && exit 0
src="$dest/src"
if [ ! -d "$src/.git" ]; then
  git clone -q https://github.com/Rinnegatamante/vitaGL.git "$src"
fi
git -C "$src" fetch -q --depth 1 origin "$rev" 2>/dev/null || true
git -C "$src" checkout -q "$rev"
make -C "$src" clean >/dev/null
make -C "$src" -j"$(nproc)" NO_SPLASHSCREEN=1 >/dev/null
cp "$src/libvitaGL.a" "$dest/"
echo "$rev" > "$dest/rev"
