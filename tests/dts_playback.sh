#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
flags=()
if [[ ${NV_TEST_P7_MAPPING:-0} == 1 ]];then flags+=(-DNV_DV7_MAPPING_TEST);fi
if [[ ${NV_TEST_MP4:-0} == 1 || ${NV_TEST_MP4_BUILD:-0} == 1 ]]; then flags+=(-DNV_MP4_BD_TEST); fi
if [[ ${NV_TEST_TRUEHD:-0} == 1 ]]; then flags+=(-DNV_DV_TRUEHD_TEST); fi
if [[ ${SANITIZE:-0} == 1 ]]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
if [[ $(uname -s) == Darwin ]]; then
 cc "${flags[@]}" -std=c11 -D_GNU_SOURCE -Wall -Wextra -Werror -Isrc \
  -D__real_clock_gettime=clock_gettime -c tests/dts_playback.c -o "$tmp/fixture.o"
 cc "${flags[@]}" -std=c11 -D_GNU_SOURCE -DNV_DTS_WEBOS_RECOVERY -Wall -Wextra -Werror -Isrc \
  -Dclock_gettime=__wrap_clock_gettime "$tmp/fixture.o" src/dts/dts_playback.c src/js.c src/linguas.c src/audioinfo.c -pthread -lm -o "$tmp/test"
else
 cc "${flags[@]}" -std=c11 -D_GNU_SOURCE -DNV_DTS_WEBOS_RECOVERY -Wall -Wextra -Werror -Isrc tests/dts_playback.c src/dts/dts_playback.c src/js.c src/linguas.c src/audioinfo.c -pthread -lm -Wl,--wrap=clock_gettime -o "$tmp/test"
fi
if [[ ${NV_TEST_P7_MAPPING:-0} == 1 ]];then "$tmp/test" p7-mapping;exit;fi
if [[ ${NV_TEST_MP4:-0} == 1 ]];then
 for scenario in mp4-seek-series mp4-seek-off mp4-seek-timeout mp4-seek-paused mp4-seek-cancel mp4-seek-audio;do "$tmp/test" "$scenario";done
 exit
fi
for scenario in audio-hold-ready audio-hold-gap audio-hold-paused audio-hold-deadline audio-hold-cancel audio-hold-readwait audio-hold-total fixed-aac fixed-ac3 fixed-aac-paused fixed-ac3-paused fixed-refused fixed-invalid fixed-nondv fixed-rejected handoff-format handoff-paused handoff-cancel handoff-disabled handoff-refused handoff-prime-error; do
 "$tmp/test" "$scenario"
done
if [[ ${NV_TEST_TRUEHD:-0} == 1 ]]; then "$tmp/test" dv-truehd;exit;fi
for scenario in startup-same startup-audio startup-fallback startup-no-dv resumeseek seekfallback timestamp timestampbound timestampprogress timestampopenfail timestampcancel timestamppause timestampdv timestampseek timestampaudio preparefail pause backpressure delayedid cancel seek seeksegment pausedseek pacing ordinal filtered ambiguous dv-codec dv-file-ordinal audio audioerror eof eoferror pausedreload eofpause overflow subtitles reordered core nocore volume cpu pausedcore controls stallpaced stallrecover stallbound stallnoclock stalltransient stallpause stallseek stallpausedseek stallrecovernoclock stallrecovercancel stallseeknoclock stallrecoverunloaded sustained; do
 "$tmp/test" "$scenario"
done
DV7_OFF=1 "$tmp/test" buffer
for scenario in resume-init-pct resume-init-seconds resume-init-paused resume-init-audio resume-init-no-duration resume-init-rejected; do
 "$tmp/test" "$scenario"
done

for scenario in buffered-seek buffered-audio buffered-paused buffered-dv buffered-refused buffered-format buffered-stall buffered-stale buffered-dv-stall buffered-dv-paused buffered-dv-prime buffered-dv-cancel; do
 "$tmp/test" "$scenario"
done
