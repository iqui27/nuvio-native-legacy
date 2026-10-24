// CONQUISTAS. Ver conquistas.h.
#include "conquistas.h"
#include "conquistasdados.h"
#include "conquistasarte.h"
#include "tex_cache.h"
#include "ajustes.h"
#include "anim.h"
#include "revela.h"
#include "botoes.h"
#include "idioma.h"
#include "ponteiro.h"
#include "text.h"
#define NV_TXT_TELA
#include "tela21.h"
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
#define LINHAS_VISIVEIS linhasVis
#define TRILHA_LIGADA   0   // 1 quando conquistas_abrir_trilha() existir de verdade

enum { V_HUB = CONQUISTAS_VISTA_HUB, V_FAM = CONQUISTAS_VISTA_FAMILIAS, V_DET = CONQUISTAS_VISTA_DETALHE };
enum { A_TODAS, A_GANHAS, A_ANDAMENTO, A_SECRETAS, A_COMUNIDADE, A_N };
enum { B_VOLTAR = 1, B_FAMS, B_ORDEM, B_RETOMAR, T_ABA0 = 10, F_CARD0 = 30, C_CARD0 = 100 };

typedef struct { int id; GfxRect r; } Item;
typedef struct { int r, g, b; } Tinta;

static int   aberto, view, aba, ordem, famF = -1, topo, foco, detIdx, famAnt;
static float entrada, tempo, scrollY, scrollV, tabSel, tabFoco;
static float focos[C_CARD0 + CQ_MAX];
static int vistaAnim = -1;
static float qf(int id) { return focos[id]; }
static Uint32 backUltimo;
static int   backPressionado, backVisto;
static CqEstado st;
static int   vis[CQ_MAX], nvis;
static float ox, oy, largura, gradeW;
static int colunas = 3, linhasVis = 3;

// Aviso depois dos creditos.
static int    avAberto, avFam, avBackPress;
static float  avTempo, avAlfa;
static Uint32 avBackUltimo;
static char   avTitulo[96], avSub[128];

// Medal colors are artwork, independent of the user's focus accent.
static const Tinta COR_FAM[CQ_FAM_N] = { {47,158,152}, {197,138,58}, {79,134,214}, {194,165,58}, {138,111,214}, {212,82,74} };
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

static const Tinta PRI = { 243, 242, 239 }, SEC = { 186, 192, 204 }, MUDO = { 150, 156, 168 }, AMBAR = { 150, 156, 168 };

int  conquistas_aberto(void) { return aberto; }
int  conquistas_vista(void) { return view; }
int  conquistas_foco(void) { return foco; }
int  conquistas_aba(void) { return aba; }
int  conquistas_topo(void) { return topo; }
int  conquistas_filtradas(void) { return nvis; }
int  conquistas_abrir_trilha(const char *chave) { (void)chave; return 0; }

void conquistas_teste_esquecer(void) {
  memset(focos, 0, sizeof focos);
  tempo = scrollY = scrollV = tabSel = tabFoco = 0; vistaAnim = -1;
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
  linhas = (nvis + colunas - 1) / colunas;
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
  t21_inicio(&ox, &largura);
  ox -= MG;
  oy = 0;
  gradeW = largura - 444.0f;
  colunas = gradeW < 1100.0f ? 2 : 3;
  linhasVis = gfx_escala_txt() > 1.0f ? 2 : 3;
}

static GfxRect em(float x, float y, float w, float h) {
  if (view == V_DET && y >= 108.0f) {
    float k = fminf(1, largura / 1624.0f);
    return (GfxRect){ox + MG + (x - MG) * k, oy + ANC(y), w * k, h};
  }
  return (GfxRect){ ox + x, oy + y, w, h };
}

static void poe(Item *it, int *n, int id, GfxRect r) {
  if (*n >= MAX_ITENS) return;
  it[*n].id = id;
  it[*n].r = r;
  (*n)++;
}

