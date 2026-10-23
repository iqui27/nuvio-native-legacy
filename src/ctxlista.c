// Ver ctxlista.h.
#include "ctxlista.h"
#include "ctxmenu.h"
#include "catalogo.h"
#include "ilha.h"
#include "idioma.h"
#include "descoberta.h"
#include "layout.h"
#include "segurar.h"
#include "anim.h"
#include "ponteiro.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Os primeiros 150 ms sao toque, nao gesto: a ilha so reage depois deles (o
// mesmo corte de detail.c).
#define CTXH_TOQUE_MS 150u

static int ehOk(SDL_Keycode k) {
  return k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE;
}

int ctxhold_evento(CtxHold *h, const SDL_Event *e, int celulaAceita) {
  SDL_Keycode k;
  if (!h || !e || (e->type != SDL_KEYDOWN && e->type != SDL_KEYUP)) return CTXH_NADA;
  k = e->key.keysym.sym;
  if (!ehOk(k)) {
    if (e->type == SDL_KEYDOWN) ctxhold_cancelar(h);
    return CTXH_NADA;
  }
  if (e->type == SDL_KEYDOWN) {
    // A repeticao do controle nao e um segundo toque nem rearma o relogio.
    if (e->key.repeat) return h->armado ? CTXH_CONSUMIDO : CTXH_NADA;
    if (!celulaAceita) { ctxhold_cancelar(h); return CTXH_NADA; }
    h->armado = 1; h->longo = 0; h->desde = SDL_GetTicks();
    return CTXH_CONSUMIDO;
  }
  // KEYUP. Sem armar aqui nao e clique (a mesma guarda da home): o OK que
  // fechou a barra lateral ou o menu solta sobre esta tela.
  if (!h->armado) return CTXH_NADA;
  ctxhold_cancelar(h);
  if (ponteiro_ok_longo()) return CTXH_LONGO;
  return CTXH_TOQUE;
}

float ctxhold_progresso(const CtxHold *h, Uint32 agora) {
  if (!h || !h->armado) return 0.0f;
  return anim_clamp((float)(agora - h->desde) / (float)NV_HOLD_MS, 0.0f, 1.0f);
}

int ctxhold_passo(CtxHold *h, Uint32 agora, int feedback) {
  float p;
  if (!h || !h->armado || h->longo) return 0;
  if (ctx_aberto()) { ctxhold_cancelar(h); return 0; }
  p = ctxhold_progresso(h, agora);
  if (p >= 1.0f && segurar_venceu(agora, h->desde ? h->desde : 1, "lista")) { h->longo = 1; return 1; }
  if (feedback && agora - h->desde >= CTXH_TOQUE_MS)
    ilha_atividade(i18n("Segure para opções"), p);
  return 0;
}

void ctxhold_cancelar(CtxHold *h) {
  if (!h) return;
  h->armado = 0; h->longo = 0; h->desde = 0;
}

int ctxlista_indice(const char *imdb, long tmdb) {
  int i, n;
  if (imdb && imdb[0] && strncmp(imdb, "tmdb:", 5)) {
    i = cat_indice_por_imdb(imdb);
    if (i >= 0) return i;
  }
  if (imdb && !strncmp(imdb, "tmdb:", 5) && tmdb <= 0) tmdb = atol(imdb + 5);
  if (tmdb > 0) {
    n = cat_n();
    for (i = 0; i < n; i++) {
      const CatItem *ci = cat_item(i);
      if (ci && ci->tmdb == tmdb && strcmp(ci->tipo, "channel")) return i;
    }
  }
  return -1;
}

int ctxlista_abrir(int idx, GfxRect r, const char *arte) {
  if (idx < 0 || idx >= cat_n() || !cat_item(idx)) return 0;
  ctx_fileira(NULL, NULL);          // lista de tela nao tem "Estilo da fileira"
  ctx_dispensar_retomar(0);
  ctx_abrir_cartaz(idx, r, arte);
  return ctx_aberto();
}

// O pedido que espera o meta. Vale SPOT_PESSOA_PRAZO_MS (app.c): um titulo que
// chegue depois disso e outro pedido, e abrir um menu que ninguem espera seria
// pior que nao abrir.
#define CTXL_PRAZO_MS 20000u
static struct { int vivo, solto; Uint32 desde; GfxRect r; char arte[NV_ARTE_URL_MAX]; } pend;

int ctxlista_pedir(const char *imdb, long tmdb, const char *tipo,
                   const char *titulo, const char *ano, const char *poster,
                   GfxRect r, const char *arte) {
  if (desc_titulo_buscando()) return 0;
  if ((!imdb || !imdb[0]) && tmdb <= 0) return 0;
  pend.vivo = 1; pend.solto = ponteiro_ok_longo(); pend.desde = SDL_GetTicks(); pend.r = r;
  snprintf(pend.arte, sizeof pend.arte, "%s", arte ? arte : "");
  desc_pedir_titulo_semente(imdb ? imdb : "", tmdb, tipo, titulo, ano, poster);
  return 1;
}

int ctxlista_tomar(int idx) {
  if (!pend.vivo) return 0;
  pend.vivo = 0;
  // Velho ou com outro menu na frente: engole, nao abre a pagina que ninguem pediu.
  if (SDL_GetTicks() - pend.desde <= CTXL_PRAZO_MS && !ctx_aberto()) {
    ctxlista_abrir(idx, pend.r, pend.arte);
    if (pend.solto && ctx_aberto()) {
      SDL_Event up; SDL_zero(up); up.type = SDL_KEYUP;
      up.key.keysym.sym = SDLK_RETURN; up.key.state = SDL_RELEASED;
      ctx_evento(&up);
    }
  }
  return 1;
}
