// Desenho do OSD do canal ao vivo, do banner do zapping e do cartao de erro.
// Ver aovivo.h; separado da conta para ela ser testada sem GL.
//
// A MESMA CARA DO GUIA (revisao de 29/09/2026, pedido do dono: "ver se esta
// bom e se esta seguindo nossa identidade e o estilo do guia de TV atual").
// A primeira versao tinha vocabulario proprio para coisas que o guia ja
// resolve, e cada uma virou a do guia:
//   - marca do canal numa PLACA escura translucida -> guia_logo_desenhar, a
//     regra do dono de 16/09 ("sem azulejo"; recortado vira branco);
//   - numero numa pilula cinza e nome em 56 px, disputando com o titulo do
//     programa -> numero cinza solto + nome em HEADLINE, como a linha do canal
//     do heroi do guia; o degrau grande fica com o PROGRAMA (TITULO2);
//   - AO VIVO com ponto, em TXT_MINI, ao lado do nome -> o selo do guia, na
//     linha do horario do programa (e o programa que esta no ar);
//   - "HD" num chip de texto -> a MARCA de formato (badges.h), como no resto
//     do app; categoria como texto solto -> a etiqueta do guia;
//   - "A SEGUIR" num bloco a direita, cortado em 520 px e sem alinhar com
//     nada -> a linha unica do guia (rotulo, hora, titulo) na coluna do texto;
//   - barra de 8 px na largura do bloco -> o trilho de 4 px do heroi do guia;
//   - pilulas de botao com numeros proprios (68 px, texto de 20 px, repouso em
//     branco a 14%) -> botao_pilula secundario de botoes.c, a tabela unica;
//   - "Carregando o fluxo" em AMBAR cravado -> texto secundario neutro.
// GLASS UI (03/10): tudo isso passa a ser ilha — a do canal no canto, a hora
// em pilula, Informacoes, o zapping e o erro; a barra e a do player e o
// "Carregando o fluxo" volta ao ambar, agora como estado de aviso.
#include "horafmt.h"
#include "aovivo.h"
#include "ajustes.h"
#include "anim.h"
#include "badges.h"
#include "botoes.h"
#include "guia.h"
#include "pausao.h"
#include "gfx.h"
#include "text.h"
#include "layout.h"
#include "idioma.h"
#include "plrui.h"
#include "plrilha.h"
#include "telefoneui.h"
#include "rolagemtoque.h"
#define NV_ESCALA_TELA   // o arquivo inteiro mede pela tela virtual (escala.h)
#include "escala.h"
#include <stdio.h>
#include <string.h>

// --- desenho ---------------------------------------------------------------------
#define AV_X        NV_ILHA_MARGEM_D    // PLR_MARGEM: o recuo do conteudo do player de filme
#define AV_LOGO_TOM 0.965f   // G_LOGO_CLARO do guia: marca recortada em branco

// Duracao em relogio, sem palavra (nao precisa de traducao): 3:12, 1:04:09.
static void relogioDur(char *b, size_t n, int seg) {
  if (seg < 0) seg = 0;
  if (seg >= 3600) snprintf(b, n, "%d:%02d:%02d", seg / 3600, (seg / 60) % 60, seg % 60);
  else snprintf(b, n, "%d:%02d", seg / 60, seg % 60);
}

static void hhmm(char *b, size_t n, time_t t) {
  struct tm lt;
  localtime_r(&t, &lt);
  hora_tela(b, n, &lt);
}

static const char *icone(int b) {
  switch (b) {
    case AV_B_PAUSA: return NULL;   // play/pause conforme o estado
    case AV_B_GUIA: return "pl_list";
    case AV_B_FAV: return "pl_star";   // pl_star-f com o canal nos favoritos (iconeDe)
    case AV_B_AUDIO: return "pl_audio-lines";
    case AV_B_LEGENDA: return "pl_captions";
    case AV_B_INFO: return "pl_info";
    case AV_B_RECARREGAR: return "pl_rotate-ccw";
    case AV_B_FONTE: return "pl_layers";
    case AV_B_AOVIVO: return "pl_skip-forward";
    case AV_B_ASPECTO: return "pl_ratio";
    case AV_B_ULTIMO: return "pl_chevron-left";
    default: return NULL;
  }
}

const char *aovivo_rotulo(int b) {
  switch (b) {
    case AV_B_PAUSA: return "Pausar";
    case AV_B_GUIA: return "Guia";
    case AV_B_ANT: return "Canal −";
    case AV_B_PROX: return "Canal +";
    case AV_B_FAV: return "Favorito";
    case AV_B_AUDIO: return "Áudio";
    case AV_B_LEGENDA: return "Legendas";
    case AV_B_INFO: return "Informações";
    case AV_B_RECARREGAR: return "Recarregar";
    case AV_B_FONTE: return "Fonte";
    case AV_B_AOVIVO: return "Voltar ao vivo";
    case AV_B_ASPECTO: return "Proporção";
    case AV_B_ULTIMO: return "Último canal";
    default: return "";
  }
}

