// TRAVA DE SERIE: a modal. Ver travatela.h.
//
// MODELO. Uma lista de linhas, cada uma de um tipo (RT_*), reconstruida por
// quadro de `modo`/`travaId`: os perfis da conta (liga e desliga quem entra), o
// modo, o PIN (so Separados), a pausa (so trava existente) e a acao. O perfil
// ATIVO e sempre membro: a trava e vista a partir dele, e tirar a si mesmo
// deixaria a tela sem dono.
//   - Sem trava (travaId == 0): as linhas mexem so no rascunho; a acao cria.
//   - Com trava: cada linha grava na hora (trava_*), e a acao a desfaz.
// Tirar o penultimo membro desfaz a trava (trava.c devolve 2).
#include "travatela.h"
#include "trava.h"
#include "perfis.h"
#include "recomenda.h"
#include "gfx.h"
#include "text.h"
#include "anim.h"
#include "layout.h"
#include "ajustes.h"
#include "idioma.h"
#include "plrui.h"
#include "ilha.h"
#include "ilhaacao.h"
#define NV_ESCALA_TELA_ATIVA   // mede pela tela do fator ativo (escala.h)
#include "escala.h"
#include <stdio.h>
#include <string.h>

#define TT_PAD     40.0f
#define TT_RAIO    32.0f
#define TT_W      860.0f
#define TT_IW     (TT_W - 2.0f * TT_PAD)
#define TT_L       72.0f      // altura da linha
#define TT_GAP      6.0f
#define TT_AV      52.0f
#define TT_CAB    100.0f      // kicker + titulo
#define TT_RODAPE  64.0f
#define TT_JANELA    6        // linhas visiveis antes de a lista rolar
#define TT_MAXL   (CONTA_PERFIL_MAX + 4)

enum { RT_PERFIL = 0, RT_MODO, RT_PIN, RT_PAUSA, RT_ACAO };

static int    aberto, foco, topo;
static float  anim, wAnim, hAnim, rolar;
static int    assentar_ = 1;
static char   serie[DIARIO_ID_MAX], titulo[64];
static int    travaId;                       // 0 = criando
static int    sel[CONTA_PERFIL_MAX + 1];     // por indice de perfil (1..8)
static int    modo, pin, pausada;
static char   aviso[96];
static float  focoAnim[TT_MAXL];

static int teclaOk(SDL_Keycode k) {
  return k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE;
}

int travatela_aberto(void) { return aberto; }

// --- LINHAS ------------------------------------------------------------------

static int nPerfis(void) {
  int n = perfis_n();
  return n > CONTA_PERFIL_MAX ? CONTA_PERFIL_MAX : n;
}

static int nLinhas(void) {
  return nPerfis() + 1 + (modo == TRAVA_SEPARADOS) + (travaId != 0) + 1;
}

// Tipo da linha `i`; `*perfil` recebe o indice do perfil nas linhas RT_PERFIL.
static int linhaTipo(int i, int *perfil) {
  int np = nPerfis();
  if (i < np) {
    const ContaPerfil *p = perfis_item(i);
    if (perfil) *perfil = p ? p->indice : 0;
    return RT_PERFIL;
  }
  i -= np;
  if (i == 0) return RT_MODO;
  i--;
  if (modo == TRAVA_SEPARADOS) { if (i == 0) return RT_PIN; i--; }
  if (travaId) { if (i == 0) return RT_PAUSA; i--; }
  return RT_ACAO;
}

// Com trava gravada, o estado vem do arquivo (outra tela ou o servidor de
// sincronia podem tela mudado); sem ela, e o rascunho.
static void ler(void) {
  Trava t;
  int i;
  if (!travaId) return;
  if (!trava_obter(travaId, &t)) { aberto = 0; return; }
  modo = t.modo; pin = t.pin; pausada = t.pausada;
  memset(sel, 0, sizeof sel);
  for (i = 0; i < t.n; i++) sel[t.perfil[i]] = 1;
}

