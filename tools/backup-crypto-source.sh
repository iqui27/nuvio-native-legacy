#!/bin/sh
set -eu
# Download on the build host, before entering an SDK container without curl.
# Reuse the same maintained mbedTLS release already used by Android.
root=${1:?usage: backup-crypto-source.sh CACHE_DIRECTORY}
mkdir -p "$root"
archive="$root/mbedtls-3.6.7.tar.bz2"
digest=a7e8bcbec0e6f761b4af24f25677626b35f762f68eef79c08677a363212d11f6
if [ ! -f "$archive" ]; then
  curl --proto '=https' --tlsv1.2 -fsSL \
    https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-3.6.7/mbedtls-3.6.7.tar.bz2 -o "$archive.tmp"
  mv "$archive.tmp" "$archive"
fi
if command -v sha256sum >/dev/null 2>&1; then
  actual=$(sha256sum "$archive" | cut -d ' ' -f1)
else
  actual=$(shasum -a 256 "$archive" | cut -d ' ' -f1)
fi
[ "$actual" = "$digest" ] || { echo 'backup crypto: source checksum mismatch' >&2; exit 1; }
if [ ! -f "$root/mbedtls-3.6.7/include/mbedtls/version.h" ]; then tar -xjf "$archive" -C "$root"; fi
