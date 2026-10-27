// Exercise shared channel and source playback entries without network access.
#include "catalogo.h"
#include "streams.h"
#include "perfilinfantil.h"
#include <assert.h>
static int kids, warnings, checks, source_checks, opened, detail_index, file_checks;
static CatItem movie;
static const char *rating;
// Stop file requests at source lookup, before any playback state changes.
static const Stream *file_source(int index) { (void)index; file_checks++; return NULL; }
static int source_player_open(void) { return opened; }
static int source_player_index(void) { return 7; }
static int source_detail_index(void) { return detail_index; }
static const CatItem *source_item(int index) { (void)index; return &movie; }
static const char *source_rating(void) { return rating; }
static int source_episode(int *t, int *e) { *t = *e = 0; return 0; }
// Stop allowed requests at the next gate, before network or player setup.
static int source_gate(int index, int t, int e) {
  (void)t; (void)e;
  assert(index == (opened ? 7 : detail_index));
  source_checks++;
  return 1;
}
static int child_profile(void) { return kids; }
// Default parental rules: classification up to 10, no time limit used up.
static int rule_rating(const char *c) { return !kids || perfil_infantil_classif_ok(c); }
static int rule_time(float seconds) { (void)seconds; return 0; }
#define perfil_regra_classif rule_rating
#define perfil_regra_tempo rule_time
static void blocked_notice(const char *text) { (void)text; warnings++; }
static int reject_channel(const CatItem *item) { (void)item; checks++; return 0; }
#define perfil_infantil_ativo child_profile
#define glem_aviso_curto blocked_notice
#define g5ui_check_channel reject_channel
#define player_aberto source_player_open
#define player_indice source_player_index
#define detail_indice source_detail_index
#define detail_ep_foco source_episode
#define cat_item source_item
#define extras_ficha_classificacao source_rating
#define travaui_abrir_espera source_gate
#define stream_item file_source
#include "../src/app.c"
#undef perfil_infantil_ativo
#undef perfil_regra_classif
#undef perfil_regra_tempo
#undef glem_aviso_curto
#undef g5ui_check_channel
static int source_restriction_tests(void) {
  int failures = 0;
  const char *blocked[] = { "16", "18", "", NULL };
  for (opened = 0; opened <= 1; opened++) {
    detail_index = opened ? 9 : 7;
    for (int manual = 0; manual <= 1; manual++) {
      for (unsigned i = 0; i < sizeof blocked / sizeof *blocked; i++) {
        kids = 1; warnings = source_checks = 0;
        rating = blocked[i];
        snprintf(movie.classificacao, sizeof movie.classificacao, "%s", blocked[i] ? blocked[i] : "");
        // An unrelated Detail page must not lend its safe rating to playback.
        if (opened) rating = "L";
        aguardandoFonte = folhaParaTocar = 1;
        tocarFonteEscolhida(0, manual);
        if (warnings != 1 || source_checks || aguardandoFonte || folhaParaTocar) {
          printf("FAIL: restricted source reached playback gate (rating=%s, open=%d, manual=%d)\n",
                 blocked[i] ? blocked[i] : "unknown", opened, manual);
          failures++;
        }
      }
      for (unsigned i = 0; i < sizeof blocked / sizeof *blocked; i++) {
        kids = 1; warnings = file_checks = 0;
        rating = opened ? "L" : blocked[i];
        snprintf(movie.classificacao, sizeof movie.classificacao, "%s", blocked[i] ? blocked[i] : "");
        aguardandoFonte = folhaParaTocar = 1;
        abrirParte(0, NULL, 0);
        if (warnings != 1 || file_checks || aguardandoFonte || folhaParaTocar) {
          puts("FAIL: restricted source file bypassed rating policy");
          failures++;
        }
      }
      const char *allowed[] = { "L", "10" };
      for (unsigned i = 0; i < sizeof allowed / sizeof *allowed; i++) {
        for (int fallback = 0; fallback <= 1; fallback++) {
          warnings = source_checks = 0;
          rating = fallback ? "" : allowed[i];
          snprintf(movie.classificacao, sizeof movie.classificacao, "%s", allowed[i]);
          tocarFonteEscolhida(0, manual);
          assert(warnings == 0 && source_checks == 1);
          file_checks = 0;
          abrirParte(0, NULL, 0);
          assert(warnings == 0 && file_checks == 1);
        }
      }
      kids = 0; warnings = source_checks = 0; rating = "18";
      snprintf(movie.classificacao, sizeof movie.classificacao, "18");
      tocarFonteEscolhida(0, manual);
      assert(warnings == 0 && source_checks == 1);
      file_checks = 0;
      abrirParte(0, NULL, 0);
      assert(warnings == 0 && file_checks == 1);
    }
  }
  if (!failures) puts("PASS: shared source entry enforces kids ratings; safe and adult requests continue");
  return failures;
}
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
  return source_restriction_tests() ? 1 : 0;
}
