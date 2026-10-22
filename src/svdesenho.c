// Pecas de desenho do social. Ver svdesenho.h.
#include "svdesenho.h"
#include "ajustes.h"
#include "anim.h"
#include "botoes.h"
#include "layout.h"
#include "tex_cache.h"
#include "idioma.h"
#include "recomenda.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

float svd_foco_visual(float f) {
  f = anim_clamp(f, 0.0f, 1.0f);
  return f * f * (3.0f - 2.0f * f);
}
float svd_foco_texto(float f) { return ajustes_vidro() ? 0.0f : svd_foco_visual(f); }

void svd_superficie(GfxRect r, float raio, float f, float a) {
  float cr, cg, cb, v = svd_foco_visual(f);
  if (ajustes_vidro()) {
    gfx_vidro_superficie(r, raio, a);
    if (v > .001f) gfx_vidro_foco(r, raio, v, a);
    return;
  }
  ajustes_acento(&cr, &cg, &cb);
  gfx_cor(r, raio, .062f, .066f, .079f, .92f * a);
  if (v > .001f) {
    botao_luz(r, v * .5f, a);
    gfx_cor(r, raio, cr, cg, cb, v * a);
  }
}

void svd_txt_foco(TxtLinha repouso, TxtLinha foco, float x, float y, float f, float a) {
  if (f < 0.999f) txt_desenhar_alpha(repouso, x, y, a * (1.0f - f));
  if (f > 0.001f) txt_desenhar_alpha(foco, x, y, a * f);
}

void svd_ponto_vivo(float cx, float cy, float d, float borda, float a, Uint32 t) {
  // O PULSO E SO ALFA, 2 s por ciclo, entre 45 % e 100 % — o mesmo do desenho
  // aprovado. Nada de escala: um disco que cresce e encolhe puxa o olho a cada
  // segundo, e a fileira inteira passaria a "piscar".
  float p = 1.0f;
  if (!ajustes_animacoes_reduzidas())
    p = 0.725f + 0.275f * cosf((float)(t % 2000u) / 2000.0f * 6.2831853f);
  if (borda > 0.0f)
    gfx_rect((GfxRect){ cx - d * 0.5f - borda, cy - d * 0.5f - borda, d + 2 * borda, d + 2 * borda },
             0, GFX_DISCO, 0, 0, 0, 0, 0.06f, 0.06f, 0.07f, a);
  gfx_rect((GfxRect){ cx - d * 0.5f, cy - d * 0.5f, d, d }, 0, GFX_DISCO, 0, 0, 0, 0,
           SVD_VIVO_R, SVD_VIVO_G, SVD_VIVO_B, a * p);
}

// Cor de fundo da inicial, estavel por pessoa.
static void corDoId(const char *id, float *r, float *g, float *b) {
  static const float pal[6][3] = {
    { .184f, .435f, .878f }, { .545f, .353f, .235f }, { .416f, .298f, .576f },
    { .204f, .541f, .478f }, { .690f, .325f, .392f }, { .420f, .478f, .200f } };
  unsigned h = 2166136261u;
  const unsigned char *p = (const unsigned char *)(id ? id : "");
  while (*p) { h ^= *p++; h *= 16777619u; }
  *r = pal[h % 6][0]; *g = pal[h % 6][1]; *b = pal[h % 6][2];
}

static void inicial(const char *nome, char *dst, size_t tam) {
  size_t z = 1;
  if (!nome || !nome[0]) { snprintf(dst, tam, "?"); return; }
  while (nome[z] && z < 4 && ((unsigned char)nome[z] & 0xc0) == 0x80) z++;
  if (z >= tam) z = tam - 1;
  memcpy(dst, nome, z);
  dst[z] = 0;
  // Minuscula vira maiuscula so no ASCII; o resto fica como veio.
  if (dst[0] >= 'a' && dst[0] <= 'z') dst[0] = (char)(dst[0] - 32);
}

static void foto(GfxRect r, const char *url, const char *nome, const char *id, float a) {
  GLuint t = (url && url[0]) ? tex_obter_larg(url, r.w) : 0;
  gfx_rect(r, 0, GFX_DISCO, 0, 0, 0, 0, 0.09f, 0.09f, 0.10f, a);
  if (t) {
    gfx_tex_aspect_atual = tex_aspecto(url);
    gfx_rect(r, t, GFX_AVATAR, 0, 0, 0, 0, 1, 1, 1, a);
    gfx_tex_aspect_atual = 0.0f;
    return;
  }
  { float cr, cg, cb;
    char ini[8];
    TxtEstilo est = r.w >= 120.0f ? TXT_TITULO2 : r.w >= 64.0f ? TXT_TITULO3
                  : r.w >= 44.0f ? TXT_CALLOUT : TXT_CAPTION2;
    TxtLinha l;
    corDoId(id && id[0] ? id : nome, &cr, &cg, &cb);
    gfx_rect(r, 0, GFX_DISCO, 0, 0, 0, 0, cr, cg, cb, a);
    inicial(nome, ini, sizeof ini);
    l = txt_linha(est, ini, 255, 255, 255, 255);
    txt_desenhar_alpha(l, r.x + (r.w - l.w) * 0.5f, r.y + (r.h - l.h) * 0.5f, a); }
}

