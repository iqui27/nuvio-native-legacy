// Ver desempenho.h.
#include "desempenho.h"
#include "ajustes.h"
#include "gfx.h"
#include "text.h"
#include "idioma.h"
#include "idiomacod.h"
#include "tex_cache.h"
#include "cacheboost.h"
#include "memlog.h"
#include "video.h"
#include <stdio.h>
#include <string.h>

#define DS_N 36   // 36 amostras de 3 s: os ultimos 1 min 48 s

static float piorSerie[DS_N];
static int   nSerie;
static float uFps, uPior, uTelaMb, uRss;
static long uLivre = -1;
static int   uJanks, uTela, uFila, uDesp, uDespTela, temAmostra, trailerTocando;
#ifdef DESEMPENHO_TESTE
static int formaFixa = -1;
void desempenho_teste_forma(int forma) { formaFixa = forma; }
#endif

void desempenho_amostra(float fps, float piorMs, int janks, int texTela, float texTelaMb,
                        int filaTex, int despejos, int despejosTela, float rssMb) {
  if (nSerie == DS_N) { memmove(piorSerie, piorSerie + 1, sizeof piorSerie - sizeof *piorSerie); nSerie--; }
  piorSerie[nSerie++] = piorMs;
  uFps = fps; uPior = piorMs; uJanks = janks; uTela = texTela; uTelaMb = texTelaMb;
  uFila = filaTex; uDesp = despejos; uDespTela = despejosTela; uRss = rssMb;
  uLivre = video_ativo() ? memlog_livre_mb() : -1;
  temAmostra = 1;
}

void desempenho_trailer(int tocando) { trailerTocando = tocando; }

int desempenho_forma(void) {
  int f = ajustes_medidor_desempenho();
#ifdef DESEMPENHO_TESTE
  if (formaFixa >= 0) f = formaFixa;
#endif
  if (!temAmostra || trailerTocando || f < DS_MINIMO || f > DS_GRANDE) return DS_DESLIGADO;
  return f;
}

#define DS_TX 243, 242, 239
#define DS_AMBAR 0.910f, 0.722f, 0.290f
#define DS_VERDE 0.298f, 0.765f, 0.541f
#define DS_VAO   14.0f     // o CT_VAO da ilha: hora | fio | conteudo
#define DS_PONTO 9.0f
static int lento(void) { return uFps < 45.0f; }
static void num(char *d, size_t t, double v, int casas) {
  char *p;
  snprintf(d, t, "%.*f", casas, v);
  if ((p = strchr(d, '.')) != NULL) *p = idioma_ponto_decimal(ajustes_idioma()) ? '.' : ',';
}
static TxtLinha dsT(TxtEstilo e, const char *s) { return txt_linha(e, s, DS_TX, 255); }

// --- a linha da hora ---------------------------------------------------------------
// Numeros em mono (tabulares): o trecho nao danca de largura a cada amostra.
static void memoria(char *s, size_t n) {
  if (uLivre >= 0 && video_ativo())
    snprintf(s, n, i18n("%.0f MB · %ld livre"), uRss, uLivre);
  else snprintf(s, n, "%.0f MB", uRss);
}
typedef struct { TxtLinha fps, pior, ram; } Linha;
static float montar(int forma, Linha *L) {
  char a[24], b[24], c[96];
  float w;
  num(a, sizeof a, uFps, 0);
  snprintf(b, sizeof b, "%s UI fps", a);
  L->fps = lento() ? txt_linha(TXT_MONO18B, b, 232, 184, 74, 255) : dsT(TXT_MONO18B, b);
  w = DS_VAO + 1.5f + DS_VAO + DS_PONTO + 10.0f + (float)L->fps.w;
  L->pior.w = L->ram.w = 0;
  if (forma == DS_MINIMO) {
    num(a, sizeof a, uPior, 0);
    snprintf(b, sizeof b, "%s ms", a);
    memoria(c, sizeof c);
    L->pior = dsT(TXT_MONO16, b);
    L->ram = dsT(TXT_MONO16, c);
    w += 16.0f + (float)L->pior.w + 14.0f + (float)L->ram.w;
  }
  return w;
}

float desempenho_linha_w(int forma) {
  Linha L;
  if (forma <= DS_DESLIGADO || !temAmostra) return 0.0f;
  return montar(forma, &L);
}

