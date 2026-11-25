// FAROLGOSTOUI — ver farolgostoui.h.
// Sem armazenamento por thread; zero log com titulo, perfil ou gosto.
#include "farolgostoui.h"
#include "ajustes.h"
#include "anim.h"
#include "botoes.h"
#include "farol.h"
#include "farolgosto.h"
#include "farolsinal.h"
#include "faroltexto.h"
#include "gfx.h"
#include "idioma.h"
#include "ilha.h"
#include "layout.h"
#include "perfis.h"
#include "perfilinfantil.h"
#include "perfisessao.h"
#include "plrui.h"
#include "ponteiro.h"
#include "tela21.h"
#include "text.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

// Ganchos fracos prontos para farolconsent (#53).
enum {
  FAROL_CONSENT_NAO_PERGUNTADO_VAL = 0,
  FAROL_CONSENT_SO_TV_VAL = 1,
  FAROL_CONSENT_SIM_VAL = 2
};
extern int  farolconsent_llm_ativo(void) __attribute__((weak));
extern int  farolconsent_estado(void) __attribute__((weak));
extern int  farolconsent_definir(int estado) __attribute__((weak));
extern void farolconsentui_abrir(void) __attribute__((weak));

enum { FGUI_FECHADO = 0, FGUI_APRENDEU = 1, FGUI_ZERAR = 2 };

static int modoAtual = FGUI_FECHADO;
static int querAberto;
static int foco;          // APRENDEU: 0=Voltar, 1=Secao uso/lista; ZERAR: 0=Zerar, 1=Cancelar
static int scrollIdx;     // APRENDEU: deslocamento vertical leve para listas
static float entradaProg;
static FarolGostoResumo resumoCache;
static FarolUsoContadores usoCache;
static int usoInfantil;
static int llmConsentLocal;
static int llmPerfil = -1;

static int aprMaxScroll(void) {
  return resumoCache.vazio ? 1 : 3;
}

static int backKey(const SDL_Event *e) {
  SDL_Keycode k = e->key.keysym.sym;
  return k == SDLK_ESCAPE || k == SDLK_BACKSPACE || k == SDLK_AC_BACK ||
         e->key.keysym.scancode == NV_SCANCODE_BACK;
}

static int okKey(SDL_Keycode k) {
  return k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE || k == SDLK_SELECT;
}

void farolgostoui_resumo(FarolGostoResumo *dst) {
  if (!dst) return;
  memset(dst, 0, sizeof *dst);
  dst->nGeneros = farol_gosto_top(FG_DIM_GENERO, dst->generos, FGUI_TOP_N);
  dst->nPessoas = farol_gosto_top(FG_DIM_PESSOA, dst->pessoas, FGUI_TOP_N);
  dst->nTemas   = farol_gosto_top(FG_DIM_TEMA, dst->temas, FGUI_TOP_N);
  dst->vazio = (dst->nGeneros == 0 && dst->nPessoas == 0 && dst->nTemas == 0);
}

void farolgostoui_abrir_aprendeu(void) {
  farolgostoui_resumo(&resumoCache);
  usoInfantil = perfil_infantil_ativo() ? 1 : 0;
  if (usoInfantil) memset(&usoCache, 0, sizeof usoCache);
  else farolsinal_uso_obter(&usoCache);
  modoAtual = FGUI_APRENDEU;
  querAberto = 1;
  scrollIdx = 0;
  foco = 1; // Inicia no topo da folha; DOWN alcanca Voltar (0) sem beco.
}

void farolgostoui_abrir_zerar(void) {
  if (perfil_ajustes_travados(perfis_ativo())) return;
  modoAtual = FGUI_ZERAR;
  querAberto = 1;
  foco = 1; // Inicia no botao seguro ("Cancelar"), nunca no destrutivo.
  scrollIdx = 0;
}

void farolgostoui_fechar(void) {
  querAberto = 0;
  modoAtual = FGUI_FECHADO;
  entradaProg = 0.0f;
}

void farolgostoui_abrir_sessao(void) {
  farolgostoui_fechar();
  memset(&resumoCache, 0, sizeof resumoCache);
  memset(&usoCache, 0, sizeof usoCache);
  usoInfantil = 0;
  llmConsentLocal = 0;
  llmPerfil = -1;
}

