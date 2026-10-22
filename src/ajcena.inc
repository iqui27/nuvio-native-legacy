// AJCENA: o kit das previas GERADAS dos Ajustes (2.0.2).
//
// O dono (06/10): "Essas fotos reais dos settings atrapalham mais do que
// ajudam. Vamos trocar tudo por imagens geradas mesmo, mais visual do que vai
// acontecer do que usar a foto. Da pra ser mais animado e mais facil de
// entender." Toda previa dos Ajustes e um DESENHO ESQUEMATICO feito com as
// primitivas do gfx (retangulo arredondado, rampa de luz, anel, icone): cartaz
// = bloco de cor, texto = barra cinza, fileira = blocos em linha, video =
// faixa de luz andando. O acento do usuario marca o que muda.
//
// Vocabulario unico: TODAS as previas usam estas pecas, no mesmo quadro de
// 640 x 360 unidades (escalado para a largura pedida), com o mesmo fundo,
// raio, matizes e foco. Sem textura (so os icones do app), sem fosco, sem
// rede: um punhado de quads por quadro (Mali-G52).
//
// Movimento: laços lentos e suaves (2-4 s) que mostram a mudanca. Com
// animacoes reduzidas o tempo para e cada peca fica no estado FINAL.
// Incluido por ajustes_ux_desenho.inc (funcoes static, como o resto da UI
// dos Ajustes: nenhum script de teste ou de build precisa listar mais um .c).
#define AJC_W 640.0f
#define AJC_H 360.0f
#define AJC_NMATIZ 8

static struct {
  float x, y, w, h, s, a, t;
  float ar, ag, ab, tinta;
  int red;
} C;

// Os matizes dos cartazes abstratos: escuros e pouco saturados, para o acento
// do usuario continuar sendo o que chama o olho.
static const float MATIZ[AJC_NMATIZ][3] = {
  { 0.20f, 0.42f, 0.50f },   // petroleo
  { 0.62f, 0.32f, 0.28f },   // terracota
  { 0.38f, 0.30f, 0.58f },   // violeta
  { 0.64f, 0.50f, 0.22f },   // ambar
  { 0.22f, 0.34f, 0.60f },   // azul
  { 0.58f, 0.28f, 0.44f },   // rosa
  { 0.26f, 0.48f, 0.34f },   // verde
  { 0.34f, 0.38f, 0.46f },   // ardosia
};

#ifdef AJUSTES_TESTE
static float ajcTesteT = -1.0f;
#endif
static GfxRect ajc_r(float x, float y, float w, float h) {
  return (GfxRect){ C.x + x * C.s, C.y + y * C.s, w * C.s, h * C.s };
}
static float ajc_px(float v) { return v * C.s; }
static float ajc_x(float v) { return C.x + v * C.s; }
static float ajc_y(float v) { return C.y + v * C.s; }
static float ajc_t(void) { return C.t; }
static int ajc_reduzido(void) { return C.red; }
static void ajc_acento(float *r, float *g, float *b) { *r = C.ar; *g = C.ag; *b = C.ab; }

static float ajc_onda(float periodo) {
  if (C.red || periodo <= 0) return 1.0f;
  return 0.5f - 0.5f * cosf(C.t / periodo * 6.2831853f);
}
static float ajc_fase(float periodo) {
  if (C.red || periodo <= 0) return 1.0f;
  return fmodf(C.t / periodo, 1.0f);
}
static int ajc_passo(float periodo, int n) {
  if (n <= 1) return 0;
  if (C.red || periodo <= 0) return n - 1;
  return (int)fmodf(C.t / periodo, (float)n);
}
static float ajc_passo_t(float periodo) {
  float f;
  if (C.red || periodo <= 0) return 1.0f;
  // a mudanca acontece no primeiro terco do passo; o resto e parado
  f = fmodf(C.t / periodo, 1.0f) * 3.0f;
  if (f > 1.0f) f = 1.0f;
  return f * f * (3.0f - 2.0f * f);
}

