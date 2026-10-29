// Captures of the F04 subtitle selector and of the two subtitles on screen
// (legendasui.c + legenda2.c through the real player draw path). GL window,
// not part of the suite; the pixel checks below keep it honest:
//
//   bash tests/legendas_shot.sh /Volumes/ExternalSSD/nv-f04-shots
//
// Frames: simple view, More options, dual subtitles without and with the
// controls, and a three-line primary to show the bands do not collide.
#include "catalogo.h"
#include "nvtmp.h"
#include "player.h"
#include "ajustes.h"
#include "faixas.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "episodios.h"
#include "streams.h"
#include "plrilha.h"
#include "video.h"
#include "addons.h"
#include "legenda.h"
#include "legenda2.h"
#include "legendasui.h"
#include "linguas.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static GLuint fbo, fboTex;
static int LW = 1920, LH = 1080;
static const char *saida;
static unsigned char quadro[1920 * 1080 * 3];

static void ler(void) {
  glFinish(); glBindFramebuffer(GL_FRAMEBUFFER, fbo); glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, LW, LH, GL_RGB, GL_UNSIGNED_BYTE, quadro);
}
static void salvar(const char *id) {
  SDL_Surface *s = SDL_CreateRGBSurface(0, LW, LH, 24, 0xff, 0xff00, 0xff0000, 0);
  char nome[700];
  int y;
  ler();
  for (y = 0; y < LH; y++)
    memcpy((unsigned char *)s->pixels + y * s->pitch, quadro + (size_t)(LH - 1 - y) * LW * 3, (size_t)LW * 3);
  snprintf(nome, sizeof nome, "%s/%s.bmp", saida, id);
  assert(SDL_SaveBMP(s, nome) == 0);
  SDL_FreeSurface(s);
  fprintf(stderr, "captura: %s\n", nome);
}
// Near-white pixels in a screen rectangle (top-left origin), from the last read.
static int brancos(int x0, int y0, int x1, int y1) {
  int x, y, n = 0;
  for (y = y0; y < y1; y++)
    for (x = x0; x < x1; x++) {
      const unsigned char *p = quadro + ((size_t)(LH - 1 - y) * LW + x) * 3;
      if (p[0] > 235 && p[1] > 235 && p[2] > 235) n++;
    }
  return n;
}

static Uint32 relogio = 100000;
static void quadros(int n) {
  int i;
  for (i = 0; i < n; i++) {
    relogio += 16;
    SDL_PumpEvents(); txt_novo_quadro(); tex_novo_quadro(); tex_bombear(8);
    player_atualizar(1.f / 60, relogio);
    episodios_atualizar(1.f / 60);
    stream_folha_atualizar(1.f / 60, relogio); faixas_atualizar(1.f / 60, relogio);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo); glViewport(0, 0, LW, LH);
    glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
    player_desenhar(relogio);
    episodios_desenhar();
    stream_folha_desenhar(relogio);
    faixas_desenhar(relogio);
    plrilha_desenhar(relogio);
    SDL_Delay(1);
  }
}
static void tecla(SDL_Keycode k) {
  SDL_Event ev; memset(&ev, 0, sizeof ev);
  ev.type = SDL_KEYDOWN; ev.key.keysym.sym = k; faixas_evento(&ev);
  ev.type = SDL_KEYUP; faixas_evento(&ev);
  quadros(3);
}

static void faixa(VideoFaixa *f, int numero, const char *rot, const char *idioma, const char *codec, int letreiro) {
  memset(f, 0, sizeof *f);
  f->numero = numero; f->ordinalMkv = -1; f->letreiro = letreiro;
  snprintf(f->rotulo, sizeof f->rotulo, "%s", rot);
  snprintf(f->idioma, sizeof f->idioma, "%s", idioma);
  snprintf(f->codec, sizeof f->codec, "%s", codec);
}
static void addon(Legenda *l, const char *idioma, const char *nome, const char *prov, const char *arquivo, const char *url) {
  memset(l, 0, sizeof *l);
  snprintf(l->idioma, sizeof l->idioma, "%s", idioma);
  snprintf(l->provedor, sizeof l->provedor, "%s", prov);
  snprintf(l->arquivo, sizeof l->arquivo, "%s", arquivo);
  snprintf(l->url, sizeof l->url, "%s", url);
  snprintf(l->rotulo, sizeof l->rotulo, "%s%s%.36s", nome, arquivo[0] ? "  \xc2\xb7  " : "", arquivo);
}

static const char *SRT_EN =
  "1\n00:00:08,000 --> 00:00:30,000\nWe never planned to come back here.\nNot after what happened in the valley.\n\n";
