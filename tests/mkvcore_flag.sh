# Sourced by the mkv*.sh tests. NV_RUST_MKVCORE=1 builds the same tests against
# the Rust core (-DNV_RUST_MKVCORE + libmkvcore.a); unset/0 is the C path.
# Expand as ${MKVCORE_CC[@]+"${MKVCORE_CC[@]}"} (bash 3.2 + set -u).
MKVCORE_CC=()
if [ "${NV_RUST_MKVCORE:-0}" = 1 ]; then
  MKVCORE_LIB=$(bash tools/rust-mkvcore.sh)
  MKVCORE_CC=(-DNV_RUST_MKVCORE "$MKVCORE_LIB")
  echo "[mkvcore] Rust core ON ($MKVCORE_LIB)"
fi
