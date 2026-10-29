// CAPTURAS DO CABECALHO DO PAINEL SOCIAL (07/10/2026): titulo = aba aberta, faixa
// de abas compacta na linha do titulo, folha Editar e aba Agenda. Base: socialui_shot.c.
// (herdado) CAPTURAS DO SOCIAL REDESENHADO (02/10/2026), sem rede e sem interacao:
//
//   fileira "Amigos assistindo" da home (amigosfil.c) com 0, 1 e 3 amigos e
//   com um amigo ao vivo — o rosto em foco com os cartoes abertos e o foco
//   dentro do primeiro cartao;
//   painel da tecla azul: aba Atividade (tela B) e aba Amigos (tela A);
//   perfil do amigo (tela C, amigoperfil.c).
//
// Os dados sao os de exemplo de socialvis.c (socialvis_demo, so com
// -DNV_SOCIALVIS_DEMO); a arte e local (deploy/app/art). recomenda.c e
// INCLUIDO, como em tests/social_shot.c, para semear contatos por dentro.
//
//   bash tests/socialui_shot.sh /tmp/nv-socialui
#include "../src/rede.c"
#include "celular.h"
#include "celbotao.h"
#include "recenviar.h"
static int phoneOpen;
int celular_disponivel(void) { return 1; }
int celular_abrir(const char *t) { (void)t; phoneOpen = 1; return 1; }
void celular_fechar(void) { phoneOpen = 0; }
int celular_estado(void) { return phoneOpen ? CEL_ESPERANDO : CEL_PARADO; }
const char *celular_url(void) { return phoneOpen ? "http://192.0.2.1/fixture" : ""; }
int celular_pegar(char *d, size_t n) { (void)d; (void)n; return 0; }
int celular_porta(void) { return 0; }
#include "addons.h"
#define NV_REC_URL "http://127.0.0.1:8799"
// A CONTA TRAKT E O LOGIN DO SIMKL NA TV, so para as linhas de "Contas
// ligadas" (o resto do app continua com os de verdade, sem login).
#define trakt_ativo     shot_trakt_ativo
#define sessao_token_copiar shot_sessao_token_copiar
#define sessao_logada shot_sessao_logada
#define simklauth_token shot_simklauth_token
#include "../src/recomenda.c"
#include "nvtmp.h"
#undef trakt_ativo
#undef sessao_token_copiar
#undef sessao_logada
#undef simklauth_token
static int contasNaTv;
int shot_trakt_ativo(void) { return contasNaTv; }
// sessao.c (#203) entrega o token por copia.
int shot_sessao_token_copiar(char *d, size_t n) { snprintf(d, n, "%s", contasNaTv ? "tok-shot" : ""); return contasNaTv; }
int shot_sessao_logada(void) { return contasNaTv; }
const char *shot_simklauth_token(void) { return contasNaTv ? "simkl-shot" : ""; }
#include "ajustes.h"
#include "amigoperfil.h"
#include "amigosfil.h"
#include "idioma.h"
#include "idiomacod.h"
#include "catalogo.h"
#include "dados.h"
#include "gfx.h"
#include "home.h"
#include "layout.h"
#include "salvospainel.h"
#include "agenda.h"
#include "avisos.h"
#include "ctxmenu.h"
#include "reacao.h"
#include "recresp.h"
#include "socialvis.h"
#include "svdesenho.h"
#include "tex_cache.h"
#include "text.h"
#include "shot_arte.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "gl_compat.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// O ALCANCE E O NOME sao os de verdade de recomenda.c (a ponte V2 ligou, e os
// stubs que moravam aqui passaram a redefinir as funcoes dela). O nome do
// perfil e semeado direto na estatica, como os contatos.

static SDL_Window *janela;
static const char *dirDados;
enum { D_HOME = 0, D_PAINEL, D_PERFIL };
static int desenho;

