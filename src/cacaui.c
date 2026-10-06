// Tela Caca. Ver cacaui.h.
#include "cacaui.h"
#include "caca.h"
#include "conquista.h"
#include "ajustes.h"
#include "anim.h"
#include "gfx.h"
#include "idioma.h"
#include "layout.h"
#include "plrui.h"
#include "svdesenho.h"
#include "tex_cache.h"
#include "text.h"
#include <stdio.h>
#include <string.h>

// Medidas (1920x1080). Pôster w185 do TMDB desenhado a 118x177: 2 linhas
// visiveis, no maximo ~20 texturas na tela (plano 15: <= 40 w185).
#define CU_TOPO      52.0f
#define CU_SUG_Y     138.0f
#define CU_CORPO_Y   236.0f
#define CU_LISTA_W   452.0f
#define CU_LIN_H     96.0f
#define CU_LIN_GAP   10.0f
#define CU_LIN_VIS   7
#define CU_PW        110.0f
#define CU_PH        165.0f
#define CU_PGAP      16.0f
#define CU_PLIN      2
#define CU_PAINEL_H  776.0f
#define CU_PAD       32.0f
#define CU_SUG_MAX   3
#define CU_BOT_MAX   6

enum { Z_SUG = 0, Z_LISTA, Z_POST, Z_BOT };
enum { B_FILEIRA = 0, B_COLECAO, B_DOC, B_CURTA, B_AMIGOS, B_PARAR };

static int zona, sel, lista0, pCol, pLin, pLin0, bot, sug;
static int sair, temPedido, pediuBusca;
static char pedido[24];
static float entrada, fLista[CACA_MAX], fPost[64], fBot[CU_BOT_MAX], fSug[CU_SUG_MAX + 2];
// O seletor de trilhas (modal).
static int trilhasAberto, tSel, tIni;
static float fTrilhas;

static float colunasPost(void);

static int nSug(int *idx) {
  int v[CU_SUG_MAX];
  int n = caca_sugestoes(v, CU_SUG_MAX), i;
  if (idx) for (i = 0; i < n; i++) idx[i] = v[i];
  return n;
}
static int nPills(void) { return nSug(NULL) + 2; }

static int ehPessoa(const CacaInfo *c) { return c->tipo == CACA_DIRECAO || c->tipo == CACA_ATUACAO; }

// Os botoes desta cacada, na ordem da tela. Devolve quantos.
static int botoes(int *ids) {
  CacaInfo c;
  int n = 0;
  if (!caca_info(sel, &c)) return 0;
  ids[n++] = B_FILEIRA; ids[n++] = B_COLECAO;
  if (ehPessoa(&c)) { ids[n++] = B_DOC; ids[n++] = B_CURTA; }
  ids[n++] = B_AMIGOS; ids[n++] = B_PARAR;
  return n;
}

static void rotuloBotao(int id, const CacaInfo *c, const char **rot, const char **ico) {
  switch (id) {
    case B_FILEIRA: *rot = (c->flags & CACA_F_FILEIRA) ? "Tirar da Home" : "Fazer fileira"; *ico = "aj_rows-3"; break;
    case B_COLECAO: *rot = (c->flags & CACA_F_COLECAO) ? "Desfazer coleção" : "Fazer coleção"; *ico = "aj_folders"; break;
    case B_DOC:     *rot = (c->escopo & CACA_ESC_DOC) ? "Sem documentários" : "+ Documentários"; *ico = "aj_film"; break;
    case B_CURTA:   *rot = (c->escopo & CACA_ESC_CURTA) ? "Sem curtas" : "+ Curtas"; *ico = "aj_clapperboard"; break;
    case B_AMIGOS:  *rot = (c->flags & CACA_F_PRIVADA) ? "Só eu vejo" : "Amigos veem"; *ico = (c->flags & CACA_F_PRIVADA) ? "aj_eye-off" : "aj_users"; break;
    default:        *rot = "Parar"; *ico = "aj_x"; break;
  }
}