static const char *rotuloDe(const AoVivoOsd *o, int b) {
  if (b == AV_B_PAUSA) return o->pausado ? "Continuar" : "Pausar";
  if (b == AV_B_FAV && o->favorito) return "Nos favoritos";
  // O modo em vigor no proprio botao (Original, Zoom cinema...): o OK cicla,
  // e a pessoa ve para onde foi sem toast.
  if (b == AV_B_ASPECTO && o->aspecto && o->aspecto[0]) return o->aspecto;
  return aovivo_rotulo(b);
}
static const char *iconeDe(const AoVivoOsd *o, int b) {
  // FAVORITO: estrela CHEIA quando o canal esta nos favoritos, contorno quando
  // nao (dono, 03/10: "o favorito tem que deixar a estrela filled quando
  // clicar"). `favorito` e relido a cada quadro (guia_e_favorito), entao o OK
  // troca o desenho no quadro seguinte.
  if (b == AV_B_FAV) return o->favorito ? "pl_star-f" : "pl_star";
  return b == AV_B_PAUSA ? (o->pausado ? "pl_play-f" : "pl_pause-f") : icone(b);
}

// A FILEIRA DE BOTOES. Pilulas secundarias da tabela unica (botoes.h): mesmo
// corpo de texto, icone, repouso, foco no acento com tinta por contraste e o
// vidro de sempre. A unica licenca e a FOLGA: sao ate dez botoes numa linha,
// e com a folga de 28 px do secundario a fileira em portugues ja passava da
// margem com "Nos favoritos" e o Pausar. Com 22 px e 12 px entre eles cabe.
// Se ainda assim nao couber (alemao, russo), os botoes com icone e FORA do
// foco viram pilula redonda so com o icone, e o rotulo fica no que tem foco,
// como no player de filme. Nunca passa da margem.
// --- O OSD DO CANAL NO GLASS UI (mockup de 03/10, "aovivo-osd/atras") ------------
// O canal vira uma ILHA no canto (logo, nome, numero, marca de resolucao,
// favorito); a hora e a pilula a direita; embaixo o programa — AO VIVO em
// vermelho (estado da transmissao, nao acento), horario, titulo 52/700,
// descricao e A SEGUIR —, a MESMA barra do player e a fileira: os botoes com
// nome (Guia, Canal -, Canal +) em pilula, os de icone em disco 68 no material
// da ilha, o focado vira a pilula no acento com o nome dentro.
#define AV_BTN_Y   (NV_TELA_H - 130.0f)   // 950 em 1080; tela virtual (escala.h)
#define AV_BTN_D    68.0f

#ifdef NV_TOUCH_UI
static AoVivoToqueFn toqueAcao;
static int toqueQuadro;
static float toqueRol;
static ToqueRolagem toqueLista;
static int toqueFoco = -2;
void aovivo_toque_reiniciar(void) {
  toqueRol = 0; toquerol_limpar(&toqueLista); toqueFoco = -2;
}
void aovivo_toque_definir(AoVivoToqueFn fn, int quadro) {
  toqueAcao = fn; toqueQuadro = quadro;
}
static void toqueBotao(int acao, int quadro) { if (toqueAcao) toqueAcao(acao, 0, quadro); }
static void toqueErro(int acao, int quadro) { if (toqueAcao) toqueAcao(acao, 1, quadro); }
static int toqueRolar(const PonteiroRolagem *e) {
  if (!toqueAcao) return 0;
  return toquerol_evento(&toqueLista, e);
}
#endif

// O FOCO DA FILEIRA E DO CARTAO DE ERRO EM MOLA (o dono: "tudo meio
// duro"). O disco vira a pilula com o nome crescendo, em ~120 ms na mola
// padrao de foco (NV_MOLA_FOCO), em vez de saltar num quadro. O desenho
// fotografa o estado (tests/aovivo_shot) e NAO recebe dt do player: o relogio
// e o SDL_GetTicks da ultima passada DAQUELA superficie (um por superficie,
// para o erro desenhado todo quadro nao manter o OSD "fresco" e entregar o
// quadro no meio do voo), e `osdPasso` e puro para o teste
// (tests/animacoes205.c) dirigir os quadros que quiser.
static float    osdFoco[AV_B_N], erroFocoA[3];
static unsigned char osdFocoPronto[AV_B_N], erroFocoPronto[3];
static Uint32 osdUlt, erroUlt;
static unsigned osdQuadro, erroQuadro;
static int osdFocoAlvo = -1, erroFocoAlvo = -1;
static void osdPasso(float dt) {
  int i, red = ajustes_animacoes_reduzidas();
  for (i = 0; i < AV_B_N; i++) {
    float alvo = (i == osdFocoAlvo) ? 1.0f : 0.0f;
    // Cada APARICAO nasce assentada no foco atual (o alvo e zerado quando a
    // superficie sai da tela, antes do passo): "pronto" vale por visita, nao
    // por processo.
    if (!osdFocoPronto[i]) { osdFoco[i] = alvo; osdFocoPronto[i] = 1; }
    else osdFoco[i] = red ? alvo : anim_mola(osdFoco[i], alvo, dt, NV_MOLA_FOCO);
  }
  if (osdFocoAlvo < 0) memset(osdFocoPronto, 0, sizeof osdFocoPronto);
}
static void erroPasso(float dt) {
  int i, red = ajustes_animacoes_reduzidas();
  for (i = 0; i < 3; i++) {
    float alvo = (i == erroFocoAlvo) ? 1.0f : 0.0f;
    if (!erroFocoPronto[i]) { erroFocoA[i] = alvo; erroFocoPronto[i] = 1; }
    else erroFocoA[i] = red ? alvo : anim_mola(erroFocoA[i], alvo, dt, NV_MOLA_FOCO);
  }
  if (erroFocoAlvo < 0) memset(erroFocoPronto, 0, sizeof erroFocoPronto);
}
// dt da superficie (SDL_GetTicks DELA, com teto de 50 ms) e se ela REAPARECEU:
// reaparecer e nao ter desenhado no quadro anterior (gfx_quadro_n), nao um
// buraco de relogio — um engasgo de 250 ms com o OSD na tela nao pode virar um
// salto, e o cartao de erro desenhado todo quadro nao mexe no OSD.
static float tic(Uint32 *ult, unsigned *quadro, int *reapareceu) {
  Uint32 agora = SDL_GetTicks();
  float dt = *ult ? (float)(agora - *ult) / 1000.0f : 0.0f;
  *reapareceu = gfx_quadro_n - *quadro > 1u;
  *ult = agora; *quadro = gfx_quadro_n;
  return dt > 0.05f ? 0.05f : dt;
}
static void osdTic(void) {
  int reapareceu;
  float dt = tic(&osdUlt, &osdQuadro, &reapareceu);
  if (reapareceu) memset(osdFocoPronto, 0, sizeof osdFocoPronto);
  osdPasso(dt);
}
static void erroTic(void) {
  int reapareceu;
  float dt = tic(&erroUlt, &erroQuadro, &reapareceu);
  if (reapareceu) memset(erroFocoPronto, 0, sizeof erroFocoPronto);
  erroPasso(dt);
}

