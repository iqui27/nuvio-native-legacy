// Capturas da linha de AutoSync (F05) no seletor de legendas do F04
// (legendasui.c, aberto por faixas_abrir_em(1)), que cresce da ilha do relogio. Tudo real: legenda externa baixada por
// legenda.c, referencia embutida lida por Range (legref.c + rede.c) de um MKV
// servido por tests/legref_rangesrv.py, analise pela engine (autosync.c).
// Fora da suite (janela GL e olho humano):
//
//   bash tests/legsync_shot.sh /Volumes/ExternalSSD/nv-f05-shots
//
// NUVIO_SHOT_IDIOMA=N troca o idioma (2 = romeno). Fonte da interface: Montserrat (a TV
// do dono).
#include "catalogo.h"
#include "nvtmp.h"
#include "player.h"
#include "ajustes.h"
#include "faixas.h"
#include "plrilha.h"
#include "legsync.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "episodios.h"
#include "streams.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static GLuint fbo, fboTex;
static int LW = 1920, LH = 1080;
static char MKV[300];
static char srtBom[300], ruim[300];
// faixas.c de mentira: a "outra" legenda do mesmo idioma e a boa; ou nenhuma.
static int trocadorBom(const char *idioma, const uint64_t *tent, int n, int voltar, char *nome, unsigned tam) {
  (void)idioma;
  if (voltar) return 0;
  for (int k = 0; k < n; k++) if (tent[k] == legsync_hash_url(srtBom)) return 0;
  snprintf(nome, tam, "OpenSubtitles");
  legsync_primaria_externa(srtBom, "pt", "OpenSubtitles");
  return 1;
}
static int trocadorNenhum(const char *idioma, const uint64_t *tent, int n, int voltar, char *nome, unsigned tam) {
  (void)idioma; (void)tent; (void)n; (void)nome; (void)tam;
  if (voltar) { legsync_primaria_externa(ruim, "pt", "SubDL"); return 1; }
  return 0;
}

static void salvar(const char *saida, const char *sufixo) {
  char nome[700];
  SDL_Surface *s = SDL_CreateRGBSurface(0, LW, LH, 24, 0xff, 0xff00, 0xff0000, 0);
  int y; unsigned char *p, *t;
  snprintf(nome, sizeof nome, "%s-%s.bmp", saida, sufixo);
  glFinish(); glBindFramebuffer(GL_FRAMEBUFFER, fbo); glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, LW, LH, GL_RGB, GL_UNSIGNED_BYTE, s->pixels);
  p = s->pixels; t = malloc((size_t)s->pitch);
  for (y = 0; y < LH / 2; y++) {
    memcpy(t, p + y * s->pitch, (size_t)s->pitch);
    memcpy(p + y * s->pitch, p + (LH - 1 - y) * s->pitch, (size_t)s->pitch);
    memcpy(p + (LH - 1 - y) * s->pitch, t, (size_t)s->pitch);
  }
  free(t);
  assert(SDL_SaveBMP(s, nome) == 0);
  SDL_FreeSurface(s);
  printf("captura: %s\n", nome);
}

static Uint32 relogio = 100000;
static void quadros(int n) {
  int i;
  for (i = 0; i < n; i++) {
    relogio += 16;
    SDL_PumpEvents(); txt_novo_quadro(); tex_novo_quadro(); tex_bombear(6);
    player_atualizar(1.f / 60, relogio);
    // Sem video de verdade o player nao chama o passo; a fixture chama, com a
    // URL do MKV, buffer folgado e sem seek.
    legsync_passo(MKV, 30.0, 60.0, 0, relogio);
    episodios_atualizar(1.f / 60);
    stream_folha_atualizar(1.f / 60, relogio); faixas_atualizar(1.f / 60, relogio);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo); glViewport(0, 0, LW, LH);
    glClearColor(.62f, .66f, .74f, 1); glClear(GL_COLOR_BUFFER_BIT);
    player_desenhar(relogio);
    faixas_desenhar(relogio);
    plrilha_desenhar(relogio);   // a ilha (e o que cresce dela) por cima, como em app.c
    SDL_Delay(1);
  }
}

static void tecla(SDL_Keycode k) {
  SDL_Event ev; memset(&ev, 0, sizeof ev);
  ev.type = SDL_KEYDOWN; ev.key.keysym.sym = k; faixas_evento(&ev);
  quadros(2);
}