void desempenho_linha(int forma, float x, float yc, float a) {
  Linha L;
  if (forma <= DS_DESLIGADO || !temAmostra || a < 0.01f) return;
  montar(forma, &L);
  x += DS_VAO;
  // O fio entre a hora e o medidor: o mesmo do cartao ao lado do relogio.
  gfx_cor((GfxRect){ x, yc - 13.0f, 1.5f, 26.0f }, 0.0f, 1.0f, 1.0f, 1.0f, 0.22f * a);
  x += 1.5f + DS_VAO;
  if (lento()) gfx_cor((GfxRect){ x, yc - DS_PONTO * 0.5f, DS_PONTO, DS_PONTO }, 0.5f, DS_AMBAR, a);
  else gfx_cor((GfxRect){ x, yc - DS_PONTO * 0.5f, DS_PONTO, DS_PONTO }, 0.5f, DS_VERDE, a);
  x += DS_PONTO + 10.0f;
  txt_desenhar_alpha(L.fps, x, yc - (float)L.fps.h * 0.5f, a);
  x += (float)L.fps.w;
  if (forma == DS_MINIMO) {
    x += 16.0f;
    txt_desenhar_alpha(L.pior, x, yc - (float)L.pior.h * 0.5f, a * 0.7f);
    x += (float)L.pior.w + 14.0f;
    txt_desenhar_alpha(L.ram, x, yc - (float)L.ram.h * 0.5f, a * 0.7f);
  }
}

// --- Menor: a segunda linha --------------------------------------------------------
#define MN_PAD 24.0f
#define MN_H   38.0f
static TxtLinha linhaMenor(void) {
  char p[16], s[256], q[48], t[48], m[96];
  num(p, sizeof p, uPior, 0);
  snprintf(q, sizeof q, i18n("pior %s ms"), p);
  snprintf(t, sizeof t, i18n("%d texturas"), uTela);
  memoria(m, sizeof m);
  snprintf(s, sizeof s, "%s · %d %s · %s · %s", q, uJanks, i18n("janks"), m, t);
  return dsT(TXT_MONO16, s);
}

// --- Grande: o painel ---------------------------------------------------------------
#define GR_W   520.0f
#define GR_PAD 28.0f
#define GR_STAT (1 + 11 + 20.6f + 11)
// F07: one more stat line ("Cache de seek") where the player has the cache.
#define GR_H   (18 + 22 + 14 + 96 + 30 + (4 + (cacheboost_suportado() ? 1 : 0)) * GR_STAT + 12 + 29 + 8 + 24)
static float stat(const char *k, const char *v, float x, float y, float w, float a) {
  TxtLinha lk = dsT(TXT_AJ_ESTADO, k), lv = dsT(TXT_AJ_ESTADO, v);
  gfx_cor((GfxRect){ x, y, w, 1 }, 0, 1, 1, 1, 0.07f * a);
  txt_desenhar_alpha(lk, x, y + 12, 0.45f * a);
  txt_desenhar_alpha(lv, x + w - lv.w, y + 12, 0.88f * a);
  return GR_STAT;
}