// Com nome em repouso: os sem icone (Canal -/+) e o Guia, a porta principal.
static int comNome(const AoVivoOsd *o, int b) { return !iconeDe(o, b) || b == AV_B_GUIA; }
static float larguraBotaoAv(const AoVivoOsd *o, int b, float foco) {
  const char *ic = iconeDe(o, b);
  float t = (float)txt_largura(TXT_G21B, i18n(rotuloDe(o, b)));
  float wf = 22.0f + (ic ? 30.0f + 12.0f : 0.0f) + t + 28.0f;
  if (comNome(o, b)) {
    float wr = 26.0f * 2.0f + (ic ? 26.0f + 12.0f : 0.0f) + t;
    return anim_mistura(wr, wf, foco);
  }
  return anim_mistura(AV_BTN_D, wf, foco);
}
static void fileiraBotoes(const AoVivoOsd *o, float y, float a) {
  float x = AV_X;
  int i, tinta = plrui_tinta();
#ifdef NV_TOUCH_UI
  int phone = telefoneui_ativo(), pont = phone && toqueAcao && a > .99f;
  GfxRect faixa = {AV_X, y, NV_TELA_W - 2 * AV_X, AV_BTN_D};
  if (phone) {
    if (toqueFoco != o->foco) { toquerol_limpar(&toqueLista); toqueFoco = o->foco; }
    float total = 0, fx = 0, fw = 0;
    for (i = 0; i < o->nBotoes; i++) {
      float w = larguraBotaoAv(o, o->botoes[i], osdFoco[o->botoes[i]]);
      if (i == o->foco) { fx = total; fw = w; }
      total += w + (i + 1 < o->nBotoes ? 12 : 0);
    }
    float max = fmaxf(0, total - faixa.w);
    if (!toquerol_livre(&toqueLista)) {
      if (fx < toqueRol) toqueRol = fx;
      if (fx + fw > toqueRol + faixa.w) toqueRol = fx + fw - faixa.w;
    }
    toqueRol = toquerol_clamp(toqueRol, 0, max);
    toquerol_vincular(&toqueLista, faixa, gfx_escala(), 0, max, 0, &toqueRol);
    if (pont) ponteiro_rolagem(toqueRolar);
    gfx_recorte(faixa.x, faixa.y, faixa.w, faixa.h); x -= toqueRol;
  }
#endif
  for (i = 0; i < o->nBotoes; i++) {
    int b = o->botoes[i];
    // FOCO EM MOLA: 0..1 interpola largura, disco/pilula, icone e rotulo.
    // A tinta do TEXTO troca no ponto medio (convencao: rasterizado nao
    // recolore por quadro); superficie e icone crossfadezam de verdade.
    float f = osdFoco[b], al = comNome(o, b) ? 1.0f : f;
    int emFoco = f > 0.5f;
    const char *ic = iconeDe(o, b), *rot = rotuloDe(o, b);
    float w = larguraBotaoAv(o, b, f), k;
    GfxRect r = { x, y, w, AV_BTN_D };
#ifdef NV_TOUCH_UI
    if (!phone && x + w > NV_TELA_W - AV_X) break;
    if (phone && (x + w <= faixa.x || x >= faixa.x + faixa.w)) { x += w + 12; continue; }
#else
    if (x + w > NV_TELA_W - AV_X) break;
#endif
    if (f > 0.005f) plrui_pilula_foco(r, a * f);
    if (f < 0.995f) plrui_disco_osd(r, a * (1.0f - f));
    k = (emFoco ? tinta : 230) / 255.0f;
    if (f > 0.005f || comNome(o, b)) {
      float tx = x + anim_mistura(26.0f, 22.0f, f), is = anim_mistura(26.0f, 30.0f, f);
      TxtLinha l = txt_linha(TXT_G21B, rot, emFoco ? tinta : 230, emFoco ? tinta : 230, emFoco ? tinta : 230, 255);
      if (ic) { gfx_icone((GfxRect){ tx, y + (AV_BTN_D - is) * 0.5f, is, is }, ic, k, k, k, a * al); tx += is + 12.0f; }
      txt_desenhar_alpha(l, tx, y + (AV_BTN_D - (float)l.h) * 0.5f, a * al);
    }
    if (f < 0.995f && ic && !comNome(o, b))
      gfx_icone((GfxRect){ x + 19.0f, y + 19.0f, 30.0f, 30.0f }, ic, k, k, k, a * (1.0f - f));
#ifdef NV_TOUCH_UI
    if (pont) {
      float ax = fmaxf(x, faixa.x), ar = fminf(x + w, faixa.x + faixa.w);
      if (ar > ax) ponteiro_alvo(ax, y, ar - ax, AV_BTN_D, NULL, toqueBotao, b, toqueQuadro);
    }
#endif
    x += w + 12.0f;
  }
#ifdef NV_TOUCH_UI
  if (phone) gfx_sem_recorte();
#endif
}

