// Episode change frees the source list on the UI thread while the sheet still
// asks for badges. selosPacoteDa must not read the freed block.
#include "streams.h"
#include "selospacote.h"
#include <assert.h>
int fontefalha_addon_ao_vivo(const char *key) { (void)key; return 0; }
#include <stdio.h>
#include <string.h>

Uint32 SDL_GetTicks(void) { return 1; }
int debrid_ativo(void) { return 0; }
int p2p_ativo(void) { return 0; }
int selospacote_ativo(void) { return 0; }
unsigned selospacote_versao(void) { return 1; }
int selospacote_casar(const char *const *p, int n, unsigned short *o, int m) {
  (void)p; (void)n; (void)o; (void)m; return 0;
}
void selospacote_colorido(int c) { (void)c; }
int selospacote_embutidos_ok(void) { return 1; }
const SeloFiltro *selospacote_filtro(unsigned short id) { (void)id; return NULL; }
int ajustes_selos_coloridos(void) { return 0; }
unsigned player_recarregar_contexto(void) { return 0; }

int main(void) {
  Stream s[2] = {0};
  Stream copia;
  strcpy(s[0].url, "https://video.invalid/a.mp4");
  strcpy(s[0].rotulo, "A");
  strcpy(s[1].url, "https://video.invalid/b.mp4");
  strcpy(s[1].rotulo, "B");
  stream_definir_alvo("tt1:1:1");
  stream_definir_lista(s, 2);
  copia = *stream_item(0);
  stream_invalidar("episode changed");
  assert(stream_n() == 0);
  assert(stream_selos_ha(&copia) == 0);
  puts("selos_lista_viva: ok");
  return 0;
}
