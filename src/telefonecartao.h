#ifndef NV_TELEFONE_CARTAO_H
#define NV_TELEFONE_CARTAO_H
#include "telefoneui.h"
#include "rolagemtoque.h"
#include "ajustes.h"
#include "text.h"
#include "idioma.h"

#ifdef NV_TOUCH_UI
/* Phone cards keep native type, a scrolling body and fixed decision buttons.
 * Callers own content and actions; this helper never records a decision. */
typedef struct {
  GfxRect painel, corpo;
  ToqueRolagem rolagem;
  float offset;
  int chave, botoes, interativo;
} TelefoneCartao;

static inline void telefonecartao_limpar(TelefoneCartao *c) {
  c->offset = 0; c->chave = -1; toquerol_limpar(&c->rolagem);
}
static inline void telefonecartao_medir(TelefoneCartao *c, float w, float h, int botoes) {
  float rodape = botoes * 76.0f + (botoes > 0 ? (botoes - 1) * 12.0f + 24.0f : 0);
  c->painel = (GfxRect){24, 24, w - 48, h - 48};
  c->corpo = (GfxRect){52, 48, w - 104, h - 96 - rodape};
  c->botoes = botoes;
}
static inline float telefonecartao_texto(TxtEstilo estilo, const char *s,
                                        float x, float y, float w, float leading, float a) {
  return txt_bloco_corta(estilo, s, 243, 242, 239, x, y, w, leading, a, 0);
}
static inline float telefonecartao_titulo(const char *s, float x, float y, float w, float a) {
  return telefonecartao_texto(TXT_ILHA_TITULO, s, x, y, w, 48, a) + 20;
}
static inline float telefonecartao_item(const char *icone, const char *titulo, const char *texto,
                                       float x, float y, float w, float a) {
  float xTexto = x + (icone ? 48 : 0), largura = w - (icone ? 48 : 0), h;
  if (icone && a > 0) gfx_icone((GfxRect){x, y + 4, 30, 30}, icone, .953f, .949f, .937f, a);
  h = telefonecartao_texto(TXT_CALLOUT, titulo, xTexto, y, largura, 38, a);
  if (texto && texto[0]) h += 8 + telefonecartao_texto(TXT_CAPTION, texto, xTexto, y + h + 8, largura, 30, .68f * a);
  return h + 24;
}
static inline void telefonecartao_comecar(TelefoneCartao *c, float total, int chave,
                                         int interativo, PonteiroRolagemFn rolar, float a) {
  if (chave != c->chave) { telefonecartao_limpar(c); c->chave = chave; }
  c->interativo = interativo;
  c->offset = fminf(c->offset, fmaxf(0, total - c->corpo.h));
  if (interativo) {
    ponteiro_camada();
    ponteiro_alvo(0, 0, c->painel.w + 48, c->painel.h + 48, NULL, NULL, 0, 0);
    toquerol_vincular(&c->rolagem, c->corpo, gfx_escala(), 0, total - c->corpo.h, 1, &c->offset);
    ponteiro_rolagem(rolar);
  }
  gfx_cor((GfxRect){0, 0, c->painel.w + 48, c->painel.h + 48}, 0, 0, 0, 0, .72f * a);
  gfx_cor(c->painel, 36 / c->painel.h, .055f, .059f, .071f, .98f * a);
  gfx_recorte(c->corpo.x, c->corpo.y, c->corpo.w, c->corpo.h);
}
static inline void telefonecartao_alvo(TelefoneCartao *c, GfxRect r,
                                      PonteiroFn focar, PonteiroFn ativar, int a, int b) {
  if (c->interativo) ponteiro_alvo_faixa(r.x, r.y, r.w, r.h,
                         c->corpo.y, c->corpo.y + c->corpo.h, focar, ativar, a, b);
}
static inline GfxRect telefonecartao_botao_r(const TelefoneCartao *c, int i) {
  float h = c->botoes * 76.0f + (c->botoes - 1) * 12.0f;
  return (GfxRect){c->corpo.x, c->painel.y + c->painel.h - 24 - h + i * 88,
                   c->corpo.w, 76};
}
static inline void telefonecartao_botao_em(TelefoneCartao *c, GfxRect r, const char *rot,
                                          int foco, PonteiroFn focar, PonteiroFn ativar, int i, int b, float a, int corpo) {
  float ar, ag, ab;
  int tinta = foco ? ajustes_tinta_foco() : 243;
  float th = txt_bloco_corta(TXT_ILHA_ITEM, rot, tinta, tinta, tinta, 0, 0, r.w - 48, 30, 0, 2);
  ajustes_acento(&ar, &ag, &ab);
  gfx_cor(r, .5f, foco ? ar : .141f, foco ? ag : .149f, foco ? ab : .173f, a);
  txt_bloco_corta(TXT_ILHA_ITEM, rot, tinta, tinta, tinta, r.x + 24, r.y + (r.h - th) * .5f, r.w - 48, 30, a, 2);
  if (corpo) telefonecartao_alvo(c, r, focar, ativar, i, b);
  else if (c->interativo) ponteiro_alvo(r.x, r.y, r.w, r.h, focar, ativar, i, b);
}
static inline void telefonecartao_botao(TelefoneCartao *c, int i, const char *rot,
                                       int foco, PonteiroFn focar, PonteiroFn ativar, int b, float a) {
  telefonecartao_botao_em(c, telefonecartao_botao_r(c, i), rot, foco, focar, ativar, i, b, a, 0);
}
#endif
#endif