void cacaui_abrir(int foco) {
  zona = caca_n() > 0 ? Z_LISTA : Z_SUG;
  sel = foco >= 0 && foco < caca_n() ? foco : 0;
  lista0 = sel >= CU_LIN_VIS ? sel - CU_LIN_VIS + 1 : 0;
  pCol = pLin = pLin0 = bot = sug = 0;
  sair = temPedido = pediuBusca = 0;
  trilhasAberto = 0;
  entrada = 0.0f;
  memset(fLista, 0, sizeof fLista); memset(fPost, 0, sizeof fPost);
  memset(fBot, 0, sizeof fBot); memset(fSug, 0, sizeof fSug);
  caca_n_trilhas();   // ja pede o catalogo, para o seletor abrir cheio
}

int cacaui_quer_sair(void) { int v = sair; sair = 0; return v; }
int cacaui_pediu_titulo(char *imdb, size_t tam) {
  if (!temPedido) return 0;
  temPedido = 0;
  if (imdb && tam) snprintf(imdb, tam, "%s", pedido);
  return 1;
}
int cacaui_pediu_busca(void) { int v = pediuBusca; pediuBusca = 0; return v; }

static int nPost(void) { return caca_n_filmes(sel); }

static void eventoTrilhas(SDL_Keycode k, int ok, int voltar) {
  int n = caca_n_trilhas();
  if (voltar) { trilhasAberto = 0; return; }
  if (k == SDLK_UP && tSel > 0) tSel--;
  else if (k == SDLK_DOWN && tSel + 1 < n) tSel++;
  else if (ok && n > 0) {
    CacaTrilha t;
    if (caca_trilha(tSel, &t)) {
      int i = caca_criar(t.tipo, t.alvo, t.lista, t.nome, t.foto);
      if (i >= 0) { sel = i; zona = Z_LISTA; }
    }
    trilhasAberto = 0;
  }
  if (tSel < tIni) tIni = tSel;
  if (tSel >= tIni + 9) tIni = tSel - 8;
}