// A marca do canal numa caixa clara de raio 16 (110 x 64 no OSD).
static void caixaLogo(const char *logo, const char *nome, GfxRect r, float a) {
  if (ajustes_vidro()) gfx_cor(r, 16.0f / r.h, 1, 1, 1, 0.08f * a);
  else gfx_cor(r, 16.0f / r.h, 0.141f, 0.149f, 0.173f, a);
  guia_logo_desenhar(logo, nome, (GfxRect){ r.x + 10.0f, r.y + 8.0f, r.w - 20.0f, r.h - 16.0f },
                     r.w - 20.0f, r.h - 16.0f, AV_LOGO_TOM, a);
}

// Pilula vermelha AO VIVO: 34 de altura, ponto branco, 15/800.
static float seloAoVivo(float x, float y, float h, float a) {
  const char *av = i18n("AO VIVO");
  TxtLinha l = txt_linha(TXT_MINI, av, 255, 255, 255, 255);
  float tw = txt_tracking(TXT_MINI, av, 255, 255, 255, -1.0f, 0.0f, 1.0f, 1.5f);   // medido COM o tracking
  float w = 14.0f + 8.0f + 8.0f + tw + 14.0f;
  gfx_cor((GfxRect){ x, y, w, h }, 0.5f, 0.898f, 0.282f, 0.302f, a);
  gfx_cor((GfxRect){ x + 14.0f, y + h * 0.5f - 4.0f, 8.0f, 8.0f }, 0.5f, 1, 1, 1, a);
  txt_tracking(TXT_MINI, av, 255, 255, 255, x + 30.0f, y + (h - (float)l.h) * 0.5f, a, 1.5f);
  return w;
}

