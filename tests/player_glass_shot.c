// CAPTURAS DO PLAYER NO GLASS UI, quadro a quadro, para comparar lado a lado
// com o mockup aprovado em 03/10 (player-mockup.html, um PNG por quadro e
// material). Fora da suite: janela GL e olho humano.
//
//   bash tests/player_glass_shot.sh <dir> [quadro...]
//
// Cada quadro sai em <dir>/<id>-vidro.bmp ou <id>-solido.bmp, conforme
// NUVIO_SHOT_VIDRO (1 = Glass UI ligado). NUVIO_SHOT_MOCK aponta para a pasta
// do mockup (as artes img/bd, img/lg, img/ep dele entram como o "video"); sem
// ela, as artes do pacote. A hora fica em 20:19, como no mockup.
//
// O que e de rede (Seekr, intro, guia parental) entra por gancho de captura
// (NV_SHOT_HOOKS): nada aqui consulta servico nem gasta cota.
#include "catalogo.h"
#include "nvtmp.h"
#include "player.h"
#include "idioma.h"
#include "pausao.h"
#include "ajustes.h"
#include "faixas.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "episodios.h"
#include "streams.h"
#include "plrilha.h"
#include "intro.h"
#include "video.h"
#include "cacheboost.h"
#include "extras.h"
#include "badges.h"
#include "seekr.h"
#include "parental.h"
#include "vistoep.h"
#include "progresso.h"
#include "posplay.h"
#include "reacao.h"
#include "trailer.h"
#include "guialembrete.h"
#include "aovivo.h"
#include "ilha.h"
#include "ondever.h"
#include "estatvivoui.h"
void ponteiro_teste_toque(int ligado);
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static GLuint fbo, fboTex;
static int LW = 1920, LH = 1080;
static const char *saida, *material;
static char mock[600];

static void salvar(const char *id) {
  char nome[800];
  SDL_Surface *s = SDL_CreateRGBSurface(0, LW, LH, 24, 0xff, 0xff00, 0xff0000, 0);
  int y; unsigned char *p, *t;
  glFinish(); glBindFramebuffer(GL_FRAMEBUFFER, fbo); glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, LW, LH, GL_RGB, GL_UNSIGNED_BYTE, s->pixels);
  p = s->pixels; t = malloc((size_t)s->pitch);
  for (y = 0; y < LH / 2; y++) {
    memcpy(t, p + y * s->pitch, (size_t)s->pitch);
    memcpy(p + y * s->pitch, p + (LH - 1 - y) * s->pitch, (size_t)s->pitch);
    memcpy(p + (LH - 1 - y) * s->pitch, t, (size_t)s->pitch);
  }
  free(t);
  snprintf(nome, sizeof nome, "%s/%s-%s.bmp", saida, id, material);
  assert(SDL_SaveBMP(s, nome) == 0);
  SDL_FreeSurface(s);
  fprintf(stderr, "captura: %s\n", nome);
}

static Uint32 relogio = 100000;
// Telas fora do player (trailer, ao vivo, home): desenhadas por cima.
static void (*extra)(void);
// Fundo desenhado ANTES do player (o video claro/escuro por tras do anel).
static void (*fundo)(void);
static void estatsDesenhar(void) { estatvivo_desenhar(relogio); }
#ifndef FOSCO_ZERA
#define FOSCO_ZERA() ((void)0)
#endif
#ifndef FOSCO_N
#define FOSCO_N(id) ((void)0)
#endif
static void fundoClaro(void) { gfx_cor((GfxRect){ 0, 0, 1920, 1080 }, 0, 0.92f, 0.90f, 0.82f, 1.0f); }
static void fundoEscuro(void) { gfx_cor((GfxRect){ 0, 0, 1920, 1080 }, 0, 0.03f, 0.03f, 0.04f, 1.0f); }
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
    if (fundo) fundo();
    gfx_hdr_ui = player_ui_hdr();   // como app.c: UI em HDR (esmaecer.h)
    player_desenhar(relogio);
    episodios_desenhar();
    stream_folha_desenhar(relogio);
    faixas_desenhar(relogio);
    plrilha_desenhar(relogio);
    gfx_hdr_ui = 1.0f;
    if (extra) extra();
    SDL_Delay(1);
  }
}

static const char *img(const char *rel) {
  static char b[8][700];
  static int k;
  k = (k + 1) % 8;
  if (mock[0]) snprintf(b[k], sizeof b[k], "%s/%s", mock, rel);
  else snprintf(b[k], sizeof b[k], "deploy/app/art/19.jpg");
  return b[k];
}

static CatItem filme, serie;
static void titulos(void) {
  memset(&filme, 0, sizeof filme);
  snprintf(filme.tipo, sizeof filme.tipo, "movie");
  snprintf(filme.titulo, sizeof filme.titulo, "Project Hail Mary");
  snprintf(filme.backdrop, sizeof filme.backdrop, "%s", img("img/bd/13.jpg"));
  snprintf(filme.logo, sizeof filme.logo, "%s", img("img/lg/13.png"));
  { const char *lg = getenv("NUVIO_SHOT_LOGO");   // PNG transparente: o logo do titulo
    if (lg && *lg) snprintf(filme.logo, sizeof filme.logo, "%s", lg); }
  snprintf(filme.meta, sizeof filme.meta, "2026 \xc2\xb7 2h 37min \xc2\xb7 Aventura \xc2\xb7 Com\xc3\xa9" "dia \xc2\xb7 12");
  memset(&serie, 0, sizeof serie);
  snprintf(serie.tipo, sizeof serie.tipo, "series");
  snprintf(serie.titulo, sizeof serie.titulo, "Fallout");
  snprintf(serie.backdrop, sizeof serie.backdrop, "%s", img("img/bd/00.jpg"));
  snprintf(serie.logo, sizeof serie.logo, "%s", img("img/lg/00.png"));
  snprintf(serie.meta, sizeof serie.meta, "2024 \xc2\xb7 56 min \xc2\xb7 A\xc3\xa7\xc3\xa3o \xc2\xb7 Aventura \xc2\xb7 14");
  snprintf(serie.nomeEpisodio, sizeof serie.nomeEpisodio, "The Head");
  serie.temporada = 1; serie.episodio = 3;
}

static void abrir(const CatItem *c) {
  CatItem lista[1];
  lista[0] = *c;
  cat_definir(lista, 1);
  player_abrir(0, NULL);
  if (!strcmp(c->tipo, "series")) player_definir_episodio(c->temporada, c->episodio);
  player_erro_fonte(); player_limpar_erro_fonte();
}

static void simular(int largura, int altura, const char *hdr, int dv, int atmos) {
  VideoSimulacao v;
  memset(&v, 0, sizeof v);
  v.largura = largura; v.altura = altura; v.dv = dv; v.atmos = atmos;
  snprintf(v.hdr, sizeof v.hdr, "%s", hdr ? hdr : "");
  video_simular(&v);
}


static void faixa(VideoFaixa *f, const char *rot, const char *idioma, const char *codec) {
  memset(f, 0, sizeof *f);
  snprintf(f->rotulo, sizeof f->rotulo, "%s", rot);
  snprintf(f->idioma, sizeof f->idioma, "%s", idioma);
  snprintf(f->codec, sizeof f->codec, "%s", codec);
  f->ordinalMkv = -1;
}
// O filme com as faixas do mockup (4 de audio, 6 de legenda).
static void simularFaixas(void) {
  VideoSimulacao v;
  memset(&v, 0, sizeof v);
  v.largura = 3840; v.altura = 1606; v.dv = 1; v.atmos = 1;
  v.nAudio = 4; v.audioAtual = 0;
  faixa(&v.audio[0], "Ingl\xc3\xaas  \xc2\xb7  Dolby Atmos \xc2\xb7 TrueHD \xc2\xb7 7.1", "en", "");
  faixa(&v.audio[1], "Portugu\xc3\xaas (Brasil)  \xc2\xb7  E-AC3 \xc2\xb7 5.1", "pt", "");
  faixa(&v.audio[2], "Espanhol (Am\xc3\xa9rica Latina)  \xc2\xb7  E-AC3 \xc2\xb7 5.1", "es", "");
  faixa(&v.audio[3], "Ingl\xc3\xaas \xe2\x80\x94 Coment\xc3\xa1rio do diretor  \xc2\xb7  AAC \xc2\xb7 2.0", "en", "");
  v.nLeg = 6; v.legAtual = 0;
  faixa(&v.leg[0], "Portugu\xc3\xaas (Brasil)", "pt", "S_TEXT/UTF8");
  faixa(&v.leg[1], "Portugu\xc3\xaas (Brasil) \xe2\x80\x94 Letreiros", "pt", "S_TEXT/UTF8");
  faixa(&v.leg[2], "Ingl\xc3\xaas", "en", "S_TEXT/UTF8");
  faixa(&v.leg[3], "Ingl\xc3\xaas \xe2\x80\x94 SDH", "en", "S_TEXT/UTF8");
  faixa(&v.leg[4], "Espanhol", "es", "S_TEXT/UTF8");
  faixa(&v.leg[5], "Franc\xc3\xaas", "fr", "S_TEXT/UTF8");
  video_simular(&v);
}
static void teclaFaixas(SDL_Keycode k) {
  SDL_Event ev; memset(&ev, 0, sizeof ev);
  ev.type = SDL_KEYDOWN; ev.key.keysym.sym = k; faixas_evento(&ev);
  quadros(2);
}


