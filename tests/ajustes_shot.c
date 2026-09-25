// CAPTURA DA TELA DE AJUSTES, sem interacao e sem rede.
//
// Existe pelo motivo que tests/player_regression.c ja registra: interface de TV
// julgada so por codigo sai ilegivel a 3 m. Aqui as duas telas que mudam neste
// trabalho — a lista de Ajustes e a folha "Ordenar e ativar fileiras" — sao
// desenhadas com dados de mentira e gravadas em BMP, para serem OLHADAS.
//
// NAO CHAMA dados_iniciar DE PROPOSITO. Sem ela `dados_dir()` e "", entao
// fileiras.c nao le nem escreve arquivo nenhum: a lista comeca vazia (o estado
// de quem nunca abriu o app) e a captura nao mexe no fileirasui.txt de quem
// roda o teste.
#include "ajustes.h"
#include "rail_shot.h"
#include "fileiras.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void tecla(SDL_Keycode k) {
  SDL_Event e = { 0 };
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  ajustes_evento(&e);
}

static void captura(const char *nome, SDL_Window *win) {
  int i;
  rail_shot_aplicar();
  for (i = 0; i < 60; i++) {
    SDL_PumpEvents();
    txt_novo_quadro();
    tex_novo_quadro();
    tex_bombear(6);
    ajustes_atualizar(1.0f / 60.0f, SDL_GetTicks());
    glClearColor(0.025f, 0.025f, 0.03f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ajustes_desenhar(SDL_GetTicks());
    rail_shot_desenhar(MENU_AJUSTES);
    if (i == 59) {
      unsigned char *pix = malloc(1920 * 1080 * 4);
      SDL_Surface *s;
      int y;
      assert(pix);
      glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
      s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
      assert(s);
      for (y = 0; y < 1080; y++)
        memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
      assert(SDL_SaveBMP(s, nome) == 0);
      SDL_FreeSurface(s);
      free(pix);
    }
    SDL_GL_SwapWindow(win);
  }
  printf("captura: %s\n", nome);
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : "/tmp/nuvio-ajustes";
  char nome[600];
  SDL_Window *w;
  SDL_GLContext gl;
  int i;

  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("Nuvio: revisao dos Ajustes", SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED, 1920, 1080,
                       SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(w);
  gl = SDL_GL_CreateContext(w);
  assert(gl);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");

  // FILEIRAS DE MENTIRA cobrindo as origens que a folha sabe distinguir: o
  // catalogo de addon (com nome de addon e tipo), o grupo de colecoes, e as
  // fileiras que o proprio app monta.
  fil_registrar("continue_watching", "Continuar assistindo", "", "", 12);
  fil_registrar("social_activity", "Entre amigos", "", "", 6);
  fil_registrar("com.linvo.cinemeta_movie_top", "Popular", "Cinemeta", "movie", 40);
  fil_registrar("xperience_series_foryou", "For You", "Xperience", "series", 24);
  fil_registrar("collection_a24", "A24", "", "", 9);
  fil_registrar("tmdb.addon_movie_trending", "Em alta", "TMDB", "movie", 20);
  fil_registrar("aiostreams_series_novos", "Séries novas", "AIOStreams", "series", 18);
  fil_registrar("akashi_movie_anime", "Anime", "Akashi", "movie", 30);
  fil_registrar("mdblist_movie_oscar", "Vencedores do Oscar", "MDBList", "movie", 15);
  fil_registrar("sem.nome_movie_x", "", "", "", -1);
  // Mais catalogos do que o limite (7): os que passam ficam NA FILA. E dois
  // removidos, para a aba "Fora da Home" ter o que agrupar por addon.
  fil_registrar("xperience_movie_acao", "Ação", "Xperience", "movie", 12);
  fil_registrar("xperience_movie_terror", "Terror", "Xperience", "movie", 12);
  fil_registrar("xperience_series_animes", "Animes", "Xperience", "series", 12);
  fil_registrar("aiostreams_movie_top", "Top 100", "AIOStreams", "movie", 12);
  fil_registrar("akashi_series_dorama", "Doramas", "Akashi", "series", 12);
  fil_registrar("akashi_movie_bollywood", "Bollywood", "Akashi", "movie", 12);
  fil_remover(7);   // Anime
  fil_remover(9);   // sem nome
  fil_remover(12);  // Animes

  ajustes_iniciar();

  // A TELA ABRE COM O FOCO NA COLUNA DE CATEGORIAS, na primeira (Conta), e a
  // lista mostra o que ha nela.
  snprintf(nome, sizeof nome, "%s-lista.bmp", saida);
  captura(nome, w);

  // Todo caminho daqui em diante e o do CONTROLE: setas, OK e Voltar. As
  // categorias sao as de TELA[] em ajustes.c, na ordem do web: 0 Conta,
  // 1 Aparencia, 2 Layout, 3 Conteudo, 4 Integracoes, 5 Reproducao, 6 Trakt e
  // Simkl, 7 Avancado, 8 Sobre. Quem mudar a ordem la conserta os numeros aqui.
  tecla(SDLK_DOWN); tecla(SDLK_DOWN);          // Layout, ainda no indice
  snprintf(nome, sizeof nome, "%s-indice.bmp", saida);
  captura(nome, w);
  tecla(SDLK_RETURN);                          // entra: foco no 1o grupo, fechado
  snprintf(nome, sizeof nome, "%s-secao.bmp", saida);
  captura(nome, w);
  tecla(SDLK_RETURN);                          // abre "Layout da Home"
  snprintf(nome, sizeof nome, "%s-grupo.bmp", saida);
  captura(nome, w);
  tecla(SDLK_DOWN);                            // Posteres horizontais: interruptor
  snprintf(nome, sizeof nome, "%s-interruptor.bmp", saida);
  captura(nome, w);

  // Voltar fecha o grupo e devolve o foco ao cabecalho; o grupo seguinte e
  // "Conteudo da Home", onde moram as fileiras.
  tecla(SDLK_ESCAPE);
  tecla(SDLK_DOWN);
  tecla(SDLK_RETURN);
  tecla(SDLK_DOWN);                            // Limite de fileiras
  snprintf(nome, sizeof nome, "%s-previa-fileiras.bmp", saida);
  captura(nome, w);
  tecla(SDLK_DOWN);                            // Ordenar e ativar fileiras
  snprintf(nome, sizeof nome, "%s-acao.bmp", saida);
  captura(nome, w);
  tecla(SDLK_RETURN);
  snprintf(nome, sizeof nome, "%s-fileiras.bmp", saida);
  captura(nome, w);

  // Uma fileira de CATALOGO em foco, na coluna do card.
  for (i = 0; i < 2; i++) tecla(SDLK_DOWN);
  tecla(SDLK_RIGHT);
  tecla(SDLK_RIGHT);
  snprintf(nome, sizeof nome, "%s-fileiras-catalogo.bmp", saida);
  captura(nome, w);

  // A FILA: desce ate depois do separador.
  for (i = 0; i < 7; i++) tecla(SDLK_DOWN);
  snprintf(nome, sizeof nome, "%s-fileiras-fila.bmp", saida);
  captura(nome, w);

  // ABA "FORA DA HOME": sobe ate a barra, direita troca a aba, desce na lista.
  for (i = 0; i < 12; i++) tecla(SDLK_UP);
  tecla(SDLK_RIGHT);
  tecla(SDLK_DOWN);
  snprintf(nome, sizeof nome, "%s-fileiras-fora.bmp", saida);
  captura(nome, w);

  // OK adiciona: com a home cheia, entra na fila e a tela avisa.
  tecla(SDLK_RETURN);
  snprintf(nome, sizeof nome, "%s-fileiras-adicionada.bmp", saida);
  captura(nome, w);
  // Desce ate o ULTIMO botao ("Atualizar tudo"): tem de ser alcancavel.
  for (i = 0; i < 12; i++) tecla(SDLK_DOWN);
  snprintf(nome, sizeof nome, "%s-fileiras-botao.bmp", saida);
  captura(nome, w);

  // "MEMORIA USADA POR IMAGENS" (Avancado, 5a linha): o painel da direita
  // ganha barra, grafico e estatisticas do cache. Tres Voltar: fecha a folha,
  // fecha o grupo (o foco sobe ao cabecalho), vai ao indice.
  tecla(SDLK_ESCAPE);
  tecla(SDLK_ESCAPE);
  tecla(SDLK_ESCAPE);
  for (i = 0; i < 12; i++) tecla(SDLK_UP);
  for (i = 0; i < 7; i++) tecla(SDLK_DOWN);    // Avancado
  tecla(SDLK_RETURN);
  for (i = 0; i < 4; i++) tecla(SDLK_DOWN);
  snprintf(nome, sizeof nome, "%s-imagens.bmp", saida);
  captura(nome, w);

  // A PREVIA DO CARTAZ: Layout > Estilo dos cartoes (6o grupo) > Largura.
  tecla(SDLK_LEFT);                            // a esquerda tambem leva ao indice
  for (i = 0; i < 5; i++) tecla(SDLK_UP);      // Layout
  tecla(SDLK_RETURN);
  for (i = 0; i < 5; i++) tecla(SDLK_DOWN);    // grupos todos fechados
  tecla(SDLK_RETURN);
  tecla(SDLK_DOWN);
  snprintf(nome, sizeof nome, "%s-previa-cartaz.bmp", saida);
  captura(nome, w);

  // MODO EDICAO numa lista de valores: Reproducao > Qualidade maxima.
  tecla(SDLK_ESCAPE); tecla(SDLK_ESCAPE);      // grupo -> cabecalho -> indice
  for (i = 0; i < 3; i++) tecla(SDLK_DOWN);    // Reproducao
  tecla(SDLK_RETURN);
  for (i = 0; i < 4; i++) tecla(SDLK_DOWN);    // pula os dois rotulos de bloco
  tecla(SDLK_RETURN);
  snprintf(nome, sizeof nome, "%s-edicao.bmp", saida);
  captura(nome, w);

  // SAIR DA CONTA PEDE DOIS OK: o primeiro so arma. A captura para no armado.
  tecla(SDLK_ESCAPE); tecla(SDLK_ESCAPE);
  for (i = 0; i < 12; i++) tecla(SDLK_UP);     // Conta
  tecla(SDLK_RETURN);
  tecla(SDLK_DOWN); tecla(SDLK_DOWN);
  tecla(SDLK_RETURN);
  snprintf(nome, sizeof nome, "%s-sair-armado.bmp", saida);
  captura(nome, w);
  assert(!ajustes_quer_sair());                // um OK nao sai

  // COR DE DESTAQUE ROSA (a da captura aprovada no DESIGN.md), e as duas
  // telas principais de novo com ela: Aparencia > Cor de destaque, seis passos.
  tecla(SDLK_LEFT);
  tecla(SDLK_DOWN);                            // Aparencia
  tecla(SDLK_RETURN);
  tecla(SDLK_RETURN);
  for (i = 0; i < 6; i++) tecla(SDLK_RIGHT);
  tecla(SDLK_RETURN);
  snprintf(nome, sizeof nome, "%s-rosa-aparencia.bmp", saida);
  captura(nome, w);
  tecla(SDLK_LEFT);
  tecla(SDLK_DOWN);                            // Layout
  tecla(SDLK_RETURN);
  tecla(SDLK_DOWN);                            // Conteudo da Home
  tecla(SDLK_RETURN);
  for (i = 0; i < 4; i++) tecla(SDLK_DOWN);    // Barra lateral moderna
  snprintf(nome, sizeof nome, "%s-rosa-grupo.bmp", saida);
  captura(nome, w);
  tecla(SDLK_LEFT);
  snprintf(nome, sizeof nome, "%s-rosa-indice.bmp", saida);
  captura(nome, w);

  // REMOVER O PORTAL tambem pede dois OK: Conteudo > Remover o portal Stalker,
  // armado com um OK. A captura para no armado.
  tecla(SDLK_DOWN);                            // Conteudo
  tecla(SDLK_RETURN);
  for (i = 0; i < 3; i++) tecla(SDLK_DOWN);    // Addons, portal, MAC, remover
  tecla(SDLK_RETURN);
  snprintf(nome, sizeof nome, "%s-remover-armado.bmp", saida);
  captura(nome, w);

  // APARENCIA COM AS LINHAS DA COR (merge da 1.5): "Cor de destaque" num tema
  // dinamico (Dinamica gradiente) e "Cor da logo" logo abaixo, que so vale com
  // ele — a captura para com o foco nela, para mostrar que ela esta ATIVA.
  tecla(SDLK_ESCAPE);
  for (i = 0; i < 12; i++) tecla(SDLK_UP);
  tecla(SDLK_DOWN);                            // Aparencia
  tecla(SDLK_RETURN);
  tecla(SDLK_RETURN);                          // edita a cor (esta em Rosa)
  for (i = 0; i < 8; i++) tecla(SDLK_RIGHT);   // Rosa(6) -> Dinamica gradiente(14)
  tecla(SDLK_RETURN);
  tecla(SDLK_DOWN);                            // Cor da logo
  snprintf(nome, sizeof nome, "%s-aparencia-cor.bmp", saida);
  captura(nome, w);
  assert(ajustes_cor_viva() != 0);
  // De volta ao Rosa: sem titulo em cena o dinamico nao tem arte de onde tirar
  // a cor, e as paginas abaixo sao para julgar a tela, nao a cor viva.
  tecla(SDLK_UP);
  tecla(SDLK_RETURN);
  for (i = 0; i < 8; i++) tecla(SDLK_LEFT);
  tecla(SDLK_RETURN);
  assert(ajustes_cor_viva() == 0);
  tecla(SDLK_DOWN);

  // AVANCADO > DIAGNOSTICO: o teste de velocidade colado no diagnostico.
  tecla(SDLK_LEFT);
  for (i = 0; i < 6; i++) tecla(SDLK_DOWN);    // Avancado
  tecla(SDLK_RETURN);
  for (i = 0; i < 6; i++) tecla(SDLK_DOWN);    // pula o rotulo "Diagnóstico"
  snprintf(nome, sizeof nome, "%s-avancado-velocidade.bmp", saida);
  captura(nome, w);
  tecla(SDLK_RETURN);                          // OK pede a tela do teste
  assert(ajustes_pediu_velocidade());

  // TODAS AS LINHAS, categoria a categoria e grupo a grupo, para serem olhadas
  // (o merge da 1.5 tirou o icone das linhas e o pos nas categorias e nos
  // grupos). Paginas de 6 passos. `LINHAS` e quantos itens com foco a
  // categoria tem no nivel de cima (grupos contam um); `GRUPOS`, as linhas de
  // cada grupo — espelho de TELA[] em ajustes.c.
  { static const int LINHAS[9] = { 3, 4, 6, 8, 3, 9, 3, 7, 4 };
    static const int GRUPOS[9][6] = {
      [2] = { 5, 15, 9, 9, 3, 11 },            // Layout
      [4] = { 14, 10, 1 },                     // Integracoes
    };
    int c, g, p, k;
    for (c = 0; c < 9; c++) {
      tecla(SDLK_LEFT);
      for (i = 0; i < 12; i++) tecla(SDLK_UP);
      for (i = 0; i < c; i++) tecla(SDLK_DOWN);
      tecla(SDLK_RETURN);
      for (p = 0; p * 6 < LINHAS[c]; p++) {
        snprintf(nome, sizeof nome, "%s-todas-c%d-%d.bmp", saida, c, p);
        captura(nome, w);
        for (k = 0; k < 6; k++) tecla(SDLK_DOWN);
      }
      // E A ULTIMA LINHA: os saltos de 6 param antes dela quando a conta nao
      // fecha (Reproducao tem 9 — os idiomas ficavam de fora).
      if (LINHAS[c] > 6) {
        for (k = 0; k < 12; k++) tecla(SDLK_DOWN);
        snprintf(nome, sizeof nome, "%s-todas-c%d-fim.bmp", saida, c);
        captura(nome, w);
      }
      for (g = 0; g < 6 && GRUPOS[c][g]; g++) {
        for (i = 0; i < 12; i++) tecla(SDLK_UP);
        for (i = 0; i < g; i++) tecla(SDLK_DOWN);
        tecla(SDLK_RETURN);                    // abre o grupo g
        for (p = 0; p * 6 < GRUPOS[c][g]; p++) {
          for (k = 0; k < (p ? 6 : 1); k++) tecla(SDLK_DOWN);
          snprintf(nome, sizeof nome, "%s-todas-c%d-g%d-%d.bmp", saida, c, g, p);
          captura(nome, w);
        }
        // Aqui SEM passar do fim: abaixo da ultima opcao vem o cabecalho do
        // grupo seguinte, e dele o Voltar iria ao indice em vez de fechar.
        if (GRUPOS[c][g] > 6 && GRUPOS[c][g] > 1 + 6 * (p - 1)) {
          for (k = 1 + 6 * (p - 1); k < GRUPOS[c][g]; k++) tecla(SDLK_DOWN);
          snprintf(nome, sizeof nome, "%s-todas-c%d-g%d-fim.bmp", saida, c, g);
          captura(nome, w);
        }
        tecla(SDLK_ESCAPE);                    // fecha; foco volta ao cabecalho
      }
    } }

  // "EXPERIMENTAR A COR VIVA" (cartao de novidades): a tela reabre ja em
  // Aparencia, com o foco na lista, na linha da cor.
  ajustes_abrir_na_cor();
  ajustes_iniciar();
  assert(!ajustes_foco_no_indice());
  snprintf(nome, sizeof nome, "%s-abrir-na-cor.bmp", saida);
  captura(nome, w);

  tex_encerrar();
  txt_encerrar();
  gfx_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(w);
  SDL_Quit();
  puts("PASS: capturas da tela de Ajustes gravadas.");
  return 0;
}