static void aovivo_osd_desenharCorpo_(const AoVivoOsd *o, float a);
// Camada ampliada (escala.h): o corpo desenha na tela virtual.
void aovivo_osd_desenhar(const AoVivoOsd *o, float a) {
  ESCALA_INI();
  aovivo_osd_desenharCorpo_(o, a);
  ESCALA_FIM();
}
static void aovivo_osd_desenharCorpo_(const AoVivoOsd *o, float a) {
  time_t agoraT = time(NULL);
  if (a <= 0.004f || !o) return;
  // O alvo das molas e o foco DESTE quadro; o relogio e interno porque o
  // desenho nao recebe dt (o player chama por quadro, o shot fotografa).
  osdFocoAlvo = o->nBotoes > 0 && o->foco >= 0 ? o->botoes[o->foco] : -1;
  osdTic();
  gfx_veu_css((GfxRect){ 0, 0, NV_TELA_W, 260.0f }, 1, 1.38f, 1.0f, 0.52f * a);
  gfx_veu_css((GfxRect){ 0, NV_TELA_H - 520.0f, NV_TELA_W, 520.0f }, 0, 1.25f, 1.0f, 0.80f * a);

  // --- A ILHA DO CANAL ------------------------------------------------------
  { TxtLinha nome = txt_linha_corta(TXT_G26B, o->nome && o->nome[0] ? o->nome : "Canal", 243, 242, 239, 255, 700.0f);
    char nb[16] = "";
    float w2 = (float)nome.w, x2;
    int fm = marca_resolucao(o->res);
    TxtLinha num = { 0 };
    float lw = 0.0f;
    if (o->numero > 0) { snprintf(nb, sizeof nb, "%d", o->numero); num = txt_linha(TXT_G18R, nb, 243, 242, 239, 140); }
    if (fm >= 0) lw = marca_formato_largura((FormatoMarca)fm, 20.0f);
    { float lin = (float)num.w + (num.w ? 12.0f : 0.0f) + lw + (lw > 0 ? 12.0f : 0.0f) + (o->favorito ? 18.0f : 0.0f);
      if (lin > w2) w2 = lin; }
    { GfxRect il = { AV_X, 48.0f, 16.0f + 110.0f + 20.0f + w2 + 26.0f, 96.0f };
      plrui_material(il, 30.0f, 0, a);
      caixaLogo(o->logo, o->nome, (GfxRect){ il.x + 16.0f, il.y + 16.0f, 110.0f, 64.0f }, a);
      x2 = il.x + 16.0f + 110.0f + 20.0f;
      txt_desenhar_alpha(nome, x2, il.y + 18.0f, a);
      { float ry = il.y + 18.0f + (float)nome.h + 4.0f, xs = x2;
        if (num.w) { txt_desenhar_alpha(num, xs, ry, a); xs += (float)num.w + 12.0f; }
        if (fm >= 0) xs += marca_formato((FormatoMarca)fm, xs, ry + 1.0f, 20.0f, 0.953f, 0.949f, 0.937f, a * 0.75f) + 12.0f;
        else if (o->res[0]) xs += badge_desenhar(xs, ry, o->res, BADGE_NEUTRO, a) + 12.0f;
        if (o->favorito) gfx_icone((GfxRect){ xs, ry + 2.0f, 18.0f, 18.0f }, "pl_star-f", 0.961f, 0.773f, 0.259f, a); } } }

  // --- O ESTADO DA PAUSA, abaixo da ILHA DO RELOGIO -------------------------
  // A HORA NAO E MAIS DESENHADA AQUI (dono, 03/10: "no live tv o source nao ta
  // saindo do relogio, nem o audio nem a legenda, igual e no outro player").
  // Era uma pilula propria, 20 px abaixo da ilha do player, e as folhas de
  // Fontes, Audio, Legendas e as Informacoes cresciam de OUTRO lugar (a ilha do
  // player, sem relogio no canal). Agora a hora e a ilha do player
  // (plrilha_relogio, chamado por player.c) e tudo nasce dela, como no filme.
  { float yr = NV_ILHA_Y + 56.0f + 14.0f;   // Y_TOPO + PIL_H de plrilha.c + vao
    GfxRect ir;
    if (plrilha_rect(&ir) && ir.y + ir.h + 14.0f > yr) yr = ir.y + ir.h + 14.0f;
    if (o->pausado) { pausao_selo(NV_TELA_W - NV_ILHA_MARGEM_D, yr, 1, a); yr += PAUSAO_SELO_H + 12.0f; }
    if (o->pausado && (o->pausaS > 0 || o->janelaS >= 60)) {
      char l2[160] = "", p1[64] = "", p2[80] = "";
      TxtLinha t;
      if (o->pausaS > 0) { char d[24]; relogioDur(d, sizeof d, o->pausaS); snprintf(p1, sizeof p1, i18n("Pausado há %s"), d); }
      if (o->janelaS >= 60) { char d[24]; relogioDur(d, sizeof d, o->janelaS); snprintf(p2, sizeof p2, i18n("Dá para voltar até %s"), d); }
      snprintf(l2, sizeof l2, "%s%s%s", p1, (p1[0] && p2[0]) ? "  \xc2\xb7  " : "", p2);
      t = txt_linha(TXT_ILHA_SUB, l2, 243, 242, 239, 200);
      txt_desenhar_alpha(t, NV_TELA_W - NV_ILHA_MARGEM_D - t.w, yr, a);
      yr += t.h + 6.0f;
    }
    if (o->bufferando) {
      // "Carregando o fluxo" e estado de aviso: ambar.
      TxtLinha lb = txt_linha(TXT_ILHA_SUB, "Carregando o fluxo…", 240, 185, 74, 255);
      txt_desenhar_alpha(lb, NV_TELA_W - NV_ILHA_MARGEM_D - lb.w, yr, a);
      yr += lb.h + 6.0f;
    } }

  // --- O PROGRAMA, de baixo para cima a partir da barra ----------------------
  { const AoVivoEpg *e = &o->epg;
    float yBar = AV_BTN_Y - 46.0f, yb = NV_TELA_H - 210.0f;
    // A barra do programa e a barra do player; atras do ao vivo, o trecho
    // entre o ponto e o ao vivo fica no branco do buffer.
    if (e->temAgora) {
      float f = e->progresso < 0.0f ? 0.0f : (e->progresso > 1.0f ? 1.0f : e->progresso), fa = 0.0f;
      if (o->atrasoS > 0 && e->agoraFim > e->agoraIni) {
        fa = (float)o->atrasoS / (float)(e->agoraFim - e->agoraIni);
        if (fa > f) fa = f;
      }
      plrui_barra(AV_X, yBar, NV_TELA_W - 2.0f * AV_X, f - fa, f, 0, NULL, 0, a);
    }
    if (e->temProx) {
      char hp[12];
      float lx = AV_X;
      hhmm(hp, sizeof hp, e->proxIni);
      yb -= 24.0f;
      lx += plrui_kicker("A seguir", lx, yb + 3.0f, 243, 242, 239, a * 0.45f) + 14.0f;
      { TxtLinha hr = txt_linha(TXT_ILHA_SUB, hp, 243, 242, 239, 153);
        txt_desenhar_alpha(hr, lx, yb, a); lx += (float)hr.w + 14.0f; }
      { TxtLinha tt = txt_linha_corta(TXT_ILHA_SEG, e->proxTit, 243, 242, 239, 255, AV_X + 1300.0f - lx);
        txt_desenhar_alpha(tt, lx, yb, a); }
      yb -= 16.0f;
    }
    { char descBuf[600];
      const char *descLivre = guia_desc_livre(o->desc, descBuf, sizeof descBuf);
      if (descLivre[0]) {
        float h = txt_bloco_corta(TXT_CAPTION2, descLivre, 243, 242, 239, -1.0f, 0.0f, 1100.0f, 31.5f, 0.0f, 2);
        yb -= h;
        txt_bloco_corta(TXT_CAPTION2, descLivre, 243, 242, 239, AV_X, yb, 1100.0f, 31.5f, a * 0.70f, 2);
        yb -= 10.0f;
      } }
    if (e->temAgora) {
      TxtLinha lt = txt_linha_corta(TXT_G52B, e->agoraTit, 243, 242, 239, 255, 1300.0f);
      yb -= (float)lt.h;
      txt_desenhar_alpha(lt, AV_X, yb, a);
      yb -= 16.0f;
    }
    { char meta[160];
      float mx = AV_X;
      yb -= 34.0f;
      if (o->atrasoS > 0) {
        char d[24], ds[32], l1[96];
        TxtLinha t, t2;
        relogioDur(d, sizeof d, o->atrasoS);
        snprintf(ds, sizeof ds, "\xe2\x88\x92%s", d);
        t = txt_linha(TXT_G16B, ds, 243, 242, 239, 255);
        { GfxRect r = { mx, yb, (float)t.w + 28.0f, 34.0f };
          gfx_cor(r, 0.5f, 1, 1, 1, 0.14f * a);
          txt_desenhar_alpha(t, r.x + 14.0f, r.y + (34.0f - (float)t.h) * 0.5f, a);
          mx += r.w + 16.0f; }
        snprintf(l1, sizeof l1, i18n("%s atrás do ao vivo"), d);
        t2 = txt_linha(TXT_G20M, l1, 243, 242, 239, 179);
        txt_desenhar_alpha(t2, mx, yb + (34.0f - (float)t2.h) * 0.5f, a);
        mx += (float)t2.w + 16.0f;
      } else mx += seloAoVivo(mx, yb, 34.0f, a) + 16.0f;
      if (e->temAgora) {
        char h1[12], h2[12], resto[64];
        int falta = (int)((e->agoraFim - agoraT + 59) / 60);
        if (falta < 0) falta = 0;
        hhmm(h1, sizeof h1, e->agoraIni); hhmm(h2, sizeof h2, e->agoraFim);
        snprintf(resto, sizeof resto, i18n("%d min restantes"), falta);
        snprintf(meta, sizeof meta, "%s \xe2\x80\x93 %s \xc2\xb7 %s", h1, h2, resto);
      } else snprintf(meta, sizeof meta, "%s", i18n("Sem grade de programação"));
      { TxtLinha t = txt_linha_corta(TXT_G20M, meta, 243, 242, 239, 168, AV_X + 1300.0f - mx);
        txt_desenhar_alpha(t, mx, yb + (34.0f - (float)t.h) * 0.5f, a); } } }

  fileiraBotoes(o, AV_BTN_Y, a);
}


