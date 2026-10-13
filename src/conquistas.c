// CONQUISTAS. Ver conquistas.h.
#include "conquistas.h"
#include "conquistasdados.h"
#include "ajustes.h"
#include "anim.h"
#include "botoes.h"
#include "idioma.h"
#include "ponteiro.h"
#include "text.h"
#define NV_ESCALA_TELA_ATIVA
#include "escala.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define Q_W         1920.0f
#define Q_H         1080.0f
#define MG           148.0f
#define ABRIR_MS     320.0f
#define FECHAR_MS    180.0f
#define PILULA_H      56.0f
#define PILULA_PAD    26.0f
#define ICONE         26.0f
#define MAX_ITENS     48
#define BACK_DEBOUNCE_MS 250
#define AVISO_S        6.0f
#define AVISO_FADE_MS  220.0f
#define LINHAS_VISIVEIS 3
#define TRILHA_LIGADA   0   // 1 quando conquistas_abrir_trilha() existir de verdade

enum { V_HUB = CONQUISTAS_VISTA_HUB, V_FAM = CONQUISTAS_VISTA_FAMILIAS, V_DET = CONQUISTAS_VISTA_DETALHE };
enum { A_TODAS, A_GANHAS, A_ANDAMENTO, A_SECRETAS, A_COMUNIDADE, A_N };
enum { B_VOLTAR = 1, B_FAMS, B_ORDEM, B_RETOMAR, T_ABA0 = 10, F_CARD0 = 30, C_CARD0 = 100 };

typedef struct { int id; GfxRect r; } Item;
typedef struct { int r, g, b; } Tinta;

static int   aberto, view, aba, ordem, famF = -1, topo, foco, detIdx, famAnt;
static float entrada;
static Uint32 backUltimo;
static int   backPressionado, backVisto;
static CqEstado st;
static int   vis[CQ_MAX], nvis;
static float ox, oy;

// Aviso depois dos creditos.
static int    avAberto, avFam, avBackPress;
static float  avTempo, avAlfa;
static Uint32 avBackUltimo;
static char   avTitulo[96], avSub[128];

static const unsigned COR_FAM[CQ_FAM_N] = { 0x2f9e98, 0xc58a3a, 0x4f86d6, 0xc2a53a, 0x8a6fd6, 0xd4524a };
static const char *ICO_FAM[CQ_FAM_N] = { "aj_list-ordered", "aj_clapperboard", "aj_compass",
                                         "aj_star", "aj_message-square", "aj_moon" };

typedef struct { const char *nome, *desc; int breve; } FamLinha;
typedef struct { const char *nome, *lema; FamLinha l[3]; } FamInfo;
// breve = depende do nosso servidor (ambar): a tela diz "Em breve".
static const FamInfo FAM[CQ_FAM_N] = {
  { "Trilhas e listas", "termine uma jornada", {
      { "Primeiro passo", "Veja o primeiro filme de uma trilha", 0 },
      { "Trilha completa", "Veja todos os filmes de uma trilha", 0 },
      { "Lista concluída", "Termine uma lista com 10 filmes ou mais", 0 } } },
  { "Diretores", "conheça uma obra inteira", {
      { "De ponta a ponta", "Veja todos os longas de um diretor", 0 },
      { "Retrospectiva", "Veja 5 filmes de um diretor, em ordem", 0 },
      { "Mesmo olhar", "Veja 3 filmes seguidos do mesmo diretor", 0 } } },
  { "Explorador", "saia da sua rota", {
      { "Décadas", "Veja filmes de 8 décadas diferentes", 1 },
      { "Volta ao mundo", "Veja filmes de 10 países", 1 },
      { "Do silêncio", "Veja 3 filmes anteriores a 1930", 1 } } },
  { "Prêmios · Caça", "os vencedores, em ordem", {
      { "Melhor Filme, em ordem", "Os vencedores do Oscar de Melhor Filme", 0 },
      { "Top 250", "Veja 25, 50, 100 ou 250 do top do Trakt", 0 },
      { "Palma de Ouro", "Os vencedores da Palma de Ouro", 1 } } },
  { "Comunidade", "conversa que ajuda", {
      { "Boa conversa", "Comentários seus que ajudam outras pessoas", 1 },
      { "Spoiler fechado", "Comentários com spoiler marcado", 1 },
      { "Lista adotada", "Pessoas fixaram uma lista sua", 1 } } },
  { "Temporadas", "opcional, sem pressa", {
      { "Outubro do terror", "Oito filmes de terror, no seu tempo", 1 },
      { "Março das diretoras", "Trilha de março, aberta o ano todo", 1 },
      { "Palma em maio", "Trilha de maio, com o Festival de Cannes", 1 } } },
};

static const char *NIVEL[CQ_NIVEIS] = { "Primeiro passo", "Meio caminho", "Reta final", "Trilha completa", "Em ordem" };

static const Tinta PRI = { 243, 242, 239 }, SEC = { 186, 192, 204 }, MUDO = { 150, 156, 168 }, AMBAR = { 212, 164, 58 };

int  conquistas_aberto(void) { return aberto; }
int  conquistas_vista(void) { return view; }
int  conquistas_foco(void) { return foco; }
int  conquistas_aba(void) { return aba; }
int  conquistas_topo(void) { return topo; }
int  conquistas_filtradas(void) { return nvis; }
int  conquistas_abrir_trilha(const char *chave) { (void)chave; return 0; }

void conquistas_teste_esquecer(void) {
  aberto = 0; entrada = 0.0f; view = V_HUB; aba = A_TODAS; ordem = 0; famF = -1; topo = 0;
  foco = B_VOLTAR; detIdx = 0; backPressionado = backVisto = 0; nvis = 0;
  avAberto = 0; avBackPress = 0; avBackUltimo = 0; avTempo = avAlfa = 0.0f;
  memset(&st, 0, sizeof st);
}

// ------------------------------------------------------------------ filtros
static int ganha(const CqEntrada *e) { return e->nivel >= 1; }
static int andando(const CqEntrada *e) { return e->total > 0 && e->vistos < e->total; }

static int pertence(const CqEntrada *e, int a) {
  switch (a) {
    case A_GANHAS: return ganha(e);
    case A_ANDAMENTO: return andando(e);
    case A_COMUNIDADE: return e->familia == CQ_FAM_COMUNIDADE;
    case A_SECRETAS: return 0;   // so o servidor conhece os secretos
    default: return 1;
  }
}

static int chave_ordem(const CqEntrada *e) {
  return andando(e) ? e->vistos * 1000 / e->total : -1;   // o terminado vai para o fim
}

