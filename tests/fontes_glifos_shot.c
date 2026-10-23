// CAPTURA DA FOLHA DE FONTES com nomes de addon cheios de emoji, bandeira,
// versalete e tracos de caixa (#144): nada pode sair como quadradinho.
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
#include "nvtmp.h"
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

#define N 8
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
  { char texto[3200];
    // Como stream_parse: rotulo + descricao, e o DV tambem vale como dado.
    snprintf(texto, sizeof texto, "%s %s", rotulo, descricao);
    s->badges = badges_detectar(texto); }
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
  const char *saida = argc > 1 ? argv[1] : nvt("/nuvio-fontepref");
  char nome[600];
  SDL_Window *w;
  SDL_GLContext gl;
  Stream v[N];
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
      // NUVIO_SHOT_VIDRO=1: a mesma folha na Interface de vidro (#198: os
      // selos coloridos tem de sair iguais nos dois materiais).
      if (getenv("NUVIO_SHOT_VIDRO") && atoi(getenv("NUVIO_SHOT_VIDRO"))) fprintf(f, "vidroLocal 0\n");
      // NUVIO_SHOT_SELOS=0: a fileira branca de antes (Selos coloridos desligado).
      if (getenv("NUVIO_SHOT_SELOS") && !atoi(getenv("NUVIO_SHOT_SELOS"))) fprintf(f, "selosColoridosLocal 1\n");
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

  fonte(&v[0], "AIOJorge", "1080p ⚡ Stream",
        "Mushoku Tensei S₀₃ᴇ₁₄ ◈ 1.77 GB 🎞️ 📦 ᴇN · ᴊA · ꜱᴜB\nRᴇLᴇAꜱᴇ ⚡ ɴᴇᴛꜰʟɪx", 1080, 0, 0);
  fonte(&v[1], "AIOJorge", "🎞️ 1080P Bluray",
        "📁 The Disastrous Life of Saiki K. S01E08 ✅\n💾 517 MB / 25.7 GB 🌐 🇬🇧 · 🇧🇷 · 🇯🇵 (🇬🇧)\n🔊 DDP5.1 Atmos", 1080, 0, 0);
  fonte(&v[2], "AIOStreams", "⭐ 𝗕𝗹𝘂𝗥𝗮𝘆 ｜ 𝐇𝐃𝐑 ｜ 𝗗𝗩",
        "🎬 Silo S02E05 │ 2160p ┃ HEVC ▸ 10bit ◆ ATMOS ● Dual Audio\n🧲 Torrentio 👤 154 💾 11.2 GB ⚙️ YTS ☁️ RD+", 2160, 0, 1);
  fonte(&v[3], "Torrentio", "Torrentio\n4k",
        "Silo.S02E05.2160p.WEB-DL.DV.HDR.Atmos.mp4\n👤 32 💾 11.2 GB ⚙️ ThePirateBay\n🇬🇧 / 🇪🇸 / 🇫🇷", 2160, 1, 1);
  fonte(&v[4], "MediaFusion", "🌐 ᴍᴇᴅɪᴀꜰᴜꜱɪᴏɴ ⚡",
        "❌ Uncached ⏳ Movie.2024.1080p.WEB-DL.x265-GRP 📦 4.2 GB 🔊 AAC 2.0 🇧🇷 Dublado Português", 1080, 0, 0);
  fonte(&v[5], "Addon JP", "デッドプール 🎞️ 1080p",
        "デッドプール & ウルヴァリン 🇯🇵 字幕 💾 6.1 GB ✓ ⭐⭐⭐⭐", 1080, 0, 0);
  fonte(&v[6], "Longo", "Nome muito longo de fonte que precisa de reticencias no fim da linha para caber ⚡ 4K 🎞️ HDR",
        "Uma descricao enorme que passa de duas linhas inteiras, com bastante texto corrido e depois enfeites 🎞️ 📦 💾 🔊 🌐 para conferir que o corte acontece so no fim da ultima linha visivel sem quebrar palavra no meio nem sobrar quadradinho nenhum aqui", 720, 0, 0);
  fonte(&v[7], "Plain", "Plain 720p", "Silo.S02E05.720p.mkv", 720, 0, 0);
  stream_definir_lista(v, N);
  stream_folha_contexto("T2:E5 · Silo");
  stream_folha_abrir();
  tecla(SDLK_DOWN);   // do grupo de botoes para a lista
  snprintf(nome, sizeof nome, "%s-1.bmp", saida);
  captura(nome, w);
  for (i = 0; i < 3; i++) tecla(SDLK_DOWN);
  snprintf(nome, sizeof nome, "%s-2.bmp", saida);
  captura(nome, w);
  for (i = 0; i < 3; i++) tecla(SDLK_DOWN);
  snprintf(nome, sizeof nome, "%s-3.bmp", saida);
  captura(nome, w);
  return 0;
}
