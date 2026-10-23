// CABECALHO DA CENTRAL DE CONTROLE na ilha do player (titulo alinhado ao corpo).
// Nao entra na suite: janela GL. bash tests/central_cab_shot.sh <prefixo>
// (derivado de fontes_cabecalho_shot.c)
// CAPTURAS DO CABECALHO DA FOLHA DE FONTES (#202, dono: "subir os botoes para
// o lado do titulo e as abas em cima, para o cabecalho nao ficar tao grande").
//
// Estados: lista com "Melhor para esta TV" na primeira linha, a fileira de
// botoes em foco com "So MP4" ligado, e as abas de addon em foco. Cada um em
// portugues e em alemao (os rotulos mais longos), com e sem o "Sem HDR" da LG
// (6 botoes), na folha solta (pagina do titulo) e na ilha do player.
//
// Mede, em pixels da tela, do topo da folha ate a primeira linha da lista
// (o cabecalho), e confere que nenhum botao passa por cima do titulo nem sai
// da folha. NAO ENTRA NA SUITE (*_shot.sh): janela GL e olho humano.
//   bash tests/fontes_cabecalho_shot.sh /Volumes/ExternalSSD/nv-202-fontes-out/after
#include "catalogo.h"
#include "player.h"
#include "ajustes.h"
#include "faixas.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "episodios.h"
#include "streams.h"
#include "plrilha.h"
#include "central.h"
#include "video.h"
#include "badges.h"
#include "idioma.h"
#include "nvtmp.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static GLuint fbo, fboTex;
enum { LW = 1920, LH = 1080 };
static const char *saida;
static unsigned char quadro[LW * LH * 3];
static int noPlayer;
static Uint32 relogio = 100000;

static void salvar(const char *id) {
  SDL_Surface *s = SDL_CreateRGBSurface(0, LW, LH, 24, 0xff, 0xff00, 0xff0000, 0);
  char nome[700];
  glFinish(); glBindFramebuffer(GL_FRAMEBUFFER, fbo); glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, LW, LH, GL_RGB, GL_UNSIGNED_BYTE, quadro);
  for (int y = 0; y < LH; y++)
    memcpy((unsigned char *)s->pixels + y * s->pitch, quadro + (size_t)(LH - 1 - y) * LW * 3, (size_t)LW * 3);
  snprintf(nome, sizeof nome, "%s-%s.png", saida, id);
  assert(IMG_SavePNG(s, nome) == 0);
  SDL_FreeSurface(s);
  printf("captura: %s\n", nome);
}

static void quadros(int n) {
  for (int i = 0; i < n; i++) {
    relogio += 16;
    SDL_PumpEvents(); txt_novo_quadro(); tex_novo_quadro(); tex_bombear(8);
    if (noPlayer) player_atualizar(1.f / 60, relogio);
    central_atualizar(1.f / 60, relogio);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo); glViewport(0, 0, LW, LH);
    glClearColor(0.025f, 0.025f, 0.03f, 1); glClear(GL_COLOR_BUFFER_BIT);
    gfx_novo_quadro();
    if (noPlayer) player_desenhar(relogio);
    else {
      gfx_cor((GfxRect){ 0, 0, LW, LH }, 0, .26f, .17f, .12f, 1);
      gfx_cor((GfxRect){ 0, 0, LW, 360 }, 0, .55f, .36f, .22f, 1);
    }
    central_desenhar(relogio);
    plrilha_desenhar(relogio);
  }
}
static void tecla(SDL_Keycode k) {
  SDL_Event e; memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = k;
  stream_folha_evento(&e);
  quadros(2);
}

int main(int argc, char **argv) {
  const char *li = getenv("NUVIO_SHOT_IDIOMA");
  saida = argc > 1 ? argv[1] : nvt("/nv-fontes-cab");
  noPlayer = 1;
  video_shot_pode_forcar_sdr(getenv("NUVIO_SHOT_SEMHDR") && atoi(getenv("NUVIO_SHOT_SEMHDR")));
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *w = SDL_CreateWindow("Nuvio: cabecalho das fontes", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(w);
  SDL_GLContext gl = SDL_GL_CreateContext(w); assert(gl); SDL_GL_SetSwapInterval(0);
  glGenTextures(1, &fboTex); glBindTexture(GL_TEXTURE_2D, fboTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, LW, LH, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glGenFramebuffers(1, &fbo); glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTex, 0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  glViewport(0, 0, LW, LH); gfx_tamanho_alvo(LW, LH); assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1)); tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");
  badges_carregar("deploy/app/art");
  { char caminho[700]; FILE *f;
    snprintf(caminho, sizeof caminho, "%s/ajustes.txt", getenv("NUVIO_DADOS"));
    f = fopen(caminho, "w"); assert(f);
    // Montserrat: a fonte da TV do dono (Inter e so o padrao do Mac).
    fprintf(f, "idioma %d\nselected_theme 2\nvidroLocal 0\nfonteInterface %d\n",
            li && *li ? atoi(li) : 0, TXT_FAMILIA_MONTSERRAT);
    fclose(f);
    ajustes_dir(getenv("NUVIO_DADOS")); }
  ajustes_iniciar();


  {
    struct tm lt; time_t t = time(NULL);
    VideoSimulacao vs;
    CatItem c;
    localtime_r(&t, &lt); lt.tm_hour = 20; lt.tm_min = 19; lt.tm_sec = 0;
    plrilha_shot_hora(mktime(&lt));
    memset(&c, 0, sizeof c);
    snprintf(c.tipo, sizeof c.tipo, "series");
    snprintf(c.titulo, sizeof c.titulo, "Silo");
    snprintf(c.backdrop, sizeof c.backdrop, "deploy/app/art/19.jpg");
    cat_definir(&c, 1);
    player_abrir(0, NULL);
    player_shot_video(1);
    memset(&vs, 0, sizeof vs);
    vs.largura = 3840; vs.altura = 2160; vs.pronto = 1; vs.pos = 12.0; vs.duracao = 3360.0;
    video_simular(&vs);
    player_shot_estado(relogio, 12.0f, 3360.0f, 1, 0, 0, 0);
    player_shot_esconder();
    quadros(10);
  }
  central_abrir();
  quadros(240);
  salvar("central");
  // #205: o titulo recua para CC_PAD (20) e so ele; a linha do relogio (com o
  // "termina as", ha duracao) segue encostada a direita (recuo de 28).
  { GfxRect r; float tx, fx; int tf, ok = plrilha_rect(&r);
    plrilha_shot_linha(&tx, &fx, &tf);
    printf("ilha x=%.1f w=%.1f titulo=%.1f fim=%.1f terminaAs=%d\n", r.x, r.w, tx, fx, tf);
    if (!ok || fabsf(tx - (r.x + 20.0f)) > 1.0f) { puts("FALHA: titulo fora do recuo de 20"); return 1; }
    if (fabsf(fx - (r.x + r.w - 28.0f)) > 1.0f) { puts("FALHA: relogio saiu da direita"); return 1; }
    if (!tf) { puts("FALHA: sem \"termina as\""); return 1; } }
  puts("PASS: cabecalho da central");
  return 0;
}