void svd_avatar(GfxRect r, const char *url, const char *nome, const char *id, float a) {
  foto(r, url, nome, id, a);
}

// Anel por fora de um disco: `folga` px de vao, `esp` de traco.
static void anelDisco(GfxRect r, float folga, float esp, float cr, float cg, float cb, float a) {
  gfx_anel_fora(r, 0.5f, folga, esp, cr, cg, cb, a);
}

void svd_rosto(GfxRect r, const SvAmigo *am, float foco, float a, Uint32 t) {
  float esc = 1.0f + 0.08f * svd_foco_visual(foco);
  float d = r.w * esc;
  GfxRect fr = { r.x + (r.w - d) * 0.5f, r.y + (r.h - d) * 0.5f, d, d };
  float esp = d >= 100.0f ? 5.0f : 3.0f, folga = d >= 100.0f ? 5.0f : 3.0f;
  if (!am) return;
  // O anel de estado: vermelho ao vivo ganha do laranja da novidade.
  if (am->agora) anelDisco(fr, folga, esp, SVD_VIVO_R, SVD_VIVO_G, SVD_VIVO_B, a);
  else if (am->novo) anelDisco(fr, folga, esp, SVD_NOVO_R, SVD_NOVO_G, SVD_NOVO_B, a);
  else anelDisco(fr, folga, esp * 0.6f, 0.23f, 0.23f, 0.27f, a);
  // O FOCO E UM SEGUNDO ANEL POR FORA, na cor de realce (o degrade do tema
  // dinamico vem de gfx_rect). Ele nao substitui o de estado: com o foco no
  // rosto de quem esta ao vivo a pessoa continua vendo o vermelho.
  if (foco > 0.01f) {
    float ar, ag, ab;
    ajustes_acento(&ar, &ag, &ab);
    anelDisco(fr, folga + esp + 5.0f, esp, ar, ag, ab, a * svd_foco_visual(foco));
  }
  foto(fr, am->avatar, am->nome, am->id, a);
  if (am->agora) {
    float pd = d * 0.17f;
    float cx = fr.x + fr.w * 0.5f + d * 0.5f * 0.7071f;
    float cy = fr.y + fr.h * 0.5f + d * 0.5f * 0.7071f;
    svd_ponto_vivo(cx, cy, pd, pd * 0.18f, a, t);
  }
}

// Selo de canto do rosto: coracao (gostou, #e5566e) ou check (viu, verde).
static void seloAmigo(float cx, float cy, float d, int gostou, float a) {
  float ic = d * 0.62f;
  gfx_rect((GfxRect){ cx - d * 0.5f - d * 0.11f, cy - d * 0.5f - d * 0.11f, d * 1.22f, d * 1.22f },
           0, GFX_DISCO, 0, 0, 0, 0, 0.03f, 0.035f, 0.045f, a);
  if (gostou) gfx_rect((GfxRect){ cx - d * 0.5f, cy - d * 0.5f, d, d }, 0, GFX_DISCO, 0, 0, 0, 0,
                       0.898f, 0.337f, 0.431f, a);
  else gfx_rect((GfxRect){ cx - d * 0.5f, cy - d * 0.5f, d, d }, 0, GFX_DISCO, 0, 0, 0, 0,
                0.235f, 0.561f, 0.420f, a);
  gfx_icone((GfxRect){ cx - ic * 0.5f, cy - ic * 0.5f, ic, ic }, gostou ? "aj_heart" : "aj_check",
            1.0f, 1.0f, 1.0f, a);
}

