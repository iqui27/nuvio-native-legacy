// CAPTURA DA FOLHA DE FONTES COM O STREAMFIT (F03).
//
// O que precisa de olho: a linha de conexao da fonte em foco (origem, idade,
// Mbps sustentados, demanda x orcamento, ou "sem dados" com o motivo), o
// "Acima da conexao" no fim da fileira de selos, a fonte pesada no fim do
// proprio grupo, e a marca da escolha automatica pesada que deixa de dizer
// "Melhor para esta TV" sem fingir que o automatico mudou.
//
// Medidas SINTETICAS injetadas pela API real (streamfit_passiva /
// streamfit_diagnostico): nao e rede nem TV. NAO ENTRA NA SUITE (*_shot.sh).
#include "catalogo.h"
#include "nvtmp.h"
#include "streams.h"
#include "streamfit.h"
#include "badges.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "ajustes.h"
#include "vidro_fundo.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void fonte(Stream *s, const char *host, const char *descricao, int altura,
                  int mp4, int dv, double mbpsDemanda, long mb) {
  memset(s, 0, sizeof *s);
  snprintf(s->provedor, sizeof s->provedor, "AIOStreams | ElfHosted");
  snprintf(s->rotulo, sizeof s->rotulo, "Silo S02 E05");
  snprintf(s->descricao, sizeof s->descricao, "%s", descricao);
  snprintf(s->url, sizeof s->url, "https://%s/video/%ld.%s", host, mb, mp4 ? "mp4" : "mkv");
  s->altura = altura; s->mp4 = mp4; s->dolbyVision = dv; s->fileIdx = -1;
  s->tamanhoMB = mb;
  // behaviorHints.videoSize exato: demanda media = bytes*8/runtime.
  s->tamanhoBytes = mbpsDemanda > 0 ? (uint64_t)(mbpsDemanda * 1e6 / 8.0 * 3000.0) : 0;
  s->badges = badges_detectar(descricao);
}

