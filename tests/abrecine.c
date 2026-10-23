// ABERTURA CINEMA (#205): a regra da tela de abertura premium, sem GL.
//  - padrao Cinema; o ajuste Detalhada abre direto no cartao;
//  - Para cima/Para baixo na Cinema mostra a Detalhada (so durante a abertura);
//  - a linha de demora so depois de ABRECINE_DEMORA_MS;
//  - o crossfade so comeca com o primeiro quadro real (furo do .tpk/LG);
//  - animacoes reduzidas: recuo e saida sem mola.
#include "abrecine.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
  AbreCine a;
  Uint32 t = 1000;
  float dt = 1.0f / 60.0f;
  int i;

  // Padrao Cinema: abrindo, a tela cinema cobre, sem linha de demora.
  memset(&a, 0, sizeof a);
  abrecine_passo(&a, 1, 0, 0, t, dt, 0);
  assert(abrecine_cinema(&a) && a.veu == 1.0f && a.foco < 0.2f);
  assert(!abrecine_demorando(&a, t + ABRECINE_DEMORA_MS - 1));
  assert(abrecine_demorando(&a, t + ABRECINE_DEMORA_MS));
  // O recuo da arte chega perto do fim em ~1,5 s.
  for (i = 0; i < 90; i++) abrecine_passo(&a, 1, 0, 0, t + 16u * i, dt, 0);
  assert(a.foco > 0.95f);

  // Tecla: Para cima vira detalhada; outra tecla nao.
  assert(!abrecine_tecla(&a, SDLK_RETURN) && abrecine_cinema(&a));
  assert(abrecine_tecla(&a, SDLK_UP) && !abrecine_cinema(&a));
  assert(!abrecine_tecla(&a, SDLK_DOWN));   // ja detalhada: a tecla e do cartao

  // Abertura nova volta ao padrao (Cinema), e Para baixo tambem alterna.
  abrecine_passo(&a, 0, 1, 0, t, dt, 0);
  abrecine_passo(&a, 1, 0, 0, t + 5000, dt, 0);
  assert(abrecine_cinema(&a) && a.desde == t + 5000 && a.foco < 0.2f);
  assert(abrecine_tecla(&a, SDLK_DOWN) && a.detalhada);

  // Ajuste Detalhada: nasce detalhada, nada de cinema.
  memset(&a, 0, sizeof a);
  abrecine_passo(&a, 1, 0, 1, t, dt, 0);
  assert(!abrecine_cinema(&a) && a.detalhada);

  // Crossfade: sem quadro real a tela segura inteira, por quanto tempo for.
  memset(&a, 0, sizeof a);
  abrecine_passo(&a, 1, 0, 0, t, dt, 0);
  for (i = 0; i < 240; i++) abrecine_passo(&a, 0, 0, 0, t + 16u * i, dt, 0);
  assert(a.veu == 1.0f && abrecine_cinema(&a));   // 3,8 s sem quadro: segura
  assert(!abrecine_tecla(&a, SDLK_UP));   // a abertura acabou: a tecla e do player
  // Primeiro quadro: o veu cai aos poucos (crossfade), nao num corte.
  abrecine_passo(&a, 0, 1, 0, t, dt, 0);
  assert(a.veu > 0.5f && a.veu < 1.0f);
  for (i = 0; i < 120; i++) abrecine_passo(&a, 0, 1, 0, t + 16u * i, dt, 0);
  assert(a.veu == 0.0f && !abrecine_cinema(&a));
  // Sem quadro nunca (so audio, host mudo): solta no teto, nao fica na arte.
  memset(&a, 0, sizeof a);
  abrecine_passo(&a, 1, 0, 0, t, dt, 0);
  abrecine_passo(&a, 0, 0, 0, t + 100, dt, 0);
  abrecine_passo(&a, 0, 0, 0, t + 100 + ABRECINE_SEGURA_MS - 1, dt, 0);
  assert(a.veu == 1.0f);
  abrecine_passo(&a, 0, 0, 0, t + 100 + ABRECINE_SEGURA_MS, dt, 0);
  assert(a.veu < 1.0f);

  // Animacoes reduzidas: recua de uma vez e sai num corte, ainda so com imagem.
  memset(&a, 0, sizeof a);
  abrecine_passo(&a, 1, 0, 0, t, dt, 1);
  assert(a.foco == 1.0f);
  abrecine_passo(&a, 0, 0, 0, t, dt, 1);
  assert(a.veu == 1.0f);
  abrecine_passo(&a, 0, 1, 0, t, dt, 1);
  assert(a.veu == 0.0f);

  // Linha de estado: resolvendo ate a URL chegar, depois carregando o video.
  assert(abrecine_etapa(1) == ABRECINE_RESOLVENDO);
  assert(abrecine_etapa(0) == ABRECINE_CARREGANDO);

  // OSD calado: desde a abertura ate a primeira tecla com a fonte aberta, tambem
  // no DV que comeca pausado (a abertura acaba e ninguem aperta nada).
  memset(&a, 0, sizeof a);
  abrecine_passo(&a, 1, 0, 0, t, dt, 0);
  assert(abrecine_osd_calado(&a));
  abrecine_acordou(&a, 1);                       // tecla com a fonte abrindo: nao conta
  assert(abrecine_osd_calado(&a));
  abrecine_passo(&a, 0, 0, 0, t + 100, dt, 0);   // fonte aberta, DV pausado, sem imagem
  assert(abrecine_osd_calado(&a));
  abrecine_passo(&a, 0, 1, 0, t + 200, dt, 0);   // primeiro quadro: suave, ainda sem OSD
  for (i = 0; i < 120; i++) abrecine_passo(&a, 0, 1, 0, t + 16u * i, dt, 0);
  assert(!abrecine_cinema(&a) && abrecine_osd_calado(&a));
  abrecine_acordou(&a, 0);                       // OK/setas: o OSD aparece
  assert(!abrecine_osd_calado(&a));
  abrecine_passo(&a, 0, 1, 0, t, dt, 0);         // e fica (so uma abertura nova cala de novo)
  assert(!abrecine_osd_calado(&a));
  abrecine_passo(&a, 1, 0, 0, t + 9000, dt, 0);
  assert(abrecine_osd_calado(&a));

  puts("PASS: abrecine: padrao Cinema, tecla, demora, crossfade so com quadro real e OSD calado ate a tecla");
  return 0;
}