static void painel(GfxRect r, float a) {
  float x = r.x + GR_PAD, cw = r.w - 2 * GR_PAD, y = r.y;
  char v[64], p[32];
  int i, itens, pend, quentes;
  long bytes, bytesQ, teto = tex_orcamento_bytes();
  tex_estatisticas(&itens, &pend, &bytes, &quentes, &bytesQ);
  // Fio sob a linha da hora: o cabecalho da ilha crescida (plrilha faz igual).
  gfx_cor((GfxRect){ r.x, r.y, r.w, 1 }, 0, 1, 1, 1, 0.07f * a);
  y += 18;
  // "pior 21 ms · 0 janks" a esquerda, numeros em branco forte; "a cada 3 s"
  // na ponta. A linha de texto perde os espacos das pontas quando tem acento
  // ou "·": os espacos entram a mao.
  { char nb[16], b[16];
    TxtLinha l1, l2, l3, l4, l5, d;
    float xd = x, base = y + 18, esp = (float)(txt_largura(TXT_AJ_ESTADO, "a a") - txt_largura(TXT_AJ_ESTADO, "aa"));
    num(nb, sizeof nb, uPior, 0); snprintf(p, sizeof p, "%s ms", nb);
    snprintf(b, sizeof b, "%d", uJanks);
    l1 = dsT(TXT_AJ_ESTADO, i18n("pior")); l2 = dsT(TXT_LOG_18B, p); l3 = dsT(TXT_AJ_ESTADO, "·");
    l4 = dsT(TXT_LOG_18B, b); l5 = dsT(TXT_AJ_ESTADO, i18n("janks"));
    d = dsT(TXT_G18M, i18n("a cada 3 s"));
    txt_desenhar_alpha(l1, xd, base - l1.h * 0.78f, 0.6f * a); xd += l1.w + esp;
    txt_desenhar_alpha(l2, xd, base - l2.h * 0.78f, a);        xd += l2.w + esp;
    txt_desenhar_alpha(l3, xd, base - l3.h * 0.78f, 0.6f * a); xd += l3.w + esp;
    txt_desenhar_alpha(l4, xd, base - l4.h * 0.78f, a);        xd += l4.w + esp;
    txt_desenhar_alpha(l5, xd, base - l5.h * 0.78f, 0.6f * a);
    txt_desenhar_alpha(d, x + cw - d.w, base - d.h * 0.78f, 0.5f * a);
    y += 22 + 14; }
  // grafico do pior quadro: uma barra por amostra, ambar acima de 33 ms
  { float gx = x + 10, gy = y + 10, gw = cw - 20, gh = 74, pw = gw / DS_N, ly = gy + gh * (1 - 33.0f / 70.0f), dx;
    gfx_cor((GfxRect){ x, y, cw, 90 }, 12.0f / 90, 0, 0, 0, 0.18f * a);
    for (i = 0; i < nSerie; i++) {
      float vv = piorSerie[i] / 70.0f, hh, bx = gx + gw - (nSerie - i) * pw;
      if (vv > 1) vv = 1;
      hh = gh * vv; if (hh < 3) hh = 3;
      if (piorSerie[i] > 33) gfx_cor((GfxRect){ bx, gy + gh - hh, pw - 2, hh }, 0, DS_AMBAR, 0.6f * a);
      else gfx_cor((GfxRect){ bx, gy + gh - hh, pw - 2, hh }, 0, 0.953f, 0.949f, 0.937f, 0.18f * a);
    }
    for (dx = 0; dx < gw; dx += 10) gfx_cor((GfxRect){ gx + dx, ly, 4, 1 }, 0, 1, 1, 1, 0.22f * a);
    { TxtLinha l = dsT(TXT_AJ_MINI12, "33 ms"); txt_desenhar_alpha(l, gx + 4, ly - 6 - l.h, 0.45f * a); }
    y += 90 + 6; }
  { char ha[48];
    TxtLinha la, lb;
    int seg = nSerie * 3;
    if (seg < 60) snprintf(ha, sizeof ha, i18n("pior quadro · há %d s"), seg);
    else snprintf(ha, sizeof ha, i18n("pior quadro · há %d min"), (seg + 30) / 60);
    la = dsT(TXT_AJ_MINI14, ha); lb = dsT(TXT_AJ_MINI14, i18n("agora"));
    txt_desenhar_alpha(la, x, y, 0.4f * a); txt_desenhar_alpha(lb, x + cw - lb.w, y, 0.4f * a);
    y += 16 + 14; }
  memoria(v, sizeof v);
  y += stat(i18n("Memória do app"), v, x, y, cw, a);
  snprintf(v, sizeof v, "%d · %.0f MB", uTela, uTelaMb);
  y += stat(i18n("Texturas na tela"), v, x, y, cw, a);
  snprintf(v, sizeof v, "%d", uFila);
  y += stat(i18n("Fila de texturas"), v, x, y, cw, a);
  snprintf(v, sizeof v, i18n("%d · %d da tela"), uDesp, uDespTela);
  y += stat(i18n("Despejadas"), v, x, y, cw, a);
  // F07: what the player's seek cache holds now / its effective limit, or why
  // it is off. Reported by the backend every 2 s; free to read here.
  if (cacheboost_suportado()) {
    char c[48];
    cacheboost_cache_texto(c, sizeof c);
    y += stat(i18n("Cache de seek"), i18n(c), x, y, cw, a);
  }
  gfx_cor((GfxRect){ x, y, cw, 1 }, 0, 1, 1, 1, 0.07f * a);
  y += 12;
  { TxtLinha k = dsT(TXT_ILHA_GENERO, i18n("Cache de imagens")), l;
    float fr = teto > 0 ? (float)bytes / (float)teto : 0;
    snprintf(v, sizeof v, "%ld / %ld MB", bytes >> 20, teto >> 20);
    l = dsT(TXT_ILHA_GENERO, v);
    txt_desenhar_alpha(k, x, y, 0.45f * a);
    txt_desenhar_alpha(l, x + cw - l.w, y, 0.88f * a);
    y += 20 + 9;
    if (fr > 1) fr = 1;
    gfx_cor((GfxRect){ x, y, cw, 8 }, 0.5f, 1, 1, 1, 0.07f * a);
    if (fr > 0.005f) gfx_cor((GfxRect){ x, y, cw * fr, 8 }, 0.5f, DS_VERDE, 0.8f * a); }
}

void desempenho_corpo_tam(int forma, float *w, float *h) {
  float cw = 0.0f, ch = 0.0f;
  if (temAmostra && forma == DS_MENOR) { TxtLinha l = linhaMenor(); cw = (float)l.w + 2 * MN_PAD; ch = MN_H; }
  else if (temAmostra && forma == DS_GRANDE) { cw = GR_W; ch = GR_H; }
  if (w) *w = cw;
  if (h) *h = ch;
}

void desempenho_corpo(int forma, GfxRect r, float a) {
  if (!temAmostra || a < 0.01f) return;
  if (forma == DS_MENOR) {
    TxtLinha l = linhaMenor();
    // A segunda linha fica sob a primeira, centrada na ilha, mais apagada.
    txt_desenhar_alpha(l, r.x + (r.w - (float)l.w) * 0.5f, r.y + 12.0f - (float)l.h * 0.5f, 0.7f * a);
  } else if (forma == DS_GRANDE) painel(r, a);
}

#ifdef DESEMPENHO_TESTE
void desempenho_teste_serie(const float *pior, int n) { int i; nSerie = 0; for (i = 0; i < n && i < DS_N; i++) piorSerie[nSerie++] = pior[i]; }
#endif