int farolgostoui_aberto(void) {
  return querAberto || (modoAtual != FGUI_FECHADO && entradaProg > 0.002f);
}

int farolgostoui_modo(void) { return querAberto ? modoAtual : FGUI_FECHADO; }
int farolgostoui_foco(void) { return foco; }

int farolgostoui_llm_ativo(void) {
  int p = perfis_ativo();
  if (llmPerfil != p) {
    llmConsentLocal = 0;
    llmPerfil = p;
  }
  if (perfil_infantil_ativo()) return 0;
  if (farolconsent_llm_ativo) return farolconsent_llm_ativo() ? 1 : 0;
  if (farolconsent_estado)
    return (farolconsent_estado() == FAROL_CONSENT_SIM_VAL) ? 1 : 0;
  return llmConsentLocal;
}

int farolgostoui_llm_definir(int permitirLlm) {
  int p = perfis_ativo();
  int v = permitirLlm ? 1 : 0;
  if (llmPerfil != p) {
    llmConsentLocal = 0;
    llmPerfil = p;
  }
  if (perfil_infantil_ativo() || perfil_ajustes_travados(p)) return 0;
  if (farolconsent_definir) {
    int ok = farolconsent_definir(v ? FAROL_CONSENT_SIM_VAL : FAROL_CONSENT_SO_TV_VAL);
    llmConsentLocal = ok ? v : farolgostoui_llm_ativo();
    if (ok && v && farolconsentui_abrir) farolconsentui_abrir();
    return ok;
  }
  llmConsentLocal = v;
  if (v && farolconsentui_abrir) farolconsentui_abrir();
  return 1;
}

static void confirmarZerar(void) {
  if (perfil_ajustes_travados(perfis_ativo())) {
    querAberto = 0;
    return;
  }
  farol_gosto_zerar(NULL);
  ilha_avisar("farol-gosto", ILHA_OK, "check", i18n(FT_GOSTO_ZERADO), 4000u, 0);
  querAberto = 0;
}

static void onClickBotao(int id, int b) {
  (void)b;
  if (!querAberto) return;
  if (modoAtual == FGUI_APRENDEU) {
    querAberto = 0;
  } else if (modoAtual == FGUI_ZERAR) {
    if (id == 0) confirmarZerar();
    else querAberto = 0;
  }
}

static void onHoverBotao(int id, int b) {
  (void)b;
  if (querAberto) foco = id;
}

void farolgostoui_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (!e || !querAberto) return;
  if (e->type == SDL_MOUSEBUTTONDOWN || e->type == SDL_MOUSEMOTION ||
      e->type == SDL_MOUSEBUTTONUP || e->type == SDL_MOUSEWHEEL)
    return;
  if (e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  if (backKey(e)) {
    querAberto = 0;
    return;
  }
  if (okKey(k)) {
    if (e->key.repeat) return;
    if (modoAtual == FGUI_APRENDEU) {
      if (foco == 0) querAberto = 0;
      else foco = 0;
    } else if (modoAtual == FGUI_ZERAR) {
      if (foco == 0) confirmarZerar();
      else querAberto = 0;
    }
    return;
  }
  if (modoAtual == FGUI_APRENDEU) {
    int maxSc = aprMaxScroll();
    if (k == SDLK_UP) {
      if (foco == 0) {
        foco = 1;
        scrollIdx = maxSc;
      } else if (scrollIdx > 0) {
        scrollIdx--;
      }
    } else if (k == SDLK_DOWN) {
      if (foco != 0 && scrollIdx < maxSc) {
        scrollIdx++;
      } else {
        foco = 0;
      }
    } else if (k == SDLK_LEFT || k == SDLK_RIGHT) {
      foco = 0;
    }
  } else if (modoAtual == FGUI_ZERAR) {
    if (k == SDLK_LEFT || k == SDLK_UP) foco = 0;
    else if (k == SDLK_RIGHT || k == SDLK_DOWN) foco = 1;
  }
}

void farolgostoui_atualizar(float dt, Uint32 agora) {
  (void)agora;
  if (!querAberto && entradaProg <= 0.002f) {
    modoAtual = FGUI_FECHADO;
    entradaProg = 0.0f;
    return;
  }
  entradaProg = t21_entrada(entradaProg, dt, querAberto);
  if (!querAberto && entradaProg <= 0.002f) {
    modoAtual = FGUI_FECHADO;
    entradaProg = 0.0f;
  }
}