static void filtrar(void) {
  int i, j, linhas;
  nvis = 0;
  for (i = 0; i < st.n; i++) {
    const CqEntrada *e = &st.e[i];
    if (famF >= 0 && e->familia != famF) continue;
    if (!pertence(e, aba)) continue;
    vis[nvis++] = i;
  }
  if (ordem) {
    for (i = 1; i < nvis; i++) {   // insercao: estavel, e sao poucas dezenas
      int v = vis[i], k = chave_ordem(&st.e[v]);
      for (j = i - 1; j >= 0 && chave_ordem(&st.e[vis[j]]) < k; j--) vis[j + 1] = vis[j];
      vis[j + 1] = v;
    }
  }
  linhas = (nvis + 2) / 3;
  if (topo > linhas - LINHAS_VISIVEIS) topo = linhas - LINHAS_VISIVEIS;
  if (topo < 0) topo = 0;
}

static int contar(int a) {
  int i, n = 0;
  for (i = 0; i < st.n; i++) {
    if (famF >= 0 && st.e[i].familia != famF) continue;
    if (pertence(&st.e[i], a)) n++;
  }
  if (a == A_COMUNIDADE && n == 0 && famF < 0) n = cqd_comunidade();
  return n;
}

// A trilha mais adiantada que ainda nao acabou.
static int cand_retomar(void) {
  int i, melhor = -1, k, bk = -1;
  for (i = 0; i < st.n; i++) {
    const CqEntrada *e = &st.e[i];
    if (e->vistos <= 0 || !andando(e)) continue;
    k = e->vistos * 1000 / e->total;
    if (k > bk) { bk = k; melhor = i; }
  }
  return melhor;
}

static int retomar_ligado(void) { return TRILHA_LIGADA && cand_retomar() >= 0; }

// Quantos filmes valem o nivel `n` (1..4) desta entrada.
static int alvo_nivel(const CqEntrada *e, int n) {
  static const int topo250[4] = { 25, 50, 100, 250 };
  if (!strncmp(e->chave, "top:", 4)) return topo250[n - 1];
  if (n == 1) return 1;
  if (n == 2) return (e->total + 1) / 2;
  if (n == 3) return (e->total * 3 + 3) / 4;
  return e->total;
}

// ------------------------------------------------------------------- layout
static void origem(void) {
  ox = (NV_TELA_W - Q_W) * 0.5f;
  oy = (NV_TELA_H - Q_H) * 0.5f;
}

static GfxRect em(float x, float y, float w, float h) { return (GfxRect){ ox + x, oy + y, w, h }; }

static void poe(Item *it, int *n, int id, GfxRect r) {
  if (*n >= MAX_ITENS) return;
  it[*n].id = id;
  it[*n].r = r;
  (*n)++;
}

static float larg_pilula(const char *s, const char *ic) {
  return (float)txt_largura(TXT_V2_CHIP, s) + 2.0f * PILULA_PAD + (ic ? ICONE + 12.0f : 0.0f);
}

static void rotulo_aba(int a, char *dst, size_t n) {
  switch (a) {
    case A_GANHAS:     snprintf(dst, n, i18n("Ganhas %d"), contar(a)); break;
    case A_ANDAMENTO:  snprintf(dst, n, i18n("Em andamento %d"), contar(a)); break;
    case A_SECRETAS:   snprintf(dst, n, i18n("Secretas %d"), contar(a)); break;
    case A_COMUNIDADE: snprintf(dst, n, i18n("Comunidade %d"), contar(a)); break;
    default:           snprintf(dst, n, "%s", i18n("Todas")); break;
  }
}

#define TABS_Y   262.0f
#define GRADE_Y  344.0f
#define PASSO_Y  190.0f
#define CARD_W   380.0f
#define CARD_H   170.0f
#define COL_X    (MG + 1180.0f + 40.0f)
#define COL_W    404.0f

static GfxRect r_aba(int a) {
  char b[64];
  float x = MG;
  int i;
  for (i = 0; i < A_N; i++) {
    float w;
    rotulo_aba(i, b, sizeof b);
    w = larg_pilula(b, NULL);
    if (i == a) return em(x, TABS_Y, w, PILULA_H);
    x += w + 10.0f;
  }
  return em(0, 0, 0, 0);
}

static GfxRect r_ordem(void) {
  float w = larg_pilula(i18n("Mais perto de completar"), "aj_funnel");
  return em(Q_W - MG - w, TABS_Y, w, PILULA_H);
}

static GfxRect r_voltar(void) { return em(MG, 32.0f, larg_pilula(i18n("Voltar"), NULL), PILULA_H); }

static GfxRect r_fams(void) {
  float w = larg_pilula(i18n("O que dá emblema"), "aj_folders");
  return em(Q_W - MG - w, 32.0f, w, PILULA_H);
}

static GfxRect r_card(int i) {
  int col = i % 3, lin = i / 3 - topo;
  return em(MG + (float)col * (CARD_W + 20.0f), GRADE_Y + (float)lin * PASSO_Y, CARD_W, CARD_H);
}

static int card_visivel(int i) { int lin = i / 3 - topo; return lin >= 0 && lin < LINHAS_VISIVEIS; }

#define FAM_W  ((1624.0f - 2.0f * 28.0f) / 3.0f)
#define FAM_H  330.0f
static GfxRect r_famcard(int i) {
  return em(MG + (float)(i % 3) * (FAM_W + 28.0f), 250.0f + (float)(i / 3) * (FAM_H + 28.0f), FAM_W, FAM_H);
}

static GfxRect r_retomar_btn(void) { return em(COL_X + 24.0f, GRADE_Y + 190.0f, COL_W - 48.0f, PILULA_H); }

static int montar(Item *it) {
  int n = 0, i;
  origem();
  filtrar();
  poe(it, &n, B_VOLTAR, r_voltar());
  if (view == V_HUB) {
    if (!ajustes_conquistas()) return n;
    poe(it, &n, B_FAMS, r_fams());
    for (i = 0; i < A_N; i++) poe(it, &n, T_ABA0 + i, r_aba(i));
    poe(it, &n, B_ORDEM, r_ordem());
    for (i = 0; i < nvis; i++) if (card_visivel(i)) poe(it, &n, C_CARD0 + i, r_card(i));
    if (retomar_ligado()) poe(it, &n, B_RETOMAR, r_retomar_btn());
  } else if (view == V_FAM) {
    for (i = 0; i < CQ_FAM_N; i++) poe(it, &n, F_CARD0 + i, r_famcard(i));
  }
  return n;
}

int conquistas_teste_itens(int *ids, GfxRect *rs, int max) {
  Item it[MAX_ITENS];
  int n = montar(it), i;
  for (i = 0; i < n && i < max; i++) { ids[i] = it[i].id; rs[i] = it[i].r; }
  return i;
}

