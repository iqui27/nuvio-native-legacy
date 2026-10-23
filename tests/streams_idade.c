// Idade real da lista reaproveitada: nao reinicia validade de URL assinada.
#include "streams.h"
#include "selospacote.h"
#include <assert.h>
int fontefalha_addon_ao_vivo(const char *key) { (void)key; return 0; }
#include <stdio.h>
#include <string.h>
#include <stdint.h>

static Uint32 relogio;
Uint32 SDL_GetTicks(void) { return relogio; }
int debrid_ativo(void) { return 0; }
int p2p_ativo(void) { return 0; }
// This fixture tests signed-link age, with the optional badge package off.
// The production list now invokes these boundaries before publishing it.
int selospacote_ativo(void) { return -1; }
unsigned selospacote_versao(void) { return 1; }
int selospacote_casar(const char *const *p, int n, unsigned short *o, int m) {
  (void)p; (void)n; (void)o; (void)m; return 0;
}
// Idem selosAgendar/selosCalcular desde o #402: o pacote desligado os faz
// voltar cedo, mas os simbolos continuam no link do teste.
void selospacote_colorido(int c) { (void)c; }
int selospacote_embutidos_ok(void) { return 0; }
const SeloFiltro *selospacote_filtro(unsigned short id) { (void)id; return NULL; }
int ajustes_selos_coloridos(void) { return 0; }
int main(void) {
  Stream s = {0};
  strcpy(s.url, "https://video.invalid/assinado.mp4");
  assert(stream_idade_ms() == UINT32_MAX);
  stream_definir_alvo("tt1:2:3");
  relogio = 10000;
  stream_definir_lista_idade(&s, 1, 7000);
  assert(stream_n() == 1 && stream_lista_do_alvo("tt1:2:3"));
  assert(stream_idade_ms() == 7000);
  relogio += 5000;
  assert(stream_idade_ms() == 12000);
  stream_definir_lista(&s, 1);
  assert(stream_idade_ms() == 0);
  relogio = 10000;
  stream_definir_lista_idade(&s, 1, 10000);
  assert(stream_idade_ms() == 10000);  // timestamp original zero e valido
  relogio = UINT32_MAX - 5;
  stream_definir_lista_idade(&s, 1, 12);
  relogio = 4;
  assert(stream_idade_ms() == 22);
  relogio = 0;
  stream_definir_lista(&s, 1);
  assert(stream_idade_ms() == 0);
  stream_definir_lista(NULL, 0);
  puts("streams idade: PASS (idade original, lista nova, zero, wrap Uint32)");
  return 0;
}