float svd_amigos_pilha(float x, float y, float d, const AmigosTitulo *t, int n,
                       int selos, const float anel[3], float a) {
  float passo = d * 0.71f, aro = d * 0.07f, larg = d;
  int i;
  if (!t || n <= 0) return 0.0f;
  if (n > t->n) n = t->n;
  for (i = 0; i < n; i++) {
    const AmigoTit *f = &t->a[i];
    float fx = x + passo * (float)i;
    gfx_rect((GfxRect){ fx, y, d, d }, 0, GFX_DISCO, 0, 0, 0, 0, anel[0], anel[1], anel[2], a);
    foto((GfxRect){ fx + aro, y + aro, d - 2 * aro, d - 2 * aro }, f->avatar, f->nome, f->id, a);
    // Quem so me RECOMENDOU (sem ter visto, que se saiba) fica sem selo: o
    // check diria "viu" sem prova.
    if (selos && (f->viu || f->gostou)) {
      float sd = d * 0.40f;
      seloAmigo(fx + d - sd * 0.42f, y + d - sd * 0.38f, sd, f->gostou, a);
    }
    larg = passo * (float)i + d;
  }
  return larg;
}

float svd_amigos_chip(float x, float y, float h, float maxW, const AmigosTitulo *t, float a) {
  static const float ANEL[3] = { 0.035f, 0.04f, 0.05f };
  float d = h * 0.74f, pad = (h - d) * 0.5f;
  int rostos, resto;
  TxtLinha l;
  char num[16];
  float larg = 0.0f, tw = 0.0f;
  if (!t || t->total <= 0 || t->n <= 0) return 0.0f;
  for (rostos = t->n < 2 ? t->n : 2; rostos >= 0; rostos--) {
    resto = t->total - rostos;
    tw = 0.0f;
    if (resto > 0) {
      snprintf(num, sizeof num, "+%d", resto);
      l = txt_linha(TXT_CAPTION, num, 245, 245, 248, 255);
      tw = (float)l.w + pad * 1.2f;
    }
    larg = pad * 1.6f + (rostos ? d + d * 0.71f * (float)(rostos - 1) : 0.0f) + tw;
    if (rostos == 0) larg = pad * 2.0f + tw;
    if (larg <= maxW) break;
  }
  if (rostos < 0 || larg > maxW) return 0.0f;
  gfx_cor((GfxRect){ x, y + 2.0f, larg, h }, 0.5f, 0, 0, 0, 0.25f * a);
  gfx_cor((GfxRect){ x, y, larg, h }, 0.5f, 0.03f, 0.035f, 0.045f, 0.70f * a);
  { float cx = x + pad * 0.8f;
    if (rostos) cx += svd_amigos_pilha(cx, y + pad, d, t, rostos, 0, ANEL, a);
    else cx = x + pad;
    resto = t->total - rostos;
    if (resto > 0) {
      snprintf(num, sizeof num, "+%d", resto);
      l = txt_linha(TXT_CAPTION, num, 245, 245, 248, 255);
      txt_desenhar_alpha(l, cx + (rostos ? pad * 0.6f : 0.0f), y + (h - l.h) * 0.5f, a);
    } }
  return larg;
}

void svd_rosto_acao(GfxRect r, const char *icone, float foco, float a) {
  float esc = 1.0f + 0.08f * svd_foco_visual(foco);
  float d = r.w * esc;
  GfxRect fr = { r.x + (r.w - d) * 0.5f, r.y + (r.h - d) * 0.5f, d, d };
  float esp = 5.0f, folga = 5.0f, ic = d * 0.30f;
  anelDisco(fr, folga, 3.0f, 0.23f, 0.23f, 0.27f, a);
  if (foco > 0.01f) {
    float ar, ag, ab;
    ajustes_acento(&ar, &ag, &ab);
    anelDisco(fr, folga + esp + 5.0f, esp, ar, ag, ab, a * svd_foco_visual(foco));
  }
  gfx_rect(fr, 0, GFX_DISCO, 0, 0, 0, 0, 0.15f, 0.145f, 0.176f, a);
  gfx_icone((GfxRect){ fr.x + (d - ic) * 0.5f, fr.y + (d - ic) * 0.5f, ic, ic },
            icone, 0.74f, 0.73f, 0.78f, a);
}

void svd_barra(GfxRect r, int pct, float a) {
  float ar, ag, ab, p = anim_clamp((float)pct / 100.0f, 0.0f, 1.0f);
  GfxRect c = r;
  if (pct < 0) return;
  ajustes_acento(&ar, &ag, &ab);
  gfx_cor(r, 0.5f, 1.0f, 1.0f, 1.0f, 0.18f * a);
  c.w = r.w * p;
  if (c.w >= r.h) gfx_cor(c, 0.5f, ar, ag, ab, a);
}

void svd_poster(GfxRect r, const char *url, float raio, float a) {
  GLuint t = (url && url[0]) ? tex_obter_larg(url, r.w) : 0;
  if (t) {
    gfx_tex_aspect_atual = tex_aspecto(url);
    gfx_rect(r, t, GFX_CARD, 0.0f, 0.0f, 0.0f, raio, 0, 0, 0, a);
    gfx_tex_aspect_atual = 0.0f;
  } else {
    gfx_cor(r, raio, NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G, NV_COR_ESQUELETO_B, a);
  }
}

