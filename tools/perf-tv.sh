#!/bin/bash
# Usage: bash tools/perf-tv.sh <adb-serial> [output-dir]
# Requires an already installed, logged-in TV app with seeded rows/collections.
# No install/data reset/logcat clear. Perf-only anchors avoid saved layout writes.
# The final player/stats samples are WAITING UI, not video decode/playback.
# Use the same profile, seeded content, UI resolution/theme and cache warmness
# on both runs. Dismiss first-run cards; disable fps-cena automation beforehand.
# Outputs: numeric-only perf.log, window-level perf.csv, summary.txt (MISSING
# lists unobserved screens). Check coverage before using a comparison.
# C9: create perf-tv in dados_dir(), navigate manually, collect [perf-tv] lines,
# then: python3 tools/perf-compara.py --capture perf.log output-dir
# Remove perf-tv to stop. Android: property is restored automatically.
set -euo pipefail
if [[ $# -lt 1 || $# -gt 2 ]]; then
  echo 'Usage: bash tools/perf-tv.sh <adb-serial> [output-dir]' >&2
  exit 2
fi
serial=$1
out=${2:-perf-tv-output}
package=${PERF_TV_PACKAGE:-space.nuvio.nativelegacy}
if [[ ! $package =~ ^[a-zA-Z0-9_.]+$ || $serial == -* || -z $serial ]]; then
  echo 'Invalid package or serial' >&2
  exit 2
fi
command -v adb >/dev/null
command -v python3 >/dev/null
root=$(cd "$(dirname "$0")/.." && pwd)
mkdir -p "$out"
# Refuse overwriting evidence. Reusing the route never clears application data.
if [[ -e $out/perf.log || -e $out/perf.csv || -e $out/summary.txt ]]; then
  echo 'Output already contains evidence; choose an empty directory' >&2
  exit 2
fi
tmp=$(mktemp -d)
capture_pid=
original=
flag_set=0
remote() { adb -s "$serial" "$@" 2>/dev/null; }
cleanup() {
  if [[ $flag_set == 1 ]]; then
    remote shell setprop debug.nuvio.perf "${original:-0}" >/dev/null || true
  fi
  if [[ -n $capture_pid ]]; then
    kill "$capture_pid" 2>/dev/null || true
    wait "$capture_pid" 2>/dev/null || true
  fi
  rm -rf "$tmp"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
# Preserve a pre-existing flag. Never read private application files.
original=$(remote shell getprop debug.nuvio.perf | tr -d '\r\n')
if [[ -n $original && ! $original =~ ^[01]$ ]]; then
  echo 'Unexpected perf property value' >&2
  exit 2
fi
# Reset the measurement window even when an earlier run left perf enabled.
# A simultaneous perf-tv file flag must be absent on Android.
flag_set=1
remote shell setprop debug.nuvio.perf 0 >/dev/null
sleep 2
remote shell setprop debug.nuvio.perf 1 >/dev/null
# A FIFO keeps the logcat child under our cleanup trap; only strict numeric
# perf lines reach disk. A marker excludes every buffered previous run.
mkfifo "$tmp/logcat"
adb -s "$serial" logcat -v raw -s nuvio:I NVPerfCapture:I >"$tmp/logcat" 2>/dev/null &
capture_pid=$!
marker="perf-start-$$"
python3 -u - "$marker" "$tmp/logcat" >"$out/perf.log" <<'PY' &
import re
import sys
marker, source = sys.argv[1:]
active = False
with open(source, encoding="utf-8", errors="replace") as stream:
    for raw in stream:
        line = raw.strip()
        if line == marker:
            active = True
        elif active and re.fullmatch(
            r"\[perf-tv\] screen=[a-z-]+ frames=\d+ "
            r"p50_ms=\d+\.\d+ p95_ms=\d+\.\d+ max_ms=\d+\.\d+ over33=\d+ "
            r"uploads_pf=\d+\.\d+ text_pf=\d+\.\d+ blur_pf=\d+\.\d+ glass_pf=\d+\.\d+", line
        ):
            print(line, flush=True)
PY
filter_pid=$!
# Track both children, including interrupts before the first route key.
trap 'cleanup; kill "$filter_pid" 2>/dev/null || true; wait "$filter_pid" 2>/dev/null || true' EXIT
remote shell log -t NVPerfCapture "$marker" >/dev/null
remote shell am start -n "$package/.NuvioActivity" >/dev/null
sleep 12
key() { remote shell input keyevent "$1" >/dev/null; sleep 0.25; }
scroll() {
  local i
  for ((i=0; i<6; i++)); do
    key KEYCODE_DPAD_RIGHT
    key KEYCODE_DPAD_LEFT
  done
}
# Home top, then 24 fixed row steps. Fixture: Stage/Round in these rows,
# at least two cards per row; third row must support the style menu.
key KEYCODE_F1
sleep 4
for ((row=0; row<24; row++)); do
  key KEYCODE_DPAD_DOWN
  scroll
  sleep 3
  if [[ $row == 2 ]]; then
    key KEYCODE_F4                    # real row style/context menu; no selection
    sleep 4
    key KEYCODE_BACK
  fi
done
key KEYCODE_F1
key KEYCODE_F2                        # first collection, forced grid
sleep 6
for ((layout=0; layout<8; layout++)); do
  scroll
  sleep 4
  if [[ $layout -lt 7 ]]; then key KEYCODE_F2; sleep 4; fi
done
# Return to grid before opening a title (Showcase may focus its hero).
key KEYCODE_F2
key KEYCODE_DPAD_CENTER
sleep 6
key KEYCODE_F3                        # player waiting overlay, NULL source
sleep 4
key KEYCODE_DPAD_DOWN
sleep 4
key KEYCODE_F12                       # real stats panel over waiting player
sleep 5
key KEYCODE_BACK                      # stats panel
key KEYCODE_BACK                      # player; wait for exit fade
sleep 3
key KEYCODE_F1
key KEYCODE_F11                       # settings
sleep 5
key KEYCODE_DPAD_DOWN
sleep 4
key KEYCODE_F1                        # flush settings by screen transition
sleep 4
remote shell setprop debug.nuvio.perf "${original:-0}" >/dev/null
flag_set=0
sleep 2
kill "$capture_pid" 2>/dev/null || true
wait "$capture_pid" 2>/dev/null || true
capture_pid=
wait "$filter_pid"
python3 "$root/tools/perf-compara.py" --capture "$out/perf.log" "$out"
