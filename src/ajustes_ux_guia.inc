// GUIA DE USO — Ajustes › Sobre e ajuda › Guia de uso (mockup aprovado em
// 03/10/2026, guia-mockup.html). Tudo o que o Nuvio faz em 12 capitulos e 87
// recursos, compilado dos cartoes de novidades e conferido no codigo (a prova
// de cada entrada esta em design/guia-de-uso/guia.json; as tabelas C saem de
// tools/guia_inc.py para ajustes_ux_guia_dados.inc).
//
// TRES ILHAS, como Ajustes: os capitulos (com "Buscar no guia" e "Novo na
// 1.8.0" no topo), os recursos do capitulo e o inspetor, com a imagem do
// recurso, "Onde fica" e os atalhos. Todo botao faz o que diz:
//   Abrir em Ajustes  fecha o guia e pousa na linha da opcao (focarOpcao);
//   Abrir <tela>      pede a tela ao app.c (ajustes_pediu_tela);
//   Ver exemplo       mostra a cena de demonstracao das Novidades da 1.8.0 ou
//                     da 1.7 — so entra em quem tem cena de verdade.
// Aberto pelo cartao da 1.8.0, o cabecalho ganha "Voltar às novidades" e o
// Voltar devolve ao cartao (ajustes_pediu_novidades).
//
// AS IMAGENS DO INSPETOR sao telas de verdade em miniatura (gfx_mini_*): a
// folha de Fontes, os avisos da ilha, a folha de fileiras (a de Ajustes), uma
// legenda ASS, o mesmo painel em vidro e solido, o proprio guia, e as cenas
// das Novidades. Sem imagem propria, a previa da opcao-alvo em Ajustes.
// Incluido por ajustes_ux_desenho.inc, depois das pecas e das previas.
#include "novidades170.h"
#include "novidades180.h"
#include "spotlight.h"
#include "buscanorm.h"

typedef struct { const char *titulo, *icone, *sub; } GuiaCap;
enum { GT_NADA = 0, GT_GUIA_TV, GT_BIBLIOTECA, GT_AGENDA, GT_EXPLORAR, GT_PERFIL };
enum { GX_NADA = 0, GX_N180_FONTES, GX_N180_ILHA, GX_N170_ILHA, GX_N170_SPOT };
enum { GV_NADA = 0, GV_GUIA, GV_LAYOUTS, GV_FILEIRAS, GV_FONTES, GV_ILHA, GV_ASS, GV_VIDRO,
       GV_N170_SPOT, GV_N170_ILHA };
typedef struct {
  int cap;
  const char *titulo, *icone;
  int novo;
  const char *id, *txt, *onde;
  int alvo, tela, exemplo, visual;
} GuiaEnt;
#include "ajustes_ux_guia_dados.inc"
#include "p2pmotor.h"

// O cartao do P2P (gerado de guia.json) descreve so o servidor da rede. Num
// pacote com o motor embutido (p2pmotor.h) isso seria falso: a TV baixa e
// compartilha. A busca continua sobre o texto gerado (nao abre o motor).
static const char *guiaTxt(const GuiaEnt *g) {
  if (g->alvo == AJ_P2P_LIGADO && p2pmotor_disponivel())
    return "Toca torrents sem link. Sem servidor na rede, o próprio aparelho baixa e compartilha enquanto toca, com teto de disco, e apaga tudo ao fechar o player. O automático nunca escolhe P2P.";
#ifdef NV_MINIATURAS
  // .tpk: no Seekr key any more, the TV makes the thumbnails (miniatura.c).
  // Same text as the setting's help (already in every language table).
  if (g->alvo == AJ_SEEKR_LIGADO)
    return "A TV faz as miniaturas enquanto o vídeo toca: primeiro onde você procura, depois perto da posição atual e então o filme todo. Pausa sozinha se faltar memória.";
#endif
  return g->txt;
}

// A frase de cada capitulo no inspetor, com o foco no indice.
static const char *const GUIA_CAP_TXT[GUIA_NCAPS] = {
  "O controle, a busca, a conta e a atualização: o que vale em qualquer tela do app.",
  "Como a Home se monta: o layout, quais fileiras entram e em que ordem, o destaque do topo e o Continuar assistindo.",
  "O que a página de cada filme e série mostra: trailer, notas, frases, comentários e de onde vêm os dados.",
  "Da folha de Fontes ao player: como o app escolhe o que tocar e o que dá para mudar.",
  "Legendas, áudio e o idioma do app e do conteúdo.",
  "A pílula do canto da tela, os avisos que saem dela e a central onde eles ficam.",
  "O que você guarda, as listas da conta e dos serviços, a agenda das séries e o Explorar.",
  "Amigos no Nuvio, quem pode te encontrar, Trakt e Simkl, perfis e addons.",
  "O Guia de TV, os canais dos addons e do seu provedor, e o que fazer quando um canal não abre.",
  "Vidro ou sólido, a cor de destaque, a fonte e os cartazes.",
  "Medir esta TV e deixar o app mais leve ou mais bonito, conforme ela aguenta.",
  "Sem internet, addon fora do ar, Trakt desconectado, canal que não abre e vídeo que trava.",
};

// --- ESTADO ----------------------------------------------------------------------
#define GI_BUSCA 0
#define GI_NOVO  1
#define GI_CAP0  2
enum { GC_INDICE = 0, GC_LISTA, GC_BOTOES, GC_CHIP };
typedef struct {
  int idx;      // item do indice (GI_*), capitulo = idx - GI_CAP0
  int col;      // GC_*
  int ent;      // entrada em foco (indice em GUIA_ENT), -1 sem
  int botao;    // botao do inspetor em foco
  int daNov;    // aberto pelo cartao da 1.8.0
  int rotulos;  // miniatura do proprio guia com os rotulos de anatomia
} GuiaVista;
static int guiaExemplo;
static float guiaExT, guiaRol, guiaRolAlvo, guiaRolV;
static GuiaVista gv = { GI_CAP0, GC_INDICE, -1, 0, 0, 0 };
static GfxMini guiaMiniVis, guiaMiniEx;
static int guiaDesenhandoMini;   // dentro da miniatura do proprio guia: sem FBO aninhado
#ifdef NV_TOUCH_UI
static float guiaIdxRol, guiaInspRol;
static void guiaPontIndice(int i, int b);
static void guiaPontIndiceAbrir(int i, int b);
static void guiaPontEnt(int e, int b);
static void guiaPontEntAbrir(int e, int b);
static void guiaPontBotao(int b, int e);
static void guiaPontVoltar(int passo, int b);
#endif
static int pediuTela;
int ajustes_pediu_tela(void) { int v = pediuTela; pediuTela = 0; return v; }
int ajustes_guia_aberto(void) { return guiaAberto; }

static int guiaTotal(void) { return GUIA_NENT; }
static int guiaNovos(void) { int i, n = 0; for (i = 0; i < GUIA_NENT; i++) n += GUIA_ENT[i].novo; return n; }
static int guiaNoCap(int c) { int i, n = 0; for (i = 0; i < GUIA_NENT; i++) n += GUIA_ENT[i].cap == c; return n; }
static int guiaNovosNoCap(int c) { int i, n = 0; for (i = 0; i < GUIA_NENT; i++) n += GUIA_ENT[i].cap == c && GUIA_ENT[i].novo; return n; }

// A LISTA do meio para o item `idx` do indice: entradas do capitulo, ou as
// novas agrupadas por capitulo (cabecalho = -1 - capitulo).
static int guiaLista(int idx, int *out) {
  int n = 0, i, c;
  if (idx == GI_NOVO) {
    for (c = 0; c < GUIA_NCAPS; c++) {
      if (!guiaNovosNoCap(c)) continue;
      out[n++] = -1 - c;
      for (i = 0; i < GUIA_NENT; i++) if (GUIA_ENT[i].cap == c && GUIA_ENT[i].novo) out[n++] = i;
    }
    return n;
  }
  c = idx >= GI_CAP0 ? idx - GI_CAP0 : 0;
  for (i = 0; i < GUIA_NENT; i++) if (GUIA_ENT[i].cap == c) out[n++] = i;
  return n;
}
static int guiaPrimeira(int idx) {
  int l[GUIA_NENT + GUIA_NCAPS], n = guiaLista(idx, l), i;
  for (i = 0; i < n; i++) if (l[i] >= 0) return l[i];
  return -1;
}
// Os botoes do inspetor da entrada: Ver exemplo, Abrir em Ajustes, Abrir <tela>.
enum { GB_EXEMPLO = 0, GB_AJUSTES, GB_TELA };
static int guiaBotoes(int e, int *b) {
  int n = 0;
  if (e < 0) return 0;
  if (GUIA_ENT[e].exemplo) b[n++] = GB_EXEMPLO;
  if (GUIA_ENT[e].alvo >= 0) b[n++] = GB_AJUSTES;
  if (GUIA_ENT[e].tela) b[n++] = GB_TELA;
  return n;
}
static const char *guiaRotTela(int t) {
  switch (t) {
    case GT_GUIA_TV: return "Abrir o Guia de TV";
    case GT_BIBLIOTECA: return "Abrir a Biblioteca";
    case GT_AGENDA: return "Abrir a Agenda";
    case GT_EXPLORAR: return "Abrir o Explorar";
    case GT_PERFIL: return "Abrir Perfil e Stats";
    default: return "";
  }
}
static const char *guiaIcTela(int t) {
  switch (t) {
    case GT_GUIA_TV: return "aj_tv-minimal-play";
    case GT_BIBLIOTECA: return "aj_library";
    case GT_AGENDA: return "aj_calendar";
    case GT_EXPLORAR: return "aj_telescope";
    default: return "aj_chart-column";
  }
}

static void guiaAbrir(int daNov) {
  guiaAberto = 1;
  guiaExemplo = 0;
  memset(&gv, 0, sizeof gv);
  gv.daNov = daNov;
  gv.idx = GI_CAP0;
  if (daNov) { gv.col = GC_LISTA; gv.ent = guiaPrimeira(GI_CAP0); }
  else { gv.col = GC_INDICE; gv.ent = -1; }
  guiaRol = guiaRolAlvo = guiaRolV = 0;
#ifdef NV_TOUCH_UI
  guiaIdxRol = guiaInspRol = 0;
  ajToqueLivre[AJT_INDICE] = ajToqueLivre[AJT_LISTA] = ajToqueLivre[AJT_EDITOR] = 0;
#endif
}
static void guiaFechar(void) {
  guiaAberto = 0;
  guiaDoIndice = 0;
  guiaExemplo = 0;
  gfx_mini_liberar(&guiaMiniVis);
  gfx_mini_liberar(&guiaMiniEx);
}
// Sai pelo Voltar: para o cartao, se veio dele; senao, a linha do guia em
// Sobre e ajuda.
static void guiaSair(void) {
  int nov = gv.daNov, doIndice = guiaDoIndice;
  guiaFechar();
  if (nov) pediuNovidades = 1;
  else if (doIndice) focoIndice = 1;   // volta ao cartao da grade (uxTopo ficou nele)
  else focarOpcao(AJ_GUIA);
}
// "Resolver um problema" (grade, 2.0.3): o guia ja no capitulo cujo icone e
// este, com a primeira entrada em foco.
static void guiaIrCapitulo(const char *icone) {
  int c;
  for (c = 0; c < GUIA_NCAPS; c++)
    if (!strcmp(GUIA_CAPS[c].icone, icone)) {
      gv.idx = GI_CAP0 + c; gv.col = GC_LISTA; gv.ent = guiaPrimeira(gv.idx);
      return;
    }
}

