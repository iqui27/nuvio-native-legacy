#include "pausao.h"
#include "catalogo.h"
#include "ajustes.h"
#include "gfx.h"
#include "text.h"
#include "anim.h"
#include "layout.h"
#include "telefoneui.h"
#include "idioma.h"
#include "relogiofim.h"
#include "plrui.h"
#include "plrilha.h"
#include "artehero.h"
#include "tex_cache.h"
#define NV_ESCALA_TELA   // o arquivo inteiro mede pela tela virtual (escala.h)
#include "escala.h"
#include "ponteiro.h"
#include <time.h>
#include <stdio.h>
#include <string.h>

// Os cinco segundos do web (playerScreen.js:567). Nao e um numero de gosto: e o
// que separa "parei um instante" de "parei para ler". Encurtar faz o painel
// pular na cara de quem so ajustou o volume.
#define PAUSAO_ESPERA_MS  5000u

// GLASS UI (mockup aprovado em 03/10, quadro "pausa"): o veu e um degrade da
// ESQUERDA (sai o .34 da tela inteira), o titulo e o LOGO, o elenco ganha
// rosto e a barra e a MESMA do OSD (plrui_barra), na margem de 96. A hora vai
// para a pilula da ilha, que diz "Pausado". Tudo ancorado na base: o bloco
// termina a 150 px dela.
#define PAUSAO_X          96.0f    // mesmo recuo do conteudo do player (PLR_MARGEM)
#define PAUSAO_BASE      150.0f    // margem inferior da ficha
#define PAUSAO_LARG      900.0f    // largura da sinopse
#define PAUSAO_LD_SIN     36.0f    // 23 px x 1,55
#define PAUSAO_SIN_LINHAS     3
#define PAUSAO_CHIP_H     56.0f
#define PAUSAO_BARRA_Y   (NV_TELA_H - 90.0f)   // 990 em 1080; tela virtual (escala.h)

// Quantos nomes de elenco cabem. O web para em oito (:568); aqui o teto e o do
// dado, nao o do layout: CatItem guarda seis.
#define PAUSAO_ELENCO_MAX 6

static int    visivel;
static float  anim;            // 0..1, a entrada por mola
static Uint32 desdeQuando;     // quando a condicao passou a valer; 0 = nao vale
static int    idxItem = -1;
static char   idItem[64];      // o titulo de idxItem (#190; ver pausao.h)
static char   epLinha[220];

// See pausao.h. Clearing `desdeQuando` makes pausao_atualizar re-arm from NOW
// on the next frame, which is exactly what the web's schedulePauseOverlay() does.
void pausao_activity(void) {
  desdeQuando = 0;
}

void pausao_fechar(void) {
  visivel = 0;
  anim = 0.0f;
  desdeQuando = 0;
  idxItem = -1;
  idItem[0] = 0;
  epLinha[0] = 0;
}

int pausao_indice(void) { return cat_indice_vivo(idxItem, idItem); }

void pausao_atualizar(float dt, Uint32 agora, int podeSubir, int idx,
                      const char *imdb, const char *linhaEp) {
  idxItem = idx;
  snprintf(idItem, sizeof idItem, "%s", imdb ? imdb : "");
  snprintf(epLinha, sizeof epLinha, "%s", linhaEp ? linhaEp : "");

  // O ajuste e consultado AQUI e nao na abertura: desligar a opcao com o painel
  // de pe tem de derrubar o painel, e nao valer so no filme seguinte.
  if (!podeSubir || !ajustes_pausa_overlay()) {
    // schedulePauseOverlay/syncPauseOverlayState (:7397): condicao que cai
    // derruba o painel e ZERA o relogio. Rearmar de onde parou faria uma
    // sequencia de pausas curtas somar cinco segundos e o painel subir sozinho
    // no meio de uma cena.
    visivel = 0;
    desdeQuando = 0;
  } else {
    if (!desdeQuando) desdeQuando = agora;
    if (!visivel && agora - desdeQuando >= PAUSAO_ESPERA_MS) visivel = 1;
  }

  anim = anim_mola(anim, visivel ? 1.0f : 0.0f, dt,
                   visivel ? NV_MOLA_FOCO : NV_MOLA_DESFOCO);
  if (!visivel && anim < 0.004f) anim = 0.0f;
}