static void captura(const char *nome, SDL_Window *win) {
  for (int i = 0; i < 60; i++) {
    SDL_PumpEvents(); txt_novo_quadro(); tex_novo_quadro(); tex_bombear(6);
    stream_folha_atualizar(1.0f / 60.0f, SDL_GetTicks());
    glClearColor(0.025f, 0.025f, 0.03f, 1.0f); glClear(GL_COLOR_BUFFER_BIT);
    gfx_novo_quadro();
    if (vidroFundoAtivo()) vidroFundoDesenhar();
    else {
      gfx_cor((GfxRect){ 0, 0, 1920, 1080 }, 0, .26f, .17f, .12f, 1);
      gfx_cor((GfxRect){ 0, 0, 1920, 360 }, 0, .55f, .36f, .22f, 1);
    }
    stream_folha_desenhar(SDL_GetTicks());
    if (i == 59) {
      unsigned char *pix = malloc(1920 * 1080 * 4);
      SDL_Surface *s;
      assert(pix);
      glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
      s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
      assert(s);
      for (int y = 0; y < 1080; y++)
        memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
      assert(IMG_SavePNG(s, nome) == 0);
      SDL_FreeSurface(s); free(pix);
    }
    SDL_GL_SwapWindow(win);
  }
  printf("captura: %s\n", nome);
}
static void tecla(SDL_Keycode k) {
  SDL_Event e = { 0 };
  e.type = SDL_KEYDOWN; e.key.keysym.sym = k;
  stream_folha_evento(&e);
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : nvt("/nuvio-streamfit-folha");
  char nome[600];
  SDL_Window *w; SDL_GLContext gl;
  Stream v[5];
  { const char *dir = getenv("NUVIO_DADOS");
    if (dir && *dir) {
      char caminho[700]; FILE *f;
      const char *li = getenv("NUVIO_SHOT_IDIOMA");
      snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dir);
      f = fopen(caminho, "w"); assert(f);
      fprintf(f, "idioma %d\nselected_theme 2\nvidroLocal 0\n", li && *li ? atoi(li) : 0);
      // Montserrat: the owner's TV interface font (Inter is only the Mac fixture default).
      fprintf(f, "fonteInterface %d\n", TXT_FAMILIA_MONTSERRAT);
      fclose(f);
      ajustes_dir(dir);
    } }
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("Nuvio: StreamFit", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                       1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(w);
  gl = SDL_GL_CreateContext(w); assert(gl);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(64);
  vidroFundoPreparar();
  gfx_icones_dir("deploy/app/art");
  badges_carregar("deploy/app/art");

  // Rede conhecida (epoch 1). O host do CDN tem uma janela PASSIVA de 4 min
  // atras (~22 Mbps sustentados); o host do debrid tem um DIAGNOSTICO de 50
  // min atras (~60 Mbps). O resolvedor nunca foi medido.
  { uint64_t agora = streamfit_agora_ms();
    int cdn[12] = {24000,22000,23000,25000,21000,22000,26000,23000,22000,24000,23000,22000};
    int deb[8] = {62000,60000,61000,63000,59000,60000,64000,61000};
    streamfit_rede(1);
    assert(streamfit_passiva(1, "https://cdn.exemplo.invalido", cdn, 12, agora - 4 * 60000) == 12);
    assert(streamfit_diagnostico(1, "https://debrid.exemplo.invalido/x", deb, 8, agora - 50 * 60000) == 8); }

  // 4K DV no CDN pede ~31 Mbps: acima do orcamento (p20 x 0,75 ~ 16) -> pesada,
  // e e a escolha automatica (DV). 4K no debrid cabe. 1080p no CDN cabe.
  // 1080p via resolvedor e desconhecida; 720p sem videoSize tambem.
  fonte(&v[0], "cdn.exemplo.invalido", "11.6 GB | 31 Mbps\nSilo.S02E05.2160p.WEBMux.DV.HDR.HEVC.Atmos-SGF.mkv", 2160, 0, 1, 31.0, 11878);
  fonte(&v[1], "debrid.exemplo.invalido", "10.2 GB | 27 Mbps\nSilo.S02E05.2160p.HDR.mkv", 2160, 0, 0, 27.0, 10444);
  fonte(&v[2], "cdn.exemplo.invalido", "4.1 GB | 11 Mbps\nSilo.2024.S02E05.WEB-DL.1080p.mkv", 1080, 0, 0, 11.0, 4198);
  fonte(&v[3], "resolver.exemplo.invalido", "2.3 GB | 6 Mbps\nSilo.S02E05.1080p.x265-ELiTE.mkv", 1080, 0, 0, 6.0, 2355);
  fonte(&v[4], "cdn.exemplo.invalido", "1.2 GB\nSilo S02E05.mp4", 720, 1, 0, 0, 1228);
  stream_definir_alvo("tt14688458:2:5");
  stream_fit_duracao("tt14688458:2:5", 3000, SF_DUR_MEDIA);
  stream_definir_lista(v, 5);
  stream_folha_nome("Silo");
  stream_folha_contexto("T2:E5 · Silo");
  stream_folha_abrir();
  printf("automatica=%d (pesada esperada: 0)\n", stream_automatico());

  // Abre na escolha automatica; DOWN = proxima linha (1080p no CDN, passiva).
  tecla(SDLK_DOWN);
  snprintf(nome, sizeof nome, "%s-1-passiva-cabe.png", saida); captura(nome, w);
  tecla(SDLK_UP);     // a automatica pesada, no fim do grupo 4K
  snprintf(nome, sizeof nome, "%s-2-automatica-pesada.png", saida); captura(nome, w);
  tecla(SDLK_UP);     // 4K no debrid, diagnostico
  snprintf(nome, sizeof nome, "%s-3-diagnostico-cabe.png", saida); captura(nome, w);
  for (int k = 0; k < 3; k++) tecla(SDLK_DOWN);   // 1080p via resolvedor
  snprintf(nome, sizeof nome, "%s-4-resolvedor-desconhecida.png", saida); captura(nome, w);
  tecla(SDLK_DOWN);   // 720p sem videoSize
  snprintf(nome, sizeof nome, "%s-5-sem-tamanho.png", saida); captura(nome, w);

  tex_encerrar(); txt_encerrar(); gfx_encerrar();
  SDL_GL_DeleteContext(gl); SDL_DestroyWindow(w); SDL_Quit();
  puts("PASS: capturas do StreamFit gravadas.");
  return 0;
}
