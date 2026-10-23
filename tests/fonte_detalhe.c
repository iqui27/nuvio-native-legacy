// #355: a modal "Detalhes da fonte". Abre com I / F5 / AMARELA na linha em foco,
// guarda o texto INTEIRO ja limpo (emoji some, \n fica, \r sai), nunca expoe a
// url, e Voltar/OK fecham so a modal (a folha segue aberta).
#include "streams.h"
#include "layout.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void tecla(SDL_Keycode k, int scancode) {
  SDL_Event e; memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = k; e.key.keysym.scancode = (SDL_Scancode)scancode;
  stream_folha_evento(&e);
}
static void quadro(void) { stream_folha_atualizar(1.f / 60, 1000); }

int main(void) {
  static Stream v[2];
  memset(v, 0, sizeof v);
  snprintf(v[0].provedor, sizeof v[0].provedor, "AIOStreams");
  snprintf(v[0].rotulo, sizeof v[0].rotulo, "[RD+] Silo\r\n4K \xF0\x9F\x8E\x9E\xEF\xB8\x8F DV");
  snprintf(v[0].descricao, sizeof v[0].descricao,
           "\xF0\x9F\x92\xBE 12.3 GB\r\n\xF0\x9F\x94\x8A DDP 5.1 Atmos\n\xD8\xB9\xD8\xB1\xD8\xA8\xD9\x8A | PT");
  snprintf(v[0].arquivo, sizeof v[0].arquivo, "Silo.S02E05.2160p.mkv");
  snprintf(v[0].url, sizeof v[0].url, "https://x.invalid/SEGREDO-DEBRID-KEY/1.mkv");
  v[1] = v[0];
  stream_definir_alvo("tt1:2:5");
  stream_definir_lista(v, 2);
  stream_folha_abrir(); quadro();
  assert(stream_folha_aberta() && !stream_detalhe_aberto());

  // Com o foco nos botoes do cabecalho (grupo -1) a tecla nao abre nada.
  tecla(SDLK_i, 0); quadro();
  if (!stream_detalhe_aberto()) {
    for (int g = 0; g < 4 && !stream_detalhe_aberto(); g++) { tecla(SDLK_DOWN, 0); tecla(SDLK_i, 0); quadro(); }
  }
  assert(stream_detalhe_aberto());
  const char *d = stream_detalhe_texto();
  assert(strstr(d, "12.3 GB") && strstr(d, "Atmos") && strstr(d, "PT"));
  assert(!strchr(d, '\r'));
  assert(strchr(d, '\n'));                       // as quebras ficam
  assert(!strstr(d, "\xF0\x9F\x92\xBE"));        // emoji vira nada, nao caixa
  assert(!strstr(d, "SEGREDO"));                 // a url nunca entra

  // Rolar nao fecha nem fecha a folha; teclas estranhas sao engolidas.
  tecla(SDLK_DOWN, 0); tecla(SDLK_LEFT, 0); tecla(SDLK_RIGHT, 0); quadro();
  assert(stream_detalhe_aberto() && stream_folha_aberta());
  // Voltar fecha SO a modal.
  tecla(SDLK_AC_BACK, 0); quadro();
  assert(!stream_detalhe_aberto() && stream_folha_aberta());
  // AMARELA (scancode 488) e F5 abrem; OK fecha.
  tecla(SDLK_UNKNOWN, NV_SCANCODE_YELLOW); assert(stream_detalhe_aberto());
  tecla(SDLK_RETURN, 0); assert(!stream_detalhe_aberto() && stream_folha_aberta());
  tecla(SDLK_F5, 0); assert(stream_detalhe_aberto());
  tecla(SDLK_ESCAPE, 0); assert(!stream_detalhe_aberto() && stream_folha_aberta());
  // CH− da LG abre/fecha a mesma modal que AMARELA, sem fechar a folha.
  tecla(SDLK_PAGEDOWN, NV_SCANCODE_CH_DOWN); assert(stream_detalhe_aberto());
  tecla(SDLK_PAGEDOWN, NV_SCANCODE_CH_DOWN);
  assert(!stream_detalhe_aberto() && stream_folha_aberta());
  // Voltar de novo, agora sem modal, fecha a folha.
  tecla(SDLK_AC_BACK, 0); assert(!stream_folha_aberta());
  printf("fonte_detalhe: ok\n");
  return 0;
}
