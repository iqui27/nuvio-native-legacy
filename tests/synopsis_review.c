// PR #526: integration preference compatibility and playback-sheet reader.
#include "../src/ajustes.c"
#include "../src/episodios.c"
#include <assert.h>
#include <unistd.h>

static void load(const char *dir, const char *settings) {
  char path[1200];
  snprintf(path, sizeof path, "%s/ajustes.txt", dir);
  FILE *f = fopen(path, "w"); assert(f);
  fputs(settings, f); fclose(f);
  ajustes_dir(dir);
}

static void setting(const char *dir) {
  load(dir, "heroSynopsisSizeLocal 1\n");
  assert(ajustes_sinopse_tamanho() == 1);
  assert(sinopse_estilo(TXT_HERO_SIN) == TXT_SINOPSE_24);
  assert(sinopse_entrelinha(TXT_HERO_SIN, 30) == 32);
  assert(sinopse_linhas(TXT_HERO_SIN, 30, 2) == 2);
  assert(gravar()); ajustes_dir(dir);
  assert(ajustes_sinopse_tamanho() == 1);
  load(dir, "sinopsePercentualLocal 2\nheroSynopsisSizeLocal 0\n");
  assert(ajustes_sinopse_tamanho() == 0);
  load(dir, "heroSynopsisSizeLocal 1\nsinopsePercentualLocal 0\n");
  assert(ajustes_sinopse_tamanho() == 1);
  load(dir, "sinopsePercentualLocal 2\n");
  assert(ajustes_sinopse_tamanho() == 1);
  int count = 0;
  for (int i = 0; i < AJ_N; i++)
    if (!strcmp(OPCOES[i].rotulo, "Tamanho da sinopse")) count++;
  assert(count == 1);
}

static void sheet(void) {
  CatItem item = {0}; CatEp ep = {0};
  strcpy(item.imdb, "tt526"); strcpy(item.tipo, "series");
  strcpy(item.titulo, "Reader regression");
  item.nTemporadas = 1; item.temporadas[0] = 1;
  ep.temporada = ep.episodio = 1;
  strcpy(ep.nome, "Episode one"); strcpy(ep.sinopse, "Full available episode synopsis.");
  cat_definir(&item, 1); cat_definir_episodios(0, &ep, 1);
  // Set the sheet state without starting metadata discovery/network work.
  titulo = 0; atualT = atualE = 1; aberto = 1;
  guardarId(idTitulo, sizeof idTitulo, 0);
  menuAbrir(0, 1, 1, ep.nome, 0);
  int action = -1;
  for (int i = 0; i < vmN; i++) if (vmAcoes[i] == VM_SINOPSE) action = i;
  assert(action >= 0);
  vmFoco = action;
  SDL_Event event = {0}; event.type = SDL_KEYDOWN; event.key.keysym.sym = SDLK_RETURN;
  episodios_evento(&event);
  assert(sinopse_aberta() && episodios_aberto() && !vmAberto);
  event.key.keysym.sym = SDLK_DOWN; episodios_evento(&event);
  assert(sinopse_aberta() && episodios_aberto());
  event.key.keysym.sym = SDLK_ESCAPE; episodios_evento(&event);
  assert(!sinopse_aberta() && episodios_aberto());
  sinopse_abrir(ep.nome, ep.sinopse);
  episodios_fechar(); assert(!sinopse_aberta());
}

int main(int argc, char **argv) {
  char dir[] = "/tmp/nuvio-synopsis-review-XXXXXX";
  assert(mkdtemp(dir)); setenv("NUVIO_DADOS", dir, 1); dados_iniciar(dir);
  if (argc > 1 && !strcmp(argv[1], "setting")) setting(dir);
  else sheet();
  char path[1200]; snprintf(path, sizeof path, "%s/ajustes.txt", dir); unlink(path);
  puts("synopsis_review: PASS");
  return 0;
}
