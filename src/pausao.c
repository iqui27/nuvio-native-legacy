#include "pausao.h"
#include "catalogo.h"
#include "ajustes.h"
#include "gfx.h"
#include "text.h"
#include "sinopse.h"
#include "anim.h"
#include "layout.h"
#include "telefoneui.h"
#include "idioma.h"
#include "relogiofim.h"
#include "plrui.h"
#include "plrilha.h"
#include "artehero.h"
#include "pausacom.h"
#include "extras.h"
#include "notasui.h"
#include "imdbnota.h"
#include "agendaui.h"
#include "comentordem.h"
#include "idiomacod.h"
#include "tex_cache.h"
#define NV_ESCALA_TELA   // o arquivo inteiro mede pela tela virtual (escala.h)
#include "escala.h"
#include "ponteiro.h"
#include <time.h>
#include <stdio.h>
#include <string.h>

// Os quatro segundos (o web usa cinco, playerScreen.js:567; o dono pediu 4).
// Nao e um numero de gosto: e o que separa "parei um instante" de "parei para ler". Encurtar faz o painel
// pular na cara de quem so ajustou o volume.
#define PAUSAO_ESPERA_MS  4000u

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
// NOTAS: todas as que o titulo tem (IMDb, TMDB, Rotten Tomatoes, Metacritic,
// Letterboxd...), numa linha propria embaixo da meta, com vao de 40 entre elas
// (a pagina do titulo usa 24; aqui o painel e mais calmo). Sem caber, cai a de
// menor prioridade, nunca aperta.
#define PAUSAO_NOTAS_H    34.0f
#define PAUSAO_NOTAS_VAO  40.0f
#define PAUSAO_BARRA_Y   (NV_TELA_H - 90.0f)   // 990 em 1080; tela virtual (escala.h)

// Quantos nomes de elenco cabem: o que o CatItem guarda; o layout corta pela
// largura da tela.
#define PAUSAO_ELENCO_MAX CAT_ELENCO_MAX
// ELENCO e COMENTARIOS falam a lingua da pagina do titulo (detail.c: desenhaElenco
// e desenhaComentarios): avatar redondo com nome e papel centralizados; cartao de
// vidro com usuario, texto, e o rodape "9/10   17 curtidas" (mesmas cores, mesmas
// fontes, mesmas frases do i18n). Os comentarios giram a cada 6,5 s com fade nas
// pontas — sem interacao, o painel e so para olhar.
#define PAUSAO_FOTO       112.0f
#define PAUSAO_FOTO_VAO    38.0f
#define PAUSAO_CARTA_H    (PAUSAO_FOTO + 10.0f + 26.0f + 24.0f)
#define PAUSAO_COM_PAD     24.0f
#define PAUSAO_COM_LINHAS      2
#define PAUSAO_COM_H      142.0f   // 24 + 2 x 30 + 12 + rodape 22 + 24
#define PAUSAO_COM_MS    6500u
#define PAUSAO_COM_FADE   350.0f

static int    visivel;
static float  anim;            // 0..1, a entrada por mola
static Uint32 desdeQuando;     // quando a condicao passou a valer; 0 = nao vale
static Uint32 comT0;           // quando o painel subiu: relogio da rotacao dos comentarios
static int    comProntos;      // havia comentarios quando o painel subiu (nao pula depois)
static int    idxItem = -1;
static char   idItem[64];      // o titulo de idxItem (#190; ver pausao.h)
static char   epLinha[220];
static int    epTemp, epEp;  // temporada/episodio em cena (0 = filme)
static char   epSinopse[420];  // sinopse do EPISODIO; vazia cai na da serie

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
  epSinopse[0] = 0;
}

int pausao_indice(void) { return cat_indice_vivo(idxItem, idItem); }

