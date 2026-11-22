// Glue of "marcar cena" (src/cenas.c) against stubs: mark, persist per profile,
// second press sends, armed open seeks the player.
#include "cenas.h"
#include "catalogo.h"
#include "player.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static CatItem titulo;
static Uint32 relogio = 1000;
static int perfil = 1, ligado = 1, epT, epE, temDur = 1, doInicio, enviarOk = 1;
static double pos = 754.6, dur = 3600.0, retomada = -1.0;
static char disco[4][4096], nomes[4][32], toast[200], enviado[80];

Uint32 SDL_GetTicks(void) { return relogio; }
const char *i18n(const char *s) { return s; }
int perfis_ativo(void) { return perfil; }
int ajustes_marcar_cena(void) { return ligado; }
const CatItem *cat_item(int i) { return i == 7 ? &titulo : NULL; }
int player_indice(void) { return 7; }
int player_aberto(void) { return 1; }
float player_posicao_seg(void) { return (float)pos; }
int player_duracao_midia(double *s) { *s = dur; return temDur; }
void player_episodio_atual(int *t, int *e) { *t = epT; *e = epE; }
void player_do_inicio(void) { doInicio++; retomada = -1.0; }
void player_definir_retomada_fonte(double s) { retomada = s; }
void player_toast(const char *t, unsigned ms) { (void)ms; snprintf(toast, sizeof toast, "%s", t); }
int recenviar_abrir_cena(const CatItem *ci, const char *texto) {
  assert(ci == &titulo);
  snprintf(enviado, sizeof enviado, "%s", texto);
  return enviarOk;
}
static int vaga(const char *nome) {
  int i;
  for (i = 0; i < 4; i++) if (!strcmp(nomes[i], nome)) return i;
  for (i = 0; i < 4; i++) if (!nomes[i][0]) { snprintf(nomes[i], sizeof nomes[i], "%s", nome); return i; }
  abort();
}
int dados_gravar(const char *nome, const char *c) { snprintf(disco[vaga(nome)], sizeof disco[0], "%s", c); return 1; }
char *dados_ler(const char *nome) { char *c = disco[vaga(nome)]; return c[0] ? strdup(c) : NULL; }

int main(void) {
  MarcaCena m;
  snprintf(titulo.imdb, sizeof titulo.imdb, "tt12637874");
  snprintf(titulo.tipo, sizeof titulo.tipo, "series");
  epT = 1; epE = 3;

  // Setting off: the key does nothing.
  ligado = 0; cenas_tecla(); assert(cenas_n() == 0 && !toast[0]); ligado = 1;

  // First press stores the mark and tells the user.
  cenas_tecla();
  assert(cenas_n() == 1 && cenas_item(0, &m) && m.seg == 754 && m.durSeg == 3600);
  assert(m.tipo == 's' && m.temporada == 1 && m.episodio == 3 && !strcmp(m.id, "tt12637874"));
  assert(strstr(toast, "12:34") && !enviado[0]);
  assert(strstr(disco[vaga("cenas_1.txt")], "tt12637874 s 1 3 754 3600 "));

  // Second press within the window sends THAT mark, not a new one.
  relogio += 3000; pos = 757.0; cenas_tecla();
  assert(!strcmp(enviado, "cena 0h12m34s t1 e3 d3600") && cenas_n() == 1);

  // Third press is a new mark again; nearby marks merge.
  enviado[0] = 0; relogio += 1000; cenas_tecla();
  assert(!enviado[0] && cenas_n() == 1 && cenas_item(0, &m) && m.seg == 757);
  relogio += 60000; pos = 1200.0; temDur = 0; cenas_tecla();
  assert(cenas_n() == 2 && cenas_item(1, &m) && m.seg == 1200 && m.durSeg == 0);

  // No friends service: the second press explains instead of failing silently.
  enviarOk = 0; toast[0] = 0; relogio += 1000; cenas_tecla();
  assert(strstr(toast, "amigo")); enviarOk = 1;

  // Marks are per profile and survive a reload.
  perfil = 2; assert(cenas_n() == 0);
  perfil = 1; assert(cenas_n() == 2);

  // Armed open: wrong episode does not consume it; the right one seeks.
  temDur = 1; dur = 3600.0;
  cenas_armar("tt12637874", 1, 3, 754, 3600);
  epE = 4; assert(!cenas_aplicar_ao_abrir() && retomada < 0.0);
  epE = 3; assert(cenas_aplicar_ao_abrir() && retomada == 754.0 && doInicio == 1);
  assert(strstr(toast, "12:34"));
  toast[0] = 0; cenas_atualizar(); assert(!toast[0]);          // same length: no warning
  assert(!cenas_aplicar_ao_abrir());                           // one shot

  // Another cut of the title: seeks, then warns once that the moment is approximate.
  cenas_armar("tt12637874", 0, 0, 754, 3000);
  assert(cenas_aplicar_ao_abrir() && retomada == 754.0);
  toast[0] = 0; cenas_atualizar(); assert(strstr(toast, "dura"));
  toast[0] = 0; cenas_atualizar(); assert(!toast[0]);

  // Another title, an expired arm and a cleared arm do nothing.
  retomada = -1.0;
  cenas_armar("tt0000001", 0, 0, 10, 0); assert(!cenas_aplicar_ao_abrir());
  cenas_armar("tt12637874", 0, 0, 10, 0); relogio += 16u * 60u * 1000u; assert(!cenas_aplicar_ao_abrir());
  cenas_armar("tt12637874", 0, 0, 10, 0); cenas_armar(NULL, 0, 0, 0, 0); assert(!cenas_aplicar_ao_abrir());
  assert(retomada < 0.0);
  puts("cenas: ok");
  return 0;
}