int travatela_abrir(const char *s, const char *tit) {
  int ids[1], ativo = perfis_ativo();
  if (!s || !s[0] || nPerfis() < 2) return 0;
  snprintf(serie, sizeof serie, "%s", s);
  snprintf(titulo, sizeof titulo, "%s", tit ? tit : "");
  memset(sel, 0, sizeof sel);
  travaId = trava_da_serie(serie, ativo, ids, 1) > 0 ? ids[0] : 0;
  modo = TRAVA_SEPARADOS; pin = 0; pausada = 0;
  if (travaId) ler();
  else if (ativo >= 1 && ativo <= CONTA_PERFIL_MAX) sel[ativo] = 1;
  aberto = 1; foco = 0; topo = 0; aviso[0] = 0;
  memset(focoAnim, 0, sizeof focoAnim);
  rolar = 0.0f; assentar_ = 1;
  return aberto;
}

// --- ACOES -------------------------------------------------------------------

static void postar(const char *frase, IlhaDesfazer desf, int id) {
  IlhaAcao a;
  memset(&a, 0, sizeof a);
  a.icone = "aj_users";
  a.frase = frase;
  a.titulo = titulo;
  a.tipo = ILHA_OK;
  if (desf) { a.desfazer = desf; a.ctx = &id; a.ctxN = sizeof id; }
  ilhaacao_feita(&a);
}

static void desfazerCriar(const void *ctx) { trava_apagar(*(const int *)ctx); }

static void criar(void) {
  int perfis[CONTA_PERFIL_MAX], n = 0, i, id;
  for (i = 1; i <= CONTA_PERFIL_MAX; i++) if (sel[i]) perfis[n++] = i;
  if (n < 2) { snprintf(aviso, sizeof aviso, "%s", i18n("Escolha ao menos mais um perfil")); return; }
  id = trava_criar(serie, titulo, modo, pin, perfis, n);
  if (id < 0) return;
  postar(i18n("Série travada"), desfazerCriar, id);
  aberto = 0;
}

static void aplicar(void) {
  int perfil = 0, t = linhaTipo(foco, &perfil);
  aviso[0] = 0;
  if (t == RT_PERFIL) {
    if (perfil == perfis_ativo()) return;      // sempre membro
    if (!travaId) { sel[perfil] = !sel[perfil]; return; }
    if (sel[perfil]) {
      if (trava_remover_membro(travaId, perfil) == 2) {
        postar(i18n("Trava desfeita"), NULL, 0);
        aberto = 0;
      }
    } else trava_adicionar_membro(travaId, perfil);
  } else if (t == RT_MODO) {
    modo = modo == TRAVA_JUNTOS ? TRAVA_SEPARADOS : TRAVA_JUNTOS;
    if (travaId) trava_definir_modo(travaId, modo);
  } else if (t == RT_PIN) {
    pin = !pin;
    if (travaId) trava_definir_pin(travaId, pin);
  } else if (t == RT_PAUSA) {
    trava_definir_pausada(travaId, !pausada);
  } else if (t == RT_ACAO) {
    if (!travaId) { criar(); return; }
    trava_apagar(travaId);
    postar(i18n("Trava desfeita"), NULL, 0);
    aberto = 0;
  }
  ler();
}

static void ajustarJanela(void) {
  int n = nLinhas();
  if (foco < 0) foco = 0;
  if (foco >= n) foco = n - 1;
  if (foco < topo) topo = foco;
  if (foco >= topo + TT_JANELA) topo = foco - TT_JANELA + 1;
  if (topo > n - TT_JANELA) topo = n - TT_JANELA;
  if (topo < 0) topo = 0;
}

void travatela_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (!aberto || e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  // Repeticao automatica nunca e uma segunda escolha (ver recenviar.c).
  if (e->key.repeat && teclaOk(k)) return;
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
      k == SDLK_DELETE || k == SDLK_LEFT ||
      e->key.keysym.scancode == NV_SCANCODE_BACK) { aberto = 0; return; }
  if (k == SDLK_UP)   { foco--; ajustarJanela(); return; }
  if (k == SDLK_DOWN) { foco++; ajustarJanela(); return; }
  if (teclaOk(k)) aplicar();
}

// --- GEOMETRIA ---------------------------------------------------------------