void cacaui_evento(const SDL_Event *e) {
  SDL_Keycode k;
  int ok, voltar, cols = (int)colunasPost(), n;
  if (!e || e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  ok = k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE;
  voltar = k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
           e->key.keysym.scancode == NV_SCANCODE_BACK;
  if (trilhasAberto) { eventoTrilhas(k, ok, voltar); return; }
  if (voltar) { sair = 1; return; }
  if (caca_n() == 0 && zona != Z_SUG) zona = Z_SUG;
  switch (zona) {
    case Z_SUG:
      n = nPills();
      if (k == SDLK_LEFT) { if (sug > 0) sug--; else sair = 1; }
      else if (k == SDLK_RIGHT && sug + 1 < n) sug++;
      else if (k == SDLK_DOWN && caca_n() > 0) zona = Z_LISTA;
      else if (ok) {
        int idx[CU_SUG_MAX], ns = nSug(idx);
        if (sug < ns) { sel = idx[sug]; zona = Z_POST; pCol = pLin = pLin0 = 0; }
        else if (sug == ns) pediuBusca = 1;
        else { trilhasAberto = 1; tSel = tIni = 0; fTrilhas = 0.0f; }
      }
      break;
    case Z_LISTA:
      if (k == SDLK_UP) { if (sel > 0) sel--; else zona = Z_SUG; pCol = pLin = pLin0 = 0; }
      else if (k == SDLK_DOWN && sel + 1 < caca_n()) { sel++; pCol = pLin = pLin0 = 0; }
      else if (k == SDLK_LEFT) sair = 1;
      else if ((k == SDLK_RIGHT || ok) && nPost() > 0) zona = Z_POST;
      else if (k == SDLK_RIGHT || ok) zona = Z_BOT;
      break;
    case Z_POST: {
      int i = pLin * cols + pCol;
      n = nPost();
      if (k == SDLK_LEFT) { if (pCol > 0) pCol--; else zona = Z_LISTA; }
      else if (k == SDLK_RIGHT && pCol + 1 < cols && i + 1 < n) pCol++;
      else if (k == SDLK_UP) { if (pLin > 0) pLin--; else zona = Z_SUG; }
      else if (k == SDLK_DOWN) {
        if ((pLin + 1) * cols < n) { pLin++; if (pLin * cols + pCol >= n) pCol = n - 1 - pLin * cols; }
        else { zona = Z_BOT; bot = 0; }
      } else if (ok && i < n) {
        CacaFilme f;
        if (caca_filme(sel, i, &f) && f.imdb[0]) { snprintf(pedido, sizeof pedido, "%s", f.imdb); temPedido = 1; }
      }
      if (pLin < pLin0) pLin0 = pLin;
      if (pLin >= pLin0 + CU_PLIN) pLin0 = pLin - CU_PLIN + 1;
      break; }
    case Z_BOT: {
      int ids[CU_BOT_MAX], nb = botoes(ids);
      if (k == SDLK_LEFT) { if (bot > 0) bot--; else zona = Z_LISTA; }
      else if (k == SDLK_RIGHT && bot + 1 < nb) bot++;
      else if (k == SDLK_UP) zona = nPost() > 0 ? Z_POST : Z_LISTA;
      else if (ok && bot < nb) {
        switch (ids[bot]) {
          case B_FILEIRA: caca_alternar(sel, CACA_F_FILEIRA); break;
          case B_COLECAO: caca_alternar(sel, CACA_F_COLECAO); break;
          case B_DOC:     caca_alternar_escopo(sel, CACA_ESC_DOC); break;
          case B_CURTA:   caca_alternar_escopo(sel, CACA_ESC_CURTA); break;
          case B_AMIGOS:  caca_alternar(sel, CACA_F_PRIVADA); break;
          default:
            caca_parar(sel);
            if (sel >= caca_n()) sel = caca_n() - 1;
            if (sel < 0) sel = 0;
            zona = caca_n() > 0 ? Z_LISTA : Z_SUG;
            bot = 0;
            break;
        }
      }
      break; }
  }
  if (sel < lista0) lista0 = sel;
  if (sel >= lista0 + CU_LIN_VIS) lista0 = sel - CU_LIN_VIS + 1;
}

void cacaui_atualizar(float dt, Uint32 agora) {
  int red = ajustes_animacoes_reduzidas(), i, cols = (int)colunasPost();
  (void)agora;
  entrada = red ? 1.0f : anim_mola(entrada, 1.0f, dt, NV_MOLA_TELA);
#define MOLA(v, alvo) v = red ? (alvo) : anim_mola(v, (alvo), dt, NV_MOLA_FOCO)
  for (i = 0; i < CACA_MAX; i++) MOLA(fLista[i], (zona == Z_LISTA && i == sel && !trilhasAberto) ? 1.0f : 0.0f);
  for (i = 0; i < 64; i++) MOLA(fPost[i], (zona == Z_POST && i == pLin * cols + pCol && !trilhasAberto) ? 1.0f : 0.0f);
  for (i = 0; i < CU_BOT_MAX; i++) MOLA(fBot[i], (zona == Z_BOT && i == bot && !trilhasAberto) ? 1.0f : 0.0f);
  for (i = 0; i < CU_SUG_MAX + 2; i++) MOLA(fSug[i], (zona == Z_SUG && i == sug && !trilhasAberto) ? 1.0f : 0.0f);
  MOLA(fTrilhas, trilhasAberto ? 1.0f : 0.0f);
#undef MOLA
  if (caca_n() > 0 && sel >= caca_n()) sel = caca_n() - 1;
}

// --- desenho -----------------------------------------------------------------
static float xEsq(void) { return 96.0f + ajustes_rail_largura_fixa(); }
static float xDir(void) { return xEsq() + CU_LISTA_W + 36.0f; }
static float wDir(void) { return NV_TELA_W - 64.0f - xDir(); }
static float colunasPost(void) {
  float w = wDir() - 2.0f * CU_PAD + CU_PGAP;
  int c = (int)(w / (CU_PW + CU_PGAP));
  return (float)(c < 1 ? 1 : c > 32 ? 32 : c);
}

static void texto(TxtEstilo e, const char *s, int c, float x, float y, float a) {
  TxtLinha l = txt_linha(e, s, c, c, c, 255);
  txt_desenhar_alpha(l, x, y, a);
}
static float textoCorta(TxtEstilo e, const char *s, int r, int g, int b, float x, float y, float w, float a) {
  TxtLinha l = txt_linha_corta(e, s, r, g, b, 255, w);
  txt_desenhar_alpha(l, x, y, a);
  return (float)l.w;
}

static void rosto(GfxRect r, const char *url, const char *nome, float a) {
  GLuint t = url && url[0] ? tex_obter_larg(url, r.w) : 0;
  if (t) {
    gfx_tex_aspect_atual = tex_aspecto(url);
    gfx_rect(r, t, GFX_CARD, 0, 0, 0, 0.5f, 0, 0, 0, a);
    gfx_tex_aspect_atual = 0.0f;
  } else {
    char ini[8] = "?";
    float ar, ag, ab;
    ajustes_acento(&ar, &ag, &ab);
    gfx_cor(r, 0.5f, ar * 0.55f, ag * 0.55f, ab * 0.55f, a);
    if (nome && nome[0]) {
      int n = 1;
      while (nome[n] && ((unsigned char)nome[n] & 0xC0) == 0x80) n++;
      snprintf(ini, sizeof ini, "%.*s", n, nome);
    }
    { TxtLinha l = txt_linha(TXT_ILHA_INICIAL, ini, 245, 245, 250, 255);
      txt_desenhar_alpha(l, r.x + (r.w - (float)l.w) * 0.5f, r.y + (r.h - (float)l.h) * 0.5f, a); }
  }
}

static void desenharSugestoes(float a) {
  int idx[CU_SUG_MAX], ns = nSug(idx), i;
  float x = xEsq();
  char buf[160];
  for (i = 0; i < ns + 2; i++) {
    const char *rot, *ico;
    if (i < ns) {
      CacaInfo c;
      caca_info(idx[i], &c);
      snprintf(buf, sizeof buf, i18n("%s · faltam %d"), c.nome, c.total - c.vistos);
      rot = buf; ico = "aj_crosshair";
    } else if (i == ns) { rot = i18n("+ Caçar novo"); ico = "aj_search"; }
    else { rot = i18n("+ Nova trilha"); ico = "aj_list-video"; }
    x += plrui_botao(x, CU_SUG_Y, rot, ico, fSug[i], a) + 14.0f;
  }
}

static void desenharLista(float a) {
  float x = xEsq(), y = CU_CORPO_Y;
  int i, n = caca_n();
  char buf[64];
  plrui_kicker(i18n("Suas caçadas"), x, y - 34.0f, 158, 158, 168, a);
  for (i = lista0; i < n && i < lista0 + CU_LIN_VIS; i++) {
    CacaInfo c;
    GfxRect r = { x, y, CU_LISTA_W, CU_LIN_H };
    float f = fLista[i];
    float frac;
    if (!caca_info(i, &c)) continue;
    if (f > 0.01f) plrui_linha_foco(r, 18.0f, f * a);
    else if (i == sel) gfx_cor(r, 18.0f / r.h, 1, 1, 1, 0.06f * a);
    else plrui_material(r, 18.0f, 0, a * 0.85f);
    rosto((GfxRect){ x + 16.0f, y + 16.0f, 64.0f, 64.0f }, c.foto, c.nome, a);
    if (c.total > 0) snprintf(buf, sizeof buf, "%d/%d", c.vistos, c.total);
    else snprintf(buf, sizeof buf, "%s", c.estado == CACA_ERRO ? "!" : "\xe2\x80\xa6");
    { TxtLinha l = txt_linha(TXT_G20M, buf, 225, 225, 232, 255);
      float nw = CU_LISTA_W - 96.0f - 24.0f - (float)l.w - 12.0f;
      txt_desenhar_alpha(l, x + CU_LISTA_W - 22.0f - (float)l.w, y + 18.0f, a);
      textoCorta(TXT_ILHA_NOME, c.nome[0] ? c.nome : "\xe2\x80\xa6", 245, 245, 250, x + 96.0f, y + 16.0f, nw, a); }
    frac = c.total > 0 ? (float)c.vistos / (float)c.total : 0.0f;
    plrui_trilho((GfxRect){ x + 96.0f, y + 62.0f, CU_LISTA_W - 96.0f - 22.0f, 6.0f }, frac, -1, -1, -1, a);
    // O degrau mais alto ganho, pequeno, embaixo da barra.
    { int d = conq_maior(c.ganhos);
      if (d) {
        float ar, ag, ab;
        ajustes_acento(&ar, &ag, &ab);
        textoCorta(TXT_MINI, i18n(conq_rotulo(d)), (int)(ar * 255), (int)(ag * 255), (int)(ab * 255),
                   x + 96.0f, y + 70.0f, CU_LISTA_W - 130.0f, a);
      } }
    y += CU_LIN_H + CU_LIN_GAP;
  }
}

static const char *ICONE_DEGRAU[CONQ_N_DEGRAUS] = {
  "aj_circle", "aj_contrast", "aj_hexagon", "aj_star", "aj_ribbon"
};

static void desenharLegenda(const CacaInfo *c, float x, float y, float a) {
  float ar, ag, ab;
  int b;
  ajustes_acento(&ar, &ag, &ab);
  for (b = 0; b < CONQ_N_DEGRAUS; b++) {
    int ganho = (c->ganhos | c->degraus) & (1 << b);
    float k = ganho ? 1.0f : 0.38f;
    GfxRect ic = { x, y, 30.0f, 30.0f };
    if (ganho) gfx_icone(ic, ICONE_DEGRAU[b], ar, ag, ab, a);
    else gfx_icone(ic, ICONE_DEGRAU[b], 0.6f, 0.6f, 0.65f, a * 0.6f);
    { TxtLinha l = txt_linha(TXT_CAPTION2, conq_rotulo(1 << b), (int)(235 * k), (int)(235 * k), (int)(240 * k), 255);
      txt_desenhar_alpha(l, x + 38.0f, y + (30.0f - (float)l.h) * 0.5f, a);
      x += 38.0f + (float)l.w + 30.0f; }
  }
}

static void desenharVazio(float a) {
  // Sem cacada nenhuma nao ha lista a esquerda: o cartao ocupa a largura.
  float x = caca_n() > 0 ? xDir() : xEsq(), y = CU_CORPO_Y;
  float w = caca_n() > 0 ? wDir() : NV_TELA_W - 64.0f - xEsq();
  plrui_material((GfxRect){ x, y, w, 420.0f }, 28.0f, 0, a);
  gfx_icone((GfxRect){ x + CU_PAD, y + 40.0f, 64.0f, 64.0f }, "aj_crosshair", 1, 1, 1, a * 0.9f);
  texto(TXT_TITULO3, i18n("Escolha alguém para caçar"), 245, x + CU_PAD, y + 130.0f, a);
  txt_bloco_corta(TXT_CAPTION, i18n("Abra a página de uma diretora ou de um ator e aperte Caçar, ou comece uma trilha: sagas, os 250 mais bem avaliados, Oscar."),
                  190, 190, 200, x + CU_PAD, y + 196.0f, w - 2.0f * CU_PAD, 34.0f, a, 3);
  { CacaInfo z;
    memset(&z, 0, sizeof z);
    desenharLegenda(&z, x + CU_PAD, y + 330.0f, a); }
}

static void desenharDetalhe(Uint32 agora, float a) {
  CacaInfo c;
  float x = xDir(), y = CU_CORPO_Y, w = wDir(), cols = colunasPost();
  char buf[200], anos[40] = "";
  int n, i, foco = pLin * (int)cols + pCol;
  if (!caca_info(sel, &c)) { desenharVazio(a); return; }
  plrui_material((GfxRect){ x, y, w, CU_PAINEL_H }, 28.0f, 0, a);
  // Cabecalho: nome e x/y.
  snprintf(buf, sizeof buf, "%d/%d", c.vistos, c.total);
  { TxtLinha l = txt_linha(TXT_TITULO3, buf, 245, 245, 250, 255);
    if (c.total > 0) txt_desenhar_alpha(l, x + w - CU_PAD - (float)l.w, y + 26.0f, a);
    textoCorta(TXT_TITULO3, c.nome, 245, 245, 250, x + CU_PAD, y + 26.0f, w - 3.0f * CU_PAD - (float)l.w, a); }
  // "Voce viu 2 de 11 · faltam 9 · 2002–2024"
  if (c.anoIni > 0) snprintf(anos, sizeof anos, " \xc2\xb7 %ld\xe2\x80\x93%ld", c.anoIni, c.anoFim);
  if (c.total > 0 && c.vistos >= c.total)
    snprintf(buf, sizeof buf, i18n("Você viu todos os %d%s"), c.total, anos);
  else if (c.total > 0)
    snprintf(buf, sizeof buf, i18n("Você viu %d de %d · faltam %d%s"), c.vistos, c.total,
             c.total - c.vistos, anos);
  else buf[0] = 0;
  if (c.resolvendo > 0 && c.total > 0) {
    char t[220];
    snprintf(t, sizeof t, "%s \xc2\xb7 %s", buf, i18n("completando dados…"));
    snprintf(buf, sizeof buf, "%s", t);
  }
  textoCorta(TXT_CAPTION, buf, 190, 190, 200, x + CU_PAD, y + 82.0f, w - 2.0f * CU_PAD, a);
  plrui_trilho((GfxRect){ x + CU_PAD, y + 126.0f, w - 2.0f * CU_PAD, 6.0f },
               c.total > 0 ? (float)c.vistos / (float)c.total : 0.0f, -1, -1, -1, a);
  // Pôsteres, em ordem de lancamento (ou da historia, na trilha).
  n = caca_n_filmes(sel);
  { float py = y + 158.0f;
    if (n == 0) {
      const char *msg = c.estado == CACA_ERRO ? "Não foi possível carregar. Tente de novo mais tarde."
                                              : "Carregando filmografia…";
      if (c.estado != CACA_ERRO) plrui_anel(x + CU_PAD + 18.0f, py + 40.0f, 30.0f, 0, agora, a);
      texto(TXT_CAPTION, i18n(msg), 190, x + CU_PAD + (c.estado == CACA_ERRO ? 0.0f : 52.0f), py + 26.0f, a);
    }
    for (i = pLin0 * (int)cols; i < n && i < (pLin0 + CU_PLIN) * (int)cols; i++) {
      CacaFilme f;
      int lin = i / (int)cols - pLin0, col = i % (int)cols;
      GfxRect r = { x + CU_PAD + (float)col * (CU_PW + CU_PGAP),
                    py + (float)lin * (CU_PH + CU_PGAP), CU_PW, CU_PH };
      float ff = i < 64 ? fPost[i] : 0.0f;
      if (!caca_filme(sel, i, &f)) continue;
      if (ff > 0.01f) {
        float ar, ag, ab;
        GfxRect b = { r.x - NV_ANEL_FOCO, r.y - NV_ANEL_FOCO, r.w + 2 * NV_ANEL_FOCO, r.h + 2 * NV_ANEL_FOCO };
        ajustes_acento(&ar, &ag, &ab);
        if (ajustes_vidro()) gfx_vidro_cartao(r, NV_RAIO_CARD * r.w / r.h, ff, a);
        else gfx_cor(b, (NV_RAIO_CARD * r.w + NV_ANEL_FOCO) / b.h, ar, ag, ab, ff * a);
      }
      // O QUE FALTA fica apagado (o "cinza" do mockup): o proprio pôster a
      // um terco sobre o painel escuro, sem shader novo e sem segunda camada.
      // Em foco ele acende um pouco, para dar para ler a capa.
      { float pa = f.visto ? 1.0f : 0.34f + 0.36f * ff;
      if (f.poster[0]) svd_poster(r, f.poster, NV_RAIO_CARD * r.w / r.h, a * pa);
      else {
        gfx_cor(r, NV_RAIO_CARD * r.w / r.h, NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G, NV_COR_ESQUELETO_B, a);
        txt_bloco_corta(TXT_MINI, f.titulo, 200, 200, 208, r.x + 10.0f, r.y + 12.0f, r.w - 20.0f, 22.0f, a * pa, 5);
      } }
      // O visto ganha o selo de visto no canto.
      if (f.visto) {
        GfxRect s = { r.x + r.w - 34.0f, r.y + 8.0f, 26.0f, 26.0f };
        float ar, ag, ab;
        ajustes_acento(&ar, &ag, &ab);
        gfx_cor(s, 0.5f, ar, ag, ab, a);
        gfx_icone((GfxRect){ s.x + 5.0f, s.y + 5.0f, 16.0f, 16.0f }, "aj_check", 1, 1, 1, a);
      }
    }
    // A linha do pôster em foco: titulo, ano e o que o OK faz.
    if (zona == Z_POST && foco < n) {
      CacaFilme f;
      if (caca_filme(sel, foco, &f)) {
        if (f.data > 10000000)
          snprintf(buf, sizeof buf, "%s (%ld) \xc2\xb7 %s", f.titulo, f.data / 10000,
                   i18n(f.visto ? "Visto" : "Falta ver"));
        else
          snprintf(buf, sizeof buf, "%s \xc2\xb7 %s", f.titulo, i18n(f.visto ? "Visto" : "Falta ver"));
        textoCorta(TXT_CAPTION, buf, 225, 225, 232, x + CU_PAD, py + 2.0f * (CU_PH + CU_PGAP) + 4.0f,
                   w - 2.0f * CU_PAD, a);
      }
    } else if (n > (int)cols * CU_PLIN) {
      snprintf(buf, sizeof buf, i18n("%d filmes · role para ver todos"), n);
      textoCorta(TXT_CAPTION, buf, 150, 150, 160, x + CU_PAD, py + 2.0f * (CU_PH + CU_PGAP) + 4.0f,
                 w - 2.0f * CU_PAD, a);
    } }
  // Legenda dos degraus e os botoes. Os botoes quebram em duas linhas quando
  // nao cabem (Montserrat e larga); a legenda sobe junto.
  { int ids[CU_BOT_MAX], nb = botoes(ids), linhas = 1;
    float bx = x + CU_PAD, by, larg = 0.0f;
    const char *rot, *ico;
    for (i = 0; i < nb; i++) {
      float bw;
      rotuloBotao(ids[i], &c, &rot, &ico);
      bw = plrui_botao_largura(i18n(rot), ico);
      if (larg > 0.0f && larg + bw > w - 2.0f * CU_PAD) { linhas++; larg = 0.0f; }
      larg += bw + 12.0f;
    }
    by = y + CU_PAINEL_H - 28.0f - 60.0f * (float)linhas - 12.0f * (float)(linhas - 1);
    desenharLegenda(&c, x + CU_PAD, by - 58.0f, a);
    for (i = 0; i < nb; i++) {
      float bw;
      rotuloBotao(ids[i], &c, &rot, &ico);
      bw = plrui_botao_largura(i18n(rot), ico);
      if (bx > x + CU_PAD && bx + bw > x + w - CU_PAD) { bx = x + CU_PAD; by += 72.0f; }
      plrui_botao(bx, by, i18n(rot), ico, fBot[i], a);
      bx += bw + 12.0f;
    } }
}

static void desenharTrilhas(float a) {
  int n = caca_n_trilhas(), i;
  float w = 900.0f, h = 760.0f, x = (NV_TELA_W - w) * 0.5f, y = (1080.0f - h) * 0.5f;
  if (a <= 0.01f) return;
  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, 1080.0f }, 0, 0, 0, 0, 0.55f * a);
  plrui_material((GfxRect){ x, y, w, h }, 32.0f, 1, a);
  texto(TXT_TITULO3, i18n("Nova trilha"), 245, x + 40.0f, y + 32.0f, a);
  texto(TXT_CAPTION, i18n("Sagas, listas e curadoria. OK começa a caçada."), 170, x + 40.0f, y + 92.0f, a);
  if (n == 0) texto(TXT_CAPTION, i18n("Carregando…"), 190, x + 40.0f, y + 160.0f, a);
  for (i = tIni; i < n && i < tIni + 9; i++) {
    CacaTrilha t;
    GfxRect r = { x + 24.0f, y + 150.0f + (float)(i - tIni) * 64.0f, w - 48.0f, 56.0f };
    const char *tipo;
    if (!caca_trilha(i, &t)) continue;
    if (i == tSel) plrui_linha_foco(r, 16.0f, a);
    tipo = t.tipo == CACA_SAGA ? "Saga" : t.tipo == CACA_TRILHA ? "Curadoria" : "Lista";
    textoCorta(TXT_ILHA_NOME, t.nome, 240, 240, 246, r.x + 20.0f, r.y + 12.0f, r.w - 220.0f, a);
    { TxtLinha l = txt_linha(TXT_CAPTION2, i18n(tipo), 160, 160, 170, 255);
      txt_desenhar_alpha(l, r.x + r.w - 20.0f - (float)l.w, r.y + (r.h - (float)l.h) * 0.5f, a); }
    if (caca_achar(t.tipo, t.alvo, t.lista) >= 0)
      gfx_icone((GfxRect){ r.x + r.w - 170.0f, r.y + 14.0f, 28.0f, 28.0f }, "aj_check", 0.6f, 0.9f, 0.7f, a);
  }
}