#ifdef NV_AMIGOS_POLISH
static int polishAvatar;
static void polishPixel(SDL_Surface *s, int x, int y, int *c) {
  unsigned char *p = (unsigned char *)s->pixels + y * s->pitch + x * 4;
  c[0] = p[0]; c[1] = p[1]; c[2] = p[2];
}
// Focused first column: the standard home focus ring (accent, NV_ANEL_FOCO thick) hugs the card.
static void polishFoco(SDL_Surface *s, const char *bmp) {
  const char *k = strstr(bmp, "-9");
  GfxRect r;
  float ar, ag, ab;
  int c[3];
  if (!k || k[2] < 'A' || k[2] > 'E' || strstr(bmp, "-sem")) return;
  assert(amigosfil_teste_cartao(0, &r) != -1);
  ajustes_acento(&ar, &ag, &ab);
  for (int d = 1; d <= 3; d++) {
    polishPixel(s, (int)r.x - d, (int)(r.y + r.h * 0.5f), c);
    if (abs(c[0] - (int)(ar * 255)) > 40 || abs(c[1] - (int)(ag * 255)) > 40 || abs(c[2] - (int)(ab * 255)) > 40) {
      fprintf(stderr, "%s: no home focus ring %d px left of the card (%d,%d,%d)\n", bmp, d, c[0], c[1], c[2]);
      abort();
    }
  }
}
#endif

static void gravarBmp(const char *bmp) {
  unsigned char *pix = malloc(1920 * 1080 * 4);
  SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  int y;
  assert(pix && s);
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  for (y = 0; y < 1080; y++)
    memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
  assert(SDL_SaveBMP(s, bmp) == 0);
#ifdef NV_AMIGOS_POLISH
  polishFoco(s, bmp);
  if (polishAvatar) {   // an avatar whose image is missing falls back to the coloured initial disc
    int c[3];
    polishPixel(s, 52, 88, c);
    if (c[0] < 40 && c[1] < 40 && c[2] < 40) { fprintf(stderr, "avatar fallback is a black disc\n"); abort(); }
    if (abs(c[0] - c[1]) + abs(c[1] - c[2]) < 30) { fprintf(stderr, "avatar fallback is a grey disc\n"); abort(); }
  }
#endif
#ifdef NV_AMIGOSFIL_TESTE
  if (strstr(bmp, "-9F")) for (int example = 0; example < 2; example++) {
    int low = 255, high = 0, caption = 0;
    int offset = example * 536;
    // Sample each example's art, away from badges and labels.
    for (int yy = 690; yy < 780; yy++) for (int xx = 820 + offset; xx < 1220 + offset; xx++) {
      unsigned char *p = (unsigned char *)s->pixels + yy * s->pitch + xx * 4;
      if (p[0] < low) low = p[0];
      if (p[0] > high) high = p[0];
    }
    assert(high - low > 20);
    for (int yy = 870; yy < 920; yy++) for (int xx = 780 + offset; xx < 1250 + offset; xx++) {
      unsigned char *p = (unsigned char *)s->pixels + yy * s->pitch + xx * 4;
      if (p[0] > 60 && p[1] > 60 && p[2] > 60) caption++;
    }
    assert(caption > 100);
  }
#endif
  // The art of every title card must cover the WHOLE card: the strips at its left
  // and right edges (clear of avatar and caption) cannot be one flat bar colour.
#ifndef NV_ANTES
  if (strstr(bmp, "-feed")) {
    int checked = 0;
    for (int c = 0; c < 8; c++) {
      GfxRect r;
      if (amigosfil_teste_cartao(c, &r) != 1 || r.x < 0 || r.x + r.w > 1920) continue;
      for (int side = 0; side < 2; side++) {
        // Rows above the veil, columns past the avatar: where the old contain-fit left
        // its flat side bars (~131 px each side of a 157 px wide poster).
        int lo = 255, hi = 0, x0 = (int)(side ? r.x + r.w - 90 : r.x + 70);
        for (int yy = (int)(r.y + r.h * 0.12f); yy < (int)(r.y + r.h * 0.28f); yy++)
          for (int xx = x0; xx < x0 + 20; xx++) {
            unsigned char *p = (unsigned char *)s->pixels + yy * s->pitch + xx * 4;
            for (int ch = 0; ch < 3; ch++) { if (p[ch] < lo) lo = p[ch]; if (p[ch] > hi) hi = p[ch]; }
          }
        if (hi - lo <= 6) { fprintf(stderr, "card %d %s edge is a flat bar (%d..%d)\n", c, side ? "right" : "left", lo, hi); abort(); }
      }
      checked++;
    }
    assert(checked > 0);
  }
#endif
  SDL_FreeSurface(s);
  free(pix);
  printf("captura: %s\n", bmp);
}