static float ajc_comecar(float x, float y, float w, float a) {
  GfxRect q;
  C.x = x; C.y = y; C.w = w; C.s = w / AJC_W; C.h = w * AJC_H / AJC_W; C.a = a;
  C.red = ajustes_animacoes_reduzidas();
  C.t = C.red ? 0.0f : (float)(SDL_GetTicks() % 3600000u) / 1000.0f;
#ifdef AJUSTES_TESTE
  if (ajcTesteT >= 0.0f) { C.red = 0; C.t = ajcTesteT; }   // captura: a cena num instante fixo
#endif
  ajustes_acento(&C.ar, &C.ag, &C.ab);
  C.tinta = (float)ajustes_tinta_foco() / 255.0f;
  q = (GfxRect){ x, y, w, C.h };
  // O palco: um cinza-azulado escuro, com a luz do acento entrando do canto de
  // cima e uma segunda luz fria embaixo (profundidade sem foto).
  gfx_cor(q, 22.0f * C.s / C.h, 0.075f, 0.082f, 0.102f, a);
  gfx_luz_canto(q, 22.0f * C.s / C.h, w * 0.86f, -C.h * 0.25f, C.h * 1.05f, C.ar, C.ag, C.ab, 0.20f * a);
  gfx_luz_canto(q, 22.0f * C.s / C.h, w * 0.08f, C.h * 1.15f, C.h * 0.9f, 0.45f, 0.55f, 0.75f, 0.07f * a);
  gfx_recorte(x, y, w, C.h);
  return C.h;
}
static void ajc_fim(void) { gfx_sem_recorte(); }
static float subX, subY, subS;
static void ajc_sub(float x, float y, float escala) {
  subX = C.x; subY = C.y; subS = C.s;
  C.x = C.x + x * C.s; C.y = C.y + y * C.s; C.s = C.s * escala;
}
static void ajc_sub_fim(void) { C.x = subX; C.y = subY; C.s = subS; }