// --- GEOMETRIA --------------------------------------------------------------------
#define GU_IDX_W   380.0f
#define GU_INSP_X 1176.0f
#define GU_GE_H     96.0f
#define GU_GE_GAP    2.0f
static int guiaTelefone(void) {
#ifdef NV_TOUCH_UI
  return !guiaDesenhandoMini && ajTelaCheia();
#else
  return 0;
#endif
}
static GfxRect guiaTelefoneR(void) { return (GfxRect){24, 112, NV_VTELA_W - 48, NV_VTELA_H - 136}; }
static GfxRect guiaIdxR(void) {
  if (guiaTelefone()) return guiaTelefoneR();
  return (GfxRect){ ajX0(), AJI_TOPO, GU_IDX_W, NV_VTELA_H - AJI_TOPO - AJI_MARGEM };
}
// Tela virtual menor (escala.h): a lista fica com pelo menos 560 e a proporcao
// de 1080p (700:704); o inspetor fica com o resto e sai abaixo de 440 (130% e 150%).
// Em 100% e o 1176 de sempre.
static float guiaInspX(void) {
  float x = ajX0() + GU_IDX_W + 24.0f, total, lista;
  if (ajustes_tamanho_ajustes() == 1.0f) return GU_INSP_X;
  total = NV_VTELA_W - AJI_MARGEM - x - 24.0f;
  lista = total * (700.0f / 1428.0f);
  if (lista < 560.0f) lista = 560.0f;
  if (total - lista < 440.0f) return NV_VTELA_W;   // sem inspetor
  return x + lista + 24.0f;
}
static int guiaTemInsp(void) { return guiaInspX() < NV_VTELA_W; }
static GfxRect guiaListaR(void) {
  if (guiaTelefone()) return guiaTelefoneR();
  float x = ajX0() + GU_IDX_W + 24.0f;
  float fim = guiaTemInsp() ? guiaInspX() - 24.0f : NV_VTELA_W - AJI_MARGEM;
  return (GfxRect){ x, AJI_MARGEM, fim - x, NV_VTELA_H - 2 * AJI_MARGEM };
}
static GfxRect guiaInspR(void) {
  if (guiaTelefone()) return guiaTelefoneR();
  return (GfxRect){ guiaInspX(), AJI_MARGEM, NV_VTELA_W - AJI_MARGEM - guiaInspX(), NV_VTELA_H - 2 * AJI_MARGEM };
}

// --- PECAS ---------------------------------------------------------------------------
// "• NOVO": a marca de estado (acento), 12/800 em caixa alta.
static float guiaNovoMarca(const char *s, float x, float yc, float a) {
  int r, g, b;
  float ar, ag, ab, w;
  ajAcento8(&r, &g, &b);
  ajustes_acento(&ar, &ag, &ab);
  gfx_cor((GfxRect){ x, yc - 3, 6, 6 }, 0.5f, ar, ag, ab, a);
  { char up[64];
    idioma_maiusc_em(ajustes_idioma(), up, sizeof up, i18n(s));
    w = txt_tracking(TXT_AJ_MINI12, up, r, g, b, x + 13, yc - 7.5f, a, 1.56f); }
  return 13 + w;
}
// O "Onde fica": cada alternativa numa linha ("ou" na frente da segunda), os
// pedacos separados por "›", o ultimo em Bold claro.
static float guiaTrilha(const char *onde, float x, float y, float w, float a) {
  char buf[400], *alt, *ctx = NULL;
  float y0 = y;
  int k = 0;
  snprintf(buf, sizeof buf, "%s", onde);
  for (alt = strtok_r(buf, "|", &ctx); alt; alt = strtok_r(NULL, "|", &ctx), k++) {
    char seg[12][120];
    int n = 0, i;
    float xx = x;
    char *p = alt;
    while (*p == ' ') p++;
    while (*p && n < 12) {
      char *q = strstr(p, " › ");
      size_t l = q ? (size_t)(q - p) : strlen(p);
      while (l && p[l - 1] == ' ') l--;
      { char cru[120];
        snprintf(cru, sizeof cru, "%.*s", (int)l, p);
        // 2.0.3: o rotulo novo da linha, se o guia ainda diz o antigo.
        snprintf(seg[n++], sizeof seg[0], "%s", rotuloAtualDoAntigo(cru)); }
      if (!q) break;
      p = q + strlen(" › ");
    }
    if (k) {
      TxtLinha ou = ajTxt(TXT_ILHA_HORA, "ou");
      txt_desenhar_alpha(ou, xx, y + 4, 0.35f * a);
      xx += ou.w + 8;
    }
    for (i = 0; i < n; i++) {
      int ult = i == n - 1;
      TxtLinha t = ult ? ajTxtC(TXT_AJ_SEG, seg[i], x + w - xx) : ajTxtC(TXT_AJ_18, seg[i], x + w - xx);
      if (xx + t.w > x + w && xx > x) { xx = x; y += 29.0f; }
      txt_desenhar_alpha(t, xx, y + (21 - t.h) * 0.5f, (ult ? 1.0f : 0.6f) * a);
      xx += t.w + 8;
      if (!ult) {
        TxtLinha s = txt_linha(TXT_AJ_18, "›", AJ_TX);
        txt_desenhar_alpha(s, xx, y + (21 - s.h) * 0.5f, 0.3f * a);
        xx += s.w + 8;
      }
    }
    y += 29.0f;
  }
  return y - y0;
}
// Botao 56 do inspetor (.acoes .btn): icone 22, texto 19/600.
static float guiaBotao(const char *rot, const char *ic, float x, float y, int foco, float a) {
  TxtLinha t = foco ? ajTxtT(TXT_AJ_SEG, rot) : ajTxt(TXT_AJ_SEG, rot);
  float w = 24 + 22 + 12 + t.w + 24;
  if (foco) ajPilulaAcento((GfxRect){ x, y, w, 56 }, 28, a);
  else ajNeutro((GfxRect){ x, y, w, 56 }, 28, 0.08f, AJ_CHIP_SOL, a);
  if (foco) ajIconeT(ic, x + 24, y + 17, 22, a);
  else ajIcone(ic, x + 24, y + 17, 22, 0.8f * 0.88f * a);
  txt_desenhar_alpha(t, x + 24 + 34, y + (56 - t.h) * 0.5f, (foco ? 1.0f : 0.88f) * a);
  return w;
}
// Dicas do inspetor (15 px): o rodape com o fio.
static void guiaDicas(const AjDica *d, int n, GfxRect ir, float a) {
  float x = ir.x + 32, y = ir.y + ir.h - 32 - 30;
  int i;
  gfx_cor((GfxRect){ x, y - 17, ir.w - 64, 1 }, 0, 1, 1, 1, 0.07f * a);
  x += 6;
  for (i = 0; i < n; i++) {
    TxtLinha k = ajTxt(TXT_AJ_KBD, d[i].k), l = ajTxt(TXT_ILHA_APOIO, d[i].l);
    float kw = k.w + 18.0f < 34.0f ? 34.0f : k.w + 18.0f;
    ajNeutro((GfxRect){ x, y, kw, 30 }, 15, 0.09f, AJ_CHIP_SOL, a);
    txt_desenhar_alpha(k, x + (kw - k.w) * 0.5f, y + (30 - k.h) * 0.5f, 0.82f * a);
    txt_desenhar_alpha(l, x + kw + 9, y + (30 - l.h) * 0.5f, 0.5f * a);
    x += kw + 9 + l.w + 18.0f;
  }
}