static void quadros(int n, const char *bmp) {
  int i;
  for (i = 0; i < n; i++) {
    Uint32 agora = SDL_GetTicks();
    SDL_PumpEvents();
    tex_bombear(8);
    home_atualizar(1.0f / 60.0f, agora);
    celb_atualizar(1.0f / 60.0f);
    spainel_atualizar(1.0f / 60.0f, agora);
    ctx_atualizar(1.0f / 60.0f, agora);
    if (desenho == D_PERFIL) amigoperfil_atualizar(1.0f / 60.0f, agora);
    txt_novo_quadro();
    tex_novo_quadro();
    gfx_novo_quadro();
    glClearColor(NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    if (desenho == D_PERFIL) amigoperfil_desenhar(agora);
    else {
      home_desenhar(agora);
#ifdef NV_AMIGOS_POLISH
      if (polishAvatar) {
        gfx_cor((GfxRect){20, 20, 160, 160}, 0, 0, 0, 0, 1);
        svd_avatar((GfxRect){40, 40, 96, 96}, "/nonexistent/avatar.jpg", "Zed", "id-zed", 1);
      }
#endif
      if (desenho == D_PAINEL) spainel_desenhar(agora);
      if (desenho == D_PAINEL && ctx_aberto()) ctx_desenhar(agora);
    }
    if (bmp && i == n - 1) gravarBmp(bmp);
    SDL_GL_SwapWindow(janela);
    SDL_Delay(4);
  }
}

static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = k;
  home_evento(&e);
  e.type = SDL_KEYUP;
  home_evento(&e);
}

// Leva o foco da aba Amigos a linha do tipo `nome` (spainel_foco_social).
static void focar(const char *nome) {
  SDL_Event e;
  int k;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_UP;
  for (k = 0; k < 40; k++) spainel_evento(&e);      // as abas
  e.key.keysym.sym = SDLK_DOWN;
  for (k = 0; k < 60 && strcmp(spainel_foco_social(), nome); k++) spainel_evento(&e);
  if (strcmp(spainel_foco_social(), nome)) { fprintf(stderr, "sem linha %s\n", nome); exit(1); }
}
static void painelTecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = k;
  spainel_evento(&e);
  e.type = SDL_KEYUP;
  spainel_evento(&e);
}

static void ajusta(void) {
  char cam[700];
  FILE *a;
  snprintf(cam, sizeof cam, "%s/ajustes.txt", dirDados);
  a = fopen(cam, "w");
  assert(a);
  fprintf(a, "idioma %d\ntrailerHero 1\nhomeLayoutLocal %d\nselected_theme %d\n",
          getenv("NUVIO_SHOT_EN") ? 1 : 0,
          getenv("NV_LAYOUT") ? atoi(getenv("NV_LAYOUT")) : 1,
          getenv("NV_TEMA") ? atoi(getenv("NV_TEMA")) : 9);
  // NUVIO_SHOT_FONTE=3: Montserrat, a fonte da interface da TV do dono.
  if (getenv("NUVIO_SHOT_FONTE")) fprintf(a, "fonteInterface %d\n", atoi(getenv("NUVIO_SHOT_FONTE")));
  shot_arte_material(a);   // NUVIO_SHOT_VIDRO=0: o painel no material solido
  fclose(a);
  ajustes_dir(dirDados);
}

static void contato(int i, const char *id, const char *nome, const char *av, const char *orig) {
  memset(&contatos[i], 0, sizeof contatos[i]);
  snprintf(contatos[i].id, sizeof contatos[i].id, "%s", id);
  snprintf(contatos[i].nome, sizeof contatos[i].nome, "%s", nome);
  snprintf(contatos[i].avatar, sizeof contatos[i].avatar, "%s", av);
  snprintf(contatos[i].origem, sizeof contatos[i].origem, "%s", orig);
}

// Uma recomendacao recebida de Pedro, de um titulo que eu ja comecei (o
// catalogo tem progresso nele): prova a cadeia "Voce comecou › ...".
static void semearRec(void) {
  RecItem *r = &itens[0];
  memset(r, 0, sizeof *r);
  r->id = 1; r->criado = (long long)time(NULL) - 7200;
  snprintf(r->de, sizeof r->de, "nuvio:pedro");
  snprintf(r->deNome, sizeof r->deNome, "Pedro");
  snprintf(r->deAvatar, sizeof r->deAvatar, "deploy/app/art/elenco/00_0.jpg");
  snprintf(r->imdb, sizeof r->imdb, "tt9000002");
  snprintf(r->tipo, sizeof r->tipo, "movie");
  snprintf(r->titulo, sizeof r->titulo, "Titulo 2");
  snprintf(r->poster, sizeof r->poster, "deploy/app/art/poster/22.jpg");
  r->modelo = -1;
  snprintf(r->texto, sizeof r->texto, "Terminei, sua vez");
  r->nota = 81; r->visto = 1;
  nItens = 1;
}