void pausao_atualizar(float dt, Uint32 agora, int podeSubir, int idx,
                      const char *imdb, int temporada, int episodio,
                      const char *linhaEp, const char *sinopseEp) {
  idxItem = idx;
  snprintf(idItem, sizeof idItem, "%s", imdb ? imdb : "");
  snprintf(epLinha, sizeof epLinha, "%s", linhaEp ? linhaEp : "");
  snprintf(epSinopse, sizeof epSinopse, "%s", sinopseEp ? sinopseEp : "");
  epTemp = temporada; epEp = episodio;
  // As notas tambem sao pedidas na espera (resumo por IMDb, cache e fio proprios).
  if (podeSubir && ajustes_pausa_overlay() && ajustes_pausa_notas() && imdb && !strncmp(imdb, "tt", 2) && episodio <= 0) {
    const CatItem *c = cat_item(pausao_indice());
    if (c) extras_resumo_pedir(imdb, !strcmp(c->tipo, "series"), c->sinopse[0] == 0);
  }
  // Os comentarios sao pedidos enquanto o painel ainda espera os 4 s: quando
  // ele sobe ja estao la. Idempotente por chave (pausacom.h).
  // Sem imdb o pedido limpa a lista: comentario de outro titulo nunca fica.
  if (podeSubir && ajustes_pausa_overlay() && ajustes_pausa_comentarios()) {
    pausacom_definir_idioma(idioma_iso(ajustes_idioma()));
    pausacom_pedir(imdb, temporada, episodio);
  } else if (!ajustes_pausa_comentarios()) {
    pausacom_limpar();   // desligado com o painel de pe: nada fica na tela nem na memoria
  }

  // O ajuste e consultado AQUI e nao na abertura: desligar a opcao com o painel
  // de pe tem de derrubar o painel, e nao valer so no filme seguinte.
  if (!podeSubir || !ajustes_pausa_overlay()) {
    // schedulePauseOverlay/syncPauseOverlayState (:7397): condicao que cai
    // derruba o painel e ZERA o relogio. Rearmar de onde parou faria uma
    // sequencia de pausas curtas somar quatro segundos e o painel subir sozinho
    // no meio de uma cena.
    visivel = 0;
    desdeQuando = 0;
  } else {
    if (!desdeQuando) desdeQuando = agora;
    if (!visivel && agora - desdeQuando >= PAUSAO_ESPERA_MS) {
      visivel = 1; comT0 = agora;
      // So entra no painel o que ja chegou: comentario tardio faria a ficha
      // inteira subir 140 px no meio da leitura.
      comProntos = pausacom_n() > 0;
    }
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

  // player_evento dismisses this panel on Back before reaching this handler.
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
  const char *marca, *sin;
  GLuint logo = 0;
  TxtLinha lEp = { 0 };
  int temEp = 0, temCast, nCom, i;
  NotasPlano notas;

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
  { int f, cru[EX_NFONTES];
    unsigned char querer[EX_NFONTES];
    float disp = PAUSAO_LARG;
    memset(cru, 0, sizeof cru);
    for (f = 0; f < EX_NFONTES; f++)
      querer[f] = epEp > 0 ? ajustes_nota_episodio(f) : ajustes_nota_titulo(f);
    if (!ajustes_pausa_notas()) {
      /* desligado nas Opcoes: sem fileira, sem pedido */
    } else if (epEp > 0) {
      // EPISODIO: so o que existe POR EPISODIO (IMDb e TMDB, no catalogo). A nota
      // da serie nao e a do episodio, e uma fonte sem nota nao aparece.
      int k;
      for (k = 0; k < cat_n_episodios(idxItem); k++) {
        const CatEp *ep = cat_episodio(idxItem, k);
        if (ep && ep->temporada == epTemp && ep->episodio == epEp) {
          cru[EX_IMDB] = ep->notaImdb;      // x10, como o IMDb do titulo
          cru[EX_TMDB] = ep->nota * 10;     // a escala do site (79 = 7,9 -> 790)
          break;
        }
      }
    } else {
      ExResumo r;
      if (idItem[0] && extras_resumo_obter(idItem, &r))
        for (f = 0; f < EX_NFONTES; f++) cru[f] = r.cru[f];
      if (!cru[EX_IMDB] && ajustes_mdblist_fonte(EX_IMDB) && c->imdb[0])
        cru[EX_IMDB] = imdbnota_obter(c->imdb, c->nota, !strcmp(c->tipo, "series"));
    }
    notasui_modo_icone(1);
    // O plano encaixa com o vao de 24; com o de 40 pode passar: aperta o espaco
    // ate caber (cai uma nota), em vez de juntar as que ficam.
    do {
      notasui_planejar(&notas, cru, disp, 0.0f, querer);
      disp -= 16.0f;
    } while (notas.n > 0 && notasui_plano_largura(&notas, PAUSAO_NOTAS_VAO) > PAUSAO_LARG);
    notasui_modo_icone(0);
    if (notas.n > 0) alt += 14.0f + PAUSAO_NOTAS_H; }
  sin = epSinopse[0] ? epSinopse : c->sinopse;
  if (sin[0]) {
    hSin = sinopse_bloco_corta(TXT_DET_META2, sin, 0, 0, 0, -1.0f, 0.0f,
                           PAUSAO_LARG, PAUSAO_LD_SIN, 0.0f, PAUSAO_SIN_LINHAS);
    alt += 16.0f + hSin;
  }
  temCast = c->nElenco > 0 && ajustes_pausa_elenco();
  nCom = comProntos ? pausacom_n() : 0;
  if (nCom > 0) alt += 16.0f + PAUSAO_COM_H;
  if (temCast) alt += 28.0f + 18.0f + 14.0f + PAUSAO_CARTA_H;
  // Escala de interface/texto grande encolhe a tela virtual (escala.h): a ficha
  // REFLUI em vez de sair pelo topo — primeiro cai o comentario, depois o elenco.
  { float teto = NV_TELA_H - PAUSAO_BASE - 24.0f;
    if (alt > teto && nCom > 0) { alt -= 16.0f + PAUSAO_COM_H; nCom = 0; }
    if (alt > teto && temCast) { alt -= 28.0f + 18.0f + 14.0f + PAUSAO_CARTA_H; temCast = 0; } }

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
  if (notas.n > 0) {
    y += 14.0f;
    notasui_modo_icone(1);
    notasui_desenhar_linha_vao(&notas, PAUSAO_X, y + PAUSAO_NOTAS_H * 0.5f, PAUSAO_NOTAS_VAO, a);
    notasui_modo_icone(0);
    y += PAUSAO_NOTAS_H;
  }
  if (hSin > 0.0f) {
    y += 16.0f;
    sinopse_bloco_corta(TXT_DET_META2, sin, 243, 242, 239, PAUSAO_X, y, PAUSAO_LARG,
                    PAUSAO_LD_SIN, a * 0.78f, PAUSAO_SIN_LINHAS);
    y += hSin;
  }

  // COMENTARIO DO TRAKT: um por vez, o cartao tem altura fixa (duas linhas) para
  // o painel nao pular a cada troca. Sem comentarios nada e reservado. Desenho do
  // mockup da issue: disco do Trakt a esquerda, aspas, o texto e, embaixo, "— autor",
  // a estrela amarela das notas e os pontos da rotacao.
  if (nCom > 0) {
    Uint32 t = agora - comT0;
    int k = (int)((t / PAUSAO_COM_MS) % (Uint32)nCom);
    float fase = (float)(t % PAUSAO_COM_MS), af = 1.0f;
    PcComentario cm;
    if (nCom > 1 && fase < PAUSAO_COM_FADE) af = fase / PAUSAO_COM_FADE;
    else if (nCom > 1 && fase > (float)PAUSAO_COM_MS - PAUSAO_COM_FADE) af = ((float)PAUSAO_COM_MS - fase) / PAUSAO_COM_FADE;
    y += 16.0f;
    plrui_cartao((GfxRect){ PAUSAO_X, y, PAUSAO_LARG, PAUSAO_COM_H }, 20.0f, a);
    if (pausacom_obter(k, &cm)) {
      char rodape[64], autor[96];
      float px = PAUSAO_X + PAUSAO_COM_PAD, larg = PAUSAO_LARG - 2.0f * PAUSAO_COM_PAD;
      float disco = 44.0f, x0 = px + disco + 20.0f, tx = x0 + 28.0f, tw = larg - (tx - px) - 36.0f;
      float ca = a * af;
      notasui_modo_icone(1);
      notasui_marca(EX_TRAKT, px, y + PAUSAO_COM_H * 0.5f, disco, ca);   // o logotipo do Trakt
      notasui_modo_icone(0);
      { TxtLinha lq = txt_linha(TXT_TITULO3, "\xE2\x80\x9C", 132, 138, 150, 255);
        txt_desenhar_alpha(lq, x0, y + PAUSAO_COM_PAD - 2.0f, ca * 0.9f); }
      { TxtLinha lq = txt_linha(TXT_TITULO3, "\xE2\x80\x9D", 132, 138, 150, 255);
        txt_desenhar_alpha(lq, px + larg - (float)lq.w, y + PAUSAO_COM_PAD + 26.0f, ca * 0.9f); }
      agendaui_sinopse(TXT_DET_META2, cm.texto, tx, y + PAUSAO_COM_PAD, tw, 30.0f,
                       PAUSAO_COM_LINHAS, 215, 219, 228, ca * 0.95f);
      snprintf(autor, sizeof autor, "\xE2\x80\x94 %s", cm.autor);
      if (cm.nota > 0) snprintf(rodape, sizeof rodape, i18n("%d/10   %d curtidas"), cm.nota, cm.curtidas);
      else snprintf(rodape, sizeof rodape, i18n("%d curtidas"), cm.curtidas);
      { TxtLinha la = txt_linha_corta(TXT_CAPTION2, autor, 190, 195, 205, 255, 300.0f);
        TxtLinha lr = txt_linha(TXT_CAPTION2, rodape, 150, 156, 168, 255);
        float yr = y + PAUSAO_COM_H - PAUSAO_COM_PAD - (float)lr.h, fx = x0, dotsW = 0.0f;
        txt_desenhar_alpha(la, fx, yr, ca * 0.95f);
        fx += (float)la.w + 18.0f;
        if (cm.nota > 0) {
          gfx_icone((GfxRect){ fx, yr + ((float)lr.h - 18.0f) * 0.5f, 18.0f, 18.0f }, "aj_star", 0.96f, 0.77f, 0.09f, ca);
          fx += 24.0f;
        }
        txt_desenhar_alpha(lr, fx, yr, ca * 0.9f);
        if (nCom > 1) dotsW = (float)nCom * 14.0f;
        // Idioma do comentario quando difere do da interface: o texto e de uma
        // pessoa e nao se traduz, mas se diz por que esta em outra lingua.
        if (cm.lingua[0] && !coment_mesmo_idioma(cm.lingua, idioma_iso(ajustes_idioma()))) {
          char tag[8]; size_t q;
          for (q = 0; q < sizeof tag - 1 && cm.lingua[q]; q++)
            tag[q] = (char)((cm.lingua[q] >= 'a' && cm.lingua[q] <= 'z') ? cm.lingua[q] - 32 : cm.lingua[q]);
          tag[q] = 0;
          { TxtLinha lt = txt_linha(TXT_CAPTION2, tag, 132, 138, 150, 255);
            txt_desenhar_alpha(lt, px + larg - dotsW - (float)lt.w - (dotsW > 0.0f ? 8.0f : 0.0f), yr, ca * 0.9f); }
        }
        for (i = 0; nCom > 1 && i < nCom; i++)
          gfx_cor((GfxRect){ px + larg - (float)(nCom - i) * 14.0f + 6.0f, yr + ((float)lr.h - 8.0f) * 0.5f, 8.0f, 8.0f },
                  0.5f, .95f, .95f, .97f, a * (i == k ? 0.9f : 0.3f)); }
    }
    y += PAUSAO_COM_H;
  }

  // ELENCO: o avatar redondo da pagina do titulo (GFX_AVATAR), nome e papel
  // centralizados. Sem foto, a inicial sobre #222 — nunca imagem quebrada.
  if (temCast) {
    float x = PAUSAO_X;
    y += 28.0f;
    plrui_kicker("Elenco", PAUSAO_X, y, 243, 242, 239, a * 0.45f);
    y += 18.0f + 14.0f;
    for (i = 0; i < c->nElenco && i < PAUSAO_ELENCO_MAX; i++) {
      const char *nome = c->elenco[i].nome, *foto = c->elenco[i].foto;
      float cw = PAUSAO_FOTO + PAUSAO_FOTO_VAO - 6.0f;
      GfxRect av = { x + (PAUSAO_FOTO + PAUSAO_FOTO_VAO - 6.0f - PAUSAO_FOTO) * 0.5f, y, PAUSAO_FOTO, PAUSAO_FOTO };
      GLuint tf;
      if (!nome[0]) continue;
      if (x + cw > NV_TELA_W - PAUSAO_X) break;   // uma fileira so
      tf = foto[0] ? tex_obter_larg(foto, PAUSAO_FOTO) : 0;
      if (tf) {
        gfx_tex_aspect_atual = tex_aspecto(foto);
        gfx_rect(av, tf, GFX_AVATAR, 0, 0, 0, 0.0f, 0, 0, 0, a);
        gfx_tex_aspect_atual = 0.0f;
      } else {
        char ini[5] = {0};
        gfx_cor(av, 0.5f, 0.133f, 0.133f, 0.133f, a);
        for (int q = 0; q < 4 && nome[q] && (unsigned char)nome[q] >= 0x20; q++) {
          ini[q] = nome[q];
          if ((nome[q] & 0xC0) != 0x80) { if (q) { ini[q] = 0; break; } }
        }
        { TxtLinha li = txt_linha(TXT_TITULO3, ini, 210, 212, 220, 255);
          txt_desenhar_alpha(li, av.x + (av.w - li.w) * 0.5f, av.y + (av.h - li.h) * 0.5f, a * 0.9f); }
      }
      { TxtLinha ln = txt_linha_corta(TXT_G19M, nome, 240, 240, 240, 255, cw);
        txt_desenhar_alpha(ln, x + (cw - (float)ln.w) * 0.5f, y + PAUSAO_FOTO + 10.0f, a); }
      if (c->elenco[i].papel[0]) {
        TxtLinha lp = txt_linha_corta(TXT_CAPTION2, c->elenco[i].papel, 196, 196, 196, 255, cw);
        txt_desenhar_alpha(lp, x + (cw - (float)lp.w) * 0.5f, y + PAUSAO_FOTO + 10.0f + 26.0f, a * 0.95f);
      }
      x += PAUSAO_FOTO + PAUSAO_FOTO_VAO;
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