static int janela(void) {
  int n = nLinhas();
  return n < TT_JANELA ? n : TT_JANELA;
}
static float listaH(void) {
  int j = janela();
  return (float)j * (TT_L + TT_GAP) - TT_GAP;
}
// Corpo: cabecalho + lista + a linha de descricao/aviso (44).
static float corpoH(void) { return TT_CAB + listaH() + 44.0f; }
static float alturaIlha(void) { return TT_PAD + corpoH() + TT_RODAPE + TT_PAD - 14.0f; }

void travatela_atualizar(float dt, Uint32 agora) {
  int i, red = ajustes_animacoes_reduzidas();
  (void)agora;
  if (!aberto && anim < 0.002f) { anim = 0.0f; return; }
  anim = red ? (aberto ? 1.0f : 0.0f) : anim_mola(anim, aberto ? 1.0f : 0.0f, dt, NV_MOLA_TELA);
  for (i = 0; i < TT_MAXL; i++)
    focoAnim[i] = red ? (aberto && foco == i ? 1.0f : 0.0f)
                      : anim_mola(focoAnim[i], aberto && foco == i ? 1.0f : 0.0f, dt, NV_MOLA_FOCO);
  if (!aberto) return;
  ler();
  ajustarJanela();
  { float h = alturaIlha();
    if (assentar_ || red) { wAnim = TT_W; hAnim = h; assentar_ = 0; }
    else {
      wAnim = anim_mola(wAnim, TT_W, dt, NV_MOLA_FOCO * 0.6f);
      hAnim = anim_mola(hAnim, h, dt, NV_MOLA_FOCO * 0.6f);
    }
    rolar = red ? (float)topo : anim_mola(rolar, (float)topo, dt, NV_MOLA_FOCO * 0.7f); }
}

// --- DESENHO -----------------------------------------------------------------

static void ilhaMaterial(GfxRect p, float a) {
  float raio = TT_RAIO / (p.h > 1.0f ? p.h : 1.0f);
  if (ajustes_vidro()) {
    gfx_rect((GfxRect){p.x-18.0f,p.y-8.0f,p.w+36.0f,p.h+40.0f},
             0,GFX_SOMBRA,1.0f,0,0,.5f,0,0,0,.38f*a);
    gfx_vidro_folha(p,raio,a);
    gfx_luz_canto(p, raio, p.w * .22f, -p.h * .40f, p.w * .62f, 1, 1, 1, .06f * a);
  } else plrui_material(p, TT_RAIO, 1, a);
}

static void discoAcao(GfxRect r, const char *icone, float lum, float a) {
  if (ajustes_vidro()) gfx_rect(r, 0, GFX_DISCO, 0, 0, 0, 0, 1, 1, 1, .08f * a);
  else gfx_rect(r, 0, GFX_DISCO, 0, 0, 0, 0, .141f, .149f, .173f, a);
  gfx_icone((GfxRect){ r.x + (r.w - 24.0f) * .5f, r.y + (r.h - 24.0f) * .5f, 24.0f, 24.0f },
            icone, .953f, .949f, .937f, lum * a);
}

// "#RRGGBB" -> 0..1. 0 quando o perfil nao tem cor valida.
static int corDe(const char *hex, float *r, float *g, float *b) {
  unsigned v;
  if (!hex || hex[0] != '#' || sscanf(hex + 1, "%6x", &v) != 1) return 0;
  *r = (float)((v >> 16) & 255) / 255.0f;
  *g = (float)((v >> 8) & 255) / 255.0f;
  *b = (float)(v & 255) / 255.0f;
  return 1;
}