static float larg_pilula(const char *s, const char *ic) {
  return botao_largura(s, ic, 0);
}


#define TABS_Y   ANC(262.0f)
#define GRADE_Y  (TABS_Y + PILULA_H + 26.0f)
#define PASSO_Y  190.0f
#define CARD_W   ((gradeW - (colunas - 1) * 20.0f) / colunas)
#define CARD_H   170.0f
#define COL_X    (MG + gradeW + 40.0f)
#define COL_W    404.0f

static const char *const tabs[A_N] = { "Todas", "Ganhas", "Em andamento", "Secretas", "Comunidade" };
static GfxRect r_aba(int a) {
  float x = MG + 5;
  for (int i = 0; i < A_N; i++) {
    char b[16]; snprintf(b, sizeof b, "%d", contar(i));
    float w = txt_largura(TXT_ILHA_SEG, i18n(tabs[i])) + 40;
    if (i) w += 9 + txt_largura(TXT_ILHA_NUM, b);
    if (i == a) return em(x, TABS_Y + 5, w, 44);
    x += w + 4;
  }
  return em(0, 0, 0, 0);
}

static GfxRect r_ordem(void) {
  float w = larg_pilula(i18n("Mais perto de completar"), "aj_funnel");
  return em(MG + largura - w, TABS_Y, w, PILULA_H);
}

static GfxRect r_voltar(void) { return em(MG, ANC(32.0f), larg_pilula(i18n("Voltar"), NULL), PILULA_H); }

static GfxRect r_fams(void) {
  float w = larg_pilula(i18n("O que dá emblema"), "aj_folders");
  return em(MG + largura - w, ANC(32.0f), w, PILULA_H);
}

static GfxRect r_card(int i) {
  int col = i % colunas, lin = i / colunas - topo;
  return em(MG + (float)col * (CARD_W + 20.0f), GRADE_Y + (float)lin * PASSO_Y, CARD_W, CARD_H);
}

static int card_visivel(int i) { int lin = i / colunas - topo; return lin >= 0 && lin < LINHAS_VISIVEIS; }

#define FAM_W  ((largura - 2.0f * 28.0f) / 3.0f)
#define FAM_H  fminf(330.0f, (ANC(960.0f) - ANC(250.0f) - 28.0f) / 2.0f)
static GfxRect r_famcard(int i) {
  return em(MG + (float)(i % 3) * (FAM_W + 28.0f), ANC(250.0f) + (float)(i / 3) * (FAM_H + 28.0f), FAM_W, FAM_H);
}

static GfxRect r_retomar_btn(void) { return em(COL_X + 24.0f, GRADE_Y + 270.0f * fminf(1, (ANC(920.0f) - GRADE_Y) / 550.0f) - 80.0f, COL_W - 48.0f, PILULA_H); }

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
  TXT_INI();
  Item it[MAX_ITENS];
  int n = montar(it), i;
  for (i = 0; i < n && i < max; i++) {
    float k = gfx_escala();
    ids[i] = it[i].id;
    rs[i] = (GfxRect){it[i].r.x * k, it[i].r.y * k, it[i].r.w * k, it[i].r.h * k};
  }
  TXT_FIM();
  return i;
}