static void ajc_ret(float x, float y, float w, float h, float raio, float r, float g, float b, float a) {
  GfxRect q = ajc_r(x, y, w, h);
  if (q.h < 0.5f || q.w < 0.5f) return;
  { float rp = raio * C.s, m = (q.h < q.w ? q.h : q.w) * 0.5f;
    if (rp > m) rp = m;
    gfx_cor(q, rp / q.h, r, g, b, a * C.a); }
}
static void ajc_neutro(float x, float y, float w, float h, float raio, float a) {
  ajc_ret(x, y, w, h, raio, 0.17f, 0.18f, 0.21f, a);
}
static void ajc_painel(float x, float y, float w, float h, float raio, float a) {
  ajc_ret(x, y, w, h, raio, 0.10f, 0.105f, 0.125f, a);
}
static void ajc_vidro(float x, float y, float w, float h, float raio, float a) {
  GfxRect q = ajc_r(x, y, w, h);
  ajc_ret(x, y, w, h, raio, 0.80f, 0.84f, 0.92f, 0.13f * a);
  gfx_anel(q, raio * C.s / q.h, 1.5f, 1, 1, 1, 0.22f * a * C.a);
}
static void ajc_acento_ret(float x, float y, float w, float h, float raio, float a) {
  ajc_ret(x, y, w, h, raio, C.ar, C.ag, C.ab, a);
}
static void ajc_barra(float x, float y, float w, float a) {
  ajc_ret(x, y, w, 7, 3.5f, 0.95f, 0.95f, 0.94f, a);
}
static void ajc_barras(float x, float y, float w, int n, float a) {
  static const float L[5] = { 1.0f, 0.86f, 0.94f, 0.62f, 0.78f };
  int i;
  for (i = 0; i < n; i++) ajc_barra(x, y + i * 13.0f, w * (i == n - 1 && n > 1 ? 0.55f : L[i % 5]), a * (i ? 0.42f : 0.62f));
}
static void ajc_matiz(int m, float *r, float *g, float *b) {
  m = ((m % AJC_NMATIZ) + AJC_NMATIZ) % AJC_NMATIZ;
  *r = MATIZ[m][0]; *g = MATIZ[m][1]; *b = MATIZ[m][2];
}
static void ajc_luz(float x, float y, float w, float h, float raio, float cx, float cy, float alcance,
             float r, float g, float b, float a) {
  GfxRect q = ajc_r(x, y, w, h);
  gfx_luz_canto(q, raio * C.s / q.h, cx * C.s, cy * C.s, alcance * C.s, r, g, b, a * C.a);
}
static void ajc_veu(float x, float y, float w, float h, int borda, float a) {
  gfx_veu_css(ajc_r(x, y, w, h), borda, 0, 1.0f, a * C.a);
}
// Cartaz abstrato: o bloco no matiz, a luz clara de cima, o pe escuro e duas
// barras no lugar do titulo.
static void ajc_cartaz(float x, float y, float w, float h, int matiz, float a) {
  float r, g, b, raio = w < h ? w * 0.09f : h * 0.09f;
  GfxRect q = ajc_r(x, y, w, h);
  ajc_matiz(matiz, &r, &g, &b);
  if (raio > 10) raio = 10;
  ajc_ret(x, y, w, h, raio, r * 0.75f, g * 0.75f, b * 0.75f, a);
  gfx_luz_canto(q, raio * C.s / q.h, q.w * 0.25f, -q.h * 0.15f, q.h * 0.95f,
                r + (1 - r) * 0.35f, g + (1 - g) * 0.35f, b + (1 - b) * 0.35f, 0.55f * a * C.a);
  if (h > 40) {
    gfx_veu_css((GfxRect){ q.x, q.y + q.h * 0.55f, q.w, q.h * 0.45f }, 0, 0, 1.0f, 0.45f * a * C.a);
    ajc_ret(x + w * 0.12f, y + h - (h > 90 ? 26 : 18), w * 0.62f, h > 90 ? 6 : 4, 2, 1, 1, 1, 0.75f * a);
    if (h > 90) ajc_ret(x + w * 0.12f, y + h - 15, w * 0.38f, 4, 2, 1, 1, 1, 0.40f * a);
  }
}
static void ajc_foco(float x, float y, float w, float h, float raio, float a) {
  GfxRect q = ajc_r(x, y, w, h);
  gfx_anel_fora(q, raio * C.s / q.h, 2.5f * C.s, 3.0f * C.s, C.ar, C.ag, C.ab, a * C.a);
}
// "Video" tocando: o fundo no acento escuro, duas manchas de cor que derivam e
// a faixa de luz atravessando. 4 quads.
static void ajc_video(float x, float y, float w, float h, float raio, float a) {
  float f = ajc_fase(3.2f), o = ajc_onda(5.0f);
  float r1, g1, b1, r2, g2, b2;
  ajc_matiz(2, &r1, &g1, &b1); ajc_matiz(0, &r2, &g2, &b2);
  ajc_ret(x, y, w, h, raio, C.ar * 0.35f + 0.05f, C.ag * 0.35f + 0.05f, C.ab * 0.35f + 0.06f, a);
  ajc_luz(x, y, w, h, raio, w * (0.15f + 0.5f * o), h * 0.3f, h * 0.9f, r1, g1, b1, 0.75f * a);
  ajc_luz(x, y, w, h, raio, w * (0.95f - 0.45f * o), h * 0.85f, h * 0.8f, r2, g2, b2, 0.65f * a);
  ajc_luz(x, y, w, h, raio, w * (-0.3f + 1.6f * f), h * 0.5f, h * 0.45f, 1, 1, 1, 0.16f * a);
}
static void ajc_icone(const char *nome, float cx, float cy, float d, float a) {
  gfx_icone(ajc_r(cx - d * 0.5f, cy - d * 0.5f, d, d), nome, 0.953f, 0.949f, 0.937f, a * C.a);
}
static void ajc_icone_tinta(const char *nome, float cx, float cy, float d, float a) {
  gfx_icone(ajc_r(cx - d * 0.5f, cy - d * 0.5f, d, d), nome, C.tinta, C.tinta, C.tinta, a * C.a);
}
static void ajc_play(float cx, float cy, float d, float a) {
  ajc_ret(cx - d * 0.5f, cy - d * 0.5f, d, d, d * 0.5f, 1, 1, 1, 0.92f * a);
  gfx_icone(ajc_r(cx - d * 0.22f, cy - d * 0.24f, d * 0.48f, d * 0.48f), "aj_play", 0.08f, 0.09f, 0.11f, a * C.a);
}
static void ajc_ladrilho(const char *nome, float cx, float cy, float d, int acento, float a) {
  if (acento) ajc_acento_ret(cx - d * 0.5f, cy - d * 0.5f, d, d, d * 0.28f, a);
  else ajc_neutro(cx - d * 0.5f, cy - d * 0.5f, d, d, d * 0.28f, a);
  if (acento) ajc_icone_tinta(nome, cx, cy, d * 0.5f, a);
  else ajc_icone(nome, cx, cy, d * 0.5f, a);
}
// 2.0.3: a mesma PILULA COM ESTADO das linhas (nunca a bolinha sozinha):
// ligada, clara e cheia com ponto escuro e o "texto" escuro; desligada, so o
// contorno, ponto vazado e o "texto" cinza.
static void ajc_chave(float x, float y, float liga, float a) {
  float k = liga < 0 ? 0 : liga > 1 ? 1 : liga;
  ajc_ret(x - 6, y, 52, 22, 11, 0.55f, 0.56f, 0.60f, (1 - k) * a);
  ajc_ret(x - 5, y + 1, 50, 20, 10, 0.10f, 0.105f, 0.12f, (1 - k) * a);
  ajc_ret(x - 6, y, 52, 22, 11, 0.953f, 0.949f, 0.937f, k * a);
  ajc_ret(x + 1, y + 7, 8, 8, 4, 0.75f * (1 - k) + 0.09f * k, 0.75f * (1 - k) + 0.09f * k, 0.78f * (1 - k) + 0.10f * k, a);
  ajc_ret(x + 3, y + 9, 4, 4, 2, 0.10f, 0.105f, 0.12f, (1 - k) * a);
  ajc_ret(x + 14, y + 9, 24, 4, 2, 0.75f * (1 - k) + 0.09f * k, 0.75f * (1 - k) + 0.09f * k, 0.78f * (1 - k) + 0.10f * k, a);
}
static void ajc_bolinha(float cx, float cy, float d, float r, float g, float b, float a) {
  ajc_ret(cx - d * 0.5f, cy - d * 0.5f, d, d, d * 0.5f, r, g, b, a);
}
// Progresso em 28 pontos num circulo: os de antes no acento.
static void ajc_anel(float cx, float cy, float raio, float frac, float a) {
  int i, n = 28;
  for (i = 0; i < n; i++) {
    float ang = -1.5707963f + 6.2831853f * (float)i / n;
    int on = (float)i < frac * n;
    float d = on ? 7.0f : 5.0f;
    if (on) ajc_bolinha(cx + cosf(ang) * raio, cy + sinf(ang) * raio, d, C.ar, C.ag, C.ab, a);
    else ajc_bolinha(cx + cosf(ang) * raio, cy + sinf(ang) * raio, d, 1, 1, 1, 0.16f * a);
  }
}
static void ajc_progresso(float x, float y, float w, float frac, float a) {
  if (frac < 0) frac = 0;
  if (frac > 1) frac = 1;
  ajc_ret(x, y, w, 5, 2.5f, 1, 1, 1, 0.18f * a);
  if (frac > 0.01f) ajc_ret(x, y, w * frac, 5, 2.5f, C.ar, C.ag, C.ab, a);
}
// Moldura de TV: aro, tela (desenhe o conteudo depois em x+6, y+6, w-12, h-12)
// e o pe.
static void ajc_tv(float x, float y, float w, float h, float a) {
  ajc_ret(x + w * 0.5f - 34, y + h + 4, 68, 6, 3, 0.25f, 0.26f, 0.30f, a);
  ajc_ret(x + w * 0.5f - 6, y + h - 2, 12, 8, 0, 0.25f, 0.26f, 0.30f, a);
  ajc_ret(x, y, w, h, 12, 0.03f, 0.03f, 0.04f, a);
  gfx_anel(ajc_r(x, y, w, h), 12.0f / h, 1.5f, 1, 1, 1, 0.14f * a * C.a);
}
// Legenda: duas linhas (barras na cor dela) centradas em cx, com a base em
// yBase; `fundo` = alfa da tarja escura; `borda` = o contorno escuro.
static void ajc_legenda(float cx, float yBase, float escala, float fundo, float cr, float cg, float cb,
                 float borda, float a) {
  float h = 9.0f * escala, w1 = 200.0f * escala, w2 = 140.0f * escala, gap = 7.0f * escala;
  float y2 = yBase - h, y1 = y2 - gap - h;
  if (fundo > 0.01f) {
    ajc_ret(cx - w1 * 0.5f - 10 * escala, y1 - 6 * escala, w1 + 20 * escala, h * 2 + gap + 12 * escala,
            5 * escala, 0, 0, 0, fundo * a);
  }
  if (borda > 0.01f) {
    ajc_ret(cx - w1 * 0.5f - 2, y1 - 2, w1 + 4, h + 4, h * 0.5f, 0, 0, 0, borda * 0.8f * a);
    ajc_ret(cx - w2 * 0.5f - 2, y2 - 2, w2 + 4, h + 4, h * 0.5f, 0, 0, 0, borda * 0.8f * a);
  }
  ajc_ret(cx - w1 * 0.5f, y1, w1, h, h * 0.5f, cr, cg, cb, a);
  ajc_ret(cx - w2 * 0.5f, y2, w2, h, h * 0.5f, cr, cg, cb, a);
}
