#!/bin/bash
# Builds rust/mkvcore as a static library and prints the path of the .a.
# Used with -DNV_RUST_MKVCORE (on by default in the android/mac builds; docs/plans/rust-mapa.md).
#   tools/rust-mkvcore.sh [rust-target]      e.g. aarch64-linux-android
#   tools/rust-mkvcore.sh --android DIR      DIR/arm64-v8a/libmkvcore.a + DIR/armeabi-v7a/libmkvcore.a
# Toolchain: RUSTUP_HOME/CARGO_HOME default to the external SSD install. Never
# installs anything: a missing cargo or target is an error.
set -eu
cd "$(dirname "$0")/.."
export RUSTUP_HOME="${RUSTUP_HOME:-/Volumes/ExternalSSD/rust/rustup}"
export CARGO_HOME="${CARGO_HOME:-/Volumes/ExternalSSD/rust/cargo}"
export PATH="$CARGO_HOME/bin:$PATH"
command -v cargo >/dev/null || { echo "rust-mkvcore: cargo not found" >&2; exit 1; }
if [ "${1:-}" = --android ]; then
  dest="${2:?rust-mkvcore: --android DIR}"
  for par in aarch64-linux-android:arm64-v8a armv7-linux-androideabi:armeabi-v7a; do
    mkdir -p "$dest/${par#*:}"
    cp "$("$0" "${par%%:*}")" "$dest/${par#*:}/libmkvcore.a"
  done
  echo "$dest"
  exit 0
fi
alvo="${1:-}"
args=(--release --manifest-path rust/mkvcore/Cargo.toml --target-dir "${NV_RUST_TARGET_DIR:-rust/mkvcore/target}")
[ -n "$alvo" ] && args+=(--target "$alvo")
# Keep the standalone no_std personality unless std is linked by DV7.
[ "${NUVIO_RUST_EXTERNAL_PERSONALITY:-0}" = 1 ] && args+=(--features external-personality)
cargo build -q "${args[@]}" >&2
dir="${NV_RUST_TARGET_DIR:-rust/mkvcore/target}"
if [ -n "$alvo" ]; then echo "$(pwd)/$dir/$alvo/release/libmkvcore.a"; else echo "$(pwd)/$dir/release/libmkvcore.a"; fi