// Enquanto o painel ainda esta saindo ele continua desenhado, mas ja NAO e
// visivel para quem pergunta: se fosse, o player manteria os controles
// recolhidos durante a saida e a barra so voltaria depois do fade.
int pausao_visivel(void) { return visivel; }

int pausao_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (!visivel || !e || e->type != SDL_KEYDOWN) return PAUSAO_LIVRE;
  k = e->key.keysym.sym;

  // O Back NAO e tratado aqui, e essa e uma divergencia deliberada do web. La
  // (:22138) o Back derruba o painel e ainda segue para a regra seguinte;
  // aqui o Back e a unica saida da reproducao, e roubar o primeiro toque para
  // fechar um painel informativo faria a pessoa apertar duas vezes para sair de
  // um filme. Quem quer sair, sai.
  if (k == SDLK_ESCAPE || k == SDLK_AC_BACK || k == SDLK_BACKSPACE ||
      k == SDLK_DELETE) return PAUSAO_LIVRE;

  visivel = 0;
  desdeQuando = 0;

  // playerScreen.js:22212 — OK/Play com o painel de pe derruba o painel E
  // retoma. E o gesto obvio: quem esta olhando a ficha e aperta o centro quer
  // voltar ao filme, nao so fechar uma caixa.
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE)
    return PAUSAO_RETOMAR;

  // Qualquer outra tecla (:22218): derruba o painel e devolve os controles. O
  // relogio dos 5s recomeca sozinho no proximo `pausao_atualizar`, porque
  // `desdeQuando` foi zerado — que e o `schedulePauseOverlay()` do web.
  return PAUSAO_CONSUMIU;
}

// O SELO "Pausado" (pilula de 56 px com o icone). Publico desde 29/09/2026: o
// OSD do canal ao vivo pausado usa este mesmo selo, e nao um parecido.
float pausao_selo(float x, float y, int direita, float a) {
  TxtLinha lp = txt_linha(TXT_PLR_CORPO, "Pausado", 246, 247, 250, 255);
  float d = PAUSAO_SELO_H, pw = d + 18.0f + (float)lp.w + 26.0f;
  GfxRect pil, ic;
  if (direita) x -= pw;
  pil = (GfxRect){ x, y, pw, d };
  ic  = (GfxRect){ x + 8.0f, y + 8.0f, d - 16.0f, d - 16.0f };
  if (ajustes_vidro()) gfx_vidro_painel(pil, 0.5f, 0.55f, a);
  else                 gfx_cor(pil, 0.5f, 1, 1, 1, 0.16f * a);
  gfx_icone(ic, "pause", 0.96f, 0.96f, 0.96f, 0.94f * a);
  txt_desenhar_alpha(lp, x + d + 6.0f, y + (d - (float)lp.h) * 0.5f, a);
  return pw;
}


// Os campos da meta do catalogo com o ponto de 4 px entre eles (21/400 a 66%).
static float metaPontos(const char *meta, float x, float y, float maxW, float a) {
  char m[192];
  char *q = m, *f;
  float x0 = x;
  int prim = 1;
  snprintf(m, sizeof m, "%s", meta);
  while (q && *q) {
    TxtLinha l;
    f = strstr(q, " \xc2\xb7 ");
    if (f) *f = 0;
    if (!*q) { q = f ? f + 4 : NULL; continue; }   // campo vazio: sem ponto solto
    l = txt_linha(TXT_CAPTION2, q, 243, 242, 239, 168);
    if (!prim) {
      gfx_cor((GfxRect){ x + 10.0f, y + l.h * 0.5f - 2.0f, 4.0f, 4.0f }, 0.5f, 0.953f, 0.949f, 0.937f, 0.40f * a);
      x += 24.0f;
    }
    if (x + l.w > x0 + maxW) break;
    txt_desenhar_alpha(l, x, y, a);
    x += l.w; prim = 0;
    q = f ? f + 4 : NULL;
  }
  return (float)txt_linha(TXT_CAPTION2, "Ag", 0, 0, 0, 255).h;
}