// ----------------------------------------------------------- foco / eventos
static void garantir_visivel(void) {
  if (foco >= C_CARD0) {
    int lin = (foco - C_CARD0) / 3;
    if (lin < topo) topo = lin;
    if (lin > topo + LINHAS_VISIVEIS - 1) topo = lin - (LINHAS_VISIVEIS - 1);
  }
}

static void mover_geo(int dx, int dy) {
  Item it[MAX_ITENS];
  int n = montar(it), i, cur = -1, melhor = -1;
  float cx, cy, bs = 1e9f;
  for (i = 0; i < n; i++) if (it[i].id == foco) cur = i;
  if (cur < 0) { foco = it[0].id; return; }
  cx = it[cur].r.x + it[cur].r.w * 0.5f;
  cy = it[cur].r.y + it[cur].r.h * 0.5f;
  for (i = 0; i < n; i++) {
    float ax, ay, along, cross, s;
    if (i == cur) continue;
    ax = it[i].r.x + it[i].r.w * 0.5f - cx;
    ay = it[i].r.y + it[i].r.h * 0.5f - cy;
    along = ax * (float)dx + ay * (float)dy;
    cross = dx ? fabsf(ay) : fabsf(ax);
    if (along < 1.0f) continue;
    s = along + 3.0f * cross;
    if (s < bs) { bs = s; melhor = i; }
  }
  if (melhor >= 0) foco = it[melhor].id;
}

static void mover(int dx, int dy) {
  if (view == V_HUB && foco >= C_CARD0) {
    int i = foco - C_CARD0, col = i % 3, lin = i / 3, ni;
    filtrar();
    if (dx) {
      ni = i + dx;
      if (col + dx < 0) return;
      if (col + dx > 2) { if (retomar_ligado()) foco = B_RETOMAR; return; }
      if (ni < nvis) foco = C_CARD0 + ni;
      return;
    }
    if (dy > 0) {
      ni = i + 3;
      if (ni >= nvis) { if (lin < (nvis - 1) / 3) ni = nvis - 1; else return; }
      foco = C_CARD0 + ni;
      garantir_visivel();
      return;
    }
    if (lin > 0) { foco = C_CARD0 + i - 3; garantir_visivel(); return; }
  }
  mover_geo(dx, dy);
  garantir_visivel();
}

static void abrir_detalhe(int idx) { detIdx = idx; view = V_DET; foco = B_VOLTAR; }

static void voltar(void) {
  if (view == V_DET) {
    int i;
    view = V_HUB;
    filtrar();
    foco = T_ABA0 + aba;
    for (i = 0; i < nvis; i++) if (vis[i] == detIdx) { foco = C_CARD0 + i; garantir_visivel(); }
  } else if (view == V_FAM) {
    view = V_HUB; foco = B_FAMS;
  } else if (famF >= 0) {
    famAnt = famF; famF = -1; aba = A_TODAS; topo = 0;
    view = V_FAM; foco = F_CARD0 + famAnt;
  } else {
    aberto = 0;
  }
}

static void ok(void) {
  if (foco == B_VOLTAR) { voltar(); return; }
  if (foco == B_FAMS) { view = V_FAM; foco = F_CARD0; return; }
  if (foco == B_ORDEM) { ordem = !ordem; topo = 0; return; }
  if (foco == B_RETOMAR) { int c = cand_retomar(); if (c >= 0) conquistas_abrir_trilha(st.e[c].chave); return; }
  if (foco >= T_ABA0 && foco < T_ABA0 + A_N) { aba = foco - T_ABA0; topo = 0; return; }
  if (foco >= F_CARD0 && foco < F_CARD0 + CQ_FAM_N) {
    famF = foco - F_CARD0; aba = A_TODAS; topo = 0; view = V_HUB;
    filtrar();
    foco = nvis ? C_CARD0 : T_ABA0;
    return;
  }
  if (foco >= C_CARD0) { filtrar(); if (foco - C_CARD0 < nvis) abrir_detalhe(vis[foco - C_CARD0]); }
}

void conquistas_abrir(void) {
  cqd_atualizar_local();
  cqd_carregar(&st);
  aberto = 1; view = V_HUB; aba = A_TODAS; ordem = 0; famF = -1; topo = 0; detIdx = 0;
  backPressionado = backVisto = 0;
  foco = ajustes_conquistas() ? T_ABA0 : B_VOLTAR;
  if (ajustes_animacoes_reduzidas()) entrada = 1.0f;
}

static int eh_voltar(const SDL_Event *e) {
  SDL_Keycode k = e->key.keysym.sym;
  return k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE || k == SDLK_DELETE ||
         e->key.keysym.scancode == NV_SCANCODE_BACK;
}

void conquistas_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (!aberto) return;
  if (e->type != SDL_KEYDOWN && e->type != SDL_KEYUP) return;
  k = e->key.keysym.sym;
  if (eh_voltar(e)) {
    Uint32 agora = SDL_GetTicks();
    if (e->type == SDL_KEYUP) { backPressionado = 0; return; }
    // O Back LG traduzido nao tem repeat/timestamp: descida age uma vez, e a
    // rajada depois de soltar cai no debounce (relogio proprio).
    if (!backPressionado) {
      if (backVisto && agora - backUltimo < BACK_DEBOUNCE_MS) { backUltimo = agora; return; }
      backPressionado = backVisto = 1;
      voltar();
    }
    backUltimo = agora;
    return;
  }
  if (e->type != SDL_KEYDOWN) return;
  if (k == SDLK_LEFT)  { mover(-1, 0); return; }
  if (k == SDLK_RIGHT) { mover(1, 0); return; }
  if (k == SDLK_UP)    { mover(0, -1); return; }
  if (k == SDLK_DOWN)  { mover(0, 1); return; }
  // Segurar OK nao repete: abrir pelo OK do Perfil nao pode fechar a tela sozinho.
  if ((k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) && !e->key.repeat) ok();
}

// ------------------------------------------------------------------- desenho
static void cor_hex(unsigned h, float *r, float *g, float *b) {
  *r = (float)((h >> 16) & 255) / 255.0f;
  *g = (float)((h >> 8) & 255) / 255.0f;
  *b = (float)(h & 255) / 255.0f;
}

static void texto(TxtEstilo est, const char *s, float x, float y, float maxW, Tinta t, float a) {
  TxtLinha l = txt_linha_corta(est, s, t.r, t.g, t.b, 255, maxW);
  txt_desenhar_alpha(l, x, y, a);
}

