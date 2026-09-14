// PROVA DA ISSUE #42 ("Settings are not getting saved (in player caption
// settings mainly)"): o estilo de legenda (art/player.txt) era gravado na
// pasta de ARTE (SDL_GetBasePath()+"art"), nao na de DADOS — main.c so
// resolvia isso para ajustes.c (ajustes_dir). Numa TV real, sem saida limpa
// (o processo e MORTO, nao fechado), o que sobrevive e o que foi escrito no
// diretorio de dados de verdade; a pasta de arte e tratada como cache.
//
// Este teste roda em DOIS PROCESSOS separados de proposito — "mata o
// processo e reabre" so prova alguma coisa se for um processo NOVO, com os
// `static` de player.c todos voltando ao valor inicial. Ver tests/player_prefs.sh.
//
//   argv[1] == "escrever": abre uma sessao, muda leg_cor/leg_borda, fecha.
//   argv[1] == "ler":      abre uma sessao NOVA e confere que o ajuste veio
//                          do arquivo — e que o arquivo esta na pasta de
//                          DADOS (NUVIO_DADOS), nao na de arte.
#include "player.h"
#include "video.h"
#include "catalogo.h"
#include "dados.h"
#include <SDL2/SDL.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
  const char *dados = getenv("NUVIO_DADOS");
  const char *arte  = getenv("NUVIO_ARTE_TESTE");
  char caminhoDados[600], caminhoArte[600];
  assert(dados && *dados && "defina NUVIO_DADOS (regra do repo: nunca ~/.nuvio)");
  assert(arte && *arte && "defina NUVIO_ARTE_TESTE");
  assert(argc >= 2);
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);

  // MESMA SEQUENCIA DE main.c: dados_iniciar resolve o diretorio real (honra
  // NUVIO_DADOS), player_dir recebe dados_dir() com dirArte de reserva —
  // exatamente as duas linhas acrescentadas em main.c por este conserto.
  dados_iniciar(arte);
  player_dir(dados_dir()[0] ? dados_dir() : arte);

  snprintf(caminhoDados, sizeof caminhoDados, "%s/player.txt", dados);
  snprintf(caminhoArte,  sizeof caminhoArte,  "%s/player.txt", arte);

  if (!strcmp(argv[1], "escrever")) {
    player_abrir(0, NULL);   // le o arquivo (ainda nao existe: fica no padrao)
    VideoLegendaEstilo *e = player_leg_estilo();
    e->cor = 3;          // azul (VIDEO_LEG_CORES: white,yellow,green,blue,...)
    e->borda = 2;         // sombra
    e->tamanho = 150;
    player_leg_estilo_mudou();   // GRAVA — e o que estava indo para a pasta errada
    printf("[teste] gravado: cor=%d borda=%d tamanho=%d\n", e->cor, e->borda, e->tamanho);
    return 0;
  }

  if (!strcmp(argv[1], "ler")) {
    // PROCESSO NOVO: legEstilo volta ao valor de fabrica (120,0,0,3,1,0,0,..)
    // ate prefsLer() rodar dentro de player_abrir.
    player_abrir(0, NULL);
    VideoLegendaEstilo *e = player_leg_estilo();
    FILE *fd;
    printf("[teste] lido: cor=%d borda=%d tamanho=%d\n", e->cor, e->borda, e->tamanho);
    if (e->cor != 3 || e->borda != 2 || e->tamanho != 150) {
      fprintf(stderr, "FALHOU: ajuste nao sobreviveu ao processo novo "
                       "(cor=%d borda=%d tamanho=%d, esperava 3/2/150)\n",
              e->cor, e->borda, e->tamanho);
      return 1;
    }
    fd = fopen(caminhoDados, "r");
    if (!fd) {
      fprintf(stderr, "FALHOU: player.txt nao esta na pasta de DADOS (%s)\n", caminhoDados);
      return 1;
    }
    fclose(fd);
    fd = fopen(caminhoArte, "r");
    if (fd) {
      fclose(fd);
      fprintf(stderr, "FALHOU: player.txt tambem foi escrito na pasta de ARTE (%s) — "
                       "a pasta errada continua recebendo o arquivo\n", caminhoArte);
      return 1;
    }
    printf("PASS: ajuste sobreviveu a um processo novo, gravado na pasta de dados.\n");
    return 0;
  }

  fprintf(stderr, "uso: %s escrever|ler\n", argv[0]);
  return 2;
}