static float desenharGrupoTop(const char *titulo, const char (*itens)[64],
                              int n, float x, float y, float w, float a) {
  float ar, ag, ab, cy = y;
  int i;
  TxtLinha th = t21_texto_linha(T21_GROUP, i18n(titulo),
                                T21_TEXT_R, T21_TEXT_G, T21_TEXT_B, 255, w);
  t21_texto_desenhar(th, x, cy, .85f * a);
  cy += th.h + 10.0f;
  ajustes_acento(&ar, &ag, &ab);
  if (n <= 0) {
    TxtLinha vz = t21_texto_linha(T21_SUPPORT, i18n("Nenhum ainda"),
                                  T21_TEXT_R, T21_TEXT_G, T21_TEXT_B, 255, w);
    t21_texto_desenhar(vz, x, cy, T21_TEXT_SUPPORT * a);
    return (cy + vz.h + 18.0f) - y;
  }
  {
    float cx = x, linH = 40.0f;
    for (i = 0; i < n; i++) {
      TxtLinha li = t21_texto_linha(T21_CHIP, itens[i],
                                    T21_TEXT_R, T21_TEXT_G, T21_TEXT_B, 255, w - 32.0f);
      float pw = li.w + 28.0f;
      if (cx > x && cx + pw > x + w) {
        cx = x;
        cy += linH + 8.0f;
      }
      gfx_cor((GfxRect){ cx, cy, pw, linH }, T21_R_PILL, ar, ag, ab, .16f * a);
      t21_texto_desenhar(li, cx + 14.0f, cy + (linH - li.h) * 0.5f, .95f * a);
      cx += pw + 10.0f;
    }
    cy += linH + 22.0f;
  }
  return cy - y;
}

static void desenharUsoTv(float x, float y, float w, float limY, float a, int focado) {
  typedef struct { const char *rot; uint32_t val; } UsoItem;
  const UsoItem itens[7] = {
    { "Fileiras vistas",            usoCache.vistas },
    { "Abertas",                    usoCache.abertas },
    { "Tocadas",                    usoCache.tocadas },
    { "Terminadas",                 usoCache.terminadas },
    { "Recusas",                    usoCache.recusas },
    { "Cenas montadas na TV",       usoCache.cenasTv },
    { "Cenas montadas no servidor", usoCache.cenasServidor },
  };
  float cy = y;
  int tudoZero = usoInfantil ||
    (!usoCache.vistas && !usoCache.abertas && !usoCache.tocadas &&
     !usoCache.terminadas && !usoCache.recusas &&
     !usoCache.cenasTv && !usoCache.cenasServidor);
  TxtLinha th;
  if (y + 64.0f > limY) return;
  th = t21_texto_linha(T21_GROUP, i18n("Uso do Farol nesta TV"),
                       T21_TEXT_R, T21_TEXT_G, T21_TEXT_B, 255, w);
  t21_texto_desenhar(th, x, cy, (focado ? 1.0f : .85f) * a);
  cy += th.h + 10.0f;

  if (usoInfantil) {
    TxtLinha vz = t21_texto_linha(T21_SUPPORT, i18n("Nada contado ainda"),
                                  T21_TEXT_R, T21_TEXT_G, T21_TEXT_B, 255, w);
    t21_texto_desenhar(vz, x, cy, T21_TEXT_SUPPORT * a);
    return;
  }
  if (tudoZero) {
    TxtLinha vz = t21_texto_linha(T21_SUPPORT, i18n("Nada contado ainda"),
                                  T21_TEXT_R, T21_TEXT_G, T21_TEXT_B, 255, w);
    t21_texto_desenhar(vz, x, cy, T21_TEXT_SUPPORT * a);
    cy += vz.h + 12.0f;
  } else {
    float colW = (w - 24.0f) * 0.5f, rowH = 34.0f;
    int i;
    for (i = 0; i < 7; i++) {
      int col = i & 1, row = i >> 1;
      float rx = x + (float)col * (colW + 24.0f);
      float ry = cy + (float)row * (rowH + 6.0f);
      char num[24];
      TxtLinha ll, ln;
      if (ry + rowH > limY - 30.0f) break;
      snprintf(num, sizeof num, "%u", (unsigned)itens[i].val);
      gfx_cor((GfxRect){ rx, ry, colW, rowH }, T21_R_ROW * 0.5f, 1, 1, 1,
              (focado ? .08f : .05f) * a);
      ll = t21_texto_linha(T21_SUPPORT, i18n(itens[i].rot),
                           T21_TEXT_R, T21_TEXT_G, T21_TEXT_B, 255, colW - 88.0f);
      ln = t21_texto_linha(T21_CHIP, num,
                           T21_TEXT_R, T21_TEXT_G, T21_TEXT_B, 255, 76.0f);
      t21_texto_desenhar(ll, rx + 12.0f, ry + (rowH - ll.h) * 0.5f, .88f * a);
      t21_texto_desenhar(ln, rx + colW - 12.0f - ln.w, ry + (rowH - ln.h) * 0.5f, a);
    }
    cy += 4.0f * (rowH + 6.0f) + 6.0f;
  }
  if (cy + 24.0f <= limY) {
    TxtLinha nota = t21_texto_linha(T21_SUPPORT,
      i18n("Fica só nesta TV. Zerar o gosto apaga."),
      T21_TEXT_R, T21_TEXT_G, T21_TEXT_B, 255, w);
    t21_texto_desenhar(nota, x, cy, T21_TEXT_SUPPORT * a);
  }
}