// --- AS CENAS DAS IMAGENS (tela 1920x1080 de verdade, desenhada na miniatura) ---------
// A folha de Fontes (streams.c no Glass UI) num episodio de 3 Body Problem.
static void guiaMarcas(const char *const *b, int n, float x, float yc, float a) {
  int i;
  for (i = 0; i < n; i++) x += badges_desenhar_tom(badges_bit(b[i]), x, yc - 15, 400, 30, 1, 1, 1, a) + 14.0f;
}
static float guiaFonteLinha(int foco, const char *mk, int pilula, const char *gb, const char *const *b, int nb,
                            const char *tag, int tagAc, const char *ad, const char *arq, float x, float y, float w) {
  float h = 18, ar, ag, ab, yy;
  int r8, g8, b8;
  ajustes_acento(&ar, &ag, &ab);
  ajAcento8(&r8, &g8, &b8);
  h += (mk ? (pilula ? 33.0f : 21.0f) : 0) + 31 + 10 + 30 + (arq ? 28.0f : 0) + 18;
  ajFocoLinha((GfxRect){ x, y, w, h }, 22, foco ? 1.0f : 0.0f, 1);
  x += 22; w -= 44; yy = y + 18;
  if (mk && pilula) {
    char up[64];
    float tw;
    idioma_maiusc_em(ajustes_idioma(), up, sizeof up, i18n(mk));
    tw = txt_tracking(TXT_AJ_MINI12, up, 0, 0, 0, -1, 0, 0, 1.44f);
    gfx_cor((GfxRect){ x, yy, tw + 22, 23 }, 0.5f, ar, ag, ab, 1);
    { int t = ajTinta(); txt_tracking(TXT_AJ_MINI12, up, t, t, t, x + 11, yy + 4.5f, 1, 1.44f); }
    yy += 33;
  } else if (mk) {
    char up[64];
    idioma_maiusc_em(ajustes_idioma(), up, sizeof up, i18n(mk));
    gfx_cor((GfxRect){ x, yy + 4, 7, 7 }, 0.5f, ar, ag, ab, 1);
    txt_tracking(TXT_AJ_CAPS13, up, r8, g8, b8, x + 15, yy, 1, 1.56f);
    yy += 21;
  }
  { TxtLinha n = txt_linha(TXT_G26B, "3 Body Problem", AJ_TX), e = ajTxt(TXT_AJ_18, "Temporada 1 Episódio 3");
    TxtLinha g = ajTxt(TXT_G26B, gb), u = ajTxt(TXT_AJ_16B, "GB");
    float cy = yy + 15;
    txt_desenhar_alpha(n, x, cy - n.h * 0.5f, foco ? 1.0f : 0.85f);
    txt_desenhar_alpha(e, x + n.w + 12, cy - e.h * 0.5f + 3, 0.5f);
    txt_desenhar_alpha(u, x + w - u.w, cy - u.h * 0.5f + 3, 0.45f);
    txt_desenhar(g, x + w - u.w - 5 - g.w, cy - g.h * 0.5f); }
  yy += 31 + 10;
  guiaMarcas(b, nb, x, yy + 15, foco ? 0.82f : 0.55f);
  { TxtLinha l = ajTxt(TXT_ILHA_GENERO, ad);
    float xr = x + w - l.w;
    txt_desenhar_alpha(l, xr, yy + 15 - l.h * 0.5f, 0.42f);
    if (tag) {
      if (tagAc) {
        char up[16];
        float tw;
        idioma_maiusc_em(ajustes_idioma(), up, sizeof up, tag);
        tw = txt_tracking(TXT_AJ_CAPS13, up, 0, 0, 0, -1, 0, 0, 1.3f);
        txt_tracking(TXT_AJ_CAPS13, up, r8, g8, b8, xr - 14 - tw, yy + 15 - 7, 1, 1.3f);
      } else {
        TxtLinha t = ajTxt(TXT_ILHA_GENERO, tag);
        txt_desenhar_alpha(t, xr - 14 - t.w, yy + 15 - t.h * 0.5f, 0.42f);
      } } }
  yy += 30;
  if (arq) { TxtLinha l = ajTxt(TXT_ILHA_HORA, arq); txt_desenhar_alpha(l, x, yy + 10, 0.4f); }
  return h;
}
static float guiaSecFonte(const char *nome, const char *sub, const char *qtd, float x, float y, float w) {
  TxtLinha n = ajTxt(TXT_AJ_SECAO, nome), q = ajTxt(TXT_ILHA_GENERO, qtd);
  txt_desenhar(n, x + 22, y + 13 - n.h * 0.5f);
  ajCaps(TXT_AJ_CAPS13, sub, 1.82f, x + 22 + n.w + 12, y + 7, 0.4f);
  txt_desenhar_alpha(q, x + w - 22 - q.w, y + 13 - q.h * 0.5f, 0.38f);
  gfx_cor((GfxRect){ x, y + 36, w, 1 }, 0, 1, 1, 1, 0.08f);
  return 37 + 6;
}
static void guiaCenaFontes(void) {
  GfxRect f = { 1920 - 40 - 860, 40, 860, 1000 };
  static const char *const SEG[3] = { "Todos", "AIOStreams", "Torrentio" };
  static const char *const NUM[3] = { "6", "4", "2" };
  static const char *const B1[3] = { "r-4k", "v-dv", "a-atmos" }, *const B2[3] = { "r-4k", "v-hdr10plus", "a-truehd" };
  static const char *const B3[2] = { "r-4k", "v-hdr" }, *const B4[1] = { "r-1080" };
  float x, y, w, xd, ar, ag, ab;
  ajustes_acento(&ar, &ag, &ab);
  ajImg(ajArte(AJA_FUNDO, 15), (GfxRect){ 0, 0, 1920, 1080 }, 0, 1);
  gfx_cor((GfxRect){ 0, 0, 1920, 1080 }, 0, 0, 0, 0, 0.30f);
  ajIlha(f, 36, 1, 0);
  x = f.x + 26 + 18; y = f.y + 40;
  ajKicker("3 Body Problem · T1 E3", x, y + 1, 1);
  { TxtLinha t = ajTxt(TXT_ILHA_TITULO, "Fontes"); txt_desenhar(t, x, y + 23 + (46 - t.h) * 0.5f); }
  xd = f.x + f.w - 26 - 18;
  { GfxRect d = { xd - 56, y + 13, 56, 56 };
    ajNeutro(d, 28, 0.08f, AJ_CHIP_SOL, 1);
    ajIcone("aj_rotate-cw", d.x + 17, d.y + 17, 22, 0.85f);
    xd -= 56 + 10; }
  { static const char *const C[2] = { "Dublado", "Em cache" };
    int i;
    for (i = 0; i < 2; i++) {
      TxtLinha t = ajTxt(TXT_AJ_SEG, C[i]);
      float cw = 22 + t.w + 22;
      ajNeutro((GfxRect){ xd - cw, y + 13, cw, 56 }, 28, 0.08f, AJ_CHIP_SOL, 1);
      txt_desenhar_alpha(t, xd - cw + 22, y + 13 + (56 - t.h) * 0.5f, 0.85f);
      xd -= cw + 10;
    }
    { int r, g, b;
      TxtLinha t;
      float cw;
      ajAcento8(&r, &g, &b);
      t = txt_linha(TXT_AJ_SEG, "Só MP4", r, g, b, 255);
      cw = 22 + 8 + 10 + t.w + 22;
      gfx_cor((GfxRect){ xd - cw, y + 13, cw, 56 }, 0.5f, ar, ag, ab, 0.22f);
      gfx_cor((GfxRect){ xd - cw + 22, y + 13 + 24, 8, 8 }, 0.5f, ar, ag, ab, 1);
      txt_desenhar(t, xd - cw + 40, y + 13 + (56 - t.h) * 0.5f); } }
  ajSeg(SEG, NUM, 3, 0, 0, f.x + 26 + 14, y + 91 + 27.5f, 55, 1);
  x = f.x + 26; w = f.w - 52; y = f.y + 40 + 69 + 22 + 55 + 22;
  y += guiaSecFonte("4K", "Ultra HD · HDR", "3 fontes", x, y, w);
  y += guiaFonteLinha(1, "Melhor para esta TV", 1, "9,8", B1, 3, "MP4", 1, "AIOStreams",
                      "3.Body.Problem.S01E03.2160p.DV.Atmos.mp4", x, y, w);
  y += guiaFonteLinha(0, "Reproduzindo agora", 0, "11,1", B2, 3, NULL, 0, "AIOStreams", NULL, x, y, w);
  y += guiaFonteLinha(0, NULL, 0, "7,4", B3, 2, "Fora do cache", 0, "Torrentio", NULL, x, y, w);
  y += 20; y += guiaSecFonte("1080p", "Full HD", "3 fontes", x, y, w);
  guiaFonteLinha(0, "Sua escolha anterior", 0, "3,1", B4, 1, "Dublado", 0, "AIOStreams", NULL, x, y, w);
}
// Os avisos da ilha (os textos de ilhasinais.c): rede, addon, Trakt e amizade.
static void guiaPilulaIlha(float y, const char *ic, float cr, float cg, float cb, const char *pre, const char *forte,
                           const char *pos, int azul) {
  TxtLinha a = ajTxt(TXT_ILHA_FORTE, pre), f = { 0 }, p = { 0 };
  float esp = (float)(txt_largura(TXT_ILHA_FORTE, "a a") - txt_largura(TXT_ILHA_FORTE, "aa"));
  float ea = pre[0] && pre[strlen(pre) - 1] == ' ' ? esp : 0, ep = pos && pos[0] == ' ' ? esp : 0;
  float w = 22 + 24 + 14 + a.w + ea + ep + 26, x = 48;
  if (forte) { f = ajTxt(TXT_ILHA_NOME, forte); w += f.w; }
  if (pos) { p = ajTxt(TXT_ILHA_FORTE, pos); w += p.w; }
  if (azul) w += 6 + 60 + 8 + 40;
  ajIlha((GfxRect){ x, y, w, 56 }, 28, 1, 0);
  gfx_luz_canto((GfxRect){ x, y, 120, 56 }, 0.5f, 0, 0, 100, cr, cg, cb, 0.30f);
  gfx_icone((GfxRect){ x + 22, y + 16, 24, 24 }, ic, cr, cg, cb, 1);
  x += 22 + 24 + 14;
  txt_desenhar(a, x, y + (56 - a.h) * 0.5f); x += a.w + ea;
  if (forte) { txt_desenhar(f, x, y + (56 - f.h) * 0.5f); x += f.w + ep; }
  if (pos) { txt_desenhar(p, x, y + (56 - p.h) * 0.5f); x += p.w; }
  if (azul) {
    TxtLinha k = ajTxt(TXT_AJ_KBD, "CH+"), l = ajTxt(TXT_AJ_CHIP, "abre");
    float kw = k.w + 18;
    x += 6;
    ajNeutro((GfxRect){ x, y + 13, kw, 30 }, 15, 0.09f, AJ_CHIP_SOL, 1);
    txt_desenhar_alpha(k, x + 9, y + 13 + (30 - k.h) * 0.5f, 0.82f);
    txt_desenhar_alpha(l, x + kw + 8, y + (56 - l.h) * 0.5f, 0.5f);
  }
}
static void guiaCenaIlha(void) {
  float ar, ag, ab;
  GfxRect c = { 48, 196, 1060, 300 };
  char b[200], *p;
  ajustes_acento(&ar, &ag, &ab);
  ajImg(ajArte(AJA_FUNDO, 0), (GfxRect){ 0, 0, 1920, 1080 }, 0, 1);
  gfx_cor((GfxRect){ 0, 0, 1920, 1080 }, 0, 0, 0, 0, 0.30f);
  guiaPilulaIlha(36, "aj_wifi-off", 0.910f, 0.722f, 0.290f, i18n("Sem internet. Mostrando o que já estava salvo."), NULL, NULL, 0);
  // "O addon %s nao respondeu": o nome em Bold, como ilha_forte.
  snprintf(b, sizeof b, "%s", i18n("O addon %s não respondeu"));
  p = strstr(b, "%s");
  if (p) { *p = 0; guiaPilulaIlha(116, "aj_puzzle", 0.910f, 0.722f, 0.290f, b, "Torrentio", p + 2, 0); }
  ajIlha(c, 36, 1, 0);
  gfx_luz_canto(c, 36.0f / c.h, 0, 0, c.h * 1.4f, ar, ag, ab, 0.16f);
  gfx_icone((GfxRect){ c.x + 40, c.y + 36, 22, 22 }, "aj_unlink", ar, ag, ab, 1);
  ajKicker("Trakt", c.x + 40 + 34, c.y + 38, 1);
  { TxtLinha q = ajTxt(TXT_ILHA_PERGUNTA, "A sessão do Trakt expirou");
    txt_desenhar(q, c.x + 40, c.y + 36 + 30); }
  ajBloco(TXT_AJ_TEXTO, i18n("Enquanto isso, o que você assistir não vai para o Trakt. Leva um minuto: um código na TV, o celular confirma."),
          c.x + 40, c.y + 36 + 30 + 52, 900, 30, 0.62f, 2);
  { float x = c.x + 40, y = c.y + c.h - 34 - 60;
    x += ajBotao("Reconectar", NULL, x, y, 1, 1) + 12;
    ajBotao("Depois", NULL, x, y, 0, 1); }
  snprintf(b, sizeof b, "%s", i18n("%s quer ser seu amigo"));
  p = strstr(b, "%s");
  if (p) { *p = 0; guiaPilulaIlha(560, "aj_users", ar, ag, ab, b, "Marina", p + 2, 1); }
}
// Um quadro de video com uma legenda ASS: letreiro no topo, duas falas embaixo.
static void guiaTextoContorno(TxtEstilo e, int enf, const char *s, int r, int g, int b, float cx, float y, int esp, float track) {
  TxtLinha f = txt_linha_corta_enfase(e, s, r, g, b, 255, 1900, TXT_FAMILIA_INTER, enf);
  TxtLinha k = txt_linha_corta_enfase(e, s, 11, 11, 16, 255, 1900, TXT_FAMILIA_INTER, enf);
  float x = cx - f.w * 0.5f;
  int dx, dy;
  (void)track;
  for (dy = -esp; dy <= esp; dy += esp) for (dx = -esp; dx <= esp; dx += esp)
    if (dx || dy) txt_desenhar(k, x + dx, y + dy);
  txt_desenhar_alpha(k, x, y + 6, 0.45f);
  txt_desenhar(f, x, y);
}
static void guiaCenaASS(void) {
  ajImg(ajArte(AJA_FUNDO, 14), (GfxRect){ 0, 0, 1920, 1080 }, 0, 1);
  gfx_veu_css((GfxRect){ 0, 0, 1920, 324 }, 1, 1.0f, 1.0f, 0.15f);
  gfx_veu_css((GfxRect){ 0, 324, 1920, 756 }, 0, 1.0f, 1.0f, 0.25f);
  // Letreiro (sign) no topo e as duas falas embaixo, como o assrender desenha:
  // cor, contorno e sombra de cada estilo do arquivo.
  guiaTextoContorno(TXT_LEG_180, TXT_ENF_ITALICO | TXT_ENF_NEGRITO, "Observatório de Arecibo", 255, 215, 106, 960, 86, 5, 0);
  guiaTextoContorno(TXT_LEG_100, TXT_ENF_NEGRITO, "P O R T O   R I C O   ·   1 9 7 4", 207, 232, 255, 960, 192, 3, 0);
  guiaTextoContorno(TXT_LEG_150, TXT_ENF_NEGRITO, "Você também ouviu isso?", 255, 255, 255, 960, 1080 - 184 - 74, 4, 0);
  guiaTextoContorno(TXT_LEG_150, TXT_ENF_NEGRITO, "Fica perto da antena. Não desliga nada.", 159, 227, 255, 960, 1080 - 100 - 74, 4, 0);
}
// O mesmo painel (Salvos) nos dois materiais, lado a lado.
static void guiaPainelMaterial(float x, int vidro, const char *nome) {
  GfxRect r = { x, 150, 800, 820 };
  static const char *const SEG[4] = { "Salvos", "Atividade", "Amigos", "Avisos" };
  static const char *const NUM[4] = { "24", NULL, NULL, NULL };
  static const struct { int po; const char *t, *s; } L[4] = {
    { 13, "Project Hail Mary", "Filme · 2026" }, { 15, "3 Body Problem", "Série · T1 E4" },
    { 21, "Lost", "Série · T2 E7" }, { 0, "Fallout", "Série · T1 E3" } };
  int salvo = valor[AJ_VIDRO], i;
  float y;
  valor[AJ_VIDRO] = vidro ? 0 : 1;   // a ilha deste lado no material pedido
  ajIlha(r, 36, 1, 0);
  ajKicker("Salvos", x + 44, r.y + 40, 1);
  { TxtLinha t = ajTxt(TXT_ILHA_TITULO, nome); txt_desenhar(t, x + 44, r.y + 63); }
  ajSeg(SEG, NUM, 4, 0, 0, x + 34, r.y + 150, 55, 1);
  y = r.y + 200;
  for (i = 0; i < 4; i++) {
    GfxRect ln = { x + 30, y, 740, 154 };
    TxtLinha t = ajTxt(TXT_G28B, L[i].t), s = ajTxt(TXT_AJ_SUB, L[i].s);
    if (i == 1) ajFocoLinha(ln, 22, 1, 1);
    ajImg(ajArte(AJA_POSTER, L[i].po), (GfxRect){ ln.x + 18, y + 14, 84, 126 }, 12, 1);
    txt_desenhar_alpha(t, ln.x + 124, y + 77 - t.h - 3, i == 1 ? 1.0f : 0.85f);
    txt_desenhar_alpha(s, ln.x + 124, y + 77 + 6, 0.5f);
    y += 158;
  }
  valor[AJ_VIDRO] = salvo;
}
static void guiaCenaVidro(void) {
  ajImg(ajArte(AJA_FUNDO, 7), (GfxRect){ 0, 0, 1920, 1080 }, 0, 1);
  gfx_cor((GfxRect){ 0, 0, 1920, 1080 }, 0, 0, 0, 0, 0.18f);
  guiaPainelMaterial(120, 1, "Vidro");
  guiaPainelMaterial(1000, 0, "Sólido");
}