// INFORMACOES: o CORPO da ilha do relogio (plrilha.h), pedido por player.c com
// "Informacoes" no cabecalho. As MESMAS marcas do OSD de filme no topo e as
// linhas "rotulo ...... valor" com fio entre elas. Era um cartao proprio a
// direita, abaixo da pilula da hora, que nao saia dela.
float aovivo_info_altura(const AoVivoOsd *o) {
  if (!o || o->nInfo <= 0) return 0.0f;
  return 18.0f + (o->nMarcas ? 32.0f + 16.0f : 0.0f) + o->nInfo * 46.0f + 16.0f;
}
void aovivo_info_corpo(GfxRect c, float a, void *u) {
  const AoVivoOsd *o = (const AoVivoOsd *)u;
  float y = c.y + 18.0f, mx = c.x + 30.0f, pw = c.w;
  int i;
  if (!o || a <= 0.004f) return;
  if (o->nMarcas) {
    for (i = 0; i < o->nMarcas; i++)
      mx += marca_formato((FormatoMarca)o->marcas[i], mx, y, 32.0f, 0.953f, 0.949f, 0.937f, a * 0.8f) + 18.0f;
    y += 32.0f + 16.0f;
  }
  for (i = 0; i < o->nInfo; i++) {
    char rot[72];
    const char *val = strstr(o->info[i], ": ");
    snprintf(rot, sizeof rot, "%.*s", val ? (int)(val - o->info[i]) : (int)strlen(o->info[i]), o->info[i]);
    if (i || o->nMarcas) gfx_cor((GfxRect){ c.x + 30.0f, y, pw - 60.0f, 1.0f }, 0.0f, 1, 1, 1, 0.07f * a);
    { TxtLinha lr = txt_linha_corta(TXT_G18R, rot, 243, 242, 239, 128, pw * 0.5f);
      txt_desenhar_alpha(lr, c.x + 30.0f, y + 23.0f - (float)lr.h * 0.5f, a); }
    if (val) { TxtLinha lv = txt_linha_corta(TXT_G18R, val + 2, 243, 242, 239, 255, pw * 0.5f - 40.0f);
               txt_desenhar_alpha(lv, c.x + pw - 30.0f - lv.w, y + 23.0f - (float)lv.h * 0.5f, a); }
    y += 46.0f;
  }
}

