#include "tvhome.h"
#include "progresso.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *disk;
static int profile = 1;
char *dados_ler(const char *name) { (void)name; return disk ? strdup(disk) : NULL; }
int dados_gravar(const char *name, const char *text) { (void)name; free(disk); disk = strdup(text); return 1; }
int dados_apagar(const char *name) { (void)name; return 1; }
int perfis_ativo(void) { return profile; }

static CatItem cards[5];
static void card(int i, const char *id, int season, int episode, int progress) {
  snprintf(cards[i].imdb, sizeof cards[i].imdb, "%s", id);
  snprintf(cards[i].tipo, sizeof cards[i].tipo, "%s", episode ? "series" : "movie");
  snprintf(cards[i].titulo, sizeof cards[i].titulo, "Title %s", id);
  strcpy(cards[i].poster, "https://images.metahub.space/poster/medium/test/img");
  cards[i].temporada = season; cards[i].episodio = episode; cards[i].progresso = progress;
}
int main(void) {
  ProgRegistro records[PROG_MAX];
  TvHomeItem out[8];
  card(0, "tt1111111", 0, 0, 25);
  card(1, "tt2222222:1:3", 1, 3, 0); /* Confirmed next episode from Home. */
  card(2, "tt3333333", 0, 0, 95);
  card(3, "tt4444444", 0, 0, 20);
  card(4, "kitsu:41370:1:2", 1, 2, 20);
  disk = strdup("#nvprog2\n"
    "1\ttt1111111\ttt1111111\tmovie\t0\t0\t250\t1000\t2000\t0\t1\n"
    "1\ttt2222222_s1e2\ttt2222222\tseries\t1\t2\t950\t1000\t3000\t0\t1\n"
    "1\ttt2222222_s1e1\ttt2222222\tseries\t1\t1\t200\t1000\t1000\t0\t1\n"
    "1\ttt3333333\ttt3333333\tmovie\t0\t0\t950\t1000\t4000\t0\t1\n"
    "2\ttt4444444\ttt4444444\tmovie\t0\t0\t200\t1000\t5000\t0\t1\n"
    "1\tkitsu:41370_s1e2\tkitsu:41370\tseries\t1\t2\t200\t1000\t1500\t0\t1\n");
  int n = prog_ler(records, PROG_MAX);
  int count = tvhome_select(records, n, cards, 5, 1, 1, 90, out, 8);
  assert(count == 3);
  assert(!strcmp(out[0].id, "tt2222222") && out[0].next && out[0].season == 1 &&
         out[0].episode == 3 && out[0].positionMs == 0);
  assert(!strcmp(out[1].id, "tt1111111") && out[1].positionMs == 250000 && out[1].durationMs == 1000000);
  assert(!strcmp(out[2].id, "kitsu:41370") && !out[2].next && out[2].season == 1 &&
         out[2].episode == 2 && out[2].positionMs == 200000);
  assert(tvhome_select(records, n, cards, 5, 2, 1, 90, out, 8) == 0);
  prog_marcar_removido("tt1111111");
  prog_ocultar_continuar("kitsu:41370");
  assert(tvhome_select(records, n, cards, 5, 1, 1, 90, out, 8) == 1);
  assert(out[0].next);
  assert(tvhome_select(records, n, cards, 5, 1, 1, 90, out, 0) == 0);
  profile = 2;
  n = prog_ler(records, PROG_MAX);
  assert(tvhome_select(records, n, cards, 5, 2, 1, 90, out, 8) == 1);
  assert(!strcmp(out[0].id, "tt4444444"));
  free(disk);
  puts("Android TV home selection: ok (production IDs, profile, order, resume, next, completion, removal)");
}
