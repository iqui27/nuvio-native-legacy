#!/usr/bin/env bash
set -euo pipefail
# Do not refresh unrelated Microsoft/Docker repositories just to run C tests.
# A runner with the required development packages needs no package operation.
if command -v cc >/dev/null && command -v ffmpeg >/dev/null &&
   pkg-config --exists sdl2 SDL2_image SDL2_ttf gl glesv2 egl; then
  echo 'Host compiler, FFmpeg, SDL2 and GL development packages already available.'
  exit 0
fi
. /etc/os-release
test "$ID" = ubuntu
test "$VERSION_CODENAME" = noble
sources=$(mktemp)
trap 'rm -f "$sources"' EXIT
cat > "$sources" <<'SOURCES'
deb https://archive.ubuntu.com/ubuntu noble main universe
deb https://archive.ubuntu.com/ubuntu noble-updates main universe
deb https://security.ubuntu.com/ubuntu noble-security main universe
SOURCES
options=(-o "Dir::Etc::sourcelist=$sources" -o 'Dir::Etc::sourceparts=-'
         -o APT::Get::List-Cleanup=0 -o Dpkg::Lock::Timeout=20
         -o Acquire::Retries=1 -o Acquire::http::Timeout=20
         -o Acquire::https::Timeout=20)
timeout --kill-after=10s 120s sudo -n apt-get "${options[@]}" update
timeout --kill-after=10s 180s sudo -n env DEBIAN_FRONTEND=noninteractive \
  apt-get "${options[@]}" install -y --no-install-recommends \
    build-essential pkg-config libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev libgl-dev libgles-dev libegl-dev ffmpeg
pkg-config --exists sdl2 SDL2_image SDL2_ttf gl glesv2 egl
ffmpeg -version