// ZAPPING: ilha na margem (era um cartao 900x132 opaco), com a marca, o
// numero, o nome, o programa que entra e "Trocando de canal…" com o ponto que
// respira; "+3 canais" ao saltar varios.
static void aovivo_banner_desenharCorpo_(const AoVivoBanner *b, float a);
// Camada ampliada (escala.h): o corpo desenha na tela virtual.
void aovivo_banner_desenhar(const AoVivoBanner *b, float a) {
  ESCALA_INI();
  aovivo_banner_desenharCorpo_(b, a);
  ESCALA_FIM();
}
static void aovivo_banner_desenharCorpo_(const AoVivoBanner *b, float a) {
  float w = 900.0f, h = 132.0f, x = AV_X, y = NV_TELA_H - 96.0f - h, tx;
  char s[64];
  if (a <= 0.004f || !b) return;
  gfx_veu_css((GfxRect){ 0, NV_TELA_H - 300.0f, NV_TELA_W, 300.0f }, 0, 1.0f, 1.0f, 0.60f * a);
  plrui_material((GfxRect){ x, y, w, h }, 32.0f, 0, a);
  caixaLogo(b->logo, b->nome, (GfxRect){ x + 18.0f, y + (h - 77.0f) * 0.5f, 132.0f, 77.0f }, a);
  tx = x + 18.0f + 132.0f + 22.0f;
  if (b->salto > 1 || b->salto < -1) snprintf(s, sizeof s, i18n("%+d canais"), b->salto);
  else snprintf(s, sizeof s, "%s", i18n("Trocando de canal…"));
  { TxtLinha st = txt_linha(TXT_G18R, s, 243, 242, 239, 153);
    float sx = x + w - 30.0f - (float)st.w;
    TxtLinha nome, tn = { 0 };
    float nx = tx;
    plrui_respira(sx - 12.0f - 4.0f, y + h * 0.5f, 8.0f, SDL_GetTicks(), a);
    txt_desenhar_alpha(st, sx, y + (h - (float)st.h) * 0.5f, a);
    if (b->numero > 0) {
      char nb[24];
      snprintf(nb, sizeof nb, "%d", b->numero);
      tn = txt_linha(TXT_G20M, nb, 243, 242, 239, 128);
      nx += (float)tn.w + 12.0f;
    }
    nome = txt_linha_corta(TXT_G30B, b->nome && b->nome[0] ? b->nome : "Canal", 243, 242, 239, 255, sx - 40.0f - nx);
    { float yl = y + 30.0f;
      if (tn.w) txt_desenhar_alpha(tn, tx, yl + (float)nome.h - (float)tn.h - 3.0f, a);
      txt_desenhar_alpha(nome, nx, yl, a);
      if (b->agoraTit && b->agoraTit[0]) {
        TxtLinha ag = txt_linha_corta(TXT_G20M, b->agoraTit, 243, 242, 239, 166, sx - 40.0f - tx);
        txt_desenhar_alpha(ag, tx, yl + (float)nome.h + 6.0f, a);
      } } }
}


// O SELO DO NUMERO DIGITADO (M7): como na TV, os digitos grandes no canto de
// cima e, abaixo, o canal que o numero leva — ou a frase de que nao ha. Some
// quando o player confirma (1,5 s sem tecla ou OK).
static void aovivo_numero_corpo_(const char *dig, const char *nome, const char *fonte, float a);
void aovivo_numero_desenhar(const char *digitos, const char *nome, const char *fonte, float a) {
  ESCALA_INI();
  aovivo_numero_corpo_(digitos, nome, fonte, a);
  ESCALA_FIM();
}
static void aovivo_numero_corpo_(const char *dig, const char *nome, const char *fonte, float a) {
  TxtLinha td, tn;
  char sub[200];
  float pad = 30.0f, w, h, x, y = 64.0f;
  if (a <= 0.004f || !dig || !dig[0]) return;
  td = txt_linha(TXT_TITULO2, dig, 243, 242, 239, 255);
  if (nome) {
    if (fonte && fonte[0]) snprintf(sub, sizeof sub, "%s  \xc2\xb7  %s", nome, fonte);
    else snprintf(sub, sizeof sub, "%s", nome);
    tn = txt_linha_corta(TXT_G20M, sub, 243, 242, 239, 200, 520.0f);
  } else {
    tn = txt_linha_corta(TXT_G20M, i18n("Nenhum canal com esse número"), 243, 160, 150, 230, 520.0f);
  }
  w = (float)(td.w > tn.w ? td.w : tn.w) + 2.0f * pad;
  if (w < 220.0f) w = 220.0f;
  h = (float)td.h + (float)tn.h + 2.0f * pad - 4.0f;
  x = NV_TELA_W - NV_MARGEM_X - w;
  plrui_material((GfxRect){ x, y, w, h }, 28.0f, 0, a);
  txt_desenhar_alpha(td, x + w - pad - (float)td.w, y + pad - 8.0f, a);
  txt_desenhar_alpha(tn, x + w - pad - (float)tn.w, y + pad - 8.0f + (float)td.h + 4.0f, a);
}

// O CANAL NAO ABRIU: a ilha modal (a mesma do erro de filme), com o canal no
// kicker, a frase, o motivo do Xtream e as acoes que o texto cita — Recarregar,
// Fonte, Guia — e a dica de CH+/CH-. player.c leva o foco (aovivo_erro_foco).
static int erroFoco;
void aovivo_erro_foco(int foco) { erroFoco = foco; }
static void aovivo_erro_desenharCorpo_(const char *nome, const char *logo, const char *titulo,
                          const char *dica, float a);