static void guiaDesenharTela(const GuiaVista *v, float a);
// O proprio guia em miniatura (Home e fileiras, "Três layouts da Home" em
// foco), com os rotulos de anatomia quando pedidos.
static void guiaCenaGuia(int rotulos) {
  GuiaVista m = { GI_CAP0 + 1, GC_LISTA, -1, 0, 0, 0 };
  int i;
  for (i = 0; i < GUIA_NENT; i++) if (!strcmp(GUIA_ENT[i].id, "h-layout")) m.ent = i;
  { int arte = ajArteFundoN; ajArteFundoN = 12;
    ajFundo();
    ajArteFundoN = arte; }
  guiaDesenhandoMini = 1;
  guiaDesenharTela(&m, 1);
  guiaDesenhandoMini = 0;
  if (rotulos) {
    static const struct { float x, y; const char *n, *l; } A[3] = {
      { 70, 500, "1", "Capítulos" }, { 560, 430, "2", "Recursos" }, { 1250, 640, "3", "Exemplo e atalho" } };
    gfx_cor((GfxRect){ 0, 0, 1920, 1080 }, 0, 0, 0, 0, 0.28f);
    for (i = 0; i < 3; i++) {
      TxtLinha n = ajTxtT(TXT_TITULO3, A[i].n), l = ajTxtT(TXT_TITULO3, A[i].l);
      float w = 34 + n.w + 8 + l.w + 34;
      ajPilulaAcento((GfxRect){ A[i].x, A[i].y, w, 84 }, 42, 1);
      txt_desenhar_alpha(n, A[i].x + 34, A[i].y + (84 - n.h) * 0.5f, 0.6f);
      txt_desenhar(l, A[i].x + 34 + n.w + 8, A[i].y + (84 - l.h) * 0.5f);
    }
  }
}

// Desenha a cena `gv` numa miniatura do tamanho `w` x `h` em (x, y): a regiao
// [x0, x0 + w/esc] x [y0, ...] do layout. `fade` = a base some (o .gv.fade).
static int guiaMiniCena(GfxMini *m, int tipo, int ent, GfxRect r, float x0, float y0, float esc, float raio, int fade) {
  (void)ent;
  if (guiaDesenhandoMini) return 0;
  if (!gfx_mini_alvo(m, (int)r.w, (int)r.h)) return 0;
  gfx_mini_comecar(m, x0, y0, esc);
  gfx_cor((GfxRect){ x0, y0, r.w / esc, r.h / esc }, 0, 0.047f, 0.051f, 0.063f, 1);
  switch (tipo) {
    case GV_GUIA: guiaCenaGuia(gv.daNov || ent >= 0); break;
    case GV_FONTES: guiaCenaFontes(); break;
    case GV_ILHA: guiaCenaIlha(); break;
    case GV_ASS: guiaCenaASS(); break;
    case GV_VIDRO: guiaCenaVidro(); break;
    case GV_FILEIRAS: {
      int f0 = filFoco, c0 = filCampo, a0 = filAba, t0 = filTopo, b0 = filNaBarra;
      ajImg(ajArte(AJA_FUNDO, 2), (GfxRect){ 0, 0, 1920, 1080 }, 0, 1);
      filFoco = 3; filCampo = 2; filAba = 0; filTopo = 0; filNaBarra = 0;
      desenhaFileiras();
      filFoco = f0; filCampo = c0; filAba = a0; filTopo = t0; filNaBarra = b0;
    } break;
    case GV_N170_SPOT: case GV_N170_ILHA:
      novidades170_cena_desenhar(tipo == GV_N170_SPOT ? 1 : 0, 0, 0, tipo == GV_N170_SPOT ? 5.6f : 8.0f);
      break;
    default: break;
  }
  if (fade) gfx_mini_esvanecer((GfxRect){ x0, y0 + (r.h - 60) / esc, r.w / esc, 60 / esc }, 0.85f);
  gfx_mini_terminar();
  gfx_mini_desenhar(m, r, raio, 1);
  return 1;
}

// A IMAGEM do inspetor: a cena propria, ou a previa da opcao em Ajustes.
// Devolve a altura usada.
static float guiaVisual(int tipo, int ent, float x, float y, float w) {
  switch (tipo) {
    case GV_LAYOUTS: { AjMini m = ajMiniPadrao(); m.lay = HOME_LAYOUT_MODERNA; m.art = 13; m.relogio = 2; m.rail = 1; m.semHero = 0;
                       ajMiniHome(m, x, y, w); return w * 9.0f / 16.0f; }
    case GV_GUIA:     return guiaMiniCena(&guiaMiniVis, tipo, ent, (GfxRect){ x, y, w, w * 9 / 16 }, 0, 0, w / 1920.0f, 22, 0) ? w * 9 / 16 : 0;
    case GV_ASS:      return guiaMiniCena(&guiaMiniVis, tipo, ent, (GfxRect){ x, y, w, w * 9 / 16 }, 0, 0, w / 1920.0f, 22, 0) ? w * 9 / 16 : 0;
    case GV_VIDRO:    return guiaMiniCena(&guiaMiniVis, tipo, ent, (GfxRect){ x, y, w, w * 9 / 16 }, 0, 0, w / 1920.0f, 22, 0) ? w * 9 / 16 : 0;
    case GV_FONTES:   return guiaMiniCena(&guiaMiniVis, tipo, ent, (GfxRect){ x, y, w, 440 }, 1020, 40, w / 860.0f, 22, 1) ? 440 : 0;
    case GV_ILHA:     return guiaMiniCena(&guiaMiniVis, tipo, ent, (GfxRect){ x, y, w, 400 }, 0, 10, w / 1140.0f, 22, 1) ? 400 : 0;
    case GV_FILEIRAS: return guiaMiniCena(&guiaMiniVis, tipo, ent, (GfxRect){ x, y, w, 400 }, 60, 120, w / 1160.0f, 22, 1) ? 400 : 0;
    case GV_N170_SPOT: case GV_N170_ILHA: {
      // A cena da 1.7 e de pe (760 de largura): inteira pela altura, centrada.
      float h = 440, esc = h / N170_PREVIA_H, cw = N170_PREVIA_W * esc;
      gfx_cor((GfxRect){ x, y, w, h }, 22 / h, 0.047f, 0.051f, 0.063f, 1);
      return guiaMiniCena(&guiaMiniVis, tipo, ent, (GfxRect){ x + (w - cw) * 0.5f, y, cw, h }, 0, 0, esc, 22, 0) ? h : 0;
    }
    default: break;
  }
  if (ent >= 0 && GUIA_ENT[ent].alvo >= 0 && !guiaDesenhandoMini) return ajPrevia(GUIA_ENT[ent].alvo, x, y, w);
  return 0;
}
static int guiaVisualCap(int c) {
  int i;
  if (c == 1) return GV_LAYOUTS;
  for (i = 0; i < GUIA_NENT; i++) if (GUIA_ENT[i].cap == c && GUIA_ENT[i].visual) return GUIA_ENT[i].visual;
  return GV_NADA;
}