// Uma arte do mockup como textura GL (as miniaturas do Seekr da captura).
static GLuint texDe(const char *rel) {
  SDL_Surface *sf = IMG_Load(img(rel)), *t;
  GLuint tex = 0;
  if (!sf) return 0;
  t = SDL_ConvertSurfaceFormat(sf, SDL_PIXELFORMAT_ABGR8888, 0);
  SDL_FreeSurface(sf);
  if (!t) return 0;
  glGenTextures(1, &tex); glBindTexture(GL_TEXTURE_2D, tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, t->pitch / 4);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, t->w, t->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, t->pixels);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  SDL_FreeSurface(t);
  return tex;
}

static CatItem serieComEps(void) {
    static const char *nm[8] = { "The End", "The Target", "The Head", "The Ghouls", "The Past", "The Trap", "The Radio", "The Beginning" };
    static const char *sn[8] = { "Lucy deixa o Ref\xc3\xbagio 33 pela primeira vez, atr\xc3\xa1s do pai levado na invas\xc3\xa3o.",
      "Na superf\xc3\xad" "cie, Lucy descobre que a Wasteland tem as pr\xc3\xb3prias regras.",
      "Lucy, Maximus e o Ghoul disputam a mesma recompensa no deserto.",
      "Um acordo com o Ghoul cobra um pre\xc3\xa7o que Lucy n\xc3\xa3o esperava.",
      "Em Filly, cada um enfrenta o que deixou para tr\xc3\xa1s.",
      "No Ref\xc3\xbagio 32, Norm encontra o que ningu\xc3\xa9m deveria ver.",
      "Uma transmiss\xc3\xa3o muda o rumo de todos os que est\xc3\xa3o na estrada.",
      "Griffith Observatory guarda a resposta que Lucy procurava." };
    CatEp eps[16];
  CatItem f = serie;
    int i;
    snprintf(f.imdb, sizeof f.imdb, "tt99999990");
    f.nTemporadas = 2; f.temporadas[0] = 1; f.temporadas[1] = 2;
    memset(eps, 0, sizeof eps);
    for (i = 0; i < 16; i++) {
      char r[40];
      eps[i].temporada = i < 8 ? 1 : 2; eps[i].episodio = i % 8 + 1;
      snprintf(eps[i].nome, sizeof eps[i].nome, "%s", nm[i % 8]);
      snprintf(eps[i].sinopse, sizeof eps[i].sinopse, "%s", sn[i % 8]);
      snprintf(eps[i].duracao, sizeof eps[i].duracao, "56 min");
      snprintf(eps[i].data, sizeof eps[i].data, "10 de abril de 2024");
      snprintf(r, sizeof r, "img/ep/00_1_0%d.jpg", i % 8 + 1);
      snprintf(eps[i].thumb, sizeof eps[i].thumb, "%s", img(r));
    }
    { CatItem lista[1]; lista[0] = f; cat_definir(lista, 1); }
    cat_definir_episodios(0, eps, 16);
    vistoep_definir(f.imdb, 1, 1, 1); vistoep_definir(f.imdb, 1, 2, 1);
    prog_gravar_local(f.imdb, 1, 3, 1948.0, 3360.0, -1);
    prog_gravar_local(f.imdb, 1, 5, 1210.0, 3360.0, -1);   // um nao assistido pela metade
    return f;
}

// Uma arte do mockup em tela cheia, como o video atras.
static void arteCheia(const char *rel, float escuro) {
  const char *u = img(rel);
  GLuint t = tex_obter_hero(u);
  if (t) { gfx_tex_aspect_atual = tex_aspecto(u);
           gfx_rect((GfxRect){ 0, 0, 1920, 1080 }, t, GFX_CARD, 0, 0, 0, 0, 0, 0, 0, 1);
           gfx_tex_aspect_atual = 0.0f; }
  if (escuro > 0.0f) gfx_cor((GfxRect){ 0, 0, 1920, 1080 }, 0, 0, 0, 0, escuro);
}
static void desenhaTrailer(void) {
  arteCheia("img/bd/03.jpg", 0.0f);
  trailer_osd_desenhar("One Battle After Another", 1.0f);
}

static void abrirCanal(int atras, int foco, int info, int numero, const char *nome, const char *prog,
                       const char *bd) {
  CatItem c;
  AoVivoEpg e;
  time_t t = time(NULL);
  struct tm lt;
  memset(&c, 0, sizeof c);
  snprintf(c.tipo, sizeof c.tipo, "tv");
  snprintf(c.titulo, sizeof c.titulo, "%s", nome);
  snprintf(c.imdb, sizeof c.imdb, "canal:%d", numero);
  snprintf(c.backdrop, sizeof c.backdrop, "%s", img(bd));
  snprintf(c.sinopse, sizeof c.sinopse, "Transmiss\xc3\xa3o ao vivo dos principais eventos esportivos do dia, com coment\xc3\xa1rios e reportagens.");
  { CatItem l[1]; l[0] = c; cat_definir(l, 1); }
  player_marcar_canal(&c);
  player_abrir(0, NULL);
  player_erro_fonte(); player_limpar_erro_fonte();
  { VideoSimulacao v; memset(&v, 0, sizeof v);
    v.largura = 1920; v.altura = 1080; v.pronto = 1; video_simular(&v); }
  player_shot_video(1);
  memset(&e, 0, sizeof e);
  localtime_r(&t, &lt); lt.tm_hour = 23; lt.tm_min = 18; lt.tm_sec = 0;
  e.temAgora = 1; e.agoraIni = mktime(&lt); e.agoraFim = e.agoraIni + 120 * 60;
  e.temProx = 1; e.proxIni = e.agoraFim;
  e.progresso = 0.58f;
  snprintf(e.agoraTit, sizeof e.agoraTit, "%s", prog);
  snprintf(e.proxTit, sizeof e.proxTit, "Bate-bola: Os melhores momentos");
  player_shot_canal(&e, numero, atras, foco, info);
  { struct tm h; localtime_r(&t, &h); h.tm_hour = 23; h.tm_min = 51; h.tm_sec = 0;
    plrilha_shot_hora(mktime(&h)); }
}

static void desenhaMini(void) {
  arteCheia("img/bd/15.jpg", 0.0f);
  gfx_veu_css((GfxRect){ 0, 0, 1920, 1080 }, 2, 1.0f, 0.7f, 0.80f);
  gfx_veu_css((GfxRect){ 0, 0, 1920, 1080 }, 0, 1.0f, 0.55f, 0.90f);
  player_mini_desenhar(relogio);
  glem_desenhar(relogio);
}
static void desenhaZap(void) {
  AoVivoBanner b;
  memset(&b, 0, sizeof b);
  b.nome = "Globo News"; b.logo = ""; b.agoraTit = "Em Foco com Andr\xc3\xa9ia Sadi";
  b.numero = 13; b.salto = 1;
  arteCheia("img/bd/21.jpg", 0.0f);
  aovivo_banner_desenhar(&b, 1.0f);
}

static void desenhaHomeIlha(void) {
  arteCheia("img/bd/15.jpg", 0.0f);
  gfx_veu_css((GfxRect){ 0, 0, 1920, 1080 }, 2, 1.0f, 0.7f, 0.80f);
  gfx_veu_css((GfxRect){ 0, 0, 1920, 1080 }, 0, 1.0f, 0.55f, 0.90f);
  ilha_relogio_visivel(1);
  ilha_posicionar(0);
  ilha_desenhar(relogio);
}
static void cartaoVivo(void) {
  IlhaCartao v;
  memset(&v, 0, sizeof v);
  snprintf(v.chave, sizeof v.chave, "vivo:tt99999990:1:3");
  snprintf(v.imdb, sizeof v.imdb, "tt99999990");
  v.serie = 1; v.t = 1; v.e = 3;
  snprintf(v.titulo, sizeof v.titulo, "Fallout");
  snprintf(v.epNome, sizeof v.epNome, "The Head");
  snprintf(v.sinopse, sizeof v.sinopse, "Lucy, Maximus e o Ghoul disputam a mesma recompensa no deserto.");
  snprintf(v.poster, sizeof v.poster, "%s", img("img/po/00.jpg"));
  snprintf(v.logo, sizeof v.logo, "%s", img("img/lg/00.png"));
  snprintf(v.arte, sizeof v.arte, "%s", img("img/ep/00_1_03.jpg"));
  v.progresso = 0.58f; v.restanteMin = 24;
  ilha_cartao(ILHA_VIVO, &v);
}