// Camada ampliada (escala.h): o corpo desenha na tela virtual.
void aovivo_erro_desenhar(const char *nome, const char *logo, const char *titulo,
                          const char *dica, float a) {
  ESCALA_INI();
  aovivo_erro_desenharCorpo_(nome, logo, titulo, dica, a);
  ESCALA_FIM();
}
static void aovivo_erro_desenharCorpo_(const char *nome, const char *logo, const char *titulo,
                          const char *dica, float a) {
  const float w = telefoneui_largura(960.0f, NV_TELA_W, 48.0f), pad = 44.0f, tw = w - 2.0f * pad;
  const char *tit = titulo && titulo[0] ? titulo : "Não foi possível abrir a fonte";
  const char *dc = dica && dica[0] ? dica : "Abra Fontes para escolher outra opção ou recarregar.";
  const char *rot[3] = { titulo && !strcmp(titulo, i18n("Esta linha está em uso em outra tela")) ? "Tentar de novo" : "Recarregar", "Fonte", "Guia" };
  static const char *ic[3] = { "pl_rotate-ccw", "pl_layers", "pl_list" };
  float x = (NV_TELA_W - w) * 0.5f, y = 290.0f, h, hTit, hDica, yy, lead;
  erroFocoAlvo = erroFoco;
  erroTic();
  int i;
  if (a <= 0.004f) return;
  lead = (float)txt_linha(TXT_ILHA_PERGUNTA, "Ag", 0, 0, 0, 255).h + 7.0f;
  hTit = txt_bloco_corta(TXT_ILHA_PERGUNTA, tit, 0, 0, 0, -1.0f, 0.0f, tw, lead, 0.0f, 2);
  hDica = txt_bloco_corta(TXT_ILHA_TEXTO, dc, 0, 0, 0, -1.0f, 0.0f, tw, 30.0f, 0.0f, 2);
  h = pad + 38.0f + 8.0f + hTit + 14.0f + hDica + 34.0f + 60.0f + 22.0f + 30.0f + pad;
#ifdef NV_TOUCH_UI
  if (telefoneui_ativo()) {
    const float bh = 88, gap = 12, bw = (tw - 2 * gap) / 3;
    const int ac[3] = {AV_B_RECARREGAR, AV_B_FONTE, AV_B_GUIA};
    int pont = toqueAcao && a > .99f;
    h = pad + 38 + 8 + hTit + 14 + hDica + 28 + bh + pad;
    y = fmaxf(24, (NV_TELA_H - h) * .5f);
    gfx_cor((GfxRect){0, 0, NV_TELA_W, NV_TELA_H}, 0, .031f, .035f, .043f, a);
    plrui_material((GfxRect){x, y, w, h}, 36, 1, a);
    if (pont) {
      ponteiro_camada();
      ponteiro_alvo(0, 0, NV_TELA_W, NV_TELA_H, NULL, NULL, 0, 0);
    }
    yy = y + pad;
    caixaLogo(logo, nome, (GfxRect){x + pad, yy, 66, 38}, a);
    TxtLinha nm = txt_linha_corta(TXT_G18R, nome && nome[0] ? nome : "Canal", 243, 242, 239, 150, tw - 80);
    txt_desenhar_alpha(nm, x + pad + 80, yy + (38 - nm.h) * .5f, a);
    yy += 46;
    txt_bloco_corta(TXT_ILHA_PERGUNTA, tit, 243, 242, 239, x + pad, yy, tw, lead, a, 2);
    yy += hTit + 14;
    txt_bloco_corta(TXT_ILHA_TEXTO, dc, 243, 242, 239, x + pad, yy, tw, 30, a * .62f, 2);
    yy += hDica + 28;
    for (i = 0; i < 3; i++) {
      GfxRect r = {x + pad + i * (bw + gap), yy, bw, bh};
      float f = erroFocoA[i];
      int cor = f > 0.5f ? plrui_tinta() : 230;
      if (f > 0.005f) plrui_pilula_foco(r, a * f);
      if (f < 0.995f) plrui_disco_osd(r, a * (1.0f - f));
      float th = txt_bloco_corta(TXT_G21B, rot[i], cor, cor, cor, -1, 0, bw - 24, 24, 0, 3);
      txt_bloco_corta(TXT_G21B, rot[i], cor, cor, cor, r.x + 12, yy + (bh - th) * .5f, bw - 24, 24, a, 3);
      if (pont) ponteiro_alvo(r.x, r.y, r.w, r.h, NULL, toqueErro, ac[i], toqueQuadro);
    }
    return;
  }
#endif
  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0, 0.031f, 0.035f, 0.043f, a);
  plrui_material((GfxRect){ x, y, w, h }, 36.0f, 1, a);
  yy = y + pad;
  caixaLogo(logo, nome, (GfxRect){ x + pad, yy, 66.0f, 38.0f }, a);
  plrui_kicker(nome && nome[0] ? nome : "Canal", x + pad + 66.0f + 14.0f, yy + 10.0f, 243, 242, 239, a * 0.45f);
  yy += 38.0f + 8.0f;
  txt_bloco_corta(TXT_ILHA_PERGUNTA, tit, 243, 242, 239, x + pad, yy, tw, lead, a, 2);
  yy += hTit + 14.0f;
  txt_bloco_corta(TXT_ILHA_TEXTO, dc, 243, 242, 239, x + pad, yy, tw, 30.0f, a * 0.62f, 2);
  yy += hDica + 34.0f;
  { float bx = x + pad;
    for (i = 0; i < 3; i++) bx += plrui_botao(bx, yy, rot[i], ic[i], erroFocoA[i], a) + 12.0f; }
  yy += 60.0f + 22.0f;
  { const char *k[2] = { "CH+", "CH\xe2\x88\x92" }, *r[2] = { "Próximo canal", "Canal anterior" };
    plrui_dicas(k, r, 2, x + pad, yy + 15.0f, 0, a); }
}