// --- INDICE ----------------------------------------------------------------------
static void guiaDesenharIndice(const GuiaVista *v, float a) {
  GfxRect il = guiaIdxR();
  float y = il.y + 14, ar, ag, ab;
  int j;
  ajustes_acento(&ar, &ag, &ab);
  ajIlha(il, 32, a, 0);
#ifdef NV_TOUCH_UI
  if (ajTelaCheia() && !guiaDesenhandoMini) {
    float total = (GI_CAP0 + GUIA_NCAPS) * 56.0f + 43.0f;
    float alvo = fmaxf(0, v->idx * 56.0f - (il.h - 82.0f) * 0.5f);
    y -= ajToqueRegistrar(AJT_INDICE, il, &guiaIdxRol, total - il.h, alvo, 310);
    gfx_recorte(il.x, il.y, il.w, il.h);
  }
#endif
  for (j = 0; j < GI_CAP0 + GUIA_NCAPS; j++) {
    int sel = j == v->idx, foco = sel && v->col == GC_INDICE, at = sel && !foco;
    const char *t = j == GI_BUSCA ? "Buscar no guia" : j == GI_NOVO ? "Novo na 2.0" : GUIA_CAPS[j - GI_CAP0].titulo;
    const char *ic = j == GI_BUSCA ? "aj_search" : j == GI_NOVO ? "aj_sparkles" : GUIA_CAPS[j - GI_CAP0].icone;
    GfxRect r;
    float al = foco || at ? 1.0f : 0.66f;
    if (j == GI_CAP0) { gfx_cor((GfxRect){ il.x + 30, y + 6, il.w - 60, 1 }, 0, 1, 1, 1, 0.08f * a); y += 15; }
    r = (GfxRect){ il.x + 14, y, il.w - 28, 54 };
#ifdef NV_TOUCH_UI
    if (ajTelaCheia() && !guiaDesenhandoMini)
      ponteiro_alvo_faixa(r.x, r.y, r.w, r.h, il.y, il.y + il.h,
                          guiaPontIndice, guiaPontIndiceAbrir, j, 0);
#endif
    if (foco) ajNeutro(r, 27, 0.14f, 0.169f, 0.176f, 0.204f, a);
    else if (at) gfx_cor((GfxRect){ r.x + 7, r.y + 24.5f, 5, 5 }, 0.5f, ar, ag, ab, a);
    ajIcone(ic, r.x + 18, r.y + 15, 24, (foco || at ? 1.0f : 0.55f) * a);
    { char n[16];
      TxtLinha q = { 0 }, l;
      float qx, lmax;
      int r8, g8, b8;
      ajAcento8(&r8, &g8, &b8);
      if (j == GI_NOVO) { snprintf(n, sizeof n, "%d", guiaNovos()); q = txt_linha(TXT_AJ_16B, n, r8, g8, b8, 255); }
      else if (j >= GI_CAP0) { snprintf(n, sizeof n, "%d", guiaNoCap(j - GI_CAP0)); q = ajTxt(TXT_ILHA_GENERO, n); }
      // O nome inteiro sempre que couber: a contagem anda para a direita (o
      // flex do mockup), e so corta com reticencias quando nem assim cabe.
      lmax = r.w - 56 - (q.w ? q.w + (j == GI_NOVO ? 14 : 0) + 12 : 0) - 4;
      l = ajTxtC(TXT_AJ_VALOR, t, lmax);
      txt_desenhar_alpha(l, r.x + 56, r.y + (54 - l.h) * 0.5f, al * a);
      if (q.w) {
        qx = r.x + r.w - 18 - q.w;
        if (qx < r.x + 56 + l.w + 12 + (j == GI_NOVO ? 14 : 0)) qx = r.x + 56 + l.w + 12 + (j == GI_NOVO ? 14 : 0);
        if (j == GI_NOVO) {
          txt_desenhar_alpha(q, qx, r.y + (54 - q.h) * 0.5f, a);
          gfx_cor((GfxRect){ qx - 14, r.y + 23.5f, 7, 7 }, 0.5f, ar, ag, ab, a);
        } else txt_desenhar_alpha(q, qx, r.y + (54 - q.h) * 0.5f, (foco ? 0.6f : 0.34f) * a);
      } }
    y += 56;
  }
#ifdef NV_TOUCH_UI
  if (ajTelaCheia() && !guiaDesenhandoMini) gfx_sem_recorte();
#endif
}

// --- LISTA --------------------------------------------------------------------------
// A primeira frase do texto (a linha do recurso), sem o ponto.
static const char *guiaFrase(const char *txt) {
  static char b[400];
  char *p;
  snprintf(b, sizeof b, "%s", i18n(txt));
  p = strstr(b, ". ");
  if (!p) p = strstr(b, "。");
  if (p) *p = 0;
  else { size_t n = strlen(b); if (n && b[n - 1] == '.') b[n - 1] = 0; }
  return b;
}
static void guiaDesenharEnt(int e, float x, float y, float w, int foco, int sel, int semNovo, float a) {
  GfxRect r = { x, y, w, GU_GE_H };
  const GuiaEnt *g = &GUIA_ENT[e];
  float tx = x + 88, tw = w - 88 - 22 - 24 - 12;
  if (a <= 0.004f) return;
  if (foco) ajFocoLinha(r, 22, 1, a);
  else if (sel) ajNeutro(r, 22, 0.06f, 0.125f, 0.129f, 0.153f, a);
  { GfxRect ic = { x + 22, y + 24, 48, 48 };
    if (foco) ajNeutro(ic, 15, 0.12f, 0.220f, 0.227f, 0.263f, a);
    else ajNeutro(ic, 15, 0.07f, 0.125f, 0.129f, 0.153f, a);
    ajIcone(g->icone, ic.x + 11, ic.y + 11, 26, (foco ? 1.0f : 0.72f) * a); }
  { TxtLinha t = ajTxtC(TXT_AJ_NOME, g->titulo, tw);
    float nx = tx + t.w + 12;
    txt_desenhar_alpha(t, tx, y + 20.7f + (28 - t.h) * 0.5f, (foco ? 1.0f : 0.88f) * a);
    if (g->novo && !semNovo && nx + 60 < tx + tw) guiaNovoMarca("Novo", nx, y + 34.7f, a); }
  { TxtLinha s = ajTxtC(TXT_ILHA_GENERO, guiaFrase(guiaTxt(g)), tw);
    txt_desenhar_alpha(s, tx, y + 53.7f + (21.6f - s.h) * 0.5f, (foco ? 0.62f : 0.45f) * a); }
  ajChevron(x + w - 22, y + 48, (foco ? 0.7f : 0.32f) * a);
}
static float guiaAltItem(int it) { return it < 0 ? 37 + 4 + 18 : GU_GE_H + GU_GE_GAP; }
static float guiaListaAltura(const int *l, int n) {
  float total = 0;
  for (int i = 0; i < n; i++) total += l[i] < 0 ? (i ? 18 : 8) + 41 : GU_GE_H + GU_GE_GAP;
  return total;
}
static void guiaDesenharLista(const GuiaVista *v, float a) {
  GfxRect fr = guiaListaR();
  int l[GUIA_NENT + GUIA_NCAPS], n = guiaLista(v->idx, l), i;
  float topo, base = fr.y + fr.h, y, x = fr.x + 16, w = fr.w - 32;
  char kick[160], tit[96], sub[160];
  float reserva = 0;
  ajIlha(fr, 36, a, 0);
  if (v->idx == GI_NOVO) {
    snprintf(kick, sizeof kick, "%s", "Guia de uso");
    snprintf(tit, sizeof tit, "%s", "Novo na 2.0");
    snprintf(sub, sizeof sub, i18n("%d recursos novos ou que mudaram nesta versão"), guiaNovos());
  } else {
    int c = v->idx >= GI_CAP0 ? v->idx - GI_CAP0 : 0;
    if (v->daNov) snprintf(kick, sizeof kick, "%s › %s", i18n("Novidades da 1.8.0"), i18n("Guia de uso"));
    else snprintf(kick, sizeof kick, i18n("Guia de uso · Capítulo %d de %d"), c + 1, GUIA_NCAPS);
    snprintf(tit, sizeof tit, "%s", GUIA_CAPS[c].titulo);
    snprintf(sub, sizeof sub, "%s", GUIA_CAPS[c].sub);
  }
  if (v->daNov && !guiaTelefone()) {
    TxtLinha t = ajTxt(TXT_AJ_SEG, "Voltar às novidades");
    float cw = 22 + 20 + 10 + t.w + 22;
    ajChip("Voltar às novidades", "aj_corner-up-left", fr.x + fr.w - 30 - cw, fr.y + 34, v->col == GC_CHIP, a);
#ifdef NV_TOUCH_UI
    if (ajTelaCheia() && !guiaDesenhandoMini) ponteiro_alvo(fr.x + fr.w - 30 - cw, fr.y + 34, cw, 56, NULL, guiaPontVoltar, 0, 0);
#endif
    reserva = cw + 20;
  }
  if (guiaTelefone()) {
    TxtLinha k = ajTxtC(TXT_AJ_KBD, kick, fr.w - 48), t = ajTxtC(TXT_AJ_INSP, tit, fr.w - 48);
    TxtLinha s = ajTxtC(TXT_AJ_SUB, sub, fr.w - 48);
    txt_desenhar_alpha(k, fr.x + 24, fr.y + 20, .45f * a);
    txt_desenhar_alpha(t, fr.x + 24, fr.y + 48, a);
    txt_desenhar_alpha(s, fr.x + 24, fr.y + 94, .62f * a);
    topo = fr.y + 144;
    base -= 16;
  } else {
    topo = ajCabecalhoFolha(fr, kick, tit, sub, reserva, a) - 0.0f;
    topo = fr.y + 155.6f;
  }
#ifdef NV_TOUCH_UI
  if (ajTelaCheia() && !guiaDesenhandoMini)
    ajToqueRegistrar(AJT_LISTA, (GfxRect){fr.x, topo, fr.w, base - topo}, &guiaRol,
                      guiaListaAltura(l, n) - (base - topo) + 20, guiaRol, v->idx + 320);
#endif
  gfx_recorte(fr.x, topo, fr.w, base - topo);
  y = topo - (guiaDesenhandoMini ? 0 : guiaRol);
  for (i = 0; i < n; i++) {
    float h = guiaAltItem(l[i]), yc;
    if (l[i] < 0) {
      float yy = y + (i ? 18 : 8);
      yc = yy + 18;
      if (yy + 40 >= topo && yy <= base) {
        float al = a * (yc > base - 90 ? (base - yc) / 90 : 1);
        int c = -1 - l[i];
        char q[8];
        TxtLinha t = ajTxt(TXT_AJ_SECAO, GUIA_CAPS[c].titulo), qq;
        snprintf(q, sizeof q, "%d", guiaNovosNoCap(c));
        qq = ajTxt(TXT_ILHA_GENERO, q);
        txt_desenhar_alpha(t, x + 22, yy + 13 - t.h * 0.5f, al);
        txt_desenhar_alpha(qq, x + w - 22 - qq.w, yy + 15 - qq.h * 0.5f, 0.38f * al);
        gfx_cor((GfxRect){ x, yy + 36, w, 1 }, 0, 1, 1, 1, 0.08f * al);
      }
      y += (i ? 18 : 8) + 37 + 4;
      continue;
    }
    yc = y + GU_GE_H * 0.5f;
    if (y + GU_GE_H >= topo && y <= base) {
#ifdef NV_TOUCH_UI
      if (ajTelaCheia() && !guiaDesenhandoMini)
        ponteiro_alvo_faixa(x, y, w, GU_GE_H, topo, base, guiaPontEnt, guiaPontEntAbrir, l[i], 0);
#endif
      float al = yc > base - 90 ? (base - yc) / 90 : 1;
      if (al < 0) al = 0;
      guiaDesenharEnt(l[i], x, y, w, l[i] == v->ent && v->col == GC_LISTA, l[i] == v->ent && v->col == GC_BOTOES,
                      v->idx == GI_NOVO, a * al);
    }
    y += h;
  }
  gfx_sem_recorte();
}
// Rolagem: a entrada em foco fica a ~35% da lista quando chegaria nos 90 px
// de baixo (a regra do mockup).
static void guiaRolar(void) {
  GfxRect fr = guiaListaR();
  int l[GUIA_NENT + GUIA_NCAPS], n = guiaLista(gv.idx, l), i;
  float y = 0, H = guiaTelefone() ? fr.h - 160.0f : fr.y + fr.h - (fr.y + 155.6f), total = 0, alvo = 0;
  for (i = 0; i < n; i++) {
    if (l[i] < 0) { y += (i ? 18 : 8) + 41; continue; }
    if (l[i] == gv.ent) alvo = y;
    y += GU_GE_H + GU_GE_GAP;
  }
  total = y;
  if (gv.ent < 0 || alvo + GU_GE_H <= H - 90) { guiaRolAlvo = 0; return; }
  guiaRolAlvo = alvo - H * 0.35f;
  if (guiaRolAlvo > total - H + 20) guiaRolAlvo = total - H + 20;
  if (guiaRolAlvo < 0) guiaRolAlvo = 0;
}