// Leva o foco a fileira de amigos (a segunda: Continuar e ela).
static void irFileira(void) {
  int r;
  home_ir_topo();
  for (r = 0; r < 14; r++) tecla(SDLK_UP);
  quadros(20, NULL);
  for (int k = 0; k < 8; k++) {
    tecla(SDLK_DOWN);
    quadros(3, NULL);
    char tipo[32]; snprintf(tipo, sizeof tipo, "tipo=%d ", FILEIRA_SOCIAL);
    if (strstr(home_rastro_foco(), tipo)) return;
  }
  fprintf(stderr, "social row unreachable: %s\n", home_rastro_foco());
  abort();
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : nvt("/nv-abas/s");
  static CatItem itens[40];
  static CatFileira fils[3];
  SDL_GLContext gl;
  char bmp[800], cache[700];
  int i, cen;
  

  pronto = -1; // Refuse libcurl initialization, including background prefetch.
  dados_iniciar("deploy/app/art");
  dirDados = dados_dir();
  if (!dirDados || !strstr(dirDados, "nuvio-abas-shot")) {
    fprintf(stderr, "recuse: NUVIO_DADOS tem de ser a pasta temporaria do teste\n");
    return 1;
  }
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  janela = SDL_CreateWindow("Nuvio: abas", 0, 0, 1920, 1080,
                            SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(janela);
  gl = SDL_GL_CreateContext(janela);
  assert(gl);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(192);
  snprintf(cache, sizeof cache, "%s/cache", dirDados);
  tex_cache_dir(cache);
  gfx_icones_dir("deploy/app/art");
  ajusta();
  assert(home_iniciar("deploy/app/art"));
  // No workers or network: contacts and events are seeded below.
  aparecer = REC_APARECER_SIM;
  snprintf(meuNome, sizeof meuNome, "%s", "Henrique");
  snprintf(meuCodigo, sizeof meuCodigo, "%s", "uv8scv");

  // Duas fileiras de catalogo: Continuar e Populares. A de amigos entra
  // sozinha (home.c poe a fileira social na posicao 1).
  memset(itens, 0, sizeof itens);
  memset(fils, 0, sizeof fils);
  for (i = 0; i < 20; i++) {
    CatItem *c = &itens[i];
    snprintf(c->imdb, sizeof c->imdb, "tt90%05d", i);
    snprintf(c->tipo, sizeof c->tipo, "movie");
    snprintf(c->titulo, sizeof c->titulo, "Titulo %d", i);
    snprintf(c->backdrop, sizeof c->backdrop, "deploy/app/art/%02d.jpg", 20 + i);
    snprintf(c->poster, sizeof c->poster, "deploy/app/art/poster/%02d.jpg", 20 + i);
    if (i < 6) { c->progresso = 30 + i * 8; c->restanteMin = 40; }
    // As quatro seguintes sao series na lista: a Agenda parte delas.
    if (i >= 1 && i <= 4) { snprintf(c->tipo, sizeof c->tipo, "series"); c->naLista = 1; }
  }
  snprintf(fils[0].chave, sizeof fils[0].chave, "continue_watching");
  snprintf(fils[0].titulo, sizeof fils[0].titulo, "Continuar assistindo");
  snprintf(fils[0].tipo, sizeof fils[0].tipo, "movie");
  fils[0].ini = 0; fils[0].n = 6;
  snprintf(fils[1].chave, sizeof fils[1].chave, "pop_movie");
  snprintf(fils[1].titulo, sizeof fils[1].titulo, "Popular - Filme");
  snprintf(fils[1].tipo, sizeof fils[1].tipo, "movie");
  fils[1].ini = 6; fils[1].n = 14;
  cat_definir_tudo(itens, 20, fils, 2);
  quadros(30, NULL);


  AddonRemoto addon = { .nome = "Fixture", .url = "http://127.0.0.1:1/manifest.json", .ativo = 1 };
  assert(addons_definir_lista(&addon, 1));
#ifndef NV_AMIGOSFIL_TESTE
  int classicOp = ajustes_rapido_op("friendsRowStyleLocal");
  if (classicOp >= 0) for (int k = 0; k < 5; k++) ajustes_rapido_passo(classicOp, 1);
#endif
  nContatos = 0;
  contato(nContatos++, "nuvio:pedro", "Pedro", "deploy/app/art/elenco/00_0.jpg", "nuvio");
  contato(nContatos++, "nuvio:marina", "Marina", "", "nuvio");
  contato(nContatos++, "trakt:vlern", "vlern", "", "trakt");
  socialvis_demo(4);
  desenho = D_HOME;
#ifdef NV_AMIGOSFIL_TESTE
  assert(ajustes_amigos_estilo() == 0);
  int op = ajustes_rapido_op("friendsRowStyleLocal");
  assert(op >= 0);
  for (int style = 0; style < 6; style++) {
    if (style) assert(ajustes_rapido_passo(op, 1));
    assert(ajustes_amigos_estilo() == style);
    ajustes_dir(dirDados);
    assert(ajustes_amigos_estilo() == style);
    irFileira();
    quadros(100, NULL);
    if (style == 5) snprintf(bmp, sizeof bmp, "%s-classic.bmp", saida);
    else snprintf(bmp, sizeof bmp, "%s-9%c.bmp", saida, 'A' + style);
    quadros(1, bmp);
    amigosfil_teste_conferir(1920, 1080);
    // Unfocused: focus on the hero, the row stays on screen.
    tecla(SDLK_UP); quadros(60, NULL);
    snprintf(bmp, sizeof bmp, "%s-%s-sem.bmp", saida, style == 5 ? "classic" : (char[]){'9', (char)('A' + style), 0});
    quadros(1, bmp);
    irFileira(); quadros(40, NULL);
#ifdef NV_AMIGOS_POLISH
    if (style == 5) {   // no "+ Add" column: one column per friend, RIGHT at the last avatar stops
      int nf = socialvis_n_amigos(), c = 0, k;
      assert(amigosfil_n_colunas() == nf);
      for (k = 0; k < 64 && amigosfil_tecla(SDLK_RIGHT, &c); k++) {}
      assert(k < 64 && c == nf - 1);
    }
#endif
    if (style < 5 && !getenv("NV_SHOT_NOWALK")) {   // NOWALK: captures of an older tree, no navigation asserts
      int nc = amigosfil_n_colunas();
      char title[24], pessoa[96];
      amigosfil_ok(0);
      assert(amigosfil_pediu_titulo(title, sizeof title));
      for (int c = 1; c < nc; c++) {
        tecla(SDLK_RIGHT); quadros(35, NULL);
        amigosfil_teste_conferir(1920, 1080);
      }
      // The last column is a card: no "See all" / "+ Add" pill after it.
#ifdef NV_AMIGOS_POLISH
      { GfxRect lastRect; assert(amigosfil_teste_cartao(nc - 1, &lastRect) != -1); }
#endif
      amigosfil_ok(nc - 1);
      assert(amigosfil_pediu_titulo(title, sizeof title) || amigosfil_pediu_perfil(pessoa, sizeof pessoa));
      assert(!amigosfil_pediu_ajustes() && !recenviar_aberto());
    }
  }
#ifdef NV_AMIGOS_POLISH
  { char tmp[900]; snprintf(tmp, sizeof tmp, "%s/avatar-fallback.bmp", dirDados); polishAvatar = 1; quadros(2, tmp); polishAvatar = 0; }
#endif
  assert(ajustes_rapido_passo(op, 1));
  // Today-only events: absent time/progress/reaction must stay absent.
  static SvEvento missing[4];
  for (int k = 0; k < 4; k++) {
    const SvAmigo *a = socialvis_amigo(k ? 1 : 0);
    missing[k] = a->tit[0];
    snprintf(missing[k].imdb, sizeof missing[k].imdb, "ttmissing%d", k);
    missing[k].acao = k == 2 ? SV_AGORA : SV_FIM;
    missing[k].quando = 0; missing[k].pct = -1;
    missing[k].restanteMin = -1; missing[k].reacao = SV_REAC_NADA;
    missing[k].sobreMinhaRec = 0;
    char q[80]; socialvis_quando(missing[k].quando, q, sizeof q); assert(!q[0]);
    char status[160]; socialvis_status(&missing[k], status, sizeof status);
    assert(!strstr(status, "%") && !strstr(status, " min"));
  }
  socialvis_definir_feed(missing, 4);
  irFileira(); quadros(90, NULL);
  amigosfil_teste_conferir(1920, 1080);
  snprintf(bmp, sizeof bmp, "%s-missing.bmp", saida); quadros(1, bmp);
  // Owner's case: titles with no landscape art, only a poster, plus friends with no
  // title at all (people cards). Every card shell must fill its frame.
  for (int k = 0; k < 2; k++) {
    snprintf(missing[k].imdb, sizeof missing[k].imdb, "ttposter%d", k);
    missing[k].arte[0] = 0;
    missing[k].acao = SV_FIM;
    snprintf(missing[k].pessoaId, sizeof missing[k].pessoaId, "nuvio:pedro");
    snprintf(missing[k].pessoaNome, sizeof missing[k].pessoaNome, "Pedro");
    snprintf(missing[k].poster, sizeof missing[k].poster, "deploy/app/art/poster/%02d.jpg", 3 + k);
  }
  socialvis_definir_feed(missing, 2);
  irFileira(); quadros(90, NULL);
  snprintf(bmp, sizeof bmp, "%s-feed.bmp", saida); quadros(1, bmp);
  tecla(SDLK_RIGHT); tecla(SDLK_RIGHT); quadros(40, NULL);
  snprintf(bmp, sizeof bmp, "%s-pessoas.bmp", saida); quadros(1, bmp);
  const int languages[] = { IDIOMA_DE, IDIOMA_PT, IDIOMA_RU };
  for (int k = 0; k < 3; k++) {
    char cam[800]; snprintf(cam, sizeof cam, "%s/ajustes.txt", dirDados);
    FILE *file = fopen(cam, "w"); assert(file);
    fprintf(file, "idioma %d\nfriendsRowStyleLocal 0\n", languages[k]); fclose(file);
    ajustes_dir(dirDados);
    assert(ajustes_idioma() == languages[k]);
    const char *labels[] = {"Feed de cartões", "Ao vivo em destaque", "Histórias com cartaz", "Faixa de presença", "Cartaz primeiro", "Rostos clássicos"};
    for (int j = 0; j < 6; j++) {
      TxtLinha t = txt_linha(TXT_CAPTION, i18n(labels[j]), 255, 255, 255, 255);
      assert(t.w < 600);
    }
    TxtLinha label = txt_linha(TXT_CAPTION, i18n("Estilo da fileira de amigos"), 255, 255, 255, 255);
    assert(label.w < 600);
    irFileira(); quadros(70, NULL);
    snprintf(bmp, sizeof bmp, "%s-lang-%d.bmp", saida, languages[k]); quadros(1, bmp);
    amigosfil_teste_conferir(1920, 1080);
  }
  nContatos = 0;
  socialvis_demo(0);
  for (int lang = 0; lang < 2; lang++) {
    char cam[800]; snprintf(cam, sizeof cam, "%s/ajustes.txt", dirDados);
    FILE *file = fopen(cam, "w"); assert(file);
    fprintf(file, "idioma %d\nfriendsRowStyleLocal 0\n", lang ? IDIOMA_PT : IDIOMA_EN);
    fclose(file); ajustes_dir(dirDados);
    irFileira(); quadros(100, NULL);
    assert(amigosfil_n_colunas() == 1);
    snprintf(bmp, sizeof bmp, lang ? "%s-9F-pt.bmp" : "%s-9F.bmp", saida);
    quadros(1, bmp);
    amigosfil_teste_conferir(1920, 1080);
    for (int k = 0; k < 3; k++) tecla(SDLK_RIGHT);
    quadros(35, NULL);
    assert(amigosfil_n_colunas() == 1);
    assert(amigosfil_indice_cat(1) == -1);
    amigosfil_teste_conferir(1920, 1080);
  }
#else
  irFileira();
  quadros(100, NULL);
  snprintf(bmp, sizeof bmp, "%s-classic.bmp", saida);
  quadros(1, bmp);
#endif
  tex_encerrar();
  txt_encerrar();
  gfx_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(janela);
  IMG_Quit();
  SDL_Quit();
  return 0;
}
