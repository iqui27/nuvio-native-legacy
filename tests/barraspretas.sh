#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
cc -O1 -DAJUSTES_TESTE=1 -Isrc tests/barraspretas.c -o /tmp/nv-barraspretas && /tmp/nv-barraspretas
grep -q nativeBarrasPretas android/app/src/main/java/space/nuvio/nativelegacy/NvPlayer.kt
echo "barraspretas: ok"