#ifdef NV_TOUCH_UI
// No telefone o inspetor e uma pagina: nada desaparece quando as tres colunas
// nao cabem. Texto e botoes compartilham o recorte e a rolagem do corpo.
static float guiaTelefoneConteudo(const GuiaVista *v, float x, float y, float w, float a) {
  int e = v->ent, b[3], nb = guiaBotoes(e, b);
  float inicio = y, pw = fminf(w, 560), ph = pw * 9.0f / 16.0f;
  GfxRect fr = guiaInspR();
  if (e < 0 || e >= GUIA_NENT) return 0;
  const GuiaEnt *g = &GUIA_ENT[e];
  if (a > 0) ajustes_guia_imagem(e, x + (w - pw) * .5f, y, pw, ph);
  y += ph + 24;
  { TxtLinha c = ajTxtC(TXT_AJ_KBD, GUIA_CAPS[g->cap].titulo, w);
    txt_desenhar_alpha(c, x, y, .45f * a); y += 28; }
  y += txt_bloco_corta(TXT_AJ_INSP, g->titulo, 243, 242, 239, x, y, w, 38.4f, a, 0) + 12;
  y += txt_bloco_corta(TXT_AJ_SUB, guiaTxt(g), 243, 242, 239, x, y, w, 28.5f, .66f * a, 0) + 24;
  { TxtLinha c = ajTxtC(TXT_AJ_CAPS13, "Onde fica", w);
    txt_desenhar_alpha(c, x, y, .45f * a); y += 28; }
  y += txt_bloco_corta(TXT_AJ_18, g->onde, 243, 242, 239, x, y, w, 29, .8f * a, 0) + 24;
  for (int i = 0; i < nb; i++) {
    const char *rot = b[i] == GB_EXEMPLO ? "Ver exemplo" : b[i] == GB_AJUSTES ? "Abrir em Ajustes" : guiaRotTela(g->tela);
    const char *ic = b[i] == GB_EXEMPLO ? "aj_play" : b[i] == GB_AJUSTES ? "aj_settings-2" : guiaIcTela(g->tela);
    GfxRect r = {x, y, w, 56};
    if (a > 0) {
      int f = v->botao == i, tinta = f ? ajTinta() : 243;
      TxtLinha t = txt_linha_corta(TXT_AJ_SEG, i18n(rot), tinta, f ? tinta : 242, f ? tinta : 239, 255, w - 82);
      if (f) ajPilulaAcento(r, 28, a);
      else ajNeutro(r, 28, .08f, AJ_CHIP_SOL, a);
      if (f) ajIconeT(ic, x + 24, y + 17, 22, a);
      else ajIcone(ic, x + 24, y + 17, 22, a);
      txt_desenhar_alpha(t, x + 58, y + (56 - t.h) * .5f, a);
      ponteiro_alvo_faixa(r.x, r.y, r.w, r.h, fr.y, fr.y + fr.h, NULL, guiaPontBotao, i, e);
    }
    y += 68;
  }
  return y - inicio;
}
static void guiaDesenharTelefoneInspetor(const GuiaVista *v, float a) {
  GfxRect r = guiaInspR();
  float total = guiaTelefoneConteudo(v, r.x + 24, 0, r.w - 48, 0) + 48;
  ajIlha(r, 36, a, 0);
  float off = ajToqueRegistrar(AJT_EDITOR, r, &guiaInspRol, total - r.h, 0, v->ent + 400);
  gfx_recorte(r.x, r.y, r.w, r.h);
  guiaTelefoneConteudo(v, r.x + 24, r.y + 24 - off, r.w - 48, a);
  gfx_sem_recorte();
}
static void guiaTelefoneCabecalho(void) {
  GfxRect voltar = {24, 24, 64, 64}, fechar = {NV_VTELA_W - 88, 24, 64, 64};
  TxtLinha t = ajTxtC(TXT_AJ_INSP, "Guia de uso", NV_VTELA_W - 224);
  ajNeutro(voltar, 32, .08f, AJ_CHIP_SOL, 1);
  ajNeutro(fechar, 32, .08f, AJ_CHIP_SOL, 1);
  ajIcone("aj_arrow-left", voltar.x + 20, voltar.y + 20, 24, 1);
  ajIcone("aj_x", fechar.x + 20, fechar.y + 20, 24, 1);
  txt_desenhar(t, 112, 24 + (64 - t.h) * .5f);
  ponteiro_alvo(voltar.x, voltar.y, voltar.w, voltar.h, NULL, guiaPontVoltar, 1, 0);
  ponteiro_alvo(fechar.x, fechar.y, fechar.w, fechar.h, NULL, guiaPontVoltar, 0, 0);
}
#endif

