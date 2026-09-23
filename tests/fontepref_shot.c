// CAPTURA DA FOLHA DE FONTES com a marca "Sua escolha anterior".
//
// Existe porque duas regressoes deste repositorio so foram vistas OLHANDO: o
// raio de gfx_cor e uma FRACAO DA ALTURA (nao pixels), e um anel de foco branco
// sobre pilula clara e invisivel. Uma marca de texto nova a 3 m de distancia
// entra na mesma categoria — cabe na linha? some sob o realce? colide com o
// provedor?
//
// NAO ENTRA NA SUITE (tools/testa-tudo.sh pula *_shot.sh): precisa de janela GL
// e de olho humano. Nao chama dados_iniciar: sem pasta de dados, fontepref nao
// le nem escreve arquivo nenhum e a captura nao toca no ~/.nuvio de quem roda.
#include "streams.h"
#include "fontepref.h"
#include "badges.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "ajustes.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void fonte(Stream *s, const char *provedor, const char *rotulo,
                  const char *descricao, int altura, int mp4, int dv) {
  memset(s, 0, sizeof *s);
  snprintf(s->provedor, sizeof s->provedor, "%s", provedor);
  snprintf(s->rotulo, sizeof s->rotulo, "%s", rotulo);
  snprintf(s->descricao, sizeof s->descricao, "%s", descricao);
  snprintf(s->url, sizeof s->url, "https://exemplo.invalido/%s.mkv", provedor);
  s->altura = altura; s->mp4 = mp4; s->dolbyVision = dv;
  s->tamanhoMB = altura >= 2160 ? 11264 : 2048;
  // A FILEIRA DE BADGES E O SEGUNDO ITEM QUE A CAPTURA PRECISA MOSTRAR: no app
  // ela e preenchida por stream_parse, que esta captura nao usa. Sem esta
  // linha a base da linha sai vazia e a regressao de "badge branca sobre linha
  // clara" fica invisivel justamente na ferramenta que existe para ve-la.
  s->badges = badges_detectar(descricao);
}

static void captura(const char *nome, SDL_Window *win) {
  int i;
  for (i = 0; i < 60; i++) {
    SDL_PumpEvents();
    txt_novo_quadro();
    tex_novo_quadro();
    tex_bombear(6);
    stream_folha_atualizar(1.0f / 60.0f, SDL_GetTicks());
    glClearColor(0.025f, 0.025f, 0.03f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    stream_folha_desenhar(SDL_GetTicks());
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

static void tecla(SDL_Keycode k) {
  SDL_Event e = { 0 };
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  stream_folha_evento(&e);
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : "/tmp/nuvio-fontepref";
  char nome[600];
  SDL_Window *w;
  SDL_GLContext gl;
  Stream v[5];
  int i;

  // A captura de foco usa o mesmo accent Ocean do album de sidebar quando
  // NUVIO_DADOS vem do wrapper; o tema continua selecionavel no ambiente.
  { const char *dir = getenv("NUVIO_DADOS");
    if (dir && *dir) {
      const char *temaEnv = getenv("NUVIO_SHOT_THEME");
      char caminho[700];
      FILE *f;
      int tema = temaEnv && *temaEnv ? atoi(temaEnv) : 2;
      if (tema < 0 || tema >= 12) tema = 2;
      snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dir);
      f = fopen(caminho, "w"); assert(f);
      fprintf(f, "idioma 0\nselected_theme %d\n", tema);
      fclose(f);
      ajustes_dir(dir);
    } }

  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("Nuvio: folha de fontes", SDL_WINDOWPOS_CENTERED,
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
  badges_carregar("deploy/app/art");   // quem faz isto no app e home.c

  fonte(&v[0], "Torrentio", "Torrentio\n4k",
        "Silo.S02E05.2160p.WEB-DL.DV.HDR.Atmos.mp4\n11.2 GB", 2160, 1, 1);
  fonte(&v[1], "Torrentio", "Torrentio\n1080p",
        "Silo.S02E05.1080p.WEB.x264.mkv\nEnglish", 1080, 0, 0);
  fonte(&v[2], "AIOStreams", "AIOStreams\n1080p",
        "Silo.S02E05.1080p.DUAL.mkv\nDual Audio", 1080, 0, 0);
  // A LEMBRADA CARREGA BADGES DE PROPOSITO: e a linha que recebe A MARCA e o
  // realce ao mesmo tempo, entao e nela que "pilula clara sobre linha clara" e
  // "badge branca sobre linha clara" aparecem juntas.
  // O NOME DE ADDON E LONGO DE PROPOSITO: e o corte por wProv que decide se a
  // linha do provedor encosta na marca, e com um "Torrentio" de 9 letras a
  // folga nunca e exercida — a captura passava sem provar nada.
  fonte(&v[3], "Torrentio · RealDebrid · Cached", "Torrentio\n1080p",
        "Silo.S02E05.1080p.WEB-DL.x265.DDP5.1.DUBLADO.mkv\n\xf0\x9f\x87\xa7\xf0\x9f\x87\xb7 Dublado", 1080, 0, 0);
  fonte(&v[4], "Outro Addon", "Outro Addon 720p", "Silo.S02E05.720p.mkv", 720, 0, 0);
  stream_definir_lista(v, 5);
  stream_folha_contexto("T2:E5 · Silo");

  // A DUBLADA E A LEMBRADA (indice 3), e a que esta TOCANDO e a 4K (indice 0):
  // e o caso que interessa olhar, porque as duas marcas aparecem na mesma
  // coluna em linhas diferentes.
  stream_definir_atual(0);
  stream_preferir(3);
  stream_folha_abrir();
  // stream_folha_abrir poe o foco na que esta tocando; descer ate a lembrada
  // mostra tambem como a marca se comporta SOB o realce.
  tecla(SDLK_DOWN);   // do grupo de botoes para a lista
  snprintf(nome, sizeof nome, "%s-folha.bmp", saida);
  captura(nome, w);

  // A LEMBRADA (indice 3) SOB O REALCE. E a unica combinacao que prova as duas
  // coisas de uma vez: a marca e a fileira de badges sobre a superficie CLARA.
  // Sem esta captura sobra so a linha 1 (badges sem marca) e a linha 4 (marca
  // nenhuma) — e foi assim que "pilula clara sobre linha clara" passou batido.
  for (i = 0; i < 2; i++) tecla(SDLK_DOWN);
  snprintf(nome, sizeof nome, "%s-folha-marca.bmp", saida);
  captura(nome, w);

  tecla(SDLK_DOWN);
  snprintf(nome, sizeof nome, "%s-folha-foco.bmp", saida);
  captura(nome, w);

  tex_encerrar();
  txt_encerrar();
  gfx_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(w);
  SDL_Quit();
  puts("PASS: capturas da folha de fontes gravadas.");
  return 0;
}