// O rosto do perfil: a foto quando ha, senao o disco na cor dele com a inicial.
static void rosto(GfxRect r, const ContaPerfil *p, float a) {
  float cr, cg, cb;
  if (p->avatarUrl[0] || !corDe(p->corHex, &cr, &cg, &cb)) {
    rec_avatar_estilo(r, p->avatarUrl, p->nome[0] ? p->nome : "?", p->corHex, a, TXT_ILHA_INICIAL);
    return;
  }
  gfx_rect(r, 0, GFX_DISCO, 0, 0, 0, 0, cr, cg, cb, a);
  { char ini[8] = "?";
    TxtLinha l;
    if (p->nome[0]) {
      int n = 1;
      unsigned char c = (unsigned char)p->nome[0];
      if (c >= 0xF0) n = 4; else if (c >= 0xE0) n = 3; else if (c >= 0xC0) n = 2;
      memcpy(ini, p->nome, (size_t)n); ini[n] = 0;
    }
    l = txt_linha(TXT_ILHA_INICIAL, ini, 255, 255, 255, 255);
    txt_desenhar_alpha(l, r.x + (r.w - l.w) * 0.5f, r.y + (r.h - l.h) * 0.5f, a); }
}

static void desenhaLinha(int i, float x, float y, float w, float a) {
  int perfil = 0, t = linhaTipo(i, &perfil);
  float f = i < TT_MAXL ? focoAnim[i] : 0.0f;
  float k = f < 0.0f ? 0.0f : f > 1.0f ? 1.0f : f;
  float lum = .66f + .34f * k;
  float cresce = 4.0f * k;
  GfxRect r = { x - cresce, y, w + 2.0f * cresce, TT_L };
  float tx, fim = x + w - 20.0f;
  char nome[96], valor[48];
  const char *icone = "aj_check";
  nome[0] = valor[0] = 0;
  plrui_linha_foco(r, TT_L * .5f, k * a);
  { GfxRect d = { x + 12.0f, y + (TT_L - TT_AV) * .5f, TT_AV, TT_AV };
    if (t == RT_PERFIL) {
      const ContaPerfil *p = perfis_item(i);
      if (p) {
        rosto(d, p, (.85f + .15f * k) * a);
        snprintf(nome, sizeof nome, "%s", p->nome[0] ? p->nome : i18n("Perfil"));
      }
      if (perfil == perfis_ativo()) snprintf(valor, sizeof valor, "%s", i18n("Este perfil"));
    } else {
      switch (t) {
        case RT_MODO:  snprintf(nome, sizeof nome, "%s", i18n("Modo"));
                       snprintf(valor, sizeof valor, "%s", i18n(modo == TRAVA_JUNTOS ? "Juntos" : "Separados"));
                       icone = modo == TRAVA_JUNTOS ? "aj_link" : "aj_unlink"; break;
        case RT_PIN:   snprintf(nome, sizeof nome, "%s", i18n("Pedir PIN para pular a espera"));
                       snprintf(valor, sizeof valor, "%s", i18n(pin ? "Ligado" : "Desligado"));
                       icone = "aj_key"; break;
        case RT_PAUSA: snprintf(nome, sizeof nome, "%s", i18n("Pausar a trava"));
                       snprintf(valor, sizeof valor, "%s", i18n(pausada ? "Ligado" : "Desligado"));
                       icone = "aj_pause"; break;
        default:       snprintf(nome, sizeof nome, "%s", i18n(travaId ? "Desfazer trava" : "Travar série"));
                       icone = travaId ? "aj_x" : "aj_check"; break;
      }
      discoAcao(d, icone, lum, a);
    }
    tx = d.x + TT_AV + 18.0f; }
  // O MARCADOR de quem entra: o selo de "check" na ponta; quem nao entra fica
  // com a caixa vazia.
  if (t == RT_PERFIL) {
    GfxRect cx = { fim - 32.0f, y + (TT_L - 32.0f) * .5f, 32.0f, 32.0f };
    gfx_rect(cx, 0, GFX_DISCO, 0, 0, 0, 0, 1, 1, 1, (sel[perfil] ? .18f : .08f) * a);
    if (sel[perfil])
      gfx_icone((GfxRect){ cx.x + 4.0f, cx.y + 4.0f, 24.0f, 24.0f }, "aj_check",
                .953f, .949f, .937f, lum * a);
    fim -= 32.0f + 14.0f;
  }
  if (valor[0]) {
    TxtLinha tv = txt_linha_corta(TXT_ILHA_NOME, valor, 243, 242, 239, 255, 260.0f);
    txt_desenhar_alpha(tv, fim - (float)tv.w, y + (TT_L - (float)tv.h) * .5f, (.45f + .30f * k) * a);
    fim -= (float)tv.w + 18.0f;
  }
  { TxtLinha t1 = txt_linha_corta(TXT_ILHA_NOME, nome, 243, 242, 239, 255, fim - tx);
    txt_desenhar_alpha(t1, tx, y + (TT_L - (float)t1.h) * .5f, lum * a); }
}