// ----------------------------------------------------------- foco / eventos
static void garantir_visivel(void) {
  if (foco >= C_CARD0) {
    int lin = (foco - C_CARD0) / colunas;
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
  if (view == V_HUB) {
    if (foco == B_VOLTAR && dx > 0) { foco = B_FAMS; return; }
    if (foco == B_FAMS && dx < 0) { foco = B_VOLTAR; return; }
    if (foco == B_FAMS && dy > 0) { foco = B_ORDEM; return; }
    if (foco == B_ORDEM && dy < 0) { foco = B_FAMS; return; }
  }
  if (view == V_HUB && foco >= C_CARD0) {
    int i = foco - C_CARD0, col = i % colunas, lin = i / colunas, ni;
    filtrar();
    if (dx) {
      ni = i + dx;
      if (col + dx < 0) return;
      if (col + dx >= colunas) { if (retomar_ligado()) foco = B_RETOMAR; return; }
      if (ni < nvis) foco = C_CARD0 + ni;
      return;
    }
    if (dy > 0) {
      ni = i + colunas;
      if (ni >= nvis) { if (lin < (nvis - 1) / colunas) ni = nvis - 1; else return; }
      foco = C_CARD0 + ni;
      garantir_visivel();
      return;
    }
    if (lin > 0) { foco = C_CARD0 + i - colunas; garantir_visivel(); return; }
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

static void conquistas_evento_(const SDL_Event *e) {
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
static void cor_hex(Tinta t, float *r, float *g, float *b) {
  *r = t.r / 255.0f; *g = t.g / 255.0f; *b = t.b / 255.0f;
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

// Cards retain light ink over the official neutral focus surface.
static void superficie(GfxRect r, float raioPx, float f, float a, Tinta *pri, Tinta *sec) {
  t21_ilha(r, raioPx, a);
  plrui_linha_foco(r, raioPx, f * a);
  *pri = PRI; *sec = SEC;
}

// Pilula de aba/chip/botao. sel = aba escolhida; apagada = nao faz nada hoje.
static void pilula(GfxRect r, const char *s, const char *ic, float foc, int sel, int apagada, float a) {
  (void)sel;
  botao_pilula(r, s, ic, foc, 0, 0, a * (apagada ? .45f : 1));
}

// 3D object badge of one achievement (family x tier art). Returns 0 until the
// texture is loaded, so the caller keeps the flat emblem as fallback.
static char dirArteCq[256];
void conquistas_arte_dir(const char *d) { snprintf(dirArteCq, sizeof dirArteCq, "%s", d && d[0] ? d : "."); }

static void tinta_de(const CqEntrada *e, float *r, float *g, float *b) {
  CqTinta t = cqarte_tinta(e->chave, e->familia, e->nivel);
  *r = t.r / 255.0f; *g = t.g / 255.0f; *b = t.b / 255.0f;
}

static int objeto3d(GfxRect q, const CqEntrada *e, float a) {
  char f[48], p[320];
  GLuint t;
  if (!cqarte_arquivo(e->chave, e->familia, e->nivel, f, sizeof f)) return 0;
  snprintf(p, sizeof p, "%s/conquistas/%s", dirArteCq, f);
  t = tex_obter_larg(p, q.w);
  if (!t) return 0;
  gfx_rect(q, t, GFX_TEXTO, 0, 0, 0, 0.0f, 1, 1, 1, a);
  return 1;
}

static void cadeado(GfxRect q, float a) {
  gfx_icone((GfxRect){q.x + q.w * .27f, q.y + q.h * .27f, q.w * .46f, q.h * .46f}, "aj_lock", .72f, .74f, .80f, a);
}

// estado: 0 ganho (cor da familia), 1 em andamento (apagado), 2 secreto (escuro, cadeado).
static void emblema(float x, float y, float lado, int fam, int estado, float a) {
  float r, g, b;
  GfxRect q = { x, y, lado, lado };
  cor_hex(COR_FAM[fam], &r, &g, &b);
  if (estado == 2) {
    gfx_cor(q, 0.32f, 0.106f, 0.11f, 0.129f, a);
    cadeado(q, a);
    return;
  }
  if (estado == 1) { r = r * 0.45f + 0.20f * 0.55f; g = g * 0.45f + 0.21f * 0.55f; b = b * 0.45f + 0.24f * 0.55f; }
  gfx_cor(q, 0.32f, r, g, b, a);
  gfx_icone((GfxRect){ x + lado * 0.27f, y + lado * 0.27f, lado * 0.46f, lado * 0.46f }, ICO_FAM[fam],
            1, 1, 1, (estado ? 0.65f : 1.0f) * a);
}

static void emblema_e(float x, float y, float lado, const CqEntrada *e, float a) {
  if (!objeto3d((GfxRect){ x, y, lado, lado }, e, a)) emblema(x, y, lado, e->familia, e->nivel >= 1 ? 0 : 1, a);
}

static const char *nome_nivel_arte(int nivel) {
  return i18n(nivel <= 0 ? "A conquistar" : nivel == 1 ? "Bronze" : nivel <= 3 ? "Prata" : "Ouro");
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
  texto(TXT_V2_KICK, kick, MG + ox, ANC(108.0f) + oy, 900.0f, MUDO, a);
  texto(TXT_NOV_TITULO, titulo, MG + ox, ANC(134.0f) + oy, 1000.0f, PRI, a);
  if (sub) texto_dir(TXT_V2_24, sub, ox + MG + largura, ANC(162.0f) + oy, 900.0f, SEC, a);
}

static void kicker_trilha(char *dst, size_t n, const char *terceiro) {
  if (terceiro) snprintf(dst, n, "%s › %s › %s", i18n("Perfil"), i18n("Conquistas"), terceiro);
  else snprintf(dst, n, "%s › %s", i18n("Perfil"), i18n("Conquistas"));
}

static void desenha_card(GfxRect r, const CqEntrada *e, float foc, float a) {
  Tinta p, s, mudo;
  char b[96];
  float x = r.x + 124.0f, w = CARD_W - 124.0f - 20.0f, fr, fg, fb, barW = CARD_W - 40.0f, f;
  int i;
  superficie(r, T21_R_CARD, foc, a, &p, &s);
  mudo = MUDO;
  float lado = 104 * (1 + .06f * foc);
  tinta_de(e, &fr, &fg, &fb);
  // Focus surface tinted by the achievement accent (no outline, no glow).
  gfx_cor(r, T21_R_CARD, fr, fg, fb, .12f * foc * a);
  emblema_e(r.x + 64 - lado / 2, r.y + 64 - lado / 2, lado, e, a);
  texto(TXT_V2_KICK, nome_nivel_arte(e->nivel), x, r.y + 16.0f, w, (Tinta){ (int)(fr * 255), (int)(fg * 255), (int)(fb * 255) }, a);
  bloco(TXT_ILHA_NOME, e->nome, p, x, r.y + 42.0f, w, 28.0f, a, 1);
  if (e->total > 0) snprintf(b, sizeof b, i18n("Nível %d de 5 · %d de %d"), e->nivel, e->vistos, e->total);
  else snprintf(b, sizeof b, i18n("Nível %d de 5"), e->nivel);
  texto(TXT_V2_18, b, x, r.y + 84.0f, w, s, a);
  // Barra de progresso e os cinco pontos de nivel.
  f = e->total > 0 ? (float)e->vistos / (float)e->total : 0.0f;
  if (f > 1.0f) f = 1.0f;
  plrui_trilho((GfxRect){r.x + 20, r.y + 126, barW, 5}, f, fr, fg, fb, a);
  for (i = 0; i < CQ_NIVEIS; i++) {
    GfxRect q = {r.x + 20 + i * 18, r.y + 146, 12, 12};
    float nivelA = ajustes_animacoes_reduzidas() ? 1 : anim_atraso(tempo, i, T21_STAGGER_MS, T21_ENTER_MS);
    plrui_trilho(q, i < e->nivel ? nivelA : 0, fr, fg, fb, a);
  }
  if (raridade_txt(e->chave, b, sizeof b)) texto_dir(TXT_V2_18, b, r.x + CARD_W - 20.0f, r.y + 138.0f, 200.0f, mudo, a);
}

static void painel(GfxRect r, float a) { t21_ilha(r, T21_R_SHEET, a); }

static void botao_breve(GfxRect r, const char *s, const char *ic, float foc, float a) {
  pilula(r, s, ic, foc, 0, !foc, a);
}

static void desenha_vazio(float a) {
  GfxRect r = em(MG, GRADE_Y, gradeW, fminf(550.0f, ANC(920.0f) - GRADE_Y));
  const char *tit, *corpo;
  float cx = r.x + r.w * 0.5f, w = fminf(760.0f, r.w - 80.0f);
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

static float infoK(void) { return fminf(1, (ANC(920.0f) - GRADE_Y) / 550.0f); }

static void desenha_retomar(float a) {
  float k = infoK();
  GfxRect p = em(COL_X, GRADE_Y, COL_W, 270.0f * k);
  int c = cand_retomar();
  char b[64];
  painel(p, a);
  texto(TXT_V2_KICK, i18n("Retomar"), p.x + 24.0f, p.y + 22.0f * k, COL_W - 48.0f, MUDO, a);
  if (c < 0) {
    bloco(TXT_V2_24, "Nada para retomar ainda. Quando você começar uma trilha, ela aparece aqui.", SEC,
          p.x + 24.0f, p.y + 66.0f * k, COL_W - 48.0f, 32.0f, a, 5);
    return;
  }
  emblema_e(p.x + 24.0f, p.y + 60.0f * k, 96.0f, &st.e[c], a);
  bloco(TXT_ILHA_NOME, st.e[c].nome, PRI, p.x + 136.0f, p.y + 62.0f * k, COL_W - 160.0f, 28.0f, a, 2);
  snprintf(b, sizeof b, i18n("%d de %d filmes"), st.e[c].vistos, st.e[c].total);
  texto(TXT_V2_18, b, p.x + 136.0f, p.y + 128.0f * k, COL_W - 160.0f, SEC, a);
  botao_breve(r_retomar_btn(), "Continuar a trilha", "aj_play", foco == B_RETOMAR && retomar_ligado(), a);
}

static void desenha_mapa(float a) {
  float k = infoK();
  GfxRect p = em(COL_X, GRADE_Y + 290.0f * k, COL_W, 260.0f * k);
  CqMapa m;
  char b[24];
  int i;
  const char *rot[3] = { "décadas", "países", "idiomas" };
  int val[3];
  cqd_mapa(&m);
  val[0] = m.decadas; val[1] = m.paises; val[2] = m.idiomas;
  if (!m.ok) return; // No empty statistics until the server supplies real data.
  painel(p, a);
  texto(TXT_V2_KICK, i18n("Seu mapa de descobertas"), p.x + 24.0f, p.y + 22.0f * k, COL_W - 48.0f, MUDO, a);
  for (i = 0; i < 10; i++)
    gfx_cor((GfxRect){ p.x + 24.0f + (float)i * 36.0f, p.y + 60.0f * k, 30.0f, 30.0f }, 0.25f, 1, 1, 1,
            (m.ok && m.decada[i] ? 0.55f : 0.08f) * a);
  texto(TXT_V2_18, "1920", p.x + 24.0f, p.y + 96.0f * k, 100.0f, MUDO, a);
  texto_dir(TXT_V2_18, "2020", p.x + COL_W - 26.0f, p.y + 96.0f * k, 100.0f, MUDO, a);
  for (i = 0; i < 3; i++) {
    float x = p.x + 24.0f + (float)i * 120.0f;
    if (m.ok) snprintf(b, sizeof b, "%d", val[i]); else snprintf(b, sizeof b, "—");
    texto(TXT_ILHA_TITULO, b, x, p.y + 126.0f * k, 110.0f, PRI, a);
    texto(TXT_V2_18, rot[i], x, p.y + 178.0f * k, 112.0f, SEC, a);
  }
  texto(TXT_V2_18, "Em breve, com o nosso servidor.", p.x + 24.0f, p.y + 218.0f * k, COL_W - 48.0f, AMBAR, a);
}

static void desenha_hub(float a) {
  char k[96], sub[96];
  int i, linhas, ganhas = 0, anda = 0;
  Item it[MAX_ITENS];
  int n = montar(it);
  for (i = 0; i < st.n; i++) { ganhas += ganha(&st.e[i]); anda += andando(&st.e[i]); }
  kicker_trilha(k, sizeof k, famF >= 0 ? FAM[famF].nome : NULL);
  snprintf(sub, sizeof sub, i18n("Emblemas: %d · Em andamento: %d"), ganhas, anda);
  if (!ajustes_conquistas()) {
    GfxRect r = em(MG, GRADE_Y, largura, 300.0f);
    cabecalho(a, k, i18n("Conquistas"), NULL);
    painel(r, a);
    texto(TXT_ILHA_TITULO, i18n("Conquistas desligadas"), r.x + 40.0f, r.y + 48.0f, r.w - 80.0f, PRI, a);
    bloco(TXT_V2_26, "Ligue em Ajustes › Conquistas para ver seus emblemas. Trilhas e listas funcionam do mesmo jeito.",
          SEC, r.x + 40.0f, r.y + 118.0f, 960.0f, 34.0f, a, 4);
  } else {
    cabecalho(a, k, famF >= 0 ? i18n(FAM[famF].nome) : i18n("Conquistas"), sub);
    int counts[A_N]; const char *labels[A_N];
    for (i = 0; i < A_N; i++) { labels[i] = i18n(tabs[i]); counts[i] = i ? contar(i) : -1; }
    plrui_seg(labels, counts, A_N, tabSel, tabFoco, ox + MG, TABS_Y, a);
    pilula(r_ordem(), i18n("Mais perto de completar"), "aj_funnel", qf(B_ORDEM), ordem, 0, a);
    pilula(r_fams(), i18n("O que dá emblema"), "aj_folders", qf(B_FAMS), 0, 0, a);
    if (nvis == 0) desenha_vazio(a);
    else {
      gfx_recorte(ox + MG - 8, GRADE_Y - 8, gradeW + 16, PASSO_Y * LINHAS_VISIVEIS);
      for (i = 0; i < nvis; i++) {
        GfxRect r = r_card(i);
        r.y += topo * PASSO_Y - scrollY;
        if (r.y + r.h < GRADE_Y || r.y > GRADE_Y + PASSO_Y * LINHAS_VISIVEIS) continue;
        float e = ajustes_animacoes_reduzidas() ? 1 : revela_entra(1,
          revela_onda_atraso(i % colunas, i / colunas - topo), 1 + (Uint32)(tempo * 1000));
        r.y += (1 - e) * NV_ENTRA_DY;
        desenha_card(r, &st.e[vis[i]], qf(C_CARD0 + i), a * e);
      }
      gfx_sem_recorte();
    }
    linhas = (nvis + colunas - 1) / colunas;
    if (linhas > LINHAS_VISIVEIS) {
      float alt = PASSO_Y * (float)LINHAS_VISIVEIS - 20.0f, th = alt * (float)LINHAS_VISIVEIS / (float)linhas;
      float ty = alt * (float)topo / (float)linhas;
      gfx_cor(em(COL_X - 22.0f, GRADE_Y, 6.0f, alt), 0.5f, 1, 1, 1, 0.08f * a);
      gfx_cor(em(COL_X - 22.0f, GRADE_Y + ty, 6.0f, th), 0.5f, 1, 1, 1, 0.45f * a);
    }
    desenha_retomar(a);
    desenha_mapa(a);
    bloco(TXT_V2_18, "Emblemas premiam o que você descobriu, não quanto tempo ficou na frente da tela. Sem prazo, sem sequência para manter. Tudo isso se desliga em Ajustes.",
          MUDO, ox + MG, oy + ANC(930.0f), largura, 26.0f, a, 2);
  }
  pilula(r_voltar(), i18n("Voltar"), NULL, qf(B_VOLTAR), 0, 0, a);
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
    float e = ajustes_animacoes_reduzidas() ? 1 : anim_atraso(tempo, i, T21_STAGGER_MS, T21_ENTER_MS);
    r.y += (1 - e) * NV_ENTRA_DY;
    superficie(r, T21_R_CARD, qf(F_CARD0 + i), a * e, &p, &s);
    mudo = foc ? s : MUDO;
    emblema(r.x + 24.0f, r.y + 22.0f, 64.0f, i, 0, a);
    texto(TXT_ILHA_NOME, FAM[i].nome, r.x + 104.0f, r.y + 22.0f, r.w - 128.0f, p, a);
    texto(TXT_V2_18, FAM[i].lema, r.x + 104.0f, r.y + 56.0f, r.w - 128.0f, mudo, a);
    gfx_cor((GfxRect){ r.x + 24.0f, r.y + 100.0f, r.w - 48.0f, 1.0f }, 0, 1, 1, 1, 0.10f * a);
    for (j = 0; j < 3; j++) {
      const FamLinha *l = &FAM[i].l[j];
      float y = r.y + 112.0f + (float)j * fminf(72.0f, (r.h - 168.0f) / 2.0f), tw = r.w - 24.0f - 48.0f - 14.0f - 24.0f;
      emblema(r.x + 24.0f, y + 8.0f, 48.0f, i, 1, a);
      texto(TXT_ILHA_SUB, l->nome, r.x + 86.0f, y + 4.0f, l->breve ? tw - 96.0f : tw, p, a);
      if (l->breve) texto_dir(TXT_V2_18, i18n("Em breve"), r.x + r.w - 24.0f, y + 6.0f, 96.0f, foc ? s : AMBAR, a);
      texto(TXT_V2_18, l->desc, r.x + 86.0f, y + fminf(36.0f, (r.h - 168.0f) / 2.0f - 25.0f), tw, s, a);
    }
  }
  texto(TXT_V2_18, "Fora, por princípio: sequência de dias, horas assistidas, maratonas.", ox + MG, oy + ANC(1004.0f), largura, MUDO, a);
  pilula(r_voltar(), i18n("Voltar"), NULL, qf(B_VOLTAR), 0, 0, a);
  for (i = 0; i < n; i++) alvo(it[i].r, it[i].id);
}

static void desenha_detalhe(float a) {
  const CqEntrada *e;
  char k[96], b[96];
  int i, n, nivel;
  Item it[MAX_ITENS];
  float hh, y;
  float colunaK = fminf(1, largura / 1624.0f);
  if (detIdx < 0 || detIdx >= st.n) return;
  e = &st.e[detIdx];
  nivel = e->nivel;
  n = montar(it);
  kicker_trilha(k, sizeof k, i18n(FAM[e->familia].nome));
  texto(TXT_V2_KICK, k, MG + ox, ANC(108.0f) + oy, 900.0f, MUDO, a);
  // Coluna da esquerda: emblema, nome e estado.
  emblema_e(ox + MG, oy + ANC(164.0f), 230.0f * colunaK, e, a);
  hh = bloco(TXT_NOV_TITULO, e->nome, PRI, ox + MG, oy + ANC(418.0f), 420.0f * colunaK, 58.0f, a, 2);
  y = oy + ANC(418.0f) + hh + 12.0f;
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
  botao_breve(em(MG, 700.0f + 64.0f * gfx_escala_tela_txt(), 420.0f, PILULA_H), "Fixar no perfil", "aj_bookmark", 0, a);
  // Meio: os cinco niveis.
  for (i = 0; i < CQ_NIVEIS; i++) {
    GfxRect r = em(MG + 460.0f, 164.0f + (float)i * 122.0f * gfx_escala_tela_txt(), 700.0f, 108.0f);
    int lv = i + 1, feito = lv <= nivel;
    float fr, fg, fb;
    const char *regra, *estado;
    char rb[64], eb[48];
    tinta_de(e, &fr, &fg, &fb);
    t21_ilha(r, T21_R_ROW, a);
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
    GfxRect p1 = em(MG + 1200.0f, 164.0f, 424.0f, 230.0f), p2 = em(MG + 1200.0f, 164.0f + 250.0f * gfx_escala_tela_txt(), 424.0f, 346.0f);
    int am = cqd_amigos_na_trilha(e->chave);
    painel(p1, a);
    texto(TXT_V2_KICK, i18n("Amigos nesta trilha"), p1.x + 24.0f, p1.y + 22.0f, p1.w - 48.0f, MUDO, a);
    if (am > 0) { snprintf(b, sizeof b, i18n("%d amigos nesta trilha"), am); texto(TXT_V2_26, b, p1.x + 24.0f, p1.y + 66.0f, p1.w - 48.0f, PRI, a); }
    else {
      bloco(TXT_V2_18, "Quantos amigos fazem esta trilha, sem placar e sem aviso para ninguém.", SEC, p1.x + 24.0f, p1.y + 66.0f, p1.w - 48.0f, 26.0f, a, 4);
      texto(TXT_V2_18, i18n("Em breve"), p1.x + 24.0f, p1.y + 176.0f, 200.0f, AMBAR, a);
    }
    painel(p2, a);
    texto(TXT_V2_KICK, i18n("Como conta"), p2.x + 24.0f, p2.y + 22.0f, p2.w - 48.0f, MUDO, a);
    hh = bloco(TXT_V2_18, "Cada filme vira passo quando está no histórico do Trakt ou você marca Já vi. O app avisa uma vez, no fim da sessão.",
               SEC, p2.x + 24.0f, p2.y + 62.0f, p2.w - 48.0f, 26.0f, a, 6);
    bloco(TXT_V2_18, "Filme que você já viu antes de abrir a trilha conta para os níveis 1 a 4.", SEC,
          p2.x + 24.0f, p2.y + 62.0f + hh + 16.0f, p2.w - 48.0f, 26.0f, a, 4);
  }
  pilula(r_voltar(), i18n("Voltar"), NULL, qf(B_VOLTAR), 0, 0, a);
  for (i = 0; i < n; i++) alvo(it[i].r, it[i].id);
}

void conquistas_desenhar(Uint32 agora) {
  float a = anim_suave(entrada);
  (void)agora;
  if (entrada < 0.002f) return;
  TXT_INI();
  origem();
  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 0.051f, 0.051f, 0.051f, entrada);
  if (view == V_FAM) desenha_familias(a);
  else if (view == V_DET) desenha_detalhe(a);
  else desenha_hub(a);
  TXT_FIM();
}

void conquistas_evento(const SDL_Event *e) { TXT_INI(); conquistas_evento_(e); TXT_FIM(); }

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
  if (vistaAnim != view) { vistaAnim = view; tempo = 0; }
  tempo += dt;
  for (int i = 0; i < C_CARD0 + CQ_MAX; i++)
    focos[i] = red ? (aberto && foco == i) : anim_mola(focos[i], aberto && foco == i, dt, NV_MOLA_FOCO);
  scrollY = anim_mola2_reduzida(&scrollV, scrollY, topo * PASSO_Y, dt, NV_MOLA2_SCROLL, red);
  tabSel = anim_mola(tabSel, foco >= T_ABA0 && foco < T_ABA0 + A_N ? foco - T_ABA0 : aba, dt, NV_MOLA_FOCO);
  tabFoco = anim_mola(tabFoco, foco >= T_ABA0 && foco < T_ABA0 + A_N, dt, NV_MOLA_FOCO);
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
  t21_ilha(r, T21_R_SHEET, a);
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
