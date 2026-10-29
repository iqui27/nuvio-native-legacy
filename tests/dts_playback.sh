#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
flags=()
if [[ ${SANITIZE:-0} == 1 ]]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
if [[ $(uname -s) == Darwin ]]; then
 cc "${flags[@]}" -std=c11 -D_GNU_SOURCE -Wall -Wextra -Werror -Isrc \
  -D__real_clock_gettime=clock_gettime -c tests/dts_playback.c -o "$tmp/fixture.o"
 cc "${flags[@]}" -std=c11 -D_GNU_SOURCE -DNV_DTS_WEBOS_RECOVERY -Wall -Wextra -Werror -Isrc \
  -Dclock_gettime=__wrap_clock_gettime "$tmp/fixture.o" src/dts/dts_playback.c src/js.c src/linguas.c -pthread -lm -o "$tmp/test"
else
 cc "${flags[@]}" -std=c11 -D_GNU_SOURCE -DNV_DTS_WEBOS_RECOVERY -Wall -Wextra -Werror -Isrc tests/dts_playback.c src/dts/dts_playback.c src/js.c src/linguas.c -pthread -lm -Wl,--wrap=clock_gettime -o "$tmp/test"
fi
for scenario in resumeseek seekfallback timestamp timestampbound timestampprogress timestampopenfail timestampcancel timestamppause timestampdv timestampseek timestampaudio preparefail pause backpressure delayedid cancel seek seeksegment pausedseek pacing ordinal filtered ambiguous audio audioerror eof eoferror pausedreload eofpause overflow subtitles reordered core nocore volume cpu pausedcore controls stallpaced stallrecover stallbound stallnoclock stalltransient stallpause stallseek stallpausedseek stallrecovernoclock stallrecovercancel stallseeknoclock stallrecoverunloaded sustained; do
 "$tmp/test" "$scenario"
done
DV7_OFF=1 "$tmp/test" buffer