// --- INSPETOR -----------------------------------------------------------------------
static void guiaDesenharInspetor(const GuiaVista *v, float a) {
  GfxRect ir = guiaInspR();
  if (!guiaTemInsp()) return;
  float x = ir.x + 32, w = ir.w - 64, y = ir.y + 32, ph = 0;
  AjDica d[5];
  int nd = 0, e = v->col == GC_INDICE ? -1 : v->ent;
  ajIlha(ir, 36, a, 0);
  if (e >= 0) {
    const GuiaEnt *g = &GUIA_ENT[e];
    int b[3], nb = guiaBotoes(e, b), i;
    ph = guiaVisual(g->visual, e, x, y, w);
    if (ph > 0) y += ph + 24; else y += 4;
    { float kw = ajCaps(TXT_AJ_KBD, GUIA_CAPS[g->cap].titulo, 1.96f, x, y, 0.45f * a);
      if (g->novo) guiaNovoMarca("Novo na 2.0", x + kw + 12, y + 8.5f, a); }
    y += 17 + 8;
    y += ajBloco(TXT_AJ_INSP, g->titulo, x, y, w, 38.4f, a, 2);
    y += 10;
    y += ajBloco(TXT_AJ_SUB, guiaTxt(g), x, y, w, 28.5f, 0.66f * a, 5);
    y += 18;
    gfx_cor((GfxRect){ x, y, w, 1 }, 0, 1, 1, 1, 0.07f * a);
    y += 17;
    ajCaps(TXT_AJ_CAPS13, "Onde fica", 1.82f, x, y, 0.45f * a);
    y += 16 + 8;
    y += guiaTrilha(g->onde, x, y, w, a) - 8;
    if (v->daNov && !strcmp(g->id, "c-guia")) {
      y += 18;
      y += ajEf("aj_corner-up-left", i18n("Voltar fecha o guia e devolve ao cartão das Novidades da 1.8.0. Depois, o guia fica em Ajustes › Sobre e ajuda › Guia de uso."), x, y, w, a);
    }
    if (nb) {
      float bx = x;
      y += 20;
      for (i = 0; i < nb; i++) {
        int f = v->col == GC_BOTOES && v->botao == i;
        float bw;
        if (b[i] == GB_EXEMPLO) bw = guiaBotao("Ver exemplo", "aj_play", bx, y, f, a);
        else if (b[i] == GB_AJUSTES) bw = guiaBotao("Abrir em Ajustes", "aj_settings-2", bx, y, f, a);
        else bw = guiaBotao(guiaRotTela(g->tela), guiaIcTela(g->tela), bx, y, f, a);
#ifdef NV_TOUCH_UI
        if (ajTelaCheia() && !guiaDesenhandoMini) ponteiro_alvo_faixa(bx, y, bw, 56, ir.y, ir.y + ir.h, NULL, guiaPontBotao, i, e);
#endif
        bx += bw + 12;
      }
    }
    if (v->col == GC_BOTOES && nb) {
      const char *r = b[v->botao] == GB_EXEMPLO ? "Ver exemplo" : b[v->botao] == GB_AJUSTES ? "Abrir em Ajustes" : guiaRotTela(g->tela);
      d[nd++] = (AjDica){ "OK", r };
      d[nd++] = (AjDica){ "←", "Recursos" };
    } else {
      d[nd++] = (AjDica){ "↑ ↓", "Recursos" };
      if (nb) d[nd++] = (AjDica){ "→", "Botões" };
      d[nd++] = (AjDica){ "←", "Capítulos" };
    }
    d[nd++] = (AjDica){ "Voltar", v->daNov ? "Novidades da 1.8.0" : "Ajustes" };
  } else if (v->idx >= GI_CAP0) {
    int c = v->idx - GI_CAP0;
    char k[64], q[16], q2[16];
    ph = guiaVisual(guiaVisualCap(c), -1, x, y, w);
    if (ph > 0) y += ph + 24; else y += 4;
    snprintf(k, sizeof k, i18n("Capítulo %d de %d"), c + 1, GUIA_NCAPS);
    ajCaps(TXT_AJ_KBD, k, 1.96f, x, y, 0.45f * a);
    y += 17 + 8;
    y += ajBloco(TXT_AJ_INSP, GUIA_CAPS[c].titulo, x, y, w, 38.4f, a, 2);
    y += 10;
    y += ajBloco(TXT_AJ_SUB, GUIA_CAP_TXT[c], x, y, w, 28.5f, 0.66f * a, 5);
    y += 16;
    if (c == 1) {
      AjMini m = ajMiniPadrao(), o[3];
      static const char *const N[3] = { "Moderna", "Padrão", "Dinâmica" };
      int i;
      m.art = 13; m.relogio = 2; m.rail = 1; m.semHero = 0;
      for (i = 0; i < 3; i++) { o[i] = m; o[i].lay = i; }
      y += ajOpcoesMini(o, N, 3, 0, x, y, w) + 18;
    }
    snprintf(q, sizeof q, "%d", guiaNoCap(c));
    snprintf(q2, sizeof q2, "%d", guiaNovosNoCap(c));
    y += ajMeta(i18n("Recursos"), q, x, y, w, a);
    y += ajMeta(i18n("Novo na 2.0"), q2, x, y, w, a);
    d[nd++] = (AjDica){ "→", "Recursos" };
    d[nd++] = (AjDica){ "↑ ↓", "Capítulos" };
    d[nd++] = (AjDica){ "Voltar", v->daNov ? "Novidades da 1.8.0" : "Ajustes" };
  } else {
    // "Buscar no guia" e "Novo na 2.0" com o foco no indice.
    int novo = v->idx == GI_NOVO;
    char t[160];
    ph = guiaVisual(novo ? GV_VIDRO : GV_GUIA, -1, x, y, w);
    if (ph > 0) y += ph + 24; else y += 4;
    ajCaps(TXT_AJ_KBD, "Guia de uso", 1.96f, x, y, 0.45f * a);
    y += 17 + 8;
    y += ajBloco(TXT_AJ_INSP, novo ? "Novo na 2.0" : "Buscar no guia", x, y, w, 38.4f, a, 2);
    y += 10;
    if (novo) snprintf(t, sizeof t, i18n("%d recursos novos ou que mudaram nesta versão"), guiaNovos());
    else snprintf(t, sizeof t, "%s", i18n("Procure um recurso pelo nome ou pelo que ele faz. Os ajustes que casam com a busca vêm junto."));
    y += ajBloco(TXT_AJ_SUB, t, x, y, w, 28.5f, 0.66f * a, 5);
    d[nd++] = (AjDica){ "OK", novo ? "Abrir" : "Buscar" };
    d[nd++] = (AjDica){ "↑ ↓", "Capítulos" };
    d[nd++] = (AjDica){ "Voltar", v->daNov ? "Novidades da 1.8.0" : "Ajustes" };
  }
  guiaDicas(d, nd, ir, a);
}

// "Ver exemplo": a cena das Novidades viva, numa ilha por cima do guia.
static void guiaDesenharExemplo(void) {
  int c = guiaExemplo;
  float esc, cw, ch;
  GfxRect m, pv;
  const char *tit = gv.ent >= 0 ? GUIA_ENT[gv.ent].titulo : "";
  if (c == GX_N180_FONTES || c == GX_N180_ILHA) { esc = 0.72f; cw = N180_CENA_W * esc; ch = N180_CENA_H * esc; }
  else { esc = 1.0f; cw = N170_PREVIA_W; ch = N170_PREVIA_H; }
#ifdef NV_TOUCH_UI
  if (guiaTelefone()) {
    float k = fminf(1.0f, fminf((NV_VTELA_W - 128) / cw, (NV_VTELA_H - 198) / ch));
    esc *= k; cw *= k; ch *= k;
    ponteiro_camada(); ajToqueCamada();
    ponteiro_alvo(0, 0, NV_VTELA_W, NV_VTELA_H, NULL, guiaPontVoltar, 2, 0);
  }
#endif
  ajVeuModal();
  m = (GfxRect){ (NV_VTELA_W - (cw + 80)) * 0.5f, (NV_VTELA_H - (ch + 150)) * 0.5f, cw + 80, ch + 150 };
  if (m.y < 24) { float k = (NV_VTELA_H - 48 - 150) / ch; esc *= k; cw *= k; ch *= k;
                  m = (GfxRect){ (NV_VTELA_W - (cw + 80)) * 0.5f, 24, cw + 80, ch + 150 }; }
  ajIlha(m, 36, 1, 1);
#ifdef NV_TOUCH_UI
  if (guiaTelefone()) {
    GfxRect fechar = {m.x + m.w - 80, m.y + 24, 56, 56};
    ponteiro_alvo(m.x, m.y, m.w, m.h, NULL, NULL, 0, 0);
    ajNeutro(fechar, 28, .08f, AJ_CHIP_SOL, 1);
    ajIcone("aj_x", fechar.x + 17, fechar.y + 17, 22, 1);
    ponteiro_alvo(fechar.x, fechar.y, fechar.w, fechar.h, NULL, guiaPontVoltar, 2, 0);
  }
#endif
  ajKicker("Ver exemplo", m.x + 40, m.y + 32, 1);
  { TxtLinha t = ajTxtC(TXT_AJ_NOME, tit, m.w - (guiaTelefone() ? 144 : 80)); txt_desenhar(t, m.x + 40, m.y + 56); }
  pv = (GfxRect){ m.x + 40, m.y + 100, cw, ch };
  if (gfx_mini_alvo(&guiaMiniEx, (int)cw, (int)ch)) {
    gfx_mini_comecar(&guiaMiniEx, 0, 0, esc);
    gfx_cor((GfxRect){ 0, 0, cw / esc, ch / esc }, 0, 0.051f, 0.055f, 0.067f, 1);
    if (c == GX_N180_FONTES) novidades180_cena_desenhar(N180_CENA_FONTES, guiaExT);
    else if (c == GX_N180_ILHA) novidades180_cena_desenhar(N180_CENA_ILHA, guiaExT);
    else novidades170_cena_desenhar(c == GX_N170_SPOT ? 1 : 0, 0, 0, guiaExT);
    gfx_mini_terminar();
    gfx_mini_desenhar(&guiaMiniEx, pv, 28, 1);
  }
  if (!guiaTelefone()) { AjDica d[1] = { { "Voltar", "Fechar" } };
    float w = ajDicasLargura(d, 1);
    ajDicasLinha(d, 1, m.x + m.w - 40 - w, m.y + 38, 1); }
}

static void guiaDesenharTela(const GuiaVista *v, float a) {
#ifdef NV_TOUCH_UI
  if (guiaTelefone()) {
    if (v->col == GC_INDICE) guiaDesenharIndice(v, a);
    else if (v->col == GC_BOTOES) guiaDesenharTelefoneInspetor(v, a);
    else guiaDesenharLista(v, a);
    guiaTelefoneCabecalho();
    return;
  }
#endif
  guiaDesenharIndice(v, a);
  guiaDesenharLista(v, a);
  guiaDesenharInspetor(v, a);
}
static void guiaDesenhar(void) {
  ajFundo();
  // "Buscar no guia" por cima: so a arte fica atras do Spotlight (o mockup).
  if (spot_visivel()) return;
  if (guiaExemplo) gfx_opacidade_grupo = 0.16f;
  guiaDesenharTela(&gv, 1);
  gfx_opacidade_grupo = 1.0f;
  if (guiaExemplo) guiaDesenharExemplo();
}
static void guiaAtualizar(float dt) {
  if (!guiaAberto) return;
#ifdef NV_TOUCH_UI
  if (!ajTelaCheia() || !ajToqueLivre[AJT_LISTA]) {
#endif
  guiaRolar();
  guiaRol = ajustes_animacoes_reduzidas() ? guiaRolAlvo : anim_mola2(&guiaRolV, guiaRol, guiaRolAlvo, dt, 14.0f);
#ifdef NV_TOUCH_UI
  }
#endif
  if (guiaExemplo) {
    float dur = guiaExemplo == GX_N170_SPOT ? 9.0f : guiaExemplo == GX_N170_ILHA ? 13.2f : 4.6f;
    guiaExT += dt;
    if (guiaExT > dur) guiaExT = 0;
  }
}