static void texto_dir(TxtEstilo est, const char *s, float xDir, float y, float maxW, Tinta t, float a) {
  TxtLinha l = txt_linha_corta(est, s, t.r, t.g, t.b, 255, maxW);
  txt_desenhar_alpha(l, xDir - (float)l.w, y, a);
}

static float bloco(TxtEstilo est, const char *s, Tinta t, float x, float y, float w, float lead, float a, int maxL) {
  return txt_bloco_corta(est, s, t.r, t.g, t.b, x, y, w, lead, a, maxL);
}

static void pt_foco(int id, int nada) { (void)nada; foco = id; }
static void pt_ativar(int id, int nada) { (void)nada; foco = id; ok(); }

static void alvo(GfxRect r, int id) {
  if (aberto) ponteiro_alvo(r.x, r.y, r.w, r.h, pt_foco, pt_ativar, id, 0);
}

// Cartao/linha: repouso cinza tingido, foco na cor de realce (como comecar.c).
static void superficie(GfxRect r, float raioPx, int focado, float a, Tinta *pri, Tinta *sec) {
  if (focado) {
    float fr, fg, fb, t = botao_cor_foco(&fr, &fg, &fb);
    int p = (int)(t * 255.0f + 0.5f);
    botao_luz(r, 1.0f, a);
    gfx_cor(r, raioPx / r.h, fr, fg, fb, a);
    *pri = (Tinta){ p, p, p };
    *sec = p > 128 ? (Tinta){ 238, 238, 238 } : (Tinta){ 60, 60, 60 };
  } else {
    gfx_cor(r, raioPx / r.h, 0.14f, 0.15f, 0.17f, 0.92f * a);
    *pri = PRI;
    *sec = SEC;
  }
}

// Pilula de aba/chip/botao. sel = aba escolhida; apagada = nao faz nada hoje.
static void pilula(GfxRect r, const char *s, const char *ic, int foc, int sel, int apagada, float a) {
  Tinta t;
  float x = r.x + PILULA_PAD;
  TxtLinha l;
  if (foc) {
    float fr, fg, fb, tt = botao_cor_foco(&fr, &fg, &fb);
    int p = (int)(tt * 255.0f + 0.5f);
    botao_luz(r, 1.0f, a);
    gfx_cor(r, 0.5f, fr, fg, fb, a);
    t = (Tinta){ p, p, p };
  } else if (apagada) {
    gfx_cor(r, 0.5f, 1, 1, 1, 0.04f * a);
    t = (Tinta){ 118, 123, 134 };
  } else if (sel) {
    gfx_cor(r, 0.5f, 1, 1, 1, 0.18f * a);
    t = (Tinta){ 255, 255, 255 };
  } else {
    gfx_cor(r, 0.5f, 1, 1, 1, 0.07f * a);
    t = (Tinta){ 196, 201, 212 };
  }
  if (ic) {
    gfx_icone((GfxRect){ x, r.y + (r.h - ICONE) * 0.5f, ICONE, ICONE }, ic,
              (float)t.r / 255.0f, (float)t.g / 255.0f, (float)t.b / 255.0f, a);
    x += ICONE + 12.0f;
  }
  l = txt_linha(TXT_V2_CHIP, s, t.r, t.g, t.b, 255);
  txt_desenhar_alpha(l, x, r.y + (r.h - (float)l.h) * 0.5f, a);
}

static void cadeado(GfxRect q, float a) {
  float w = q.w, h = q.h;
  gfx_anel((GfxRect){ q.x + 0.34f * w, q.y + 0.22f * h, 0.32f * w, 0.42f * h }, 0.5f, h * 0.06f, 0.72f, 0.74f, 0.80f, 0.9f * a);
  gfx_cor((GfxRect){ q.x + 0.28f * w, q.y + 0.46f * h, 0.44f * w, 0.30f * h }, 0.18f, 0.72f, 0.74f, 0.80f, 0.9f * a);
}

// estado: 0 ganho (cor da familia), 1 em andamento (apagado), 2 secreto (escuro, cadeado).
static void emblema(float x, float y, float lado, int fam, int estado, float a) {
  float r, g, b;
  GfxRect q = { x, y, lado, lado };
  float esp = lado * 0.03f < 2.0f ? 2.0f : lado * 0.03f;
  cor_hex(COR_FAM[fam], &r, &g, &b);
  if (estado == 2) {
    gfx_cor(q, 0.32f, 0.106f, 0.11f, 0.129f, a);
    gfx_anel(q, 0.32f, esp, 1, 1, 1, 0.10f * a);
    cadeado(q, a);
    return;
  }
  if (estado == 1) { r = r * 0.45f + 0.20f * 0.55f; g = g * 0.45f + 0.21f * 0.55f; b = b * 0.45f + 0.24f * 0.55f; }
  gfx_cor(q, 0.32f, r, g, b, a);
  gfx_anel(q, 0.32f, esp, 1, 1, 1, 0.22f * a);
  gfx_icone((GfxRect){ x + lado * 0.27f, y + lado * 0.27f, lado * 0.46f, lado * 0.46f }, ICO_FAM[fam],
            1, 1, 1, (estado ? 0.65f : 1.0f) * a);
}

static const char *nome_faixa(int f) {
  static const char *n[4] = { "Comum", "Incomum", "Rara", "Raríssima" };
  return i18n(n[f < 0 ? 0 : f > 3 ? 3 : f]);
}

// "Rara · 4%"; vazio quando o servidor nao sabe (honesto: nada inventado).
static int raridade_txt(const char *chave, char *dst, size_t n) {
  int pm = cqd_raridade(chave);
  if (pm < 0) return 0;
  if (pm < 10) snprintf(dst, n, i18n("%s · menos de 1%%"), nome_faixa(cqregra_faixa(pm)));
  else snprintf(dst, n, i18n("%s · %d%%"), nome_faixa(cqregra_faixa(pm)), pm / 10);
  return 1;
}

static void cabecalho(float a, const char *kick, const char *titulo, const char *sub) {
  texto(TXT_V2_KICK, kick, MG, 108.0f + oy, 900.0f, MUDO, a);
  texto(TXT_NOV_TITULO, titulo, MG, 134.0f + oy, 1000.0f, PRI, a);
  if (sub) texto_dir(TXT_V2_24, sub, ox + Q_W - MG, 162.0f + oy, 900.0f, SEC, a);
}

static void kicker_trilha(char *dst, size_t n, const char *terceiro) {
  if (terceiro) snprintf(dst, n, "%s › %s › %s", i18n("Perfil"), i18n("Conquistas"), terceiro);
  else snprintf(dst, n, "%s › %s", i18n("Perfil"), i18n("Conquistas"));
}

