#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
task_dir=$(mktemp -d); trap 'rm -rf "$task_dir"' EXIT
python3 - "$task_dir/pacing.c" <<'PY'
from pathlib import Path
import sys
s=Path('src/main.c').read_text()
start=s.index('    int playerUi = player_ui_frame(player_aberto(), gfx_n_rect, gfx_n_outros);')
end=s.index('    if (!nvPrimeiroQuadroFeito)',start)
body=s[start:end]
platform=s[s.index('#if defined(__APPLE__) || defined(NV_LINUX_DESKTOP)'):]
platform=platform[:platform.index('#endif')+len('#endif')]
assert 'player_frame_wait(playerClockUi, SDL_GetTicks() - frameStarted)' in s
assert 'nv_ceder_quadro(playerUi)' in s
Path(sys.argv[1]).write_text('''#include <assert.h>
#include <stdio.h>
#include "player_pacing.h"
'''+platform+'''
int opened=1, gfx_n_rect=2, gfx_n_outros=0, sheet, loading, assigned, retained;
int calls, requested[128], failVsync, playerUiPrevious;
#ifndef NV_SEM_WEBOS
int sourceLoadingVsyncFailed;
#endif
int player_aberto(void) { return opened; }
int stream_folha_aberta(void) { return sheet; }
int player_carregando(void) { return loading; }
int player_tem_video(void) { return assigned; }
int player_retido(void) { return retained; }
int SDL_GL_SetSwapInterval(int n) { assert(calls<128); requested[calls++]=n; return n && failVsync ? -1 : 0; }
int policy(void) {
'''+body+'''
return playerClockUi;
}
int main(void) {
  /* Waiting screen -> source selection -> source startup -> video -> seek. */
  loading=1; assert(policy()==1 && player_frame_wait(1,3)==13);
  sheet=1;
#ifndef NV_SEM_WEBOS
  assert(policy()==0 && requested[calls-1]==1);
  assert(player_frame_wait(policy(),3)==0); /* no second 16 ms limiter */
  int stable=calls; for(int i=0;i<100;i++) assert(policy()==0);
  assert(calls==stable);
  assigned=1; assert(policy()==1 && requested[calls-1]==0);
  loading=0; assert(policy()==1); /* playing */
  loading=1; assert(policy()==1); /* seeking/buffering with an assigned file */
  assigned=0; retained=1; assert(policy()==1); /* retained previous video */
  retained=0; sheet=0; assert(policy()==1); /* ordinary loading overlay */
  sheet=1; assert(policy()==0);
  sheet=0; assert(policy()==1);
  /* VSync failure returns to the old limiter, once per eligible session. */
  failVsync=1; sheet=1; stable=calls;
  assert(policy()==1 && sourceLoadingVsyncFailed);
  assert(calls==stable+2 && requested[stable]==1 && requested[stable+1]==0);
  stable=calls; for(int i=0;i<100;i++) assert(policy()==1);
  assert(calls==stable && player_frame_wait(policy(),3)==13);
  sheet=0; assert(policy()==1 && !sourceLoadingVsyncFailed);
  failVsync=0; sheet=1; assert(policy()==0);
  loading=0; assert(policy()==1); /* leaving the loading session restores policy */
  puts("PASS: actual webOS main-loop policy, transitions, retained/assigned video and bounded VSync-failure fallback");
#else
  assert(policy()==1);
  assigned=1; assert(policy()==1);
  loading=0; assert(policy()==1);
  assigned=0; retained=1; loading=1; assert(policy()==1);
  sheet=0; assert(policy()==1);
  puts("PASS: non-webOS targets preserve the original player UI clock");
#endif
  opened=0; assert(policy()==0); /* title details remain VSync driven */
  assert(player_ui_frame(1,1,1)==0); /* bare video policy retained */
  assert(player_frame_wait(0,0)==0 && player_frame_wait(1,16)==0 && player_frame_wait(1,20)==0);
  return 0;
}
''')
PY
flags=(-std=c11 -O1 -g -Wall -Wextra -Werror -Isrc)
if [[ ${SANITIZE:-0} == 1 ]]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
for target in WEBOS NV_LINUX_DESKTOP NV_TPK NV_ANDROID __EMSCRIPTEN__ __APPLE__; do
  cc "${flags[@]}" "-D$target" "$task_dir/pacing.c" -o "$task_dir/pacing"
  "$task_dir/pacing"
done