// --- O MESMO GUIA NO INSPETOR DE AJUSTES (a linha "Guia de uso") -------------------------
static void guiaInspetorAjustes(GfxRect ir) {
  float x = ir.x + 32, w = ir.w - 64, y = ir.y + 32;
  int c;
  char q[64], q2[64];
  AjDica d[3] = { { "OK", "Abrir" }, { "↑ ↓", "Navegar" }, { "←", "Categorias" } };
  guiaMiniCena(&guiaMiniVis, GV_GUIA, -1, (GfxRect){ x, y, w, w * 9.0f / 16.0f }, 0, 0, w / 1920.0f, 22, 0);
  y += w * 9.0f / 16.0f;
  x += 6.0f; w -= 12.0f;
  y += 26.0f;
  ajIcone("aj_book-open-text", x, y - 1, 20, 0.45f);
  ajCaps(TXT_MINI, "Abre uma tela", 2.1f, x + 30, y, 0.45f);
  y += 28.0f;
  y += ajBloco(TXT_AJ_INSP, "Guia de uso", x, y, w, 38.4f, 1, 2);
  y += 12.0f;
  y += ajBloco(TXT_AJ_SUB, ajudaOpcao(AJ_GUIA), x, y, w, 28.5f, 0.62f, 7);
  y += 18.0f;
  snprintf(q, sizeof q, i18n("%d em %d capítulos"), GUIA_NENT, GUIA_NCAPS);
  snprintf(q2, sizeof q2, i18n("%d recursos"), guiaNovos());
  y += ajMeta(i18n("Recursos"), q, x, y, w, 1);
  y += ajMeta(i18n("Novo na 2.0"), q2, x, y, w, 1);
  y += 22.0f;
  ajKicker("Capítulos", x, y + 1, 1);
  y += 29.0f;
  for (c = 0; c < GUIA_NCAPS; c++) {
    float cx = x + (c % 2) * (w * 0.5f + 9), cy = y + (c / 2) * 42.0f;
    TxtLinha t = ajTxtC(TXT_ILHA_GENERO, GUIA_CAPS[c].titulo, w * 0.5f - 40);
    ajIcone(GUIA_CAPS[c].icone, cx, cy + 8.5f, 19, 0.45f);
    txt_desenhar_alpha(t, cx + 29, cy + (36 - t.h) * 0.5f, 0.7f);
  }
  ajDicas(d, 3, ir.x + 32, ir.y + ir.h - 32.0f, ir.w - 64, 1);
}

// --- TECLAS ---------------------------------------------------------------------------
static void guiaAgir(int b) {
  int e = gv.ent, bs[3], nb = guiaBotoes(e, bs);
  if (e < 0 || b < 0 || b >= nb) return;
  if (bs[b] == GB_EXEMPLO) { guiaExemplo = GUIA_ENT[e].exemplo; guiaExT = 0; return; }
  if (bs[b] == GB_AJUSTES) { int op = GUIA_ENT[e].alvo; guiaFechar(); focarOpcao(op); return; }
  pediuTela = GUIA_ENT[e].tela;
  guiaFechar();
}
static int guiaVoltar(SDL_Keycode k, const SDL_Event *e) {
  return k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE || e->key.keysym.scancode == NV_SCANCODE_BACK;
}
static void guiaMoverEnt(int d) {
  int l[GUIA_NENT + GUIA_NCAPS], n = guiaLista(gv.idx, l), i, at = -1;
  for (i = 0; i < n; i++) if (l[i] == gv.ent) at = i;
  for (i = at + d; i >= 0 && i < n; i += d) if (l[i] >= 0) { gv.ent = l[i]; return; }
  if (d < 0 && gv.daNov) gv.col = GC_CHIP;   // acima da primeira: o chip "Voltar às novidades"
}
#ifdef NV_TOUCH_UI
static void guiaPontIndice(int i, int b) {
  (void)b;
  if (!guiaAberto || guiaExemplo || i < 0 || i >= GI_CAP0 + GUIA_NCAPS) return;
  if (gv.idx != i) { gv.ent = -1; guiaRol = guiaRolAlvo = guiaRolV = 0; ajToqueLivre[AJT_LISTA] = 0; }
  gv.idx = i; gv.col = GC_INDICE;
}
static void guiaPontIndiceAbrir(int i, int b) {
  guiaPontIndice(i, b);
  if (!guiaAberto || guiaExemplo || gv.idx != i) return;
  if (i == GI_BUSCA) { spot_abrir_guia(); return; }
  gv.col = GC_LISTA; gv.ent = guiaPrimeira(i);
}
static void guiaPontEnt(int e, int b) {
  (void)b;
  if (!guiaAberto || guiaExemplo || e < 0 || e >= GUIA_NENT) return;
  if (gv.idx == GI_NOVO ? !GUIA_ENT[e].novo : GUIA_ENT[e].cap != gv.idx - GI_CAP0) return;
  gv.ent = e; gv.col = GC_LISTA;
}
static void guiaPontEntAbrir(int e, int b) {
  guiaPontEnt(e, b);
  if (!guiaAberto || guiaExemplo || gv.ent != e) return;
  gv.col = GC_BOTOES; gv.botao = 0;
}
static void guiaPontBotao(int b, int e) {
  if (!guiaAberto || guiaExemplo || gv.ent != e) return;
  gv.col = GC_BOTOES; gv.botao = b; guiaAgir(b);
}
static void guiaPontVoltar(int passo, int b) {
  (void)b;
  if (!guiaAberto) return;
  if (guiaExemplo) { guiaExemplo = 0; return; }
  if (passo && guiaTelefone()) {
    if (gv.col == GC_BOTOES) { gv.col = GC_LISTA; return; }
    if (gv.col != GC_INDICE) { gv.col = GC_INDICE; return; }
  }
  guiaSair();
}
#endif
static void guiaEvento(const SDL_Event *e) {
  SDL_Keycode k;
  int ok;
  if (!e || e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  ok = k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE;
  if (guiaExemplo) { if (guiaVoltar(k, e) || ok) guiaExemplo = 0; return; }
#ifdef NV_TOUCH_UI
  if (guiaVoltar(k, e) && guiaTelefone()) { guiaPontVoltar(1, 0); return; }
  if (k == SDLK_UP || k == SDLK_DOWN) {
    if (gv.col == GC_INDICE) ajToqueLivre[AJT_INDICE] = 0;
    else ajToqueLivre[AJT_LISTA] = 0;
  }
#endif
  if (guiaVoltar(k, e)) { guiaSair(); return; }
  switch (gv.col) {
    case GC_INDICE:
      if (k == SDLK_UP && gv.idx > 0) gv.idx--;
      else if (k == SDLK_DOWN && gv.idx < GI_CAP0 + GUIA_NCAPS - 1) gv.idx++;
      else if (ok && gv.idx == GI_BUSCA) { spot_abrir_guia(); return; }
      else if ((k == SDLK_RIGHT || ok) && gv.idx != GI_BUSCA) {
        gv.col = GC_LISTA; gv.ent = guiaPrimeira(gv.idx); guiaRol = guiaRolV = 0;
      }
      break;
    case GC_CHIP:
      if (ok) { guiaSair(); return; }
      if (k == SDLK_DOWN) gv.col = GC_LISTA;
      else if (k == SDLK_LEFT) gv.col = GC_INDICE;
      break;
    case GC_LISTA: {
      int bs[3], nb = guiaBotoes(gv.ent, bs);
      if (k == SDLK_UP) guiaMoverEnt(-1);
      else if (k == SDLK_DOWN) guiaMoverEnt(+1);
      else if (k == SDLK_LEFT) gv.col = GC_INDICE;
      else if ((k == SDLK_RIGHT || ok) && (nb || guiaTelefone())) { gv.col = GC_BOTOES; gv.botao = 0; }
    } break;
    case GC_BOTOES: {
      int bs[3], nb = guiaBotoes(gv.ent, bs);
      if (ok) { guiaAgir(gv.botao); return; }
      if (k == SDLK_LEFT) { if (gv.botao > 0) gv.botao--; else gv.col = GC_LISTA; }
      else if (k == SDLK_RIGHT && gv.botao < nb - 1) gv.botao++;
      else if (k == SDLK_UP || k == SDLK_DOWN) { gv.col = GC_LISTA; guiaMoverEnt(k == SDLK_UP ? -1 : 1); }
    } break;
    default: break;
  }
}

// Busca do Spotlight no modo guia: as entradas que casam com a consulta,
// melhor primeiro (titulo antes de texto). Devolve quantas.
int ajustes_guia_buscar(const char *consulta, int *out, int max) {
  char q[160], t[600];
  int n = 0, i, passo;
  if (!consulta || !consulta[0]) return 0;
  busca_normalizar(consulta, q, sizeof q);
  if (!q[0]) return 0;
  for (passo = 0; passo < 2 && n < max; passo++)
    for (i = 0; i < GUIA_NENT && n < max; i++) {
      int j, ja = 0;
      busca_normalizar(i18n(passo == 0 ? GUIA_ENT[i].titulo : GUIA_ENT[i].txt), t, sizeof t);
      if (!strstr(t, q)) continue;
      for (j = 0; j < n; j++) if (out[j] == i) ja = 1;
      if (!ja) out[n++] = i;
    }
  return n;
}
const char *ajustes_guia_titulo(int e) { return e >= 0 && e < GUIA_NENT ? GUIA_ENT[e].titulo : ""; }
const char *ajustes_guia_texto(int e) { return e >= 0 && e < GUIA_NENT ? guiaTxt(&GUIA_ENT[e]) : ""; }
const char *ajustes_guia_icone(int e) { return e >= 0 && e < GUIA_NENT ? GUIA_ENT[e].icone : ""; }
const char *ajustes_guia_capitulo(int e) { return e >= 0 && e < GUIA_NENT ? GUIA_CAPS[GUIA_ENT[e].cap].titulo : ""; }
const char *ajustes_guia_onde(int e) { return e >= 0 && e < GUIA_NENT ? GUIA_ENT[e].onde : ""; }
int ajustes_guia_novo(int e) { return e >= 0 && e < GUIA_NENT && GUIA_ENT[e].novo; }
// A imagem de uma entrada no tamanho pedido (o melhor resultado do Spotlight).
void ajustes_guia_imagem(int e, float x, float y, float w, float h) {
  int tipo = e >= 0 && e < GUIA_NENT ? GUIA_ENT[e].visual : GV_NADA;
  if (tipo == GV_ASS || tipo == GV_VIDRO || tipo == GV_GUIA)
    guiaMiniCena(&guiaMiniEx, tipo, e, (GfxRect){ x, y, w, h }, 0, 0, w / 1920.0f, 14, 0);
  else if (tipo == GV_FONTES || tipo == GV_ILHA || tipo == GV_FILEIRAS)
    guiaMiniCena(&guiaMiniEx, tipo, e, (GfxRect){ x, y, w, h }, tipo == GV_FONTES ? 1020 : tipo == GV_FILEIRAS ? 60 : 0,
                 tipo == GV_FONTES ? 40 : tipo == GV_FILEIRAS ? 120 : 10,
                 w / (tipo == GV_FONTES ? 860.0f : tipo == GV_FILEIRAS ? 1160.0f : 1140.0f), 14, 0);
  else if (e >= 0 && e < GUIA_NENT && GUIA_ENT[e].alvo >= 0) ajustes_previa_busca(GUIA_ENT[e].alvo, x, y, w, h, 1);
  else ajIcone(ajustes_guia_icone(e), x + w * 0.5f - 24, y + h * 0.5f - 24, 48, 0.6f);
}
// Abre o guia ja na entrada `e` (o OK num resultado do Spotlight).
void ajustes_guia_ir(int e) {
  if (e < 0 || e >= GUIA_NENT) return;
  if (!guiaAberto) guiaAbrir(0);
  gv.idx = GI_CAP0 + GUIA_ENT[e].cap;
  gv.col = GC_LISTA;
  gv.ent = e;
  guiaRol = guiaRolV = 0;
}