static void desenha_card(GfxRect r, const CqEntrada *e, int foc, float a) {
  Tinta p, s, mudo;
  char b[96];
  float x = r.x + 124.0f, w = CARD_W - 124.0f - 20.0f, fr, fg, fb, barW = CARD_W - 40.0f, f;
  int i;
  superficie(r, 24.0f, foc, a, &p, &s);
  mudo = foc ? s : MUDO;
  emblema(r.x + 20.0f, r.y + 20.0f, 88.0f, e->familia, e->nivel >= 1 ? 0 : 1, a);
  bloco(TXT_ILHA_NOME, e->nome, p, x, r.y + 20.0f, w, 28.0f, a, 2);
  if (e->total > 0) snprintf(b, sizeof b, i18n("Nível %d de 5 · %d de %d"), e->nivel, e->vistos, e->total);
  else snprintf(b, sizeof b, i18n("Nível %d de 5"), e->nivel);
  texto(TXT_V2_18, b, x, r.y + 84.0f, w, s, a);
  // Barra de progresso e os cinco pontos de nivel.
  f = e->total > 0 ? (float)e->vistos / (float)e->total : 0.0f;
  if (f > 1.0f) f = 1.0f;
  gfx_cor((GfxRect){ r.x + 20.0f, r.y + 126.0f, barW, 8.0f }, 0.5f, 1, 1, 1, foc ? 0.30f : 0.12f * a);
  cor_hex(COR_FAM[e->familia], &fr, &fg, &fb);
  if (foc) { fr = fg = fb = (float)p.r / 255.0f; }
  if (f > 0.0f) gfx_cor((GfxRect){ r.x + 20.0f, r.y + 126.0f, barW * f < 8.0f ? 8.0f : barW * f, 8.0f }, 0.5f, fr, fg, fb, a);
  for (i = 0; i < CQ_NIVEIS; i++) {
    GfxRect q = { r.x + 20.0f + (float)i * 18.0f, r.y + 146.0f, 12.0f, 12.0f };
    if (i < e->nivel) gfx_cor(q, 0.3f, fr, fg, fb, a);
    else gfx_cor(q, 0.3f, 1, 1, 1, foc ? 0.30f : 0.14f * a);
  }
  if (raridade_txt(e->chave, b, sizeof b)) texto_dir(TXT_V2_18, b, r.x + CARD_W - 20.0f, r.y + 138.0f, 200.0f, mudo, a);
}

static void painel(GfxRect r, float a) { gfx_cor(r, 26.0f / r.h, 0.14f, 0.15f, 0.17f, 0.92f * a); }

static void botao_breve(GfxRect r, const char *s, const char *ic, int foc, float a) {
  pilula(r, s, ic, foc, 0, !foc, a);
  if (!foc) texto_dir(TXT_V2_18, i18n("Em breve"), r.x + r.w - 22.0f, r.y + (r.h - 24.0f) * 0.5f, 100.0f, AMBAR, a);
}

static void desenha_vazio(float a) {
  GfxRect r = em(MG, GRADE_Y, 1180.0f, 550.0f);
  const char *tit, *corpo;
  float cx = r.x + r.w * 0.5f, w = 760.0f;
  int fam = famF >= 0 ? famF : CQ_FAM_TRILHA, secreto = aba == A_SECRETAS;
  painel(r, a);
  if (aba == A_SECRETAS) {
    tit = "Nenhuma secreta por aqui ainda";
    corpo = "Emblema secreto só se revela ao ser ganho. Essa parte chega com o nosso servidor.";
  } else if (aba == A_COMUNIDADE) {
    tit = "A comunidade chega depois";
    corpo = "Emblemas de comunidade dependem do nosso servidor e da moderação. Em breve.";
  } else {
    tit = "Nenhum emblema ainda";
    corpo = "Veja um filme de uma trilha ou de um diretor e o primeiro emblema aparece aqui. Sem prazo, sem sequência.";
  }
  emblema(cx - 60.0f, r.y + 90.0f, 120.0f, fam, secreto ? 2 : 1, a);
  {
    TxtLinha l = txt_linha(TXT_ILHA_TITULO, tit, PRI.r, PRI.g, PRI.b, 255);
    txt_desenhar_alpha(l, cx - (float)l.w * 0.5f, r.y + 246.0f, a);
  }
  {
    float h = bloco(TXT_V2_26, corpo, SEC, cx - w * 0.5f, 0, w, 34.0f, 0.0f, 4);
    (void)h;
    bloco(TXT_V2_26, corpo, SEC, cx - w * 0.5f, r.y + 312.0f, w, 34.0f, a, 4);
  }
  if (aba == A_COMUNIDADE || aba == A_SECRETAS)
    texto_dir(TXT_V2_18, i18n("Em breve"), r.x + r.w - 28.0f, r.y + 24.0f, 120.0f, AMBAR, a);
}

static void desenha_retomar(float a) {
  GfxRect p = em(COL_X, GRADE_Y, COL_W, 270.0f);
  int c = cand_retomar();
  char b[64];
  painel(p, a);
  texto(TXT_V2_KICK, i18n("Retomar"), p.x + 24.0f, p.y + 22.0f, COL_W - 48.0f, MUDO, a);
  if (c < 0) {
    bloco(TXT_V2_24, "Nada para retomar ainda. Quando você começar uma trilha, ela aparece aqui.", SEC,
          p.x + 24.0f, p.y + 66.0f, COL_W - 48.0f, 32.0f, a, 5);
    return;
  }
  emblema(p.x + 24.0f, p.y + 60.0f, 96.0f, st.e[c].familia, st.e[c].nivel >= 1 ? 0 : 1, a);
  bloco(TXT_ILHA_NOME, st.e[c].nome, PRI, p.x + 136.0f, p.y + 62.0f, COL_W - 160.0f, 28.0f, a, 2);
  snprintf(b, sizeof b, i18n("%d de %d filmes"), st.e[c].vistos, st.e[c].total);
  texto(TXT_V2_18, b, p.x + 136.0f, p.y + 128.0f, COL_W - 160.0f, SEC, a);
  botao_breve(r_retomar_btn(), "Continuar a trilha", "aj_play", foco == B_RETOMAR && retomar_ligado(), a);
}