static int baixar(const char *url, long maxBytes, unsigned prazoMs,
                  int (*parar)(void *), void *u, char **corpo, long *n) {
  (void)url; (void)maxBytes; (void)prazoMs; (void)parar; (void)u;
  *n = (long)strlen(SRT_EN);
  *corpo = malloc((size_t)*n + 1);
  memcpy(*corpo, SRT_EN, (size_t)*n + 1);
  return 0;
}

static void estado(int controles) {
  VideoSimulacao v;
  memset(&v, 0, sizeof v);
  v.largura = 3840; v.altura = 2160; v.pronto = 1; v.pos = 12.0; v.duracao = 3360.0;
  v.nAudio = 1;
  faixa(&v.audio[0], 1, "Ingl\xc3\xaas  \xc2\xb7  E-AC3 \xc2\xb7 5.1", "en", "", 0);
  v.nLeg = 7; v.legAtual = -1;
  // #287: full + forced + signs of the same language, and an SDH one: the
  // simple list says which is which next to "Embutida".
  faixa(&v.leg[0], 3, "Portugu\xc3\xaas (Brasil)", "pt", "S_TEXT/UTF8", 0);
  faixa(&v.leg[1], 4, "Portugu\xc3\xaas  \xc2\xb7  For\xc3\xa7" "ada", "pt", "S_TEXT/UTF8", 1);
  v.leg[1].tipoLeg = LING_LEG_FORCADA;
  faixa(&v.leg[2], 5, "Portugu\xc3\xaas  \xc2\xb7  Signs & Songs", "pt", "S_TEXT/ASS", 1);
  v.leg[2].tipoLeg = LING_LEG_LETREIROS;
  faixa(&v.leg[3], 6, "English  \xc2\xb7  English SDH", "en", "S_TEXT/UTF8", 0);
  v.leg[3].tipoLeg = LING_LEG_SDH;
  faixa(&v.leg[4], 7, "Espa\xc3\xb1ol", "es", "S_HDMV/PGS", 0);
  // #511: anime-style tracks of one language told apart by their names.
  faixa(&v.leg[5], 8, "English  \xc2\xb7  Full", "en", "S_TEXT/ASS", 0);
  faixa(&v.leg[6], 9, "English  \xc2\xb7  Full w/ honorifics", "en", "S_TEXT/ASS", 0);
  video_simular(&v);
  player_shot_estado(relogio, 12.0f, 3360.0f, 1, 0, 0, 0);
  if (!controles) player_shot_esconder();
}