void farolgostoui_desenhar(Uint32 agora) {
  float a, s, sw = NV_LAYOUT_REAL_W, sh = NV_LAYOUT_REAL_H;
  float sheetW, sheetH, sx, sy, px, py, pw;
  GfxRect r;
  (void)agora;
  if (modoAtual == FGUI_FECHADO || (!querAberto && entradaProg <= 0.002f)) return;
  a = anim_suave(entradaProg);
  if (a <= 0.002f) return;
  s = anim_mistura(T21_SHEET_SCALE, 1.0f, a);

  gfx_cor((GfxRect){ 0, 0, sw, sh }, 0.0f, 0.0f, 0.0f, 0.0f, T21_VEIL_BLUR * a);

  sheetW = modoAtual == FGUI_ZERAR ? 760.0f : 960.0f;
  sheetH = modoAtual == FGUI_ZERAR ? 340.0f : 680.0f;
  if (sheetW > sw - 2.0f * T21_SHEET_MARGIN) sheetW = sw - 2.0f * T21_SHEET_MARGIN;
  if (sheetH > sh - 2.0f * T21_SHEET_MARGIN) sheetH = sh - 2.0f * T21_SHEET_MARGIN;
  sx = (sw - sheetW) * 0.5f;
  sy = (sh - sheetH) * 0.5f;
  r = (GfxRect){ sx, sy, sheetW, sheetH };

  {
    float cx = sx + sheetW * 0.5f, cy = sy + sheetH * 0.5f;
    r.w *= s; r.h *= s;
    r.x = cx - r.w * 0.5f;
    r.y = cy - r.h * 0.5f;
  }
  t21_ilha_folha(r, T21_R_SHEET, a);

  px = r.x + T21_SHEET_PAD;
  py = r.y + T21_SHEET_PAD;
  pw = r.w - 2.0f * T21_SHEET_PAD;

  gfx_icone((GfxRect){ px, py + 4.0f, 28.0f, 28.0f }, "aj_farol", 1, 1, 1, .9f * a);
  {
    TxtLinha kicker = t21_texto_linha(T21_KICKER, i18n(FT_NOME),
                                      T21_TEXT_R, T21_TEXT_G, T21_TEXT_B, 255, pw - 40.0f);
    t21_texto_desenhar(kicker, px + 38.0f, py + 6.0f, .75f * a);
  }
  py += 42.0f;

  if (modoAtual == FGUI_APRENDEU) {
    float btnY = r.y + r.h - T21_SHEET_PAD - T21_CTRL_H;
    float limY = btnY - 14.0f;
    TxtLinha tit = t21_texto_linha(T21_SHEET_TITLE, i18n(FT_GOSTO_APRENDEU),
                                   T21_TEXT_R, T21_TEXT_G, T21_TEXT_B, 255, pw);
    t21_texto_desenhar(tit, px, py, a);
    py += tit.h + 18.0f;

    if (resumoCache.vazio) {
      if (scrollIdx <= 0) {
        TxtLinha vz = t21_texto_linha(T21_SHEET_BODY,
          i18n("O Farol ainda não aprendeu preferências neste perfil."),
          T21_TEXT_R, T21_TEXT_G, T21_TEXT_B, 255, pw);
        t21_texto_desenhar(vz, px, py + 4.0f, T21_TEXT_SECONDARY * a);
        py += vz.h + 20.0f;
      }
      desenharUsoTv(px, py, pw, limY, a, foco != 0);
    } else {
      if (scrollIdx <= 0 && py < limY - 80.0f)
        py += desenharGrupoTop("Gêneros", resumoCache.generos, resumoCache.nGeneros,
                               px, py, pw, a);
      if (scrollIdx <= 1 && py < limY - 80.0f)
        py += desenharGrupoTop("Pessoas", resumoCache.pessoas, resumoCache.nPessoas,
                               px, py, pw, a);
      if (scrollIdx <= 2 && py < limY - 80.0f)
        py += desenharGrupoTop("Temas", resumoCache.temas, resumoCache.nTemas,
                               px, py, pw, a);
      if (py < limY - 80.0f || scrollIdx >= 3)
        desenharUsoTv(px, py, pw, limY, a, foco != 0 && scrollIdx >= 3);
    }
    {
      const char *rot = i18n("Voltar");
      TxtLinha bl = t21_texto_linha(T21_CHIP, rot, 255, 255, 255, 255, 220.0f);
      float bw = fmaxf(140.0f, bl.w + 48.0f);
      GfxRect br = { px, btnY, bw, T21_CTRL_H };
      if (foco == 0) {
        int ink = ajustes_tinta_foco();
        plrui_pilula_foco(br, a);
        bl = t21_texto_linha(T21_CHIP, rot, ink, ink, ink, 255, bw);
      } else {
        gfx_cor(br, T21_R_PILL, 1, 1, 1, T21_CHIP_REST * a);
      }
      t21_texto_desenhar(bl, br.x + (br.w - bl.w) * 0.5f,
                         br.y + (br.h - bl.h) * 0.5f, a);
      if (querAberto && a > 0.5f)
        ponteiro_alvo(br.x, br.y, br.w, br.h, onClickBotao, onHoverBotao, 0, 0);
    }
  } else if (modoAtual == FGUI_ZERAR) {
    TxtLinha tit = t21_texto_linha(T21_SHEET_TITLE, i18n(FT_GOSTO_ZERAR),
                                   T21_TEXT_R, T21_TEXT_G, T21_TEXT_B, 255, pw);
    TxtLinha msg = t21_texto_linha(T21_SHEET_BODY, i18n(FT_GOSTO_ZERAR_CONF),
                                   T21_TEXT_R, T21_TEXT_G, T21_TEXT_B, 255, pw);
    const char *labels[2] = { i18n(FT_GOSTO_ZERAR), i18n("Cancelar") };
    float bx = px, by = r.y + r.h - T21_SHEET_PAD - T21_CTRL_H;
    int i;
    t21_texto_desenhar(tit, px, py, a);
    py += tit.h + 18.0f;
    t21_texto_desenhar(msg, px, py, .85f * a);

    for (i = 0; i < 2; i++) {
      TxtLinha bl = t21_texto_linha(T21_CHIP, labels[i], 255, 255, 255, 255, 280.0f);
      float bw = fmaxf(150.0f, bl.w + 48.0f);
      GfxRect br = { bx, by, bw, T21_CTRL_H };
      if (foco == i) {
        int ink = ajustes_tinta_foco();
        plrui_pilula_foco(br, a);
        bl = t21_texto_linha(T21_CHIP, labels[i], ink, ink, ink, 255, bw);
      } else {
        gfx_cor(br, T21_R_PILL, 1, 1, 1, T21_CHIP_REST * a);
      }
      t21_texto_desenhar(bl, br.x + (br.w - bl.w) * 0.5f,
                         br.y + (br.h - bl.h) * 0.5f, a);
      if (querAberto && a > 0.5f)
        ponteiro_alvo(br.x, br.y, br.w, br.h, onClickBotao, onHoverBotao, i, 0);
      bx += bw + T21_GAP_CARD;
    }
  }
}
