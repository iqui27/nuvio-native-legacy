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

  snprintf(nome, sizeof nome, "%s-lista.bmp", saida);
  captura(nome, w);

  // Vai ate a secao das fileiras pela COLUNA DE SECOES, que e o caminho real no
  // controle: Voltar leva ao indice, baixo escolhe a categoria, OK entra nela.
  // O `secao` da linha de comando diz QUAL categoria, porque o agrupamento muda
  // e cravar o numero aqui faria a captura mirar outra tela depois.
  { int secao = argc > 2 ? atoi(argv[2]) : 1;   // Home (a folha de fileiras mora nela)
    tecla(SDLK_ESCAPE);
    for (i = 0; i < secao; i++) tecla(SDLK_DOWN);
    // O FOCO NA COLUNA DE CATEGORIAS: a categoria em foco preenchida com a
    // cor de realce, como as linhas da lista.
    snprintf(nome, sizeof nome, "%s-indice.bmp", saida);
    captura(nome, w);
    tecla(SDLK_RETURN); }
  snprintf(nome, sizeof nome, "%s-secao.bmp", saida);
  captura(nome, w);

  // Entra na folha de fileiras. A linha de acao e a ULTIMA da secao; descer ate
  // encontrar uma OP_ACAO seria adivinhar, entao o teste desce um numero fixo
  // passado na linha de comando.
  { int passos = argc > 3 ? atoi(argv[3]) : 6;  // "Ordenar e ativar fileiras", 7a da Home
    for (i = 0; i < passos; i++) tecla(SDLK_DOWN); }
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

  // A LINHA "MEMORIA USADA POR IMAGENS", que e a ultima da categoria da
  // conta: o painel da direita ganha barra, grafico e estatisticas do cache.
  // `conta` na linha de comando diz qual categoria, pelo mesmo motivo de
  // `secao`. Sai da folha, volta ao indice, escolhe a categoria e desce ate o
  // fim da categoria: 13 linhas, 12 passos. A lista e continua entre
  // categorias, entao descer "ate parar" cairia na categoria seguinte.
  { int conta = argc > 4 ? atoi(argv[4]) : 5;
    tecla(SDLK_ESCAPE);
    tecla(SDLK_ESCAPE);
    for (i = 0; i < 12; i++) tecla(SDLK_UP);
    for (i = 0; i < conta; i++) tecla(SDLK_DOWN);
    tecla(SDLK_RETURN);
    // QUANTOS PASSOS ATE A ULTIMA LINHA DA CATEGORIA. Era 13 cravado, quando
    // "Interface e conta" tinha 14 linhas. A categoria cresceu (hoje sao 17) e
    // o numero cravado passou a parar em "Sair da conta": a captura saia sem o
    // painel que ela existe para mostrar, exatamente o defeito que o comentario
    // anterior ja descrevia com outro numero. Agora e argumento, com o valor
    // certo de hoje como padrao — quem crescer a categoria conserta a chamada,
    // nao o codigo.
    { int fundo = argc > 5 ? atoi(argv[5]) : 22;   // 24 linhas hoje; "Memoria usada" e a 23a
      for (i = 0; i < fundo; i++) tecla(SDLK_DOWN); } }
  snprintf(nome, sizeof nome, "%s-imagens.bmp", saida);
  captura(nome, w);

  // AS PREVIAS DO PAINEL DE AJUDA (20/09/2026): tamanho do cartaz (categoria
  // "Cartazes", penultima linha... a de largura) e limite de fileiras (Home,
  // 3a linha). Cada uma desenhada com o valor atual.
  tecla(SDLK_ESCAPE);
  for (i = 0; i < 12; i++) tecla(SDLK_UP);
  for (i = 0; i < 4; i++) tecla(SDLK_DOWN);   // Cartazes
  tecla(SDLK_RETURN);
  for (i = 0; i < 13; i++) tecla(SDLK_DOWN);   // Largura do item
  snprintf(nome, sizeof nome, "%s-previa-cartaz.bmp", saida);
  captura(nome, w);
  tecla(SDLK_ESCAPE);
  for (i = 0; i < 12; i++) tecla(SDLK_UP);
  tecla(SDLK_DOWN);                            // Home
  tecla(SDLK_RETURN);
  tecla(SDLK_DOWN); tecla(SDLK_DOWN);          // Limite de fileiras
  snprintf(nome, sizeof nome, "%s-previa-fileiras.bmp", saida);
  captura(nome, w);

  // TODAS AS LINHAS, pagina a pagina (25/09/2026, icones Lucide por familia):
  // cada linha tem o proprio icone, e julgar a troca olhando so o topo de tres
  // categorias deixava 80 linhas sem ver. A lista e continua entre categorias,
  // entao basta entrar na primeira e descer. 6 passos por captura, e nao 7: a
  // rolagem salta os rotulos de subsecao, e com 7 as duas linhas de memoria
  // caiam entre uma pagina e a seguinte. 20 paginas passam das ~100 linhas.
  tecla(SDLK_ESCAPE);
  for (i = 0; i < 12; i++) tecla(SDLK_UP);
  tecla(SDLK_RETURN);
  { int p, k;
    for (p = 0; p < 20; p++) {
      snprintf(nome, sizeof nome, "%s-todas-%02d.bmp", saida, p);
      captura(nome, w);
      for (k = 0; k < 6; k++) tecla(SDLK_DOWN);
    } }

  tex_encerrar();
  txt_encerrar();
  gfx_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(w);
  SDL_Quit();
  puts("PASS: capturas da tela de Ajustes gravadas.");
  return 0;
}
