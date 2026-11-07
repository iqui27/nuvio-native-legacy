#include <assert.h>
#include "../src/seekpreview.h"
int main(void) {
  SeekPreview p = {.cursor=417.993, .pts=419.628, .when=1000,
                   .generation=3, .shown=1, .direct=1};
  assert(seekpreview_confirmar(&p,417.993,900,3,1014)==419.628);
  p.cursor=272.171; p.pts=269.895;
  assert(seekpreview_confirmar(&p,272.171,900,3,1014)==269.895);
  p.cursor=376.247; p.pts=389.264; p.stale=1;
  assert(seekpreview_confirmar(&p,376.247,900,3,1014)==376.247);
  p.stale=0;
  assert(seekpreview_confirmar(&p,376.247,900,3,1014)==376.247);
  p.cursor=60; p.pts=62;
  assert(seekpreview_confirmar(&p,60,900,3,1014)==62);
  assert(seekpreview_confirmar(&p,60,900,4,1014)==60);
  assert(seekpreview_confirmar(&p,60,900,3,1251)==60);
  assert(seekpreview_confirmar(&p,61,900,3,1014)==61);
  assert(seekpreview_confirmar(&p,60,61,3,1014)==60);
  p.shown=0; assert(seekpreview_confirmar(&p,60,900,3,1014)==60);
  p.shown=1; p.direct=0; assert(seekpreview_confirmar(&p,60,900,3,1014)==60);
  p.direct=1; p.pts=NAN; assert(seekpreview_confirmar(&p,60,900,3,1014)==60);
  p.pts=0; p.cursor=0; p.when=UINT32_MAX-10;
  assert(seekpreview_confirmar(&p,0.1,900,3,10)==0);
  return 0;
}