// PONTEIRO (#99): a ficha nao tem item focavel (o player para de registrar os
// controles com ela no ar). Um alvo na tela inteira, com focar que nao mexe em
// nada, faz o clique ser o OK de sempre (pausao_evento: derruba a ficha e
// retoma, o gesto do web) mesmo com a pilula da ilha registrando o dela.
static void ponteiroFicha(int a, int b) { (void)a; (void)b; }

static void pausao_desenharCorpo_(Uint32 agora, const PausaoCena *cena);
// Camada ampliada (escala.h): o corpo desenha na tela virtual.
void pausao_desenhar(Uint32 agora, const PausaoCena *cena) {
  ESCALA_INI();
  pausao_desenharCorpo_(agora, cena);
  ESCALA_FIM();
}
static void pausao_desenharCorpo_(Uint32 agora, const PausaoCena *cena) {
  const CatItem *c;
  float a = anim, y, sobe, alt = 0.0f, hSin = 0.0f, hMeta = 0.0f, lw = 0.0f, lh = 0.0f;
  const char *marca;
  GLuint logo = 0;
  TxtLinha lEp = { 0 };
  int temEp = 0, temCast, i;
  (void)agora;

  if (a <= 0.004f) return;
  if (visivel) ponteiro_alvo(0, 0, NV_TELA_W, NV_TELA_H, ponteiroFicha, NULL, 0, 0);
  // PELO TITULO, e nao so pelo indice (#190): uma troca de bloco entre o
  // pausao_atualizar e este desenho poe outro titulo na mesma posicao.
  { int i = cat_indice_vivo(idxItem, idItem);
    if (i < 0) return;
    idxItem = i; }
  c = cat_item(idxItem);
  if (!c) return;

  // --- VEUS: o degrade da esquerda (.78 -> 0 a 80%) e o leve de baixo, pelo
  // shader com dither. A arte continua viva a direita.
  gfx_veu_css((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 2, 0.0f, 0.80f, 0.78f * a);
  gfx_veu_css((GfxRect){ 0, NV_TELA_H - 300.0f, NV_TELA_W, 300.0f }, 0, 1.0f, 1.0f, 0.60f * a);

  // --- A PILULA DA ILHA diz "Pausado" (hora e "termina as" vem do player) ----
  { PlrIlhaPedido p;
    memset(&p, 0, sizeof p);
    p.icone = "pl_pause-f"; p.texto = i18n("Pausado");
    plrilha_pedir(&p); }

  // --- FICHA: medida antes, ancorada na base -----------------------------------
  marca = artehero_logo_sessao(c);
  logo = marca ? tex_obter_larg_qualquer(marca, 420) : 0;
  if (logo) {
    float ar = tex_aspecto(marca);
    lw = 420.0f; lh = ar > 0.0f ? lw / ar : 120.0f;
    if (lh > 120.0f) { lh = 120.0f; lw = lh * ar; }
  } else lh = (float)txt_linha_corta(TXT_TITULO2, c->titulo, 255, 255, 255, 255, PAUSAO_LARG).h;
  alt = 18.0f + 18.0f + lh;                         // kicker + vao + logo
  if (epLinha[0]) {
    lEp = txt_linha_corta(TXT_G30B, epLinha, 243, 242, 239, 255, PAUSAO_LARG);
    temEp = 1;
    alt += 20.0f + (float)lEp.h;
  }
  if (c->meta[0]) { hMeta = 25.0f; alt += 12.0f + hMeta; }
  if (c->sinopse[0]) {
    hSin = txt_bloco_corta(TXT_DET_META2, c->sinopse, 0, 0, 0, -1.0f, 0.0f,
                           PAUSAO_LARG, PAUSAO_LD_SIN, 0.0f, PAUSAO_SIN_LINHAS);
    alt += 16.0f + hSin;
  }
  temCast = c->nElenco > 0;
  if (temCast) alt += 28.0f + 18.0f + 14.0f + PAUSAO_CHIP_H;

  // Sobe 24px entrando. E o unico movimento do painel.
  sobe = (1.0f - a) * 24.0f;
  y = NV_TELA_H - PAUSAO_BASE - alt + sobe;

  plrui_kicker("Você está assistindo", PAUSAO_X, y, 243, 242, 239, a * 0.62f);
  y += 18.0f + 18.0f;
  if (logo) {
    gfx_rect((GfxRect){ PAUSAO_X, y, lw, lh }, logo,
             tex_marca_escura(marca) ? GFX_MARCA : GFX_TEXTO, 0, 0, 0, 0, .95f, .95f, .97f, a);
  } else txt_desenhar_alpha(txt_linha_corta(TXT_TITULO2, c->titulo, 255, 255, 255, 255, PAUSAO_LARG), PAUSAO_X, y, a);
  y += lh;
  if (temEp) { y += 20.0f; txt_desenhar_alpha(lEp, PAUSAO_X, y, a); y += lEp.h; }
  if (hMeta > 0.0f) { y += 12.0f; metaPontos(c->meta, PAUSAO_X, y, PAUSAO_LARG, a); y += hMeta; }
  if (hSin > 0.0f) {
    y += 16.0f;
    txt_bloco_corta(TXT_DET_META2, c->sinopse, 243, 242, 239, PAUSAO_X, y, PAUSAO_LARG,
                    PAUSAO_LD_SIN, a * 0.78f, PAUSAO_SIN_LINHAS);
    y += hSin;
  }

  // ELENCO com rosto: pilulas no material da ilha, a foto em disco de 44.
  if (temCast) {
    float x = PAUSAO_X;
    y += 28.0f;
    plrui_kicker("Elenco", PAUSAO_X, y, 243, 242, 239, a * 0.45f);
    y += 18.0f + 14.0f;
    for (i = 0; i < c->nElenco && i < PAUSAO_ELENCO_MAX; i++) {
      TxtLinha l = txt_linha(TXT_G19M, c->elenco[i].nome, 243, 242, 239, 255);
      const char *foto = c->elenco[i].foto;
      GLuint tf = foto[0] ? tex_obter_larg(foto, 88.0f) : 0;
      float w = 6.0f + 44.0f + 12.0f + (float)l.w + 22.0f;
      GfxRect chip = { x, y, w, PAUSAO_CHIP_H }, ro = { x + 6.0f, y + 6.0f, 44.0f, 44.0f };
      if (x + w > NV_TELA_W - PAUSAO_X) break;   // uma fileira so
      plrui_material(chip, 28.0f, 0, a);
      if (tf) {
        gfx_tex_aspect_atual = tex_aspecto(foto);
        gfx_rect(ro, tf, GFX_CARD, 0, 0, 0, 0.5f, 0, 0, 0, a);
        gfx_tex_aspect_atual = 0.0f;
      } else gfx_cor(ro, 0.5f, NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G, NV_COR_ESQUELETO_B, a);
      txt_desenhar_alpha(l, x + 6.0f + 44.0f + 12.0f, y + (PAUSAO_CHIP_H - (float)l.h) * 0.5f, a);
      x += w + 12.0f;
    }
  }

  // --- BARRA: a MESMA do OSD, com o tempo acima dela a direita -----------------
  if (cena && cena->dur > 0.0f) {
    char t1[24], t2[32], d[24];
    plrui_tempo(t1, sizeof t1, cena->pos);
    plrui_tempo(d, sizeof d, cena->dur);
    snprintf(t2, sizeof t2, "/ %s", d);
    plrui_barra(PAUSAO_X, PAUSAO_BARRA_Y, NV_TELA_W - 2.0f * PAUSAO_X, cena->pos / cena->dur, 0.0f, 0, NULL, 0, a);
    { TxtLinha l2 = txt_linha(TXT_ILHA_NOME, t2, 243, 242, 239, 128);
      TxtLinha l1 = txt_linha(TXT_ILHA_NOME, t1, 243, 242, 239, 235);
      float xr = NV_TELA_W - PAUSAO_X - l2.w;
      float timeY = telefoneui_ativo() ? PAUSAO_BARRA_Y - 52.0f : 938.0f;
      txt_desenhar_alpha(l2, xr, timeY, a);
      txt_desenhar_alpha(l1, xr - 8.0f - l1.w, timeY, a); }
  }
}
