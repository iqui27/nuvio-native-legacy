#!/bin/bash
# Verified upstream SDL/mpv binaries; macOS mpv dylibs are bundled recursively.
set -euo pipefail
DESKTOP_OUT="${NUVIO_DESKTOP_OUT:-/Volumes/ExternalSSD/tmp/rev/desktop}"
DESKTOP_CACHE="${NUVIO_DESKTOP_CACHE:-$DESKTOP_OUT/cache}"
mkdir -p "$DESKTOP_CACHE" "$DESKTOP_OUT"
fetch() {
  local url="$1" sha="$2" file="$DESKTOP_CACHE/${1##*/}"
  if [ ! -f "$file" ]; then
    curl --fail --location --proto '=https' --retry 2 "$url" -o "$file.part"
    mv "$file.part" "$file"
  fi
  printf '%s  %s\n' "$sha" "$file" | shasum -a 256 -c - >/dev/null
}
sdl_deps() {
  local target="$1" name version folder sha url archive
  while read -r name version folder macsha winsha; do
    if [ "$target" = mac ]; then
      archive="$name-$version.dmg"; sha="$macsha"
    else
      archive="$name-devel-$version-mingw.tar.gz"; sha="$winsha"
    fi
    url="https://www.libsdl.org/$folder/$archive"
    fetch "$url" "$sha"
    if [ "$target" = mac ]; then
      if [ ! -d "$DESKTOP_CACHE/frameworks/$name.framework" ]; then
        # 7zz extracts read-only DMGs even on hosts without disk-image mounting.
        command -v 7zz >/dev/null || { echo 'Install extraction tool: brew install sevenzip' >&2; exit 1; }
        7zz x -y "-o$DESKTOP_CACHE/$name-$version" "$DESKTOP_CACHE/$archive" >/dev/null
        mkdir -p "$DESKTOP_CACHE/frameworks"
        ditto "$DESKTOP_CACHE/$name-$version/$name/$name.framework" "$DESKTOP_CACHE/frameworks/$name.framework"
      fi
    else
      tar xzf "$DESKTOP_CACHE/$archive" -C "$DESKTOP_CACHE"
    fi
  done <<'DEPS'
SDL2 2.32.10 release 4a7ac31640d70214e848f994be8a12849c0f97918a7e6c2e27a40036166d1a7f 83a5d74012311edc3c0d40ea6faecbe57ad692aa033fa5dc273cc937e3938ff2
SDL2_image 2.8.8 projects/SDL_image/release 7b3d87be94c96bf6b2074c17f6379586216c1482a308b9323fd37cdb523f6d14 67446b12f14e4667b76a5721c91e419363e65b74b66cde8057edb29add90cd42
SDL2_ttf 2.24.0 projects/SDL_ttf/release 677404e23a1a5f6efcbe64697471c5161283e4af5a6403dcb92235d7d83805cd 3a09e0a967ad53eca3ff2701de4d0df3369c0368739a1088a78d9d74908ef2c3
DEPS
}
mpv_windows_deps() {
  local archive=mpv-dev-x86_64-20261021-git-e471496c56.7z
  # GitHub upstream asset digest, independently retrieved from the release API.
  local sha=efc719eff3dc5954b4a5efc800632808bcec01cc9034940dd14e5eb7fe9ea633
  local url="https://github.com/shinchiro/mpv-winbuild-cmake/releases/download/20261021/$archive"
  fetch "$url" "$sha"
  MPV_WINDOWS="$DESKTOP_CACHE/mpv-win64-20261021"
  command -v 7zz >/dev/null || { echo "Install extraction tool: brew install sevenzip" >&2; return 1; }
  mkdir -p "$MPV_WINDOWS"
  7zz x -y "-o$MPV_WINDOWS" "$DESKTOP_CACHE/$archive" >/dev/null
  test -f "$MPV_WINDOWS/include/mpv/render_gl.h"
  test -f "$MPV_WINDOWS/libmpv.dll.a"
  test -f "$MPV_WINDOWS/libmpv-2.dll"
  # libmpv imports vulkan-1.dll eagerly even when rendering through OpenGL.
  # Digest: https://vulkan.lunarg.com/sdk/files.json (Windows 1.4.363.0).
  fetch https://sdk.lunarg.com/sdk/download/1.4.363.0/windows/VulkanRT-X64-1.4.363.0-Components.zip a25a927aa8b9f0371048f1861cf88ac3b9bc9b1fb332c42d897c8ab32695769a
  unzip -qo "$DESKTOP_CACHE/VulkanRT-X64-1.4.363.0-Components.zip" -d "$DESKTOP_CACHE/vulkan-runtime"
  VULKAN_WINDOWS=$(find "$DESKTOP_CACHE/vulkan-runtime" -path '*/x64/vulkan-1.dll' -print -quit)
  test -n "$VULKAN_WINDOWS"
  fetch https://raw.githubusercontent.com/mpv-player/mpv/e471496c56/LICENSE.GPL edaef632cbb643e4e7a221717a6c441a4c1a7c918e6e4d56debc3d8739b233f6
  fetch https://raw.githubusercontent.com/mpv-player/mpv/e471496c56/LICENSE.LGPL 72b672113d642cbb8ef5dcc76938db801983c56e50b1400ab930f1a64d6dc8d9
}
mpv_mac_deps() {
  local prefix="${NUVIO_MPV_PREFIX:-$(brew --prefix mpv 2>/dev/null || true)}"
  if [ ! -f "$prefix/lib/libmpv.dylib" ]; then
    brew install mpv
    prefix=$(brew --prefix mpv)
  fi
  MPV_MAC="$prefix"
}
desktop_env() {
  local header="$1" values="$1.values" oldmask
  oldmask=$(umask)
  umask 077
  bash tools/env.sh "${NUVIO_DESKTOP_ENV_MODE:---require-core}" >/dev/null
  bash tools/env.sh --env-file "$values"
  python3 - "$values" "$header" <<'PY'
import json, sys
with open(sys.argv[1]) as source, open(sys.argv[2], 'w') as target:
    for line in source:
        key, value = line.rstrip('\n').split('=', 1)
        if key == 'NV_VERSAO': value = '2.1'
        target.write('#define ' + key + ' ' + json.dumps(value) + '\n')
PY
  chmod 600 "$header"
  rm "$values"
  umask "$oldmask"
}
desktop_art() {
  local dest="$1"
  mkdir -p "$dest"
  NUVIO_ARTE_ESTAGIO="$dest/art" bash tools/tizen-art.sh >/dev/null
  mkdir -p "$dest/art/desktop"
  cp design/desktop/ajustes-background.jpg "$dest/art/desktop/ajustes-background.jpg"
  ditto deploy/app/fonts "$dest/fonts"
  ditto deploy/app/licencas "$dest/licencas"
}
