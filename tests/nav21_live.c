// Exercise the shared channel entry used by Spotlight, guide, recall and zap.
#include "catalogo.h"
static int kids, warnings, checks;
static int child_profile(void) { return kids; }
static void blocked_notice(const char *text) { (void)text; warnings++; }
static int reject_channel(const CatItem *item) { (void)item; checks++; return 0; }
#define perfil_infantil_ativo child_profile
#define glem_aviso_curto blocked_notice
#define g5ui_check_channel reject_channel
#include "../src/app.c"
#undef perfil_infantil_ativo
#undef glem_aviso_curto
#undef g5ui_check_channel
#include <assert.h>
int main(void) {
  CatItem item = {0};
  snprintf(item.imdb, sizeof item.imdb, "g5live:test");
  snprintf(item.tipo, sizeof item.tipo, "channel");
  kids = 1;
  tocarCanal(&item);
  if (warnings != 1 || checks != 0) {
    puts("FAIL: kids channel entry must stop before channel checks or playback");
    return 1;
  }
  kids = 0;
  tocarCanal(&item);
  assert(warnings == 1 && checks == 1);
  puts("PASS: kids blocked at shared playback entry; adult reaches channel checks");
  return 0;
}