static void desenha_mapa(float a) {
  GfxRect p = em(COL_X, GRADE_Y + 290.0f, COL_W, 260.0f);
  CqMapa m;
  char b[24];
  int i;
  const char *rot[3] = { "décadas", "países", "idiomas" };
  int val[3];
  cqd_mapa(&m);
  val[0] = m.decadas; val[1] = m.paises; val[2] = m.idiomas;
  painel(p, a);
  texto(TXT_V2_KICK, i18n("Seu mapa de descobertas"), p.x + 24.0f, p.y + 22.0f, COL_W - 48.0f, MUDO, a);
  for (i = 0; i < 10; i++)
    gfx_cor((GfxRect){ p.x + 24.0f + (float)i * 36.0f, p.y + 60.0f, 30.0f, 30.0f }, 0.25f, 1, 1, 1,
            (m.ok && m.decada[i] ? 0.55f : 0.08f) * a);
  texto(TXT_V2_18, "1920", p.x + 24.0f, p.y + 96.0f, 100.0f, MUDO, a);
  texto_dir(TXT_V2_18, "2020", p.x + COL_W - 26.0f, p.y + 96.0f, 100.0f, MUDO, a);
  for (i = 0; i < 3; i++) {
    float x = p.x + 24.0f + (float)i * 120.0f;
    if (m.ok) snprintf(b, sizeof b, "%d", val[i]); else snprintf(b, sizeof b, "—");
    texto(TXT_ILHA_TITULO, b, x, p.y + 126.0f, 110.0f, PRI, a);
    texto(TXT_V2_18, rot[i], x, p.y + 178.0f, 112.0f, SEC, a);
  }
  texto(TXT_V2_18, "Em breve, com o nosso servidor.", p.x + 24.0f, p.y + 218.0f, COL_W - 48.0f, AMBAR, a);
}

static void desenha_hub(float a) {
  char k[96], sub[96], b[64];
  int i, linhas, ganhas = 0, anda = 0;
  Item it[MAX_ITENS];
  int n = montar(it);
  for (i = 0; i < st.n; i++) { ganhas += ganha(&st.e[i]); anda += andando(&st.e[i]); }
  kicker_trilha(k, sizeof k, famF >= 0 ? FAM[famF].nome : NULL);
  snprintf(sub, sizeof sub, i18n("Emblemas: %d · Em andamento: %d"), ganhas, anda);
  if (!ajustes_conquistas()) {
    GfxRect r = em(MG, GRADE_Y, 1624.0f, 300.0f);
    cabecalho(a, k, i18n("Conquistas"), NULL);
    painel(r, a);
    texto(TXT_ILHA_TITULO, i18n("Conquistas desligadas"), r.x + 40.0f, r.y + 48.0f, r.w - 80.0f, PRI, a);
    bloco(TXT_V2_26, "Ligue em Ajustes › Conquistas para ver seus emblemas. Trilhas e listas funcionam do mesmo jeito.",
          SEC, r.x + 40.0f, r.y + 118.0f, 960.0f, 34.0f, a, 4);
  } else {
    cabecalho(a, k, famF >= 0 ? i18n(FAM[famF].nome) : i18n("Conquistas"), sub);
    for (i = 0; i < A_N; i++) {
      rotulo_aba(i, b, sizeof b);
      pilula(r_aba(i), b, NULL, foco == T_ABA0 + i, aba == i, 0, a);
    }
    pilula(r_ordem(), i18n("Mais perto de completar"), "aj_funnel", foco == B_ORDEM, ordem, 0, a);
    pilula(r_fams(), i18n("O que dá emblema"), "aj_folders", foco == B_FAMS, 0, 0, a);
    if (nvis == 0) desenha_vazio(a);
    else for (i = 0; i < nvis; i++) if (card_visivel(i)) desenha_card(r_card(i), &st.e[vis[i]], foco == C_CARD0 + i, a);
    linhas = (nvis + 2) / 3;
    if (linhas > LINHAS_VISIVEIS) {
      float alt = PASSO_Y * (float)LINHAS_VISIVEIS - 20.0f, th = alt * (float)LINHAS_VISIVEIS / (float)linhas;
      float ty = alt * (float)topo / (float)linhas;
      gfx_cor(em(COL_X - 22.0f, GRADE_Y, 6.0f, alt), 0.5f, 1, 1, 1, 0.08f * a);
      gfx_cor(em(COL_X - 22.0f, GRADE_Y + ty, 6.0f, th), 0.5f, 1, 1, 1, 0.45f * a);
    }
    desenha_retomar(a);
    desenha_mapa(a);
    bloco(TXT_V2_18, "Emblemas premiam o que você descobriu, não quanto tempo ficou na frente da tela. Sem prazo, sem sequência para manter. Tudo isso se desliga em Ajustes.",
          MUDO, ox + MG, oy + 930.0f, 1624.0f, 26.0f, a, 2);
  }
  pilula(r_voltar(), i18n("Voltar"), NULL, foco == B_VOLTAR, 0, 0, a);
  for (i = 0; i < n; i++) alvo(it[i].r, it[i].id);
}

static void desenha_familias(float a) {
  char k[96];
  int i, j;
  Item it[MAX_ITENS];
  int n = montar(it);
  kicker_trilha(k, sizeof k, i18n("Famílias"));
  cabecalho(a, k, i18n("O que dá emblema"), i18n("Cada emblema tem até 5 níveis; o 5º é sempre opcional."));
  for (i = 0; i < CQ_FAM_N; i++) {
    GfxRect r = r_famcard(i);
    Tinta p, s, mudo;
    int foc = foco == F_CARD0 + i;
    superficie(r, 26.0f, foc, a, &p, &s);
    mudo = foc ? s : MUDO;
    emblema(r.x + 24.0f, r.y + 22.0f, 64.0f, i, 0, a);
    texto(TXT_ILHA_NOME, FAM[i].nome, r.x + 104.0f, r.y + 22.0f, r.w - 128.0f, p, a);
    texto(TXT_V2_18, FAM[i].lema, r.x + 104.0f, r.y + 56.0f, r.w - 128.0f, mudo, a);
    gfx_cor((GfxRect){ r.x + 24.0f, r.y + 100.0f, r.w - 48.0f, 1.0f }, 0, 1, 1, 1, 0.10f * a);
    for (j = 0; j < 3; j++) {
      const FamLinha *l = &FAM[i].l[j];
      float y = r.y + 112.0f + (float)j * 72.0f, tw = r.w - 24.0f - 48.0f - 14.0f - 24.0f;
      emblema(r.x + 24.0f, y + 8.0f, 48.0f, i, 1, a);
      texto(TXT_ILHA_SUB, l->nome, r.x + 86.0f, y + 4.0f, l->breve ? tw - 96.0f : tw, p, a);
      if (l->breve) texto_dir(TXT_V2_18, i18n("Em breve"), r.x + r.w - 24.0f, y + 6.0f, 96.0f, foc ? s : AMBAR, a);
      texto(TXT_V2_18, l->desc, r.x + 86.0f, y + 36.0f, tw, s, a);
    }
  }
  texto(TXT_V2_18, "Fora, por princípio: sequência de dias, horas assistidas, maratonas.", ox + MG, oy + 1004.0f, 1624.0f, MUDO, a);
  pilula(r_voltar(), i18n("Voltar"), NULL, foco == B_VOLTAR, 0, 0, a);
  for (i = 0; i < n; i++) alvo(it[i].r, it[i].id);
}