void svd_cartao(GfxRect r, const SvEvento *ev, float foco, float a, Uint32 t) {
  float raio = NV_RAIO_CARD, pad = r.h * 0.075f;
  float f = svd_foco_visual(foco);
  const char *arte;
  char status[160];
  if (!ev) return;
  arte = ev->arte[0] ? ev->arte : ev->poster;
  // O ARO DE FOCO DA HOME: a cor de realce por fora do cartao, com o raio de
  // fora = raio do cartao + espessura (a conta de home.c, "pontas feias").
  if (f > 0.01f) {
    float ar, ag, ab, menor = r.w < r.h ? r.w : r.h;
    GfxRect b = { r.x - NV_ANEL_FOCO, r.y - NV_ANEL_FOCO,
                  r.w + 2 * NV_ANEL_FOCO, r.h + 2 * NV_ANEL_FOCO };
    ajustes_acento(&ar, &ag, &ab);
    if (ajustes_vidro()) gfx_vidro_cartao(r, raio, f, a);
    else gfx_cor(b, (raio * menor + NV_ANEL_FOCO) / (menor + 2 * NV_ANEL_FOCO), ar, ag, ab, f * a);
  }
  svd_poster(r, arte, raio, a);
  // O VEU DA LEGENDA, de baixo ate ~70 % da altura (o mesmo gfx_veu_base dos
  // cartoes deitados da home). Sem ele o titulo branco some na arte clara.
  gfx_veu_base(r, raio, 0.70f, 0.92f * a);
  socialvis_status(ev, status, sizeof status);
  { int barra = ev->pct >= 0 && (ev->acao == SV_AGORA || ev->acao == SV_INICIO);
    float y = r.y + r.h - pad - (barra ? 14.0f : 0.0f);
    float x = r.x + pad, w = r.w - 2.0f * pad;
    int gostou = ev->reacao == SV_REAC_GOSTOU;
    TxtLinha st, ti;
    if (ev->acao == SV_AGORA) {
      st = txt_linha_corta(TXT_CAPTION, status, 255, 214, 214, 255, w - 26.0f);
      y -= (float)st.h;
      svd_ponto_vivo(x + 8.0f, y + (float)st.h * 0.5f, 14.0f, 0.0f, a, t);
      txt_desenhar_alpha(st, x + 24.0f, y, a);
    } else {
      st = gostou ? txt_linha_corta(TXT_CAPTION, status, 247, 192, 138, 255, w)
                  : txt_linha_corta(TXT_CAPTION, status, 216, 213, 224, 255, w);
      y -= (float)st.h;
      txt_desenhar_alpha(st, x, y, a);
    }
    ti = txt_linha_corta(TXT_CALLOUT, ev->titulo, 248, 248, 250, 255, w);
    txt_desenhar_alpha(ti, x, y - 4.0f - (float)ti.h, a);
    if (barra) svd_barra((GfxRect){ x, r.y + r.h - pad - 5.0f, w, 5.0f }, ev->pct, a);
  }
}

float svd_chip(float x, float y, const char *texto, int estilo, int escuro, float a) {
  TxtLinha t;
  float w;
  float fr = 1, fg = 1, fb = 1, fa = escuro ? 0.16f : 0.08f;
  int cr = escuro ? 30 : 216, cg = escuro ? 26 : 213, cb = escuro ? 22 : 224;
  if (estilo == 1) { fr = .435f; fg = .812f; fb = .592f; fa = .20f;
                     if (!escuro) { cr = 111; cg = 207; cb = 151; } }
  if (estilo == 2) { fr = .949f; fg = .635f; fb = .361f; fa = .24f;
                     if (!escuro) { cr = 247; cg = 192; cb = 138; } }
  if (escuro) { fr = fg = fb = 0.0f; }
  t = txt_linha(TXT_MINI, texto, cr, cg, cb, 255);
  w = (float)t.w + 24.0f;
  gfx_cor((GfxRect){ x, y, w, SVD_CHIP_H }, 0.5f, fr, fg, fb, fa * a);
  txt_desenhar_alpha(t, x + 12.0f, y + (SVD_CHIP_H - (float)t.h) * 0.5f, a);
  return w;
}

float svd_seta(float x, float y, int escuro, float a) {
  int c = escuro ? 60 : 124;
  TxtLinha t = txt_linha(TXT_CAPTION, "\xe2\x80\xba", c, c, c + 12, 255);
  txt_desenhar_alpha(t, x + 6.0f, y + (SVD_CHIP_H - (float)t.h) * 0.5f, a);
  return (float)t.w + 12.0f;
}
