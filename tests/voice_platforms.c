// Exercise shared voice state with Android JNI doubles and the webOS SDL path.
#include "sistexto.h"
#include "entrada_texto.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

const char *desc_tmdb_idioma(void) { return "en-US"; }
int ajustes_teclado_android(void) { return 0; }
#ifdef NV_ANDROID
static int accepted, keyboards;
int android_st_ditar(const char *language) { assert(!strcmp(language, "en-US")); return accepted; }
int android_st_teclado(const char *text, int max) { keyboards++; return 1; }
void android_st_fechar(void) {}
int android_st_evento(char *dst, size_t n) { return 0; }
#endif

int main(void) {
  char text[256];
  assert(SDL_Init(SDL_INIT_EVENTS) == 0);
  st_teste_ligar(0);
#ifdef NV_ANDROID
  assert(!st_voz_iniciar(ST_BUSCA) && st_aviso()[0]);
  accepted = 1;
  assert(st_voz_iniciar(ST_BUSCA) && st_estado() == ST_OUVINDO);
  st_teste_evento("VTitle X");
  assert(st_ler(ST_BUSCA, text, sizeof text) == ST_FIM && !strcmp(text, "Title X"));
  const char *errors[] = {"Enada", "Enegada", "Eocupado", "Eerro:7"};
  for (int i = 0; i < 4; i++) {
    assert(st_voz_iniciar(ST_BUSCA)); st_teste_evento(errors[i]);
    assert(st_ler(ST_BUSCA, text, sizeof text) == ST_CANCELOU);
    assert(st_estado() == ST_PARADO && st_aviso()[0]);
  }
  assert(st_voz_iniciar(ST_BUSCA)); st_teste_evento("Steclado:negada");
  assert(st_ler(ST_BUSCA, text, sizeof text) == ST_PEDE_TECLADO && st_aviso()[0]);
  assert(st_ime_abrir(ST_BUSCA, "", 255) && keyboards == 1 && st_aviso()[0]);
  st_fechar(ST_BUSCA);
  puts("PASS: Android recognition, rejection, errors and keyboard fallback");
#else
  // webOS advertises only its system keyboard, whose own mic handles dictation.
  assert(texto_sistema_disponivel() == TS_TECLADO && !st_voz_disponivel());
  assert(!st_voz_iniciar(ST_BUSCA) && st_aviso()[0]);
  assert(st_ime_abrir(ST_BUSCA, "", 255));
  texto_sistema_ponte_valor("Title X"); texto_sistema_ponte_fim(1);
  texto_sistema_quadro();
  SDL_Event event;
  while (SDL_PollEvent(&event)) st_evento(&event);
  assert(st_ler(ST_BUSCA, text, sizeof text) == ST_FIM && !strcmp(text, "Title X"));
  puts("PASS: webOS keyboard dictation; unavailable standalone voice has feedback");
#endif
  SDL_Quit();
  return 0;
}