static void desenha_detalhe(float a) {
  const CqEntrada *e;
  char k[96], b[96];
  int i, n, nivel;
  Item it[MAX_ITENS];
  float hh, y;
  if (detIdx < 0 || detIdx >= st.n) return;
  e = &st.e[detIdx];
  nivel = e->nivel;
  n = montar(it);
  kicker_trilha(k, sizeof k, i18n(FAM[e->familia].nome));
  texto(TXT_V2_KICK, k, MG + ox, 108.0f + oy, 900.0f, MUDO, a);
  // Coluna da esquerda: emblema, nome e estado.
  emblema(ox + MG, oy + 164.0f, 230.0f, e->familia, nivel >= 1 ? 0 : 1, a);
  hh = bloco(TXT_NOV_TITULO, e->nome, PRI, ox + MG, oy + 418.0f, 420.0f, 58.0f, a, 2);
  y = oy + 418.0f + hh + 12.0f;
  snprintf(b, sizeof b, i18n(e->total > 0 && e->vistos >= e->total ? "Completa · nível %d de 5" : "Em andamento · nível %d de 5"), nivel);
  texto(TXT_V2_24, b, ox + MG, y, 420.0f, SEC, a);
  y += 36.0f;
  if (e->total > 0) {
    snprintf(b, sizeof b, i18n("%d de %d filmes"), e->vistos, e->total);
    texto(TXT_V2_24, b, ox + MG, y, 420.0f, SEC, a);
    y += 36.0f;
  }
  if (raridade_txt(e->chave, b, sizeof b)) texto(TXT_V2_24, b, ox + MG, y, 420.0f, SEC, a);
  botao_breve(em(MG, 700.0f, 420.0f, PILULA_H), "Ir à trilha", "aj_play", 0, a);
  botao_breve(em(MG, 764.0f, 420.0f, PILULA_H), "Fixar no perfil", "aj_bookmark", 0, a);
  // Meio: os cinco niveis.
  for (i = 0; i < CQ_NIVEIS; i++) {
    GfxRect r = em(MG + 460.0f, 164.0f + (float)i * 122.0f, 700.0f, 108.0f);
    int lv = i + 1, feito = lv <= nivel;
    float fr, fg, fb;
    const char *regra, *estado;
    char rb[64], eb[48];
    cor_hex(COR_FAM[e->familia], &fr, &fg, &fb);
    gfx_cor(r, 22.0f / r.h, feito ? 0.17f : 0.14f, feito ? 0.19f : 0.15f, feito ? 0.21f : 0.17f, 0.92f * a);
    if (feito) gfx_anel(r, 22.0f / r.h, 2.0f, fr, fg, fb, 0.55f * a);
    {
      GfxRect c = { r.x + 22.0f, r.y + 32.0f, 44.0f, 44.0f };
      gfx_cor(c, 0.5f, feito ? fr : 1, feito ? fg : 1, feito ? fb : 1, (feito ? 1.0f : 0.10f) * a);
      if (feito) gfx_icone((GfxRect){ c.x + 10.0f, c.y + 10.0f, 24.0f, 24.0f }, "aj_check", 1, 1, 1, a);
      else {
        char nb[4];
        TxtLinha l;
        snprintf(nb, sizeof nb, "%d", lv);
        l = txt_linha(TXT_ILHA_NOME, nb, SEC.r, SEC.g, SEC.b, 255);
        txt_desenhar_alpha(l, c.x + (44.0f - (float)l.w) * 0.5f, c.y + (44.0f - (float)l.h) * 0.5f, a);
      }
    }
    if (lv == 5) regra = "Veja todos, na ordem do curador";
    else if (!strncmp(e->chave, "top:", 4)) { snprintf(rb, sizeof rb, i18n("Veja %d filmes"), alvo_nivel(e, lv)); regra = rb; }
    else regra = lv == 1 ? "Veja o primeiro filme" : lv == 2 ? "Chegue à metade" : lv == 3 ? "Chegue a três quartos" : "Veja todos os filmes";
    if (feito) estado = i18n("Ganho");
    else if (lv == 5) estado = i18n("Opcional");
    else if (e->total <= 0 && strncmp(e->chave, "top:", 4)) estado = "";
    else {
      int falta = alvo_nivel(e, lv) - e->vistos;
      if (falta <= 1) estado = i18n("Falta 1 passo");
      else { snprintf(eb, sizeof eb, i18n("Faltam %d passos"), falta); estado = eb; }
    }
    texto(TXT_ILHA_NOME, NIVEL[i], r.x + 86.0f, r.y + 20.0f, 330.0f, feito ? PRI : SEC, a);
    texto(TXT_V2_18, regra, r.x + 86.0f, r.y + 58.0f, 330.0f, MUDO, a);
    texto_dir(TXT_V2_18, estado, r.x + r.w - 24.0f, r.y + 42.0f, 210.0f, feito ? PRI : MUDO, a);
  }
  // Direita: amigos (ambar) e como conta.
  {
    GfxRect p1 = em(MG + 1200.0f, 164.0f, 424.0f, 230.0f), p2 = em(MG + 1200.0f, 414.0f, 424.0f, 346.0f);
    int am = cqd_amigos_na_trilha(e->chave);
    painel(p1, a);
    texto(TXT_V2_KICK, i18n("Amigos nesta trilha"), p1.x + 24.0f, p1.y + 22.0f, 376.0f, MUDO, a);
    if (am > 0) { snprintf(b, sizeof b, i18n("%d amigos nesta trilha"), am); texto(TXT_V2_26, b, p1.x + 24.0f, p1.y + 66.0f, 376.0f, PRI, a); }
    else {
      bloco(TXT_V2_18, "Quantos amigos fazem esta trilha, sem placar e sem aviso para ninguém.", SEC, p1.x + 24.0f, p1.y + 66.0f, 376.0f, 26.0f, a, 4);
      texto(TXT_V2_18, i18n("Em breve"), p1.x + 24.0f, p1.y + 176.0f, 200.0f, AMBAR, a);
    }
    painel(p2, a);
    texto(TXT_V2_KICK, i18n("Como conta"), p2.x + 24.0f, p2.y + 22.0f, 376.0f, MUDO, a);
    hh = bloco(TXT_V2_18, "Cada filme vira passo quando está no histórico do Trakt ou você marca Já vi. O app avisa uma vez, no fim da sessão.",
               SEC, p2.x + 24.0f, p2.y + 62.0f, 376.0f, 26.0f, a, 6);
    bloco(TXT_V2_18, "Filme que você já viu antes de abrir a trilha conta para os níveis 1 a 4.", SEC,
          p2.x + 24.0f, p2.y + 62.0f + hh + 16.0f, 376.0f, 26.0f, a, 4);
  }
  pilula(r_voltar(), i18n("Voltar"), NULL, foco == B_VOLTAR, 0, 0, a);
  for (i = 0; i < n; i++) alvo(it[i].r, it[i].id);
}