static int quer(int argc, char **argv, const char *id) {
  int i;
  if (argc < 3) return 1;
  for (i = 2; i < argc; i++) if (!strcmp(argv[i], id)) return 1;
  return 0;
}

int main(int argc, char **argv) {
  saida = argc > 1 ? argv[1] : nvt("/nv-player-glass");
  { const char *v = getenv("NUVIO_SHOT_VIDRO"); material = v && *v == '1' ? "vidro" : "solido"; }
  { const char *m = getenv("NUVIO_SHOT_MOCK"); snprintf(mock, sizeof mock, "%s", m ? m : ""); }
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *w = SDL_CreateWindow("Nuvio: player glass", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN); assert(w);
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
    fprintf(f, "idioma 0\nselected_theme 2\n");
    { const char *fo = getenv("NUVIO_SHOT_FONTE");   // 3 = Montserrat, a da TV do dono
      if (fo && *fo) fprintf(f, "fonteInterface %d\n", atoi(fo)); }
    fclose(f);
    ajustes_dir(getenv("NUVIO_DADOS")); }
  ajustes_iniciar();
  if (!strcmp(material, "vidro")) ajustes_definir_vidro(1);
  ajustes_teste_vidro_env();   // NUVIO_SHOT_VIDRO_OPAC / _FOSCO
  { struct tm lt; time_t t = time(NULL);
    localtime_r(&t, &lt); lt.tm_hour = 20; lt.tm_min = 19; lt.tm_sec = 0;
    plrilha_shot_hora(mktime(&lt)); }
  titulos();
  { static const char *t[8] = { "The Martian", "Interstellar", "Arrival", "Gravity",
                                "Ad Astra", "Moon", "Sunshine", "Contact" };
    static const char *an[8] = { "2015", "2014", "2016", "2013", "2019", "2009", "2007", "1997" };
    static const char *po[8] = { "img/po/12.jpg", "img/po/10.jpg", "img/po/16.jpg", "img/po/14.jpg",
                                 "img/po/32.jpg", "img/po/03.jpg", "img/po/36.jpg", "img/po/02.jpg" };
    const char *pc[8];
    int i;
    for (i = 0; i < 8; i++) pc[i] = img(po[i]);
    extras_shot_relacionados(t, an, pc, 8); }

  if (quer(argc, argv, "osd")) {
    abrir(&filme); simular(3840, 1606, "", 1, 1);
    quadros(10);
    player_shot_estado(relogio, 4360.0f, 9420.0f, 1, 0, 0, 0);
    quadros(60);
    salvar("osd");
  }
  // O OSD sobre video 2.39:1 de verdade (#fpsplayer, S90D): furo alfa 0 com as
  // faixas pretas em cima e embaixo, como o .tpk desenha.
  if (quer(argc, argv, "osd-furo")) {
    VideoSimulacao v;
    memset(&v, 0, sizeof v);
    v.largura = 3840; v.altura = 1606; v.pronto = 1;
    abrir(&filme); video_simular(&v);
    player_shot_video(1); player_shot_furo_real();
    quadros(10);
    player_shot_estado(relogio, 4360.0f, 9420.0f, 1, 0, 0, 0);
    quadros(60);
    { int i;   // tocando: a barra de progresso anda todo quadro
      for (i = 0; i < 60; i++) { player_shot_estado(relogio, 4361.0f + i / 60.0f, 9420.0f, 1, 0, 0, 0); quadros(1); } }
    salvar("osd-furo");
    player_shot_video(0); simular(0, 0, "", 0, 0);
  }
  // FROST OVER LIVE VIDEO (owner, 16/10): 16:9 video fills the hole, so every
  // player glass sits over it. FOSCO_N (tests/player_fosco.c) prints how many
  // glasses had a blur source in the last frame of each scene.
  if (quer(argc, argv, "fosco-osd") || quer(argc, argv, "fosco-audio") || quer(argc, argv, "fosco-stats")) {
    VideoSimulacao v;
    memset(&v, 0, sizeof v);
    v.largura = 3840; v.altura = 2160; v.pronto = 1;
    abrir(&filme); video_simular(&v);
    player_shot_video(1); player_shot_furo_real();
    quadros(10);
    player_shot_estado(relogio, 4360.0f, 9420.0f, 1, 0, 0, 0);
    FOSCO_ZERA();
    quadros(60);
    if (quer(argc, argv, "fosco-osd")) { salvar("fosco-osd"); FOSCO_N("fosco-osd"); }
    if (quer(argc, argv, "fosco-audio")) {
      player_shot_esconder();
      faixas_abrir_em(0);
      FOSCO_ZERA();
      quadros(90);
      salvar("fosco-audio"); FOSCO_N("fosco-audio");
      faixas_evento(&(SDL_Event){ .key = { .type = SDL_KEYDOWN, .keysym = { .sym = SDLK_ESCAPE } } });
      quadros(30);
    }
    if (quer(argc, argv, "fosco-stats")) {
      player_shot_esconder();
      estatvivo_abrir(); extra = estatsDesenhar;
      FOSCO_ZERA();
      quadros(60);
      salvar("fosco-stats"); FOSCO_N("fosco-stats");
      extra = NULL; estatvivo_fechar();
    }
    player_shot_video(0); simular(0, 0, "", 0, 0);
  }
  // UI EM HDR (#205): o mesmo OSD com o video SDR e com Dolby Vision.
  if (quer(argc, argv, "osd-sdr") || quer(argc, argv, "osd-hdr")) {
    int k;
    for (k = 0; k < 2; k++) {
      if (!quer(argc, argv, k ? "osd-hdr" : "osd-sdr")) continue;
      abrir(&filme); simular(3840, 1606, "", k, 1);
      quadros(10);
      player_shot_estado(relogio, 4360.0f, 9420.0f, 1, 0, 0, 0);
      quadros(60);
      salvar(k ? "osd-hdr" : "osd-sdr");
    }
  }
  if (quer(argc, argv, "osd-serie")) {
    IntroTrecho tr[2] = { { 67.2, 235.2, INTRO_ABERTURA }, { 3124.8, 0.0, INTRO_CREDITOS } };
    abrir(&serie); simular(3840, 2160, "HDR10", 0, 1);
    intro_shot_definir(tr, 2);
    quadros(10);
    player_shot_estado(relogio, 1948.0f, 3360.0f, 1, 5, 0, 0);
    quadros(60);
    salvar("osd-serie");
  }
  if (quer(argc, argv, "osd-barra")) {
    abrir(&filme); simular(3840, 1606, "", 1, 1);
    quadros(10);
    player_shot_estado(relogio, 4820.0f, 9420.0f, 1, 0, 1, 1);
    quadros(60);
    salvar("osd-barra");
  }
  // Foco na barra com o OSD inteiro (pedido do dono, 03/10): a fileira de
  // botoes fecha; o titulo, a barra e o tempo ficam.
  if (quer(argc, argv, "osd-barra-foco")) {
    abrir(&filme); simular(3840, 1606, "", 1, 1);
    quadros(10);
    player_shot_estado(relogio, 4820.0f, 9420.0f, 1, 0, 1, 0);
    quadros(60);
    salvar("osd-barra-foco");
  }
  // W19: botao focado e CIMA para a barra, com ponteiro/toque ativo (a fileira
  // fica de pe). Antes: pilula larga sem texto. Depois: icone so.
  if (quer(argc, argv, "osd-botao-sobe")) {
    ponteiro_teste_toque(1);
    abrir(&filme); simular(3840, 1606, "", 1, 1);
    quadros(10);
    player_shot_estado(relogio, 4820.0f, 9420.0f, 1, 2, 0, 0);
    quadros(60);
    salvar("osd-botao-foco");
    player_shot_foco(2, 1);
    quadros(60);
    salvar("osd-botao-sobe");
    player_shot_foco(2, 0);
    quadros(60);
    salvar("osd-botao-desce");
    player_shot_foco(3, 0);
    quadros(60);
    salvar("osd-botao-dir");
    ponteiro_teste_toque(0);
  }
  // #293: codec e canais da faixa tocando, em texto ao lado dos selos. Tres
  // casos: TrueHD Atmos, E-AC-3 com o audio indo ao receptor, AAC estereo.
  if (quer(argc, argv, "audio-codec") || quer(argc, argv, "audio-codec-ee3") ||
      quer(argc, argv, "audio-codec-aac")) {
    static const char *ids[3] = { "audio-codec", "audio-codec-ee3", "audio-codec-aac" };
    static const char *cod[3] = { "A_TRUEHD", "audio/eac3", "A_AAC/MPEG4/LC" };
    static const int can[3] = { 8, 6, 2 };
    int q;
    for (q = 0; q < 3; q++) {
      VideoSimulacao v;
      if (!quer(argc, argv, ids[q])) continue;
      abrir(&filme);
      memset(&v, 0, sizeof v);
      v.largura = 3840; v.altura = 1606; v.dv = 1; v.atmos = q == 0; v.pronto = 1;
      v.duracao = 9420; v.pos = 4360; v.nAudio = 1; v.audioAtual = 0;
      faixa(&v.audio[0], q == 0 ? "Ingl\xc3\xaas  \xc2\xb7  TrueHD 7.1 Atmos" : "Ingl\xc3\xaas", "en", cod[q]);
      v.audio[0].canais = can[q];
      video_simular(&v);
      cacheboost_ganho_relato(q == 1 ? CB_GANHO_PASSTHROUGH : CB_GANHO_PCM);
      quadros(10);
      player_shot_estado(relogio, 4360.0f, 9420.0f, 1, 0, 0, 0);
      quadros(120);
      salvar(ids[q]);
      cacheboost_ganho_relato(CB_GANHO_PCM);
      simular(0, 0, "", 0, 0);
    }
  }
  // A MARCA DO DTS (dono, 06/10): no canto e na ilha, com a TV tocando o DTS
  // sozinha, com o app convertendo (PR #259) e com a TV sem som.
  if (quer(argc, argv, "dts-nativo") || quer(argc, argv, "dts-convertido") ||
      quer(argc, argv, "dts-sem-som")) {
    static const char *ids[3] = { "dts-nativo", "dts-convertido", "dts-sem-som" };
    static const int est[3] = { VIDEO_DTS_NATIVO, VIDEO_DTS_CONVERTIDO, VIDEO_DTS_SEM_SOM };
    int q;
    for (q = 0; q < 3; q++) {
      VideoSimulacao v;
      if (!quer(argc, argv, ids[q])) continue;
      abrir(&filme);
      memset(&v, 0, sizeof v);
      v.largura = 1920; v.altura = 1080; v.pronto = 1; v.duracao = 9420; v.pos = 4360;
      v.nAudio = 1; v.audioAtual = 0; v.dts = est[q];
      if (q == 1) faixa(&v.audio[0], "Ingl\xc3\xaas \xc2\xb7 dts", "en", "dts");
      else faixa(&v.audio[0], "Ingl\xc3\xaas  \xc2\xb7  DTS-HD MA \xc2\xb7 5.1", "en", "A_DTS/LOSSLESS");
      video_simular(&v);
      quadros(10);
      player_shot_estado(relogio, 4360.0f, 9420.0f, 1, 0, 0, 0);
      quadros(120);
      salvar(ids[q]);
      simular(0, 0, "", 0, 0);
    }
  }
  if (quer(argc, argv, "toast-proporcao")) {
    abrir(&filme); simular(3840, 1606, "", 1, 1);
    quadros(10);
    player_shot_toast(relogio, NULL, NULL, 0, 4);
    quadros(90);
    salvar("toast-proporcao");
  }
  if (quer(argc, argv, "reconectando")) {
    abrir(&filme); simular(3840, 1606, "", 1, 1);
    quadros(10);
    { VideoSimulacao v; memset(&v, 0, sizeof v);
      v.largura = 3840; v.altura = 1606; v.pronto = 1; v.bufferandoMs = 900; v.duracao = 9420; v.pos = 4360;
      video_simular(&v); }
    player_shot_video(1);
    player_shot_toast(relogio, "Conex\xc3\xa3o caiu, reconectando\xe2\x80\xa6", "aj_wifi-off", 1, 0);
    quadros(90);
    salvar("reconectando");
    player_shot_video(0); simular(0, 0, "", 0, 0);
  }
  if (quer(argc, argv, "audio") || quer(argc, argv, "audio-direita")) {
    int dir;
    for (dir = 0; dir < 2; dir++) {
      if (!quer(argc, argv, dir ? "audio-direita" : "audio")) continue;
      ajustes_shot_valor("relogioPosLocal", dir ? 2 : 1);
      abrir(&filme); simularFaixas();
      quadros(10);
      player_shot_estado(relogio, 4360.0f, 9420.0f, 1, 0, 0, 0);
      player_shot_esconder();
      faixas_abrir_em(0);
      teclaFaixas(SDLK_DOWN);
      quadros(90);
      salvar(dir ? "audio-direita" : "audio");
      faixas_evento(&(SDL_Event){ .key = { .type = SDL_KEYDOWN, .keysym = { .sym = SDLK_ESCAPE } } });
      quadros(60);
    }
    ajustes_shot_valor("relogioPosLocal", 1);
  }
  if (quer(argc, argv, "legendas") || quer(argc, argv, "legenda-estilo")) {
    abrir(&filme); simularFaixas();
    quadros(10);
    player_shot_estado(relogio, 4360.0f, 9420.0f, 1, 0, 0, 0);
    player_shot_esconder();
    faixas_abrir_em(1);
    teclaFaixas(SDLK_DOWN);
    quadros(90);
    if (quer(argc, argv, "legendas")) salvar("legendas");
    { VideoLegendaEstilo *e = player_leg_estilo();
      e->fundo = 2; e->borda = 2; e->atrasoMs = 250; player_leg_estilo_mudou(); }
    teclaFaixas(SDLK_RIGHT);
    teclaFaixas(SDLK_DOWN); teclaFaixas(SDLK_RIGHT); teclaFaixas(SDLK_RIGHT);
    quadros(90);
    if (quer(argc, argv, "legenda-estilo")) salvar("legenda-estilo");
    faixas_evento(&(SDL_Event){ .key = { .type = SDL_KEYDOWN, .keysym = { .sym = SDLK_ESCAPE } } });
    quadros(60);
  }
  if (quer(argc, argv, "buffering-claro") || quer(argc, argv, "buffering-escuro")) {
    int q;
    for (q = 0; q < 2; q++) {
      const char *id = q ? "buffering-escuro" : "buffering-claro";
      if (!quer(argc, argv, id)) continue;
      abrir(&filme); simular(3840, 1606, "", 1, 1);
      quadros(10);
      { VideoSimulacao v; memset(&v, 0, sizeof v);
        v.largura = 3840; v.altura = 1606; v.pronto = 1; v.bufferandoMs = 900; v.duracao = 9420; v.pos = 4360;
        video_simular(&v); }
      player_shot_video(1);
      fundo = q ? fundoEscuro : fundoClaro;
      quadros(40);
      salvar(id);
      fundo = NULL;
      player_shot_video(0); simular(0, 0, "", 0, 0);
    }
  }
  if (quer(argc, argv, "abrindo-compacto") || quer(argc, argv, "abrindo-expandido") ||
      quer(argc, argv, "abrindo-fim")) {
    // A fonte abrindo: o estado compacto, o expandido (BAIXO) e o fim da
    // abertura em tres quadros (comeco, meio, perto do fim).
    Stream st;
    int q;
    memset(&st, 0, sizeof st);
    snprintf(st.rotulo, sizeof st.rotulo, "Fallout.S01E03.The.Head.2160p.WEB-DL.DDP5.1.Atmos.DV.HDR10.H265-NTb");
    snprintf(st.provedor, sizeof st.provedor, "AIOStreams");
    snprintf(st.url, sizeof st.url, "http://exemplo/fallout.mkv");
    st.tamanhoMB = (long)(18.2 * 1024);
    st.badges = badges_bit("r-4k") | badges_bit("v-hdr10") | badges_bit("a-atmos");
    for (q = 0; q < 3; q++) {
      static const char *ids[3] = { "abrindo-compacto", "abrindo-expandido", "abrindo-fim" };
      if (!quer(argc, argv, ids[q])) continue;
      stream_definir_lista(&st, 1);
      abrir(&serie); simular(0, 0, "", 0, 0);
      stream_definir_atual(0);
      player_definir_tentativa(2, 3);
      player_shot_carregando(1);
      quadros(90);
      if (q >= 1) {
        SDL_Event e; memset(&e, 0, sizeof e);
        e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_DOWN;
        player_evento(&e);
        quadros(120);
      }
      if (q == 0 || q == 1) salvar(ids[q]);
      if (q == 2) {
        player_shot_carregando(0);
        quadros(3);  salvar("abrindo-fim-1");
        quadros(7);  salvar("abrindo-fim-2");
        quadros(10); salvar("abrindo-fim-3");
        quadros(40);
      }
      player_definir_tentativa(0, 0);
    }
  }
  if (quer(argc, argv, "cinema")) {
    // A ABERTURA CINEMA (#205): o titulo recuado com o logo, a linha de demora
    // depois de 8 s, o crossfade com o primeiro quadro, e a Detalhada pelo ajuste.
    Stream st;
    VideoSimulacao v;
    memset(&st, 0, sizeof st);
    snprintf(st.rotulo, sizeof st.rotulo, "Project Hail Mary 2160p");
    snprintf(st.provedor, sizeof st.provedor, "AIOStreams");
    snprintf(st.url, sizeof st.url, "http://exemplo/phm.mkv");
    stream_definir_lista(&st, 1);
    abrir(&filme); simular(0, 0, "", 0, 0);
    stream_definir_atual(0);
    player_shot_carregando(1);
    quadros(90);  salvar("cinema-inicio");
    quadros(460); salvar("cinema-demorando");
    ajustes_shot_valor("aberturaAddonLocal", 0);   // 0 = Ligado
    quadros(2);   salvar("cinema-addon-ligado");
    ajustes_shot_valor("aberturaAddonLocal", 1);
    // O primeiro quadro: a fonte abriu e o video anda. O veu sai por cima dele.
    player_shot_carregando(0); player_shot_video(1);
    memset(&v, 0, sizeof v);
    v.largura = 3840; v.altura = 1606; v.pronto = 1; v.tocando = 1; v.pos = 0.5; v.duracao = 9420.0;
    video_simular(&v);
    quadros(7);   salvar("cinema-crossfade");
    quadros(60);
    player_shot_video(0); simular(0, 0, "", 0, 0);
    ajustes_shot_valor("telaAberturaLocal", 1);
    abrir(&filme); simular(0, 0, "", 0, 0);
    stream_definir_atual(0);
    player_shot_carregando(1);
    quadros(90);  salvar("detalhada");
    player_shot_carregando(0);
    ajustes_shot_valor("telaAberturaLocal", 0);
    // Serie: a linha do episodio, a etapa e a fonte sob o trilho.
    snprintf(st.rotulo, sizeof st.rotulo, "Fallout.S01E03.The.Head.2160p.WEB-DL.DDP5.1.Atmos.DV.HDR10.H265-NTb\nRAW ADDON DETAILS");
    stream_definir_lista(&st, 1);
    abrir(&serie); simular(0, 0, "", 0, 0);
    stream_definir_atual(0);
    player_shot_carregando(1);
    quadros(90);  salvar("cinema-serie");
    { VideoStats stats;
      video_stats_vazio(&stats); stats.bufferPct = 37;
      video_simular_estatisticas(&stats);
      player_shot_video(1); player_shot_carregando(0);
      quadros(30); salvar("cinema-buffer");
      video_simular_estatisticas(NULL);
      player_shot_video(0);
    }
    { CatItem semLogo = serie;
      semLogo.logo[0] = 0;
      abrir(&semLogo); simular(0, 0, "", 0, 0);
      stream_definir_atual(0);
      player_shot_carregando(1);
      quadros(90); salvar("cinema-sem-logo");
    }
    player_shot_carregando(0);
  }
  if (quer(argc, argv, "cinema-fundo-off") || quer(argc, argv, "cinema-fundo-nitida") ||
      quer(argc, argv, "cinema-fundo-borrada")) {
    // #508: o fundo da tela Cinema da abertura em tres modos — Desligado, Arte
    // (nitida) e Arte borrada (o padrao) — mesma abertura, so o ajuste muda.
    Stream st;
    int q;
    static const char *ids[3] = { "cinema-fundo-off", "cinema-fundo-nitida", "cinema-fundo-borrada" };
    memset(&st, 0, sizeof st);
    snprintf(st.rotulo, sizeof st.rotulo, "Project Hail Mary 2160p");
    snprintf(st.provedor, sizeof st.provedor, "AIOStreams");
    snprintf(st.url, sizeof st.url, "http://exemplo/phm.mkv");
    stream_definir_lista(&st, 1);
    for (q = 0; q < 3; q++) {
      if (!quer(argc, argv, ids[q])) continue;
      ajustes_shot_valor("aberturaFundoLocal", q);
      abrir(&filme); simular(0, 0, "", 0, 0);
      stream_definir_atual(0);
      player_shot_carregando(1);
      quadros(120);
      salvar(ids[q]);
      player_shot_carregando(0);
    }
    ajustes_shot_valor("aberturaFundoLocal", 2);   // o padrao de volta
  }
  if (quer(argc, argv, "carregando")) {
    Stream st;
    memset(&st, 0, sizeof st);
    snprintf(st.rotulo, sizeof st.rotulo, "Fallout S01E03 2160p");
    snprintf(st.provedor, sizeof st.provedor, "AIOStreams");
    snprintf(st.url, sizeof st.url, "http://exemplo/fallout.mkv");
    st.tamanhoMB = (long)(18.2 * 1024);
    st.badges = badges_bit("r-4k") | badges_bit("v-hdr10") | badges_bit("a-atmos");
    stream_definir_lista(&st, 1);
    abrir(&serie); simular(0, 0, "", 0, 0);
    stream_definir_atual(0);
    player_definir_tentativa(2, 3);
    player_shot_carregando(1);
    quadros(120);
    salvar("carregando");
    player_definir_tentativa(0, 0);
  }
  if (quer(argc, argv, "erro")) {
    abrir(&filme); simular(3840, 1606, "", 1, 1);
    quadros(10);
    player_erro_fonte_motivo("As fontes torrent desta lista n\xc3\xa3o est\xc3\xa3o no cache do debrid",
                             "Abra Fontes e escolha uma: o servi\xc3\xa7o come\xc3\xa7" "a a baixar.");
    quadros(120);
    salvar("erro");
    player_limpar_erro_fonte();
  }
  if (quer(argc, argv, "erro-causa")) {   // #205: generico (antes) x causa dita (depois)
    char t[160];
    abrir(&filme); simular(3840, 1606, "", 1, 1);
    quadros(10);
    player_erro_fonte();
    quadros(120);
    salvar("erro-antes");
    player_limpar_erro_fonte();
    snprintf(t, sizeof t, i18n("O servidor da fonte recusou o link (HTTP %d)."), 404);
    player_erro_fonte_motivo(t, i18n("O problema é do add-on ou do debrid, não do app. Abra Fontes e tente outra fonte."));
    quadros(120);
    salvar("erro-depois");
    player_limpar_erro_fonte();
  }
  if (quer(argc, argv, "seekr") || quer(argc, argv, "seekr-fita") || quer(argc, argv, "seekr-carregando")) {
    static const char *ids[3] = { "seekr", "seekr-fita", "seekr-carregando" };
    int q;
    ajustes_shot_valor("seekrChave", 1);
    ajustes_shot_valor("seekrLocal", 0);
    for (q = 0; q < 3; q++) {
      GLuint t[3] = { 0, 0, 0 };
      double cue[3] = { -1, -1, -1 };
      float pos;
      if (!quer(argc, argv, ids[q])) continue;
      ajustes_shot_valor("seekrFitaLocal", q == 1 ? 0 : 1);
      abrir(&serie); simular(3840, 2160, "HDR10", 0, 1);
      quadros(10);
      if (q == 0) { pos = 2083.0f; t[1] = texDe("img/ep/00_1_04.jpg"); cue[1] = 2083.0; }
      else if (q == 1) { pos = 1040.0f; t[0] = texDe("img/ep/00_1_02.jpg"); t[1] = texDe("img/ep/00_1_03.jpg");
                         t[2] = texDe("img/ep/00_1_08.jpg"); cue[0] = 1030.0; cue[1] = 1040.0; cue[2] = 1050.0; }
      else { pos = 2620.0f; cue[1] = 2620.0; t[1] = texDe("img/ep/00_1_06.jpg"); }
      seekr_shot(SEEKR_PRONTO, t, cue, q == 2);
      player_shot_estado(relogio, pos, 3360.0f, 1, 0, 1, 1);
      player_shot_buscando(1);
      quadros(60);
      salvar(ids[q]);
      player_shot_buscando(0);
      seekr_shot(0, NULL, NULL, 0);
      quadros(30);
    }
    ajustes_shot_valor("seekrChave", 0);
  }
  if (quer(argc, argv, "guia-parental")) {
    static const char *rot[4] = { "Viol\xc3\xaancia", "Linguagem Impr\xc3\xb3pria", "Conte\xc3\xba" "do Assustador", "Drogas/\xc3\x81lcool" };
    static const char *gr[4] = { "Moderado", "Leve", "Severo", "Leve" };
    CatItem f = filme;
    snprintf(f.classificacao, sizeof f.classificacao, "12");
    abrir(&f); simular(3840, 1606, "", 1, 1);
    { VideoSimulacao v; memset(&v, 0, sizeof v);
      v.largura = 3840; v.altura = 1606; v.pronto = 1; v.duracao = 9420; v.pos = 30;
      video_simular(&v); }
    player_shot_video(1);
    parental_shot(rot, gr, 4);
    quadros(20);
    player_shot_esconder();
    quadros(80);
    salvar("guia-parental");
    player_shot_video(0); parental_shot(NULL, NULL, 0);
  }
  // A guia dentro da ilha: meio da mola (growing), assentada com o OSD, e ja
  // fechada de volta na hora (depois da janela de 12 s).
  if (quer(argc, argv, "guia-ilha")) {
    static const char *rot[4] = { "Viol\xc3\xaancia", "Linguagem Impr\xc3\xb3pria", "Conte\xc3\xba" "do Assustador", "Drogas/\xc3\x81lcool" };
    static const char *gr[4] = { "Moderado", "Leve", "Severo", "Leve" };
    CatItem f = filme;
    snprintf(f.classificacao, sizeof f.classificacao, "12");
    abrir(&f); simular(3840, 1606, "", 1, 1);
    { VideoSimulacao v; memset(&v, 0, sizeof v);
      v.largura = 3840; v.altura = 1606; v.pronto = 1; v.duracao = 9420; v.pos = 30;
      video_simular(&v); }
    player_shot_video(1);
    parental_shot(rot, gr, 4);
    quadros(10);
    salvar("guia-ilha-crescendo");
    quadros(60);
    salvar("guia-ilha");
    quadros(760);
    salvar("guia-ilha-fechada");
    player_shot_video(0); parental_shot(NULL, NULL, 0);
  }
  if (quer(argc, argv, "pular-abertura")) {
    IntroTrecho tr[1] = { { 60.0, 140.0, INTRO_ABERTURA } };
    abrir(&serie); simular(3840, 2160, "HDR10", 0, 1);
    intro_shot_definir(tr, 1);
    quadros(10);
    player_shot_estado(relogio, 90.0f, 3360.0f, 1, 0, 0, 0);
    player_shot_esconder();
    quadros(60);
    salvar("pular-abertura");
    intro_shot_definir(NULL, 0);
  }
  if (quer(argc, argv, "pausa")) {
    static const char *nomes[5] = { "Ella Purnell", "Aaron Moten", "Walton Goggins", "Mois\xc3\xa9s Arias", "Kyle MacLachlan" };
    CatItem f = serie;
    int i;
    snprintf(f.sinopse, sizeof f.sinopse, "Lucy, Maximus e o Ghoul disputam a mesma recompensa no deserto. Cada um tem um motivo para querer a cabe\xc3\xa7" "a que todos est\xc3\xa3o ca\xc3\xa7" "ando.");
    for (i = 0; i < 5; i++) {
      char r[32];
      snprintf(f.elenco[i].nome, sizeof f.elenco[i].nome, "%s", nomes[i]);
      snprintf(r, sizeof r, "img/el/00_%d.jpg", i);
      snprintf(f.elenco[i].foto, sizeof f.elenco[i].foto, "%s", img(r));
    }
    f.nElenco = 5;
    abrir(&f); simular(3840, 2160, "HDR10", 0, 1);
    quadros(10);
    player_shot_estado(relogio, 1948.0f, 3360.0f, 0, 0, 0, 0);
    { struct tm lt; time_t t = time(NULL);
      localtime_r(&t, &lt); lt.tm_hour = 20; lt.tm_min = 41; lt.tm_sec = 0;
      plrilha_shot_hora(mktime(&lt)); }
    quadros(420);
    salvar("pausa");
    { struct tm lt; time_t t = time(NULL);
      localtime_r(&t, &lt); lt.tm_hour = 20; lt.tm_min = 19; lt.tm_sec = 0;
      plrilha_shot_hora(mktime(&lt)); }
  }
  if (quer(argc, argv, "episodios") || quer(argc, argv, "episodios-marcar")) {
    CatItem f = serieComEps();
    player_abrir(0, NULL); player_definir_episodio(1, 3);
    player_erro_fonte(); player_limpar_erro_fonte();
    simular(3840, 2160, "HDR10", 0, 1);
    quadros(10);
    player_shot_estado(relogio, 1948.0f, 3360.0f, 1, 5, 0, 0);
    { struct tm lt; time_t t = time(NULL);
      localtime_r(&t, &lt); lt.tm_hour = 20; lt.tm_min = 39; lt.tm_sec = 0;
      plrilha_shot_hora(mktime(&lt)); }
    episodios_abrir(0, 1, 3);
    quadros(30);
    episodios_shot_foco(3);
    quadros(90);
    if (quer(argc, argv, "episodios")) salvar("episodios");
    episodios_shot_menu();
    quadros(60);
    if (quer(argc, argv, "episodios-marcar")) salvar("episodios-marcar");
    { struct tm lt; time_t t = time(NULL);
      localtime_r(&t, &lt); lt.tm_hour = 20; lt.tm_min = 19; lt.tm_sec = 0;
      plrilha_shot_hora(mktime(&lt)); }
  }
  if (quer(argc, argv, "proximo")) {
    serieComEps();
    player_abrir(0, NULL); player_definir_episodio(1, 3);
    player_erro_fonte(); player_limpar_erro_fonte();
    { VideoSimulacao v; memset(&v, 0, sizeof v);
      v.largura = 3840; v.altura = 2160; v.pronto = 1; v.duracao = 3360; v.pos = 3300;
      video_simular(&v); }
    player_shot_video(1);
    quadros(5);
    posplay_shot(0, 1, 1, 4, relogio + 8000u + 120u * 16u);
    quadros(120);
    salvar("proximo");
    posplay_fechar(); player_shot_video(0); quadros(30);
  }
  if (quer(argc, argv, "mais-como-este")) {
    abrir(&filme);
    { VideoSimulacao v; memset(&v, 0, sizeof v);
      v.largura = 3840; v.altura = 2160; v.pronto = 1; v.duracao = 9420; v.pos = 9300;
      video_simular(&v); }
    player_shot_video(1);
    quadros(5);
    posplay_shot(0, 0, 0, 0, 0);
    quadros(120);
    salvar("mais-como-este");
    posplay_fechar(); player_shot_video(0); quadros(30);
  }
  if (quer(argc, argv, "reacao")) {
    abrir(&filme); simular(3840, 2160, "", 1, 1);
    quadros(10);
    player_shot_esconder();
    reacao_teste_abrir("tt99999991", "Project Hail Mary", "movie", 42, "Ana", 1);
    quadros(110);
    salvar("reacao");
  }
  if (quer(argc, argv, "fontes")) {
    static Stream st[5];
    static const char *prov[5] = { "AIOStreams", "AIOStreams", "Torrentio", "AIOStreams", "Torrentio" };
    static const long mb[5] = { 18637, 9626, 7987, 2150, 1840 };
    static const int alt[5] = { 2160, 2160, 2160, 1080, 1080 };
    int i;
    memset(st, 0, sizeof st);
    for (i = 0; i < 5; i++) {
      snprintf(st[i].rotulo, sizeof st[i].rotulo, "Fallout");
      snprintf(st[i].provedor, sizeof st[i].provedor, "%s", prov[i]);
      snprintf(st[i].url, sizeof st[i].url, "http://exemplo/%d.mkv", i);
      st[i].tamanhoMB = mb[i]; st[i].altura = alt[i]; st[i].fileIdx = -1;
    }
    st[0].badges = badges_bit("r-4k") | badges_bit("v-hdr10") | badges_bit("a-atmos") | badges_bit("q-remux");
    st[1].badges = badges_bit("r-4k") | badges_bit("v-dv") | badges_bit("a-ddp") | badges_bit("q-webdl");
    st[1].mp4 = 1; st[1].dolbyVision = 1;
    snprintf(st[1].arquivo, sizeof st[1].arquivo, "Fallout.S01E03.2160p.AMZN.WEB-DL.DV.DDP5.1.mp4");
    st[2].badges = badges_bit("r-4k") | badges_bit("v-hdr10plus") | badges_bit("a-ddp");
    st[3].badges = badges_bit("r-1080") | badges_bit("a-ddp");
    st[4].badges = badges_bit("r-1080");
    abrir(&serie); simular(3840, 2160, "HDR10", 0, 1);
    stream_definir_lista(st, 5);
    stream_definir_atual(0);
    quadros(10);
    player_shot_esconder();
    stream_folha_contexto("Fallout \xc2\xb7 T1E3");
    stream_folha_abrir();
    quadros(90);
    salvar("fontes");
  }
  // ABRINDO COMO NO APP (dono, 03/10: "so mostrar o componente de carregando
  // sem mostrar o player"): player_abrir sem o atalho de erro que `abrir()`
  // usa para esconder o OSD, entao os controles estao pedidos como na TV.
  if (quer(argc, argv, "abrindo")) {
    Stream st;
    CatItem l1[1];
    // A folha de um quadro anterior ("fontes") fecha antes.
    faixas_evento(&(SDL_Event){ .key = { .type = SDL_KEYDOWN, .keysym = { .sym = SDLK_ESCAPE } } });
    stream_folha_evento(&(SDL_Event){ .key = { .type = SDL_KEYDOWN, .keysym = { .sym = SDLK_ESCAPE } } });
    quadros(60);
    memset(&st, 0, sizeof st);
    snprintf(st.rotulo, sizeof st.rotulo, "Project Hail Mary 2160p");
    snprintf(st.provedor, sizeof st.provedor, "AIOStreams");
    snprintf(st.url, sizeof st.url, "http://exemplo/phm.mkv");
    st.tamanhoMB = (long)(21.4 * 1024);
    st.badges = badges_bit("r-4k") | badges_bit("v-dv") | badges_bit("a-atmos");
    stream_definir_lista(&st, 1);
    l1[0] = filme; cat_definir(l1, 1);
    player_abrir(0, NULL);
    simular(0, 0, "", 0, 0);
    stream_definir_atual(0);
    player_shot_carregando(1);
    quadros(120);
    salvar("abrindo");
    // A imagem chegou: os controles entram so agora.
    // (No desktop video_pronto() e 0: sem comVideo a arte faz de imagem.)
    player_shot_carregando(0);
    simular(3840, 1606, "", 1, 1);
    quadros(40);
    salvar("abrindo-tocou");
  }
  // OPENING SOURCE COM LOGO / SEM LOGO / ENCOLHENDO, e o aviso de conteudo
  // virando a hora (dono, 04/10). Montserrat: NUVIO_SHOT_FONTE=3.
  if (quer(argc, argv, "abrindo-logo") || quer(argc, argv, "abrindo-sem-logo")) {
    int com;
    for (com = 1; com >= 0; com--) {
      Stream st;
      CatItem l1[1];
      if (!quer(argc, argv, com ? "abrindo-logo" : "abrindo-sem-logo")) continue;
      faixas_evento(&(SDL_Event){ .key = { .type = SDL_KEYDOWN, .keysym = { .sym = SDLK_ESCAPE } } });
      stream_folha_evento(&(SDL_Event){ .key = { .type = SDL_KEYDOWN, .keysym = { .sym = SDLK_ESCAPE } } });
      quadros(60);
      memset(&st, 0, sizeof st);
      snprintf(st.rotulo, sizeof st.rotulo, "Project Hail Mary 2160p");
      snprintf(st.provedor, sizeof st.provedor, "AIOStreams");
      snprintf(st.url, sizeof st.url, "http://exemplo/phm.mkv");
      st.tamanhoMB = (long)(21.4 * 1024);
      st.badges = badges_bit("r-4k") | badges_bit("v-dv") | badges_bit("a-atmos");
      stream_definir_lista(&st, 1);
      l1[0] = filme;
      if (!com) l1[0].logo[0] = 0;
      cat_definir(l1, 1);
      player_abrir(0, NULL);
      simular(0, 0, "", 0, 0);
      stream_definir_atual(0);
      player_shot_carregando(1);
      quadros(60); SDL_Delay(600);   // o decode do logo e de fio proprio
      quadros(90);
      salvar(com ? "abrindo-logo" : "abrindo-sem-logo");
      // A imagem chegou: o cartao encolhe como a ilha do relogio sai.
      player_shot_carregando(0);
      simular(3840, 1606, "", 1, 1);
      quadros(com ? 6 : 14);
      salvar(com ? "abrindo-encolhendo-1" : "abrindo-encolhendo-2");
      quadros(10);
      if (com) salvar("abrindo-encolhendo-3");
      quadros(90);
    }
  }
  // O aviso de conteudo (guia parental) vira a pilula da HORA e so entao sai.
  // guia-morph: meio do voo corpo -> pilula, a hora assentada e a ilha ja fora.
  // guia-relogio-off: relogio desligado em Ajustes, encolhe e some no lugar.
  if (quer(argc, argv, "guia-morph") || quer(argc, argv, "guia-relogio-off")) {
    static const char *rot[4] = { "Viol\xc3\xaancia", "Linguagem Impr\xc3\xb3pria", "Conte\xc3\xba" "do Assustador", "Drogas/\xc3\x81lcool" };
    static const char *gr[4] = { "Moderado", "Leve", "Severo", "Leve" };
    int off;
    for (off = 0; off < 2; off++) {
      CatItem f = filme;
      if (!quer(argc, argv, off ? "guia-relogio-off" : "guia-morph")) continue;
      ajustes_shot_valor("relogioTelaLocal", off ? 1 : 0);   // 0 = Ligado
      snprintf(f.classificacao, sizeof f.classificacao, "12");
      abrir(&f); simular(3840, 1606, "", 1, 1);
      { VideoSimulacao v; memset(&v, 0, sizeof v);
        v.largura = 3840; v.altura = 1606; v.pronto = 1; v.duracao = 9420; v.pos = 30;
        video_simular(&v); }
      player_shot_video(1);
      parental_shot(rot, gr, 4);
      quadros(80);
      salvar(off ? "guia-off-aberta" : "guia-morph-aberta");
      quadros(668);   // 748 quadros: a janela de 12 s fecha em ~753
      { int k; for (k = 0; k < 5; k++) { quadros(5);
          { char n[48]; snprintf(n, sizeof n, "%s-%d", off ? "guia-off-saindo" : "guia-morph-voo", k); salvar(n); } } }
      if (!off) { quadros(45); salvar("guia-morph-hora"); }
      quadros(150);
      salvar(off ? "guia-off-fora" : "guia-morph-fora");
      player_shot_video(0); parental_shot(NULL, NULL, 0);
      ajustes_shot_valor("relogioTelaLocal", 0);
    }
  }
  // ABRINDO COM A FOLHA DE FONTES ABERTA (TV do dono, 04/10, "Continuar
  // assistindo" > Play com "escolher a fonte ao reproduzir"): o player espera
  // a escolha e a folha esta aberta. O cartao "Abrindo fonte" nao pode ir por
  // cima das linhas, nem ter caixa de detalhe vazia (nenhuma fonte escolhida).
  if (quer(argc, argv, "abrindo-folha")) {
    static Stream st[4];
    int i;
    CatItem l1[1];
    faixas_evento(&(SDL_Event){ .key = { .type = SDL_KEYDOWN, .keysym = { .sym = SDLK_ESCAPE } } });
    stream_folha_evento(&(SDL_Event){ .key = { .type = SDL_KEYDOWN, .keysym = { .sym = SDLK_ESCAPE } } });
    quadros(60);
    memset(st, 0, sizeof st);
    for (i = 0; i < 4; i++) {
      snprintf(st[i].rotulo, sizeof st[i].rotulo, "Happy Valley");
      snprintf(st[i].provedor, sizeof st[i].provedor, "AIOStreams");
      snprintf(st[i].url, sizeof st[i].url, "http://exemplo/hv%d.mkv", i);
      st[i].tamanhoMB = 1900 + i * 100; st[i].fileIdx = -1;
      st[i].badges = badges_bit("r-1080") | badges_bit("q-bluray");
    }
    stream_definir_lista(st, 4);
    l1[0] = filme; cat_definir(l1, 1);
    player_abrir(0, NULL);
    simular(0, 0, "", 0, 0);
    stream_definir_atual(-1);
    player_shot_carregando(1);
    stream_folha_contexto("Happy Valley \xc2\xb7 T1E2");
    stream_folha_abrir();
    quadros(120);
    salvar("abrindo-folha");
    player_shot_carregando(0);
    stream_folha_evento(&(SDL_Event){ .key = { .type = SDL_KEYDOWN, .keysym = { .sym = SDLK_ESCAPE } } });
    quadros(30);
  }
  // A ABA DE STREAMING ("Onde ver") da folha de Fontes, com dois servicos.
  if (quer(argc, argv, "onde-ver")) {
    static Stream st[2];
    OndeVer ov[3];
    SDL_Event ev;
    int i;
    memset(st, 0, sizeof st);
    memset(ov, 0, sizeof ov);
    for (i = 0; i < 2; i++) {
      snprintf(st[i].rotulo, sizeof st[i].rotulo, "Project Hail Mary");
      snprintf(st[i].provedor, sizeof st[i].provedor, "AIOStreams");
      snprintf(st[i].url, sizeof st[i].url, "http://exemplo/o%d.mkv", i);
      st[i].tamanhoMB = i ? 4200 : 21400; st[i].altura = i ? 1080 : 2160; st[i].fileIdx = -1;
    }
    snprintf(ov[0].nome, sizeof ov[0].nome, "Prime Video");
    snprintf(ov[0].logo, sizeof ov[0].logo, "deploy/app/art/03.jpg");
    snprintf(ov[1].nome, sizeof ov[1].nome, "Apple TV");
    snprintf(ov[1].logo, sizeof ov[1].logo, "deploy/app/art/07.jpg");
    snprintf(ov[2].nome, sizeof ov[2].nome, "Pluto TV");
    snprintf(ov[2].logo, sizeof ov[2].logo, "deploy/app/art/11.jpg");
    ov[2].gratis = 1;
    abrir(&filme); simular(3840, 1606, "", 1, 1);
    stream_definir_alvo("tt12345678");
    ondever_shot("tt12345678", ov, 3);
    stream_definir_lista(st, 2);
    stream_definir_atual(0);
    quadros(10);
    player_shot_esconder();
    stream_folha_contexto("Project Hail Mary");
    stream_folha_abrir();
    // Para a esquerda numa fonte troca a aba: "Todos" -> "Onde ver".
    memset(&ev, 0, sizeof ev); ev.type = SDL_KEYDOWN; ev.key.keysym.sym = SDLK_LEFT;
    stream_folha_evento(&ev);
    quadros(90);
    salvar("onde-ver");
    memset(&ev, 0, sizeof ev); ev.type = SDL_KEYDOWN; ev.key.keysym.sym = SDLK_ESCAPE;
    stream_folha_evento(&ev);
    quadros(60);
  }
  if (quer(argc, argv, "trailer")) {
    VideoSimulacao v; memset(&v, 0, sizeof v);
    v.pos = 52; v.duracao = 144; v.bufferFim = 86.4; video_simular(&v);
    trailer_shot_pausado(1);
    extra = desenhaTrailer;
    quadros(60);
    salvar("trailer");
    extra = NULL; trailer_shot_pausado(0);
  }
  if (quer(argc, argv, "aovivo-osd") || quer(argc, argv, "aovivo-atras")) {
    int atras = quer(argc, argv, "aovivo-atras");
    abrirCanal(atras ? 252 : 0, atras ? 7 : 0, atras, 12, "Sportv 2", "Campeonato Brasileiro: Rodada 24", "img/bd/36.jpg");
    quadros(10);
    player_shot_estado(relogio, 0, 0, 1, 0, 0, 0);
    quadros(60);
    salvar(atras ? "aovivo-atras" : "aovivo-osd");
  }
  if (quer(argc, argv, "aovivo-erro")) {
    abrirCanal(0, 0, 0, 12, "Sportv 2", "Campeonato Brasileiro: Rodada 24", "img/bd/36.jpg");
    player_shot_video(0);
    player_erro_fonte_motivo("N\xc3\xa3o foi poss\xc3\xadvel abrir a fonte", "Todas as telas da conta Xtream est\xc3\xa3o em uso (2 de 2).");
    quadros(80);
    salvar("aovivo-erro");
  }
  if (quer(argc, argv, "aovivo-mini")) {
    abrirCanal(0, 0, 0, 12, "Sportv 2", "Campeonato Brasileiro", "img/bd/36.jpg");
    quadros(10);
    player_minimizar();
    glem_teste_cartao("Jornal das Dez", "Globo News", 0);
    extra = desenhaMini;
    quadros(60);
    salvar("aovivo-mini");
    extra = NULL;
  }
  if (quer(argc, argv, "aovivo-zap")) {
    extra = desenhaZap;
    quadros(30);
    salvar("aovivo-zap");
    extra = NULL;
  }
  // AO VIVO NA ILHA DO RELOGIO (W17): a hora e a ilha do player, e Audio,
  // Legendas, Fontes e Informacoes crescem dela; sintonizando e uma linha so
  // na propria ilha; o Favorito com estrela cheia.
  if (quer(argc, argv, "aovivo-ilha")) {
    static Stream st[2];
    int i;
    SDL_Event ev;
    abrirCanal(0, 3, 0, 12, "Sportv 2", "Campeonato Brasileiro: Rodada 24", "img/bd/36.jpg");
    player_shot_favorito(1);
    quadros(10);
    player_shot_estado(relogio, 0, 0, 1, 0, 0, 0);
    quadros(60);
    salvar("aovivo-ilha-relogio");
    player_shot_favorito(2);
    quadros(20);
    salvar("aovivo-ilha-favorito");
    player_shot_favorito(0);
    // Audio e Legendas: as faixas do proprio fluxo (sem legenda de addon).
    { VideoSimulacao v; memset(&v, 0, sizeof v);
      v.largura = 1920; v.altura = 1080; v.pronto = 1; v.nAudio = 2; v.audioAtual = 0;
      faixa(&v.audio[0], "Portugu\xc3\xaas  \xc2\xb7  AAC \xc2\xb7 2.0", "pt", "");
      faixa(&v.audio[1], "Ingl\xc3\xaas  \xc2\xb7  AAC \xc2\xb7 2.0", "en", "");
      v.nLeg = 0; v.legAtual = -1;
      video_simular(&v); }
    faixas_abrir_em(0);
    quadros(10); salvar("aovivo-ilha-audio-meio");
    quadros(80); salvar("aovivo-ilha-audio");
    faixas_evento(&(SDL_Event){ .key = { .type = SDL_KEYDOWN, .keysym = { .sym = SDLK_ESCAPE } } });
    quadros(60);
    faixas_abrir_em(1);
    quadros(90); salvar("aovivo-ilha-legendas");
    faixas_evento(&(SDL_Event){ .key = { .type = SDL_KEYDOWN, .keysym = { .sym = SDLK_ESCAPE } } });
    quadros(60);
    // Fontes do canal: o mesmo canal em HLS e TS.
    memset(st, 0, sizeof st);
    for (i = 0; i < 2; i++) {
      snprintf(st[i].rotulo, sizeof st[i].rotulo, i ? "Xtream (TS)" : "Xtream (HLS, proxy)");
      snprintf(st[i].provedor, sizeof st[i].provedor, "xtream");
      snprintf(st[i].url, sizeof st[i].url, "http://exemplo/canal%d", i);
      st[i].altura = 1080; st[i].fileIdx = -1;
    }
    stream_definir_lista(st, 2);
    stream_definir_atual(0);
    player_shot_estado(relogio, 0, 0, 1, 0, 0, 0);
    quadros(30);
    stream_folha_canal(1);
    stream_folha_contexto("Sportv 2");
    stream_folha_abrir();
    quadros(90); salvar("aovivo-ilha-fontes");
    memset(&ev, 0, sizeof ev); ev.type = SDL_KEYDOWN; ev.key.keysym.sym = SDLK_ESCAPE;
    stream_folha_evento(&ev);
    quadros(60);
    // Informacoes: o corpo da ilha.
    abrirCanal(0, 7, 1, 12, "Sportv 2", "Campeonato Brasileiro: Rodada 24", "img/bd/36.jpg");
    quadros(10);
    player_shot_estado(relogio, 0, 0, 1, 0, 0, 0);
    quadros(60); salvar("aovivo-ilha-info");
    player_shot_canal(NULL, 12, 0, 0, 0);
    // Sintonizando: uma linha so, na ilha do relogio.
    abrirCanal(0, 0, 0, 13, "Globo News", "Em Foco", "img/bd/21.jpg");
    player_shot_video(0);
    { VideoSimulacao v; memset(&v, 0, sizeof v); video_simular(&v); }
    player_shot_carregando(1);
    quadros(90); salvar("aovivo-ilha-sintonizando");
    player_shot_carregando(0);
  }
  if (quer(argc, argv, "ilha-crescendo")) {
    // Tres instantes da MESMA superficie: a pilula, o meio da mola e a lista.
    abrir(&filme); simularFaixas();
    quadros(10);
    player_shot_estado(relogio, 4360.0f, 9420.0f, 1, 0, 0, 0);
    quadros(40);
    salvar("ilha-crescendo-1");
    faixas_abrir_em(0);
    teclaFaixas(SDLK_DOWN);
    quadros(10);
    salvar("ilha-crescendo-2");
    quadros(90);
    salvar("ilha-crescendo-3");
  }
  if (quer(argc, argv, "saida-modal") || quer(argc, argv, "saida-voo")) {
    struct tm lt; time_t t = time(NULL);
    ajustes_shot_valor("relogioTelaLocal", 0);
    cartaoVivo();
    extra = desenhaHomeIlha;
    if (quer(argc, argv, "saida-voo")) {
      quadros(5);
      ilha_minimizar(img("img/bd/00.jpg"));
      quadros(13);   // o meio do voo de 560 ms (o quadro do mockup)
      salvar("saida-voo");
      quadros(60);
    } else quadros(60);
    if (quer(argc, argv, "saida-modal")) {
      SDL_Event ev; memset(&ev, 0, sizeof ev);
      (void)lt; (void)t;
      ev.type = SDL_KEYDOWN; ev.key.keysym.sym = SDLK_s;
      if (!ilha_evento(&ev)) ilha_modal_abrir();
      quadros(80);
      salvar("saida-modal");
    }
    extra = NULL;
  }
  puts("player_glass_shot: ok");
  return 0;
}