static LegSyncVisao esperar(LegSyncFase f) {
  LegSyncVisao v;
  for (int i = 0; i < 3000; i++) { quadros(1); v = legsync_visao(0); if (v.fase == f) return v; }
  fprintf(stderr, "esperava fase %d, ficou %d motivo %d\n", f, v.fase, v.motivo);
  assert(!"timeout");
  return v;
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : nvt("/nv-legsync");
  const char *base = getenv("NV_LEGSYNC_BASE");   // http://127.0.0.1:PORTA/f
  char srt[300];
  LegSyncVisao v;
  int i;
  assert(base);
  snprintf(MKV, sizeof MKV, "%s/ff.mkv", base);
  snprintf(srt, sizeof srt, "%s/ext_mais2500.srt", base);
  snprintf(srtBom, sizeof srtBom, "%s", srt);
  snprintf(ruim, sizeof ruim, "%s/ext_ruim.srt", base);
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *w = SDL_CreateWindow("Nuvio: autosync", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN); assert(w);
  SDL_GLContext gl = SDL_GL_CreateContext(w); assert(gl); SDL_GL_SetSwapInterval(0);
  glGenTextures(1, &fboTex); glBindTexture(GL_TEXTURE_2D, fboTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, LW, LH, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glGenFramebuffers(1, &fbo); glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTex, 0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  glViewport(0, 0, LW, LH); gfx_tamanho_alvo(LW, LH); assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1)); tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");
  { char caminho[700]; FILE *f; const char *li = getenv("NUVIO_SHOT_IDIOMA");
    snprintf(caminho, sizeof caminho, "%s/ajustes.txt", getenv("NUVIO_DADOS"));
    f = fopen(caminho, "w"); assert(f);
    // fonteInterface 3 = Montserrat, a fonte da interface na TV do dono.
    fprintf(f, "idioma %d\nselected_theme 2\nfonteInterface 3\n", li && *li ? atoi(li) : 0);
    fclose(f);
    ajustes_dir(getenv("NUVIO_DADOS")); }
  ajustes_iniciar();
  assert(txt_fonte_interface() == TXT_FAMILIA_MONTSERRAT);
  { CatItem c; memset(&c, 0, sizeof c);
    snprintf(c.tipo, sizeof c.tipo, "movie");
    snprintf(c.titulo, sizeof c.titulo, "A Noite dos Espelhos");
    snprintf(c.backdrop, sizeof c.backdrop, "deploy/app/art/19.jpg");
    cat_definir(&c, 1); }
  player_abrir(0, NULL);
  player_erro_fonte(); player_limpar_erro_fonte();
  quadros(20);

  // Sem legenda externa: o seletor nao tem linha de AutoSync.
  faixas_abrir_em(1);
  quadros(40);
  v = legsync_visao(0); assert(v.fase == LEGSYNC_INDISPONIVEL && v.acoes == 0);
  salvar(saida, "1-sem-externa");

  // R4: automatico. A externa (+2,5 s) escolhida sincroniza SOZINHA, sem menu.
  legsync_primaria_externa(srt, "pt", "OpenSubtitles");
  for (i = 0; i < 3000 && legsync_visao(0).autoFase != 2; i++) quadros(1);
  v = legsync_visao(0);
  printf("aceito: %+d ms (referencia %s)\n", v.offsetAutoMs, v.idiomaRef);
  assert(v.autoFase == 2 && abs(v.offsetAutoMs - 2500) <= 25 && !v.autoTrocou);
  // A linha (slot principal) fica logo acima de "Mais opcoes", a ultima.
  for (i = 0; i < 12; i++) tecla(SDLK_DOWN);
  tecla(SDLK_UP);
  quadros(30);
  salvar(saida, "2-sincronizada");             // "Sincronizada" + Desfazer
  tecla(SDLK_RETURN);                          // Desfazer
  v = legsync_visao(0); assert(v.fase == LEGSYNC_DESFEITA);
  quadros(30);
  salvar(saida, "3-desfeita");

  // A escolhida nao fecha: troca sozinha por outra do mesmo idioma que sincroniza.
  legsync_definir_trocador(trocadorBom);
  legsync_primaria_externa(ruim, "pt", "SubDL");
  for (i = 0; i < 3000 && legsync_visao(0).autoFase != 2; i++) quadros(1);
  v = legsync_visao(0);
  assert(v.autoFase == 2 && v.autoTrocou && abs(v.offsetAutoMs - 2500) <= 25);
  quadros(30);
  salvar(saida, "4-trocou");

  // Nenhuma fecha: volta a original e diz que nao deu.
  legsync_definir_trocador(trocadorNenhum);
  legsync_primaria_externa(ruim, "pt", "SubDL");
  for (i = 0; i < 3000 && legsync_visao(0).autoFase != 3; i++) quadros(1);
  v = legsync_visao(0);
  assert(v.autoFase == 3 && legsync_offset_ms(0) == 0);
  quadros(30);
  salvar(saida, "5-nao-deu");
  legsync_primaria_outra(1);                   // a embutida escolhida
  quadros(30);
  salvar(saida, "6-embutida");
  puts("legsync_shot: ok");
  return 0;
}
