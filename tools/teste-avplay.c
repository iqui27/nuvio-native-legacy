// GATILHO DE TESTE DO PLAYER TIZEN. Nao entra em nenhuma build de producao:
// src/*.c nao alcanca tools/, e so a linha de teste em tools/teste-avplay.sh
// acrescenta este arquivo.
//
// POR QUE ELE EXISTE. video_tizen.c nunca tinha sido EXECUTADO — nao ha TV
// Samsung nesta bancada e webapis.avplay nao existe no Chrome. Compilar e
// linkar nao prova nada sobre a ORDEM das chamadas nem sobre os argumentos, que
// e justamente onde o AVPlay e exigente: a Samsung documenta
// open -> setDisplayRect -> prepareAsync -> play, e o setDisplayRect quer
// coordenadas no espaco 1920x1080.
//
// O que ESTE teste prova: que o app emite essa sequencia, com esses argumentos,
// contra o duble de tools/fake-avplay.js. O que ele NAO prova, e nao ha como
// provar aqui: que o video decodifica, que o plano de hardware aparece atras da
// pagina, e que o container/codec do arquivo real e aceito.
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <stdio.h>
#include "../src/video.h"

EMSCRIPTEN_KEEPALIVE
void nv_teste_avplay(void) {
  printf("[TESTE] --- inicio ---\n");

  printf("[TESTE] video_iniciar -> %d\n", video_iniciar());

  // A janela ANTES de tocar: e o que o video.h manda e o que faz o
  // setDisplayRect sair com o retangulo certo em vez de tela cheia por padrao.
  video_janela(0, 0, 1920, 1080);

  // Um MKV remux com HEVC, que e a forma tipica do link de debrid — e
  // exatamente o caso que a pesquisa apontou como risco de container.
  printf("[TESTE] video_tocar -> %d\n",
         video_tocar("https://exemplo.invalido/filme.2160p.remux.mkv"));

  printf("[TESTE] --- disparado; o resto sai pelo bombear ---\n");
  fflush(stdout);
}

// Segunda fase, chamada depois que o prepareAsync do duble respondeu: exercita
// o que so faz sentido com o player ja pronto.
EMSCRIPTEN_KEEPALIVE
void nv_teste_avplay_fase2(void) {
  printf("[TESTE] --- fase 2 ---\n");
  printf("[TESTE] pronto=%d tocando=%d dur=%.0f\n",
         video_pronto(), video_tocando(), video_duracao());
  printf("[TESTE] faixas: audio=%d legenda=%d\n",
         video_n_audio(), video_n_legenda());
  video_buscar(120.0);          // amortecido por SEEK_REPOUSO_MS
  video_pausar(1);
  video_janela(480, 270, 960, 540);   // recuo, o mesmo caminho do modo creditos
  fflush(stdout);
}

// Terceira fase: OS MODOS DE ZOOM.
//
// O AVPlay nao tem retangulo de FONTE — so setDisplayRect. video_janela_fonte
// emula o recorte inflando o destino e deslocando a origem para NEGATIVO, e o
// defeito original era que o caminho antigo (video_janela) GRAMPEAVA esse
// negativo em zero e desfazia o recorte, deixando todo modo de aspecto
// desenhando o mesmo retangulo. E o relato "no Tizen nao funciona o zoom".
//
// Aqui entram os recortes de fonte que aplicarAspecto produz para uma tela
// 1920x1080 com quadro 1920x1080, e o que se confere no log do duble e o
// setDisplayRect: ele TEM de sair com x/y negativos e tamanho maior que a tela.
// Os alvos estao no comentario de player.c:
//   1.15 -> -144,-81  2208x1242
//   1.34 -> -326,-184 2573x1447
//   1.55 -> -528,-297 2976x1674
EMSCRIPTEN_KEEPALIVE
void nv_teste_avplay_zoom(void) {
  // O RECORTE SAI DO QUADRO REAL, e nao de numeros cravados: um remux 4K tem
  // quadro 3840x2160 e um 1080p tem 1920x1080, e o recorte e uma FRACAO do
  // quadro. Cravar os numeros para 1920 e medir contra um quadro 3840 mede a
  // aritmetica do teste, nao a do app — foi o que aconteceu na primeira volta.
  static const float ESCALA[] = { 1.00f, 1.15f, 1.34f, 1.55f };
  static const char *NOME[] = { "Original", "Zoom leve", "Zoom cinema", "Zoom ultra" };
  int i, qw = video_largura(), qh = video_altura();
  printf("[TESTE] --- fase 3: modos de zoom ---\n");
  printf("[TESTE] quadro %dx%d; alvos de player.c (tela 1920x1080):\n", qw, qh);
  printf("[TESTE]   1.15 -> -144,-81 2208x1242 | 1.34 -> -326,-184 2573x1447 |"
         " 1.55 -> -528,-297 2976x1674\n");
  for (i = 0; i < 4; i++) {
    int sw = (int)(qw / ESCALA[i] + 0.5f), sh = (int)(qh / ESCALA[i] + 0.5f);
    int sx = (qw - sw) / 2, sy = (qh - sh) / 2;
    printf("[TESTE] %s (%.2fx): fonte %d,%d %dx%d\n", NOME[i], ESCALA[i],
           sx, sy, sw, sh);
    video_janela_fonte(sx, sy, sw, sh, 0, 0, 1920, 1080);
  }
  fflush(stdout);
}

// Gatilho manual para testes que nao executam o laco do app. app_atualizar
// agora bombeia antes dos retornos de login/perfis/transicoes.
EMSCRIPTEN_KEEPALIVE
void nv_teste_bombear(void) { video_bombear(); }

EMSCRIPTEN_KEEPALIVE
void nv_teste_avplay_fim(void) {
  printf("[TESTE] --- fim ---\n");
  video_parar();
  fflush(stdout);
}
#endif
