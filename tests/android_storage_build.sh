#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
NUVIO_ANDROID_OFFLINE=1 bash tools/android.sh --debug-only
