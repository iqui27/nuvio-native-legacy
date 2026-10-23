#!/bin/bash
# Pinned libdovi metadata converter. Rust >=1.88, plus the selected target's std.
# Host: bash tools/build-dv7.sh
# LG SDK: NUVIO_DV7_TARGET=armv7-unknown-linux-gnueabi bash tools/build-dv7.sh
set -euo pipefail
cd "$(dirname "$0")/.."
target="${NUVIO_DV7_TARGET:-$(rustc -vV | sed -n 's/^host: //p')}"
output="${NUVIO_DV7_ROOT:-$PWD/build/dv7}"
mkdir -p "$output"
output=$(cd "$output" && pwd)
if [ "$target" = armv7-unknown-linux-gnueabi ]; then
  export CARGO_TARGET_ARMV7_UNKNOWN_LINUX_GNUEABI_LINKER=arm-webos-linux-gnueabi-gcc
fi
cargo build --manifest-path tools/dv7/Cargo.toml --locked --release \
  --target "$target" --target-dir "$output/target"
cp "$output/target/$target/release/libnuvio_dv7.a" "$output/"
cp tools/dv7/Cargo.lock "$output/"
printf 'libdovi (dolby_vision) 3.3.2; see Cargo.lock for exact dependencies.\nhttps://github.com/quietvoid/dovi_tool\n' > "$output/SOURCE.txt"
# Preserve every transitive dependency's license alongside the binary package.
cargo metadata --manifest-path tools/dv7/Cargo.toml --locked --format-version 1 > "$output/metadata.json"
python3 - "$output" <<'PY'
import json, pathlib, shutil, sys
out=pathlib.Path(sys.argv[1]); dest=out/'licenses'; dest.mkdir(exist_ok=True)
for package in json.loads((out/'metadata.json').read_text())['packages']:
    root=pathlib.Path(package['manifest_path']).parent
    for p in root.iterdir():
        if p.is_file() and p.name.upper().startswith(('LICENSE','COPYING','NOTICE')):
            shutil.copyfile(p,dest/(package['name']+'-'+package['version']+'-'+p.name))
PY
echo "DV7 library: $output/libnuvio_dv7.a"