int main(int argc, char **argv) {
  Legenda add[6];
  char id[24];
  int semSec, comSec;
  saida = argc > 1 ? argv[1] : nvt("/nv-legendas-shot");
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *w = SDL_CreateWindow("Nuvio: legendas", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN); assert(w);
  SDL_GLContext gl = SDL_GL_CreateContext(w); assert(gl); SDL_GL_SetSwapInterval(0);
  glGenTextures(1, &fboTex); glBindTexture(GL_TEXTURE_2D, fboTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, LW, LH, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glGenFramebuffers(1, &fbo); glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTex, 0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  glViewport(0, 0, LW, LH); gfx_tamanho_alvo(LW, LH); assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1)); tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");
  { char caminho[700]; FILE *f;
    snprintf(caminho, sizeof caminho, "%s/ajustes.txt", getenv("NUVIO_DADOS"));
    f = fopen(caminho, "w"); assert(f);
    // Main = Portugues (2), second = Ingles (3) in the language list.
    // Interface font Montserrat (3): the owner's TV.
    fprintf(f, "idioma 0\nselected_theme 2\nfonteInterface 3\nlegendaIdioma 2\nlegendaSecundariaIdioma 3\n");
    // R4: NUVIO_SHOT_LEG2="pos tam cor fundo borda" (indices de Ajustes) para ver a segunda legenda
    // empilhada e com estilo proprio; vazio = padrao (no topo, igual a principal).
    { const char *e = getenv("NUVIO_SHOT_LEG2"); int p2 = 0, t2 = 0, c2 = 0, f2 = 0, b2 = 0;
      if (e && sscanf(e, "%d %d %d %d %d", &p2, &t2, &c2, &f2, &b2) == 5)
        fprintf(f, "legenda2PosLocal %d\nlegenda2TamanhoLocal %d\nlegenda2CorLocal %d\nlegenda2FundoLocal %d\nlegenda2BordaLocal %d\n", p2, t2, c2, f2, b2); }
    fclose(f);
    ajustes_dir(getenv("NUVIO_DADOS")); }
  ajustes_iniciar();
  if (getenv("NUVIO_SHOT_VIDRO")) ajustes_definir_vidro(1);
  assert(!strcmp(ling_legenda(), "pt") && !strcmp(ling_legenda2(), "en"));
  assert(txt_fonte_interface() == TXT_FAMILIA_MONTSERRAT);
  { struct tm lt; time_t t = time(NULL);
    localtime_r(&t, &lt); lt.tm_hour = 20; lt.tm_min = 19; lt.tm_sec = 0;
    plrilha_shot_hora(mktime(&lt)); }
  { CatItem c; memset(&c, 0, sizeof c);
    snprintf(c.tipo, sizeof c.tipo, "movie");
    snprintf(c.titulo, sizeof c.titulo, "A Noite dos Espelhos");
    snprintf(c.backdrop, sizeof c.backdrop, "deploy/app/art/19.jpg");
    cat_definir(&c, 1); }
  player_abrir(0, NULL);
  player_erro_fonte(); player_limpar_erro_fonte();
  player_shot_video(1);
  estado(0);
  addon(&add[0], "pt", "Portugu\xc3\xaas", "OpenSubtitles", "Noite.dos.Espelhos.2026.1080p.WEB.srt", "https://h.invalid/1");
  addon(&add[1], "pt", "Portugu\xc3\xaas", "OpenSubtitles", "", "https://h.invalid/2");
  addon(&add[2], "pt", "Portugu\xc3\xaas", "SubDL", "Noite.dos.Espelhos.Fansub.ass", "https://h.invalid/3");
  addon(&add[3], "en", "Ingl\xc3\xaas", "OpenSubtitles", "Night.of.Mirrors.2026.WEB.srt", "https://h.invalid/4");
  addon(&add[4], "en", "Ingl\xc3\xaas", "Subs.ro", "", "https://h.invalid/5");
  addon(&add[5], "es", "Espanhol", "OpenSubtitles", "", "https://h.invalid/6");
  addons_shot_legendas(add, 6);
  quadros(20);
  // faixas_reiniciar ran in player_abrir; the session starts now.
  legenda2_definir_baixador(baixar);
  legenda_definir_corpo("1\n00:00:08,000 --> 00:00:30,000\nNunca pensamos em voltar aqui.\n\n");

  // Primary = the first OpenSubtitles pt file (as if chosen), second = en file.
  faixas_escolher_externa(&add[0]);
  legenda_definir_corpo("1\n00:00:08,000 --> 00:00:30,000\nNunca pensamos em voltar aqui.\n"
                        "N\xc3\xa3o depois do que aconteceu no vale.\n\n");
  estado(0); quadros(30);
  ler(); semSec = brancos(300, 30, 1620, 260);
  salvar("legendas-so-principal");
  legendasui_id_addon(&add[3], id);
  legenda2_escolher(id, add[3].url, "en", "OpenSubtitles");
  { int i; for (i = 0; i < 200 && legenda2_estado() == LEG2_CARREGANDO; i++) usleep(5000); }
  assert(legenda2_estado() == LEG2_ATIVA);
  estado(0); quadros(30);
  ler(); comSec = brancos(300, 30, 1620, 260);
  salvar("legendas-duas-sem-controles");
  fprintf(stderr, "top band white pixels: without second %d, with second %d\n", semSec, comSec);
  if (!ajustes_leg2_junto()) assert(comSec > semSec + 2000);   // the second document is really drawn in the top band
  { int prim = brancos(200, 700, 1720, 1060);
    fprintf(stderr, "bottom band white pixels (primary): %d\n", prim);
    assert(prim > 2000); }         // and the primary is still there, at the bottom

  estado(1); quadros(30);
  salvar("legendas-duas-com-controles");
  // A three-line primary at the same time: separate bands, no overlap.
  legenda_definir_corpo("1\n00:00:08,000 --> 00:00:30,000\nNunca pensamos em voltar aqui.\n"
                        "N\xc3\xa3o depois do que aconteceu no vale,\nnem depois daquela noite.\n\n");
  estado(0); quadros(30);
  salvar("legendas-duas-principal-3-linhas");

  // The selector: simple view, then More options.
  estado(1); quadros(10);
  faixas_abrir_em(1);
  quadros(60);
  salvar("legendas-seletor-simples");
  salvar("painel-depois-curta");
  salvar("painel-v2-lista");
  // Hold OK on the row: the dropdown opens below it (#425 v2). Right goes to Estilo.
  { SDL_Event ev; memset(&ev, 0, sizeof ev);
    ev.type = SDL_KEYDOWN; ev.key.keysym.sym = SDLK_RETURN; faixas_evento(&ev);
    SDL_Delay(780); quadros(30);
    ev.type = SDL_KEYUP; faixas_evento(&ev); quadros(30); }
  salvar("painel-depois-menu");
  salvar("painel-v2-menu");
  tecla(SDLK_DOWN); tecla(SDLK_DOWN); tecla(SDLK_RETURN);
  quadros(30); assert(legendasui_versoes());
  salvar("legendas-seletor-versoes");
  tecla(SDLK_ESCAPE); quadros(20); assert(!legendasui_versoes());
  tecla(SDLK_RIGHT); quadros(40); salvar("painel-v2-estilo");
  // Estilo tab controls (owner, 11/10): each opens in place, same panel height.
  { VideoLegendaEstilo guardado = *player_leg_estilo();
    float h0 = legendasui_altura();
    quadros(60); salvar("estilo-grade");
    tecla(SDLK_RETURN); quadros(60); assert(faixas_estilo_controle() == 0);          // Tamanho
    tecla(SDLK_RIGHT); tecla(SDLK_RIGHT); tecla(SDLK_RIGHT); quadros(40);
    assert(player_leg_estilo()->tamanho == guardado.tamanho + 30);
    salvar("estilo-slider-tamanho");
    tecla(SDLK_ESCAPE); quadros(30); assert(faixas_estilo_controle() < 0 && faixas_estilo_topo());
    tecla(SDLK_DOWN); tecla(SDLK_RETURN); quadros(60); assert(faixas_estilo_controle() == 5);   // Posicao
    tecla(SDLK_RIGHT); tecla(SDLK_RIGHT); quadros(40);
    salvar("estilo-posicao");
    tecla(SDLK_RETURN); tecla(SDLK_UP); tecla(SDLK_RIGHT); tecla(SDLK_RIGHT);               // item 2, Cor
    tecla(SDLK_RETURN); quadros(60); assert(faixas_estilo_controle() == 2);
    tecla(SDLK_RIGHT); tecla(SDLK_RIGHT); quadros(40);
    salvar("estilo-cores");
    tecla(SDLK_RETURN); tecla(SDLK_LEFT);                                                   // item 1, Fonte
    tecla(SDLK_RETURN); quadros(60); assert(faixas_estilo_controle() == 1);
    tecla(SDLK_RIGHT); tecla(SDLK_DOWN); quadros(40);
    salvar("estilo-fontes");
    tecla(SDLK_RETURN); quadros(20);
    *player_leg_estilo() = guardado; player_leg_estilo_mudou();
    tecla(SDLK_LEFT); tecla(SDLK_LEFT); quadros(20); (void)h0; }
  { int i; for (i = 0; i < 12; i++) tecla(SDLK_DOWN); }   // last row: Mais opcoes
  tecla(SDLK_RETURN);
  quadros(40);
  assert(legendasui_mais());
  salvar("legendas-mais-opcoes");
  salvar("painel-depois-todas");
  { int i; for (i = 0; i < 9; i++) tecla(SDLK_DOWN); }   // #511: the English group, names told apart
  quadros(30); salvar("legendas-nomes-ingles");
  tecla(SDLK_RIGHT);                                      // aim at the second slot? no: candidate row -> Estilo
  tecla(SDLK_LEFT);
  tecla(SDLK_UP); tecla(SDLK_UP); tecla(SDLK_UP);         // to the "Usar como" row
  tecla(SDLK_RIGHT);                                      // Secundaria
  quadros(30);
  salvar("legendas-mais-opcoes-secundaria");
  { int i; for (i = 0; i < 8; i++) tecla(SDLK_DOWN); }   // the addon files, with details
  quadros(30);
  salvar("legendas-mais-opcoes-addons");
  // Read the real persisted preference, including both manual-delay endpoints.
  player_dir(getenv("NUVIO_DADOS"));
  { char nome[700]; const int offsets[] = { 60000, -60000, 15000, 60001 };
    for (int j = 0; j < 4; j++) {
      snprintf(nome, sizeof nome, "%s/player.txt", getenv("NUVIO_DADOS"));
      FILE *f = fopen(nome, "w"); assert(f); fprintf(f, "leg_atraso %d\n", offsets[j]); fclose(f);
      player_leg_estilo()->atrasoMs = 0;
      player_abrir(0, NULL);
      assert(player_leg_estilo()->atrasoMs == (j == 3 ? 0 : offsets[j]));
    }
  }
  legenda2_encerrar();
  puts("legendas_shot: captures and ±60 s preference reload OK");
  return 0;
}
