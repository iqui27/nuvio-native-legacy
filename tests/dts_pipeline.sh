#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
mkdir -p "$tmp/good" "$tmp/bad" "$tmp/native"
cc -std=c11 -D_GNU_SOURCE -fPIC -Isrc -c src/js.c -o "$tmp/js.o"
for abi in 0 1; do
  generation=3
  if [ "$abi" = 1 ]; then generation=4; fi
  g++ -std=c++11 -fPIC -shared -D_GLIBCXX_USE_CXX11_ABI=$abi -Isrc -Itests/dts_pipeline_sdk tests/dts_pipeline_native.cpp "$tmp/js.o" -o "$tmp/native/libplayerAPIs.so.$generation"
  g++ -std=c++11 -fPIC -shared -D_GLIBCXX_USE_CXX11_ABI=$abi -DDTS_NO_VOLUME -Isrc -Itests/dts_pipeline_sdk tests/dts_pipeline_native.cpp "$tmp/js.o" -o "$tmp/native/libplayerAPIs.so.$generation.none"
  g++ -std=c++11 -fPIC -shared -D_GLIBCXX_USE_CXX11_ABI=$abi -Isrc -Isrc/dts/adapter -Itests/dts_pipeline_sdk src/dts/adapter/starfish.cpp "$tmp/js.o" -o "$tmp/good/dts-starfish-webos$generation.so" -ldl -pthread
 done
cc -fPIC -shared -Isrc -Isrc/dts/adapter tests/dts_pipeline_bad.c -o "$tmp/bad/dts-starfish-webos4.so"
cc -std=c11 -Wall -Wextra -Werror -Isrc tests/dts_pipeline.c src/dts/dts_pipeline.c src/webosver.c -ldl -pthread -o "$tmp/test"
# Each runtime exports only its real generation Feed ABI. Auto probe must fall back correctly.
for generation in 3 4; do
  ln -sf "libplayerAPIs.so.$generation" "$tmp/native/libplayerAPIs.so"
  # Explicit other-generation probes must reject the Feed ABI mismatch.
  LD_LIBRARY_PATH="$tmp/native" "$tmp/test" "$tmp/good" "$tmp/bad"
  DTS_MEDIA_ID_PHASE=load LD_LIBRARY_PATH="$tmp/native" "$tmp/test" "$tmp/good" "$tmp/bad"
  DTS_MEDIA_ID_PHASE=preroll LD_LIBRARY_PATH="$tmp/native" "$tmp/test" "$tmp/good" "$tmp/bad"
  DTS_FOREGROUND_FALSE=1 DTS_MEDIA_ID_PHASE=preroll LD_LIBRARY_PATH="$tmp/native" "$tmp/test" "$tmp/good" "$tmp/bad"
  DTS_DEFER_COMPLETION=1 DTS_MEDIA_ID_PHASE=preroll LD_LIBRARY_PATH="$tmp/native" "$tmp/test" "$tmp/good" "$tmp/bad"
  ln -sf "libplayerAPIs.so.$generation.none" "$tmp/native/libplayerAPIs.so"
  DTS_EXPECT_NO_VOLUME=1 LD_LIBRARY_PATH="$tmp/native" "$tmp/test" "$tmp/good" "$tmp/bad"
done