static void desenhaLista(float x, float y, float w, float a) {
  int i, n = nLinhas(), jan = janela();
  float passo = TT_L + TT_GAP, visH = listaH();
  float desl = (rolar - (float)topo) * passo;
  int rola = n > jan;
  if (rola) gfx_recorte(x - 12.0f, y - 4.0f, w + 24.0f, visH + 8.0f);
  { float yy = y - (float)topo * passo - desl;
    for (i = 0; i < n; i++, yy += passo)
      if (yy + TT_L >= y - passo && yy <= y + visH + passo)
        desenhaLinha(i, x, yy, w, a); }
  if (rola) {
    float trilhoX = x + w + 14.0f;
    float th = visH * (float)jan / (float)n;
    float ty = y + (visH - th) * (rolar / (float)(n - jan));
    gfx_sem_recorte();
    gfx_cor((GfxRect){ trilhoX, y, 4.0f, visH }, .5f, 1, 1, 1, .08f * a);
    gfx_cor((GfxRect){ trilhoX, ty, 4.0f, th }, .5f, 1, 1, 1, .45f * a);
  }
}

static void travatela_desenharCorpo_(void);

void travatela_desenhar(Uint32 agora) {
  (void)agora;
  ESCALA_SE_COUBER_INI(TT_W, alturaIlha());
  travatela_desenharCorpo_();
  ESCALA_SE_COUBER_FIM();
}

static void travatela_desenharCorpo_(void) {
  float a = anim_suave(anim), x, y, cx, cy, h = alturaIlha();
  const char *tit, *kicker, *desc;
  if (anim < 0.01f) return;
  if (wAnim < 1.0f || hAnim < 1.0f) { wAnim = TT_W; hAnim = h; }
  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 0, 0, 0,
          (ajustes_vidro() ? 0.34f : 0.46f) * anim);
  cx = NV_TELA_W * 0.5f;
  cy = NV_TELA_H * 0.5f + (1.0f - a) * 40.0f;
  ilhaMaterial((GfxRect){ cx - wAnim * 0.5f, cy - hAnim * 0.5f, wAnim, hAnim }, a);
  x = cx - TT_W * 0.5f; y = cy - h * 0.5f;

  kicker = travaId ? "Série travada" : "Travar série";
  tit = titulo[0] ? titulo : "Quem entra na trava";
  plrui_kicker(kicker, x + TT_PAD, y + TT_PAD + 2.0f, 243, 242, 239, .45f * a);
  txt_desenhar_alpha(txt_linha_corta(TXT_ILHA_TITULO, titulo[0] ? tit : i18n(tit),
                                     243, 242, 239, 255, TT_IW),
                     x + TT_PAD, y + TT_PAD + 26.0f, a);
  desenhaLista(x + TT_PAD, y + TT_PAD + TT_CAB, TT_IW, a);

  // A LINHA DE BAIXO: o aviso, ou a frase do modo em vigor.
  desc = aviso[0] ? aviso
       : i18n(modo == TRAVA_JUNTOS ? "Um episódio visto marca para todos"
                                   : "Ninguém passa de quem está mais atrás");
  { TxtLinha t = txt_linha_corta(TXT_ILHA_APOIO, desc, 243, 242, 239, 255, TT_IW);
    txt_desenhar_alpha(t, x + TT_PAD, y + TT_PAD + TT_CAB + listaH() + 14.0f,
                       (aviso[0] ? .85f : .55f) * a); }
  { const char *teclas[] = { "↑ ↓", "OK", "Voltar" };
    const char *rotulos[] = { "Navegar", "Selecionar", "Fechar" };
    plrui_dicas(teclas, rotulos, 3, x + TT_PAD, y + h - TT_PAD - 4.0f, 0, a); }
}