void cacaui_desenhar(Uint32 agora) {
  float a = anim_suave(entrada);
  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, 1080.0f }, 0, NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, 1.0f);
  // A arte da cacada em foco atras, bem baixa: o "ambiente" (uma textura).
  { CacaInfo c;
    if (caca_info(sel, &c) && c.foto[0]) {
      GLuint t = tex_obter_larg(c.foto, 480.0f);
      if (t) {
        gfx_tex_aspect_atual = tex_aspecto(c.foto);
        gfx_rect((GfxRect){ 0, 0, NV_TELA_W, 1080.0f * 0.6f }, t, GFX_HERO, 0, 0, 0, 0, 0, 0, 0, 0.12f * a);
        gfx_tex_aspect_atual = 0.0f;
      }
    } }
  texto(TXT_TITULO2, i18n("Caça"), 245, xEsq(), CU_TOPO, a);
  desenharSugestoes(a);
  if (caca_n() > 0) { desenharLista(a); desenharDetalhe(agora, a); }
  else desenharVazio(a);
  // Atribuicao exigida pelos termos da API do TMDB.
  texto(TXT_MINI, i18n("Filmografias: TMDB. Este produto usa a API do TMDB, sem endosso ou certificação do TMDB."),
        120, xEsq(), 1080.0f - 40.0f, a * 0.9f);
  desenharTrilhas(anim_suave(fTrilhas) * a);
}
