// Capturas da opcao "Por audio" (F06) na linha de AutoSync do seletor de
// legendas (legendasui.c). Legenda externa real baixada por HTTP local
// (legenda.c + rede.c); PCM sintetico entregue como o tap do Android entrega
// (audsync_pcm), VAD/alinhamento reais e aceite pela engine (autosync.c).
// Fora da suite (janela GL e olho humano):
//
//   bash tests/audsync_shot.sh /Volumes/ExternalSSD/nv-f06-shots/audsync
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
#include "audsync.h"
#include "audsync_sint.h"
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
    legsync_audio_habilitar(ajustes_legenda_sync_audio());
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

static void ligarTap(int on) { (void)on; }

// Entrega [de, ate) como o tap: 60 ms por vez, esperando espaco no anel.
static void tocar(double de, double ate, double offset, int tipo) {
  int16_t b[960]; double t; int k = 0;
  for (t = de; t < ate; t += 0.06, k++) {
    sint_pcm(b, 960, t, offset, tipo);
    while (audsync_teste_livre() < 960) SDL_Delay(1);
    audsync_pcm(b, 960, (int64_t)llround(t * 1e6));
    if (k % 50 == 0) quadros(1);
  }
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : nvt("/nv-audsync");
  const char *base = getenv("NV_AUDSYNC_BASE");   // http://127.0.0.1:PORTA
  char srt[300];
  LegSyncVisao v;
  int i;
  assert(base);
  snprintf(MKV, sizeof MKV, "%s/filme.mkv", base);
  snprintf(srt, sizeof srt, "%s/falas.srt", base);
  sint_timeline(0, 900, 99);
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *w = SDL_CreateWindow("Nuvio: audsync", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN); assert(w);
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
    // fonteInterface 3 = Montserrat (a TV do dono); Sincronia por audio LIGADA (0).
    fprintf(f, "idioma %d\nselected_theme 2\nfonteInterface 3\nlegendaSyncAudioLocal 0\n", li && *li ? atoi(li) : 0);
    fclose(f);
    ajustes_dir(getenv("NUVIO_DADOS")); }
  ajustes_iniciar();
  assert(txt_fonte_interface() == TXT_FAMILIA_MONTSERRAT);
  assert(ajustes_legenda_sync_audio());
  { CatItem c; memset(&c, 0, sizeof c);
    snprintf(c.tipo, sizeof c.tipo, "movie");
    snprintf(c.titulo, sizeof c.titulo, "A Noite dos Espelhos");
    snprintf(c.backdrop, sizeof c.backdrop, "deploy/app/art/19.jpg");
    cat_definir(&c, 1); }
  player_abrir(0, NULL);
  player_erro_fonte(); player_limpar_erro_fonte();
  quadros(20);

  // Android com PCM: o backend do tap anunciou audio decodificado.
  audsync_backend(ligarTap); audsync_formato(AUDSYNC_FMT_PCM);
  faixas_abrir_em(1);
  quadros(20);
  legsync_primaria_externa(srt, "en", "OpenSubtitles");
  v = esperar(LEGSYNC_PRONTA);
  assert(v.acoes & LEGSYNC_ACAO_AUDIO);
  for (i = 0; i < 12; i++) tecla(SDLK_DOWN);
  tecla(SDLK_UP);
  tecla(SDLK_RIGHT); tecla(SDLK_RIGHT);        // Rapida, Completa, Por audio
  quadros(20);
  salvar(saida, "1-pronta-por-audio");
  tecla(SDLK_RETURN);                          // ouve as falas
  tocar(600, 690, 2.5, SINT_FALA | SINT_RUIDO);
  quadros(20);
  v = legsync_visao(0); assert(v.fase == LEGSYNC_OUVINDO);
  salvar(saida, "2-ouvindo");
  tocar(690, 600 + AUDSYNC_ALVO_SEG + 1, 2.5, SINT_FALA | SINT_RUIDO);
  v = esperar(LEGSYNC_ACEITA);
  printf("aceito por audio: %+d ms\n", v.offsetAutoMs);
  assert(v.audio && abs(v.offsetAutoMs - 2500) <= 100);
  quadros(30);
  salvar(saida, "3-aceita-por-audio");
  tecla(SDLK_RETURN);                          // Desfazer
  // Passthrough ligado (AC3/E-AC3/DTS em bitstream): motivo, nada desligado.
  audsync_formato(AUDSYNC_FMT_BITSTREAM);
  quadros(30);
  v = legsync_visao(0); assert(!(v.acoes & LEGSYNC_ACAO_AUDIO) && v.motivoAudio == LEGSYNC_M_AUD_PASSTHROUGH);
  salvar(saida, "4-passthrough");
  // Plataforma sem PCM (LG/Samsung): motivo.
  audsync_backend(NULL);
  quadros(30);
  salvar(saida, "5-plataforma");
  puts("audsync_shot: ok");
  return 0;
}
