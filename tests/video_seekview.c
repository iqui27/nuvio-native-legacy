#include "video_seekview.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
  NvSeekView view={0};
  nv_seekview_begin(&view,500,100);
  nv_seekview_clock(&view,100.2);
  assert(nv_seekview_pos(&view,100.2,200)==500);
  nv_seekview_sent(&view,450);
  nv_seekview_clock(&view,498.5); // Decoder preroll must not pull the bar left.
  assert(nv_seekview_pos(&view,498.5,900)==500);
  nv_seekview_clock(&view,500.1);
  assert(nv_seekview_pos(&view,500.1,1100)==500.1);
  nv_seekview_begin(&view,200,1200);nv_seekview_sent(&view,1550);
  nv_seekview_clock(&view,501);assert(nv_seekview_pos(&view,501,1600)==200);
  nv_seekview_begin(&view,300,1700);nv_seekview_sent(&view,2050);
  nv_seekview_clock(&view,200);assert(nv_seekview_pos(&view,200,2100)==300);
  nv_seekview_clock(&view,300);assert(!view.active);
  nv_seekview_begin(&view,600,UINT32_MAX-100);nv_seekview_sent(&view,UINT32_MAX-50);
  assert(nv_seekview_pos(&view,301,25)==600);
  assert(nv_seekview_pos(&view,301,15000)==301); // Failure cannot pin a fake time forever.
  puts("Seek UI: stale clocks, keyframe preroll, forward/backward replacement and wrap-safe timeout passed");
}