void conquistas_desenhar(Uint32 agora) {
  float a = anim_suave(entrada);
  (void)agora;
  if (entrada < 0.002f) return;
  ESCALA_SE_COUBER_INI(Q_W, Q_H);
  origem();
  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 0.051f, 0.051f, 0.051f, entrada);
  if (view == V_FAM) desenha_familias(a);
  else if (view == V_DET) desenha_detalhe(a);
  else desenha_hub(a);
  ESCALA_SE_COUBER_FIM();
}

// -------------------------------------------------------------------- aviso
int conquistas_aviso_aberto(void) { return avAberto; }

void conquistas_aviso_fechar(void) { avAberto = 0; avAlfa = 0.0f; }

void conquistas_aviso_armar(void) {
  CqEstado s;
  int i, n;
  if (!ajustes_conquistas_aviso()) return;
  cqd_carregar(&s);
  n = cqregra_pendentes(&s);
  if (n <= 0) return;
  i = cqregra_proximo_aviso(&s);
  if (i < 0) return;
  avFam = s.e[i].familia;
  if (n == 1) {
    const CqEntrada *e = &s.e[i];
    snprintf(avTitulo, sizeof avTitulo, "%s", i18n(NIVEL[e->nivel < 1 ? 0 : e->nivel > CQ_NIVEIS ? CQ_NIVEIS - 1 : e->nivel - 1]));
    if (e->total > 0) snprintf(avSub, sizeof avSub, i18n("%s · %d de %d passos"), e->nome, e->vistos, e->total);
    else snprintf(avSub, sizeof avSub, "%s", e->nome);
  } else {
    snprintf(avTitulo, sizeof avTitulo, i18n("%d conquistas novas"), n);
    snprintf(avSub, sizeof avSub, "%s", i18n("Ver em Perfil › Conquistas"));
  }
  cqregra_marcar_avisados(&s);
  cqd_gravar(&s);
  avAberto = 1;
  avTempo = 0.0f;
  avAlfa = ajustes_animacoes_reduzidas() ? 1.0f : 0.0f;
}

int conquistas_aviso_evento(const SDL_Event *e) {
  Uint32 agora;
  if (aberto || avAlfa < 0.002f) {
    avBackPress = 0; avBackUltimo = 0;
    return 0;
  }
  if (e->type != SDL_KEYDOWN && e->type != SDL_KEYUP) return 0;
  if (!eh_voltar(e)) return 0;
  agora = SDL_GetTicks();
  if (e->type == SDL_KEYUP) {
    if (avBackPress) { avBackPress = 0; return 1; }   // a soltura do Voltar que fechou o aviso
    return 0;
  }
  if (avAberto) { avAberto = 0; avBackPress = 1; avBackUltimo = agora; return 1; }
  // Rajada logo depois de fechar: nao pode cair na tela de baixo.
  if (avBackUltimo && agora - avBackUltimo < BACK_DEBOUNCE_MS) { avBackUltimo = agora; return 1; }
  return 0;
}

void conquistas_atualizar(float dt, Uint32 agora) {
  int red = ajustes_animacoes_reduzidas();
  (void)agora;
  if (avAberto) {
    avTempo += dt;
    if (avTempo >= AVISO_S) avAberto = 0;
    avAlfa = red ? 1.0f : anim_rampa(avAlfa, 1.0f, dt, AVISO_FADE_MS);
  } else {
    avAlfa = red ? 0.0f : anim_rampa(avAlfa, 0.0f, dt, AVISO_FADE_MS);
  }
  if (!aberto && entrada < 0.002f) { entrada = 0.0f; return; }
  if (red) { entrada = aberto ? 1.0f : 0.0f; return; }
  entrada = anim_rampa(entrada, aberto ? 1.0f : 0.0f, dt, aberto ? ABRIR_MS : FECHAR_MS);
}

void conquistas_aviso_desenhar(void) {
  float a = anim_suave(avAlfa), x, y, fr, fg, fb, f;
  GfxRect r;
  if (avAlfa < 0.002f || aberto) return;
  ESCALA_SE_COUBER_INI(Q_W, Q_H);
  origem();
  x = 120.0f; y = Q_H - 72.0f - 148.0f;
  r = em(x, y, 780.0f, 148.0f);
  gfx_cor(r, 0.2f, 0.10f, 0.11f, 0.13f, 0.96f * a);
  gfx_anel(r, 0.2f, 2.0f, 1, 1, 1, 0.10f * a);
  emblema(r.x + 30.0f, r.y + 32.0f, 84.0f, avFam, 0, a);
  cor_hex(COR_FAM[avFam], &fr, &fg, &fb);
  {
    Tinta kc = { (int)(fr * 255.0f), (int)(fg * 255.0f), (int)(fb * 255.0f) };
    texto(TXT_V2_KICK, i18n("Conquista nova"), r.x + 140.0f, r.y + 26.0f, 320.0f, kc, a);
  }
  texto_dir(TXT_V2_18, i18n("Voltar fecha"), r.x + r.w - 28.0f, r.y + 24.0f, 200.0f, MUDO, a);
  texto(TXT_ILHA_NOME, avTitulo, r.x + 140.0f, r.y + 56.0f, 610.0f, PRI, a);
  texto(TXT_V2_18, avSub, r.x + 140.0f, r.y + 94.0f, 610.0f, SEC, a);
  if (!ajustes_animacoes_reduzidas()) {
    f = 1.0f - avTempo / AVISO_S;
    if (f < 0.0f) f = 0.0f;
    gfx_cor((GfxRect){ r.x + 30.0f, r.y + r.h - 16.0f, r.w - 60.0f, 4.0f }, 0.5f, 1, 1, 1, 0.10f * a);
    if (f > 0.0f) gfx_cor((GfxRect){ r.x + 30.0f, r.y + r.h - 16.0f, (r.w - 60.0f) * f, 4.0f }, 0.5f, fr, fg, fb, 0.8f * a);
  }
  ESCALA_SE_COUBER_FIM();
}
