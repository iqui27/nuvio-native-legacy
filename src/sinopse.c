#include "sinopse.h"
#include "ajustes.h"
#include "extras.h"
#include "tex_cache.h"
#include <stdlib.h>
#include "gfx.h"
#include "idioma.h"
#include "perfis.h"
#include "plrui.h"
#include "ponteiro.h"
#include "rolagemtoque.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define NV_ESCALA_TELA
#include "escala.h"

static int corpoOriginal(TxtEstilo e) {
  switch (e) {
    case TXT_ILHA_APOIO: return 16;
    case TXT_AJ_ESTADO: return 18;
    case TXT_ILHA_SUB: return 19;
    case TXT_ILHA_TEXTO: return 20;
    case TXT_CAPTION: case TXT_HERO_SIN: return 22;
    case TXT_DET_META2: return 23;
    case TXT_DET_SIN: return 26;
    default: return 0;
  }
}
static int corpo(TxtEstilo original) {
  int n = corpoOriginal(original), tamanho = ajustes_sinopse_tamanho();
  if (!tamanho || !n) return n;
  if (original == TXT_HERO_SIN) return 24; // Integration: Grande is 24/32.
  return (n * 125 + 50) / 100;
}
TxtEstilo sinopse_estilo(TxtEstilo original) {
  if (!ajustes_sinopse_tamanho()) return original;
  switch (corpo(original)) {
    case 19: return TXT_SINOPSE_19;
    case 20: return TXT_SINOPSE_20;
    case 21: return TXT_SINOPSE_21;
    case 22: return TXT_SINOPSE_22;
    case 23: return TXT_SINOPSE_23;
    case 24: return TXT_SINOPSE_24;
    case 25: return TXT_SINOPSE_25;
    case 26: return TXT_SINOPSE_26;
    case 27: return TXT_SINOPSE_27;
    case 28: return TXT_SINOPSE_28;
    case 29: return TXT_SINOPSE_29;
    case 30: return TXT_SINOPSE_30;
    case 31: return TXT_SINOPSE_31;
    case 33: return TXT_SINOPSE_33;
    case 34: return TXT_SINOPSE_34;
    case 35: return TXT_SINOPSE_35;
    case 39: return TXT_SINOPSE_39;
    default: return original;
  }
}
float sinopse_entrelinha(TxtEstilo original, float leading) {
  int n = corpoOriginal(original);
  if (!ajustes_sinopse_tamanho()) return leading;
  if (original == TXT_HERO_SIN) return 32.0f;
  return n ? leading * (float)corpo(original) / (float)n : leading;
}
int sinopse_linhas(TxtEstilo original, float leading, int maxLinhas) {
  if (!ajustes_sinopse_tamanho() || original == TXT_HERO_SIN) return maxLinhas;
  float novo = sinopse_entrelinha(original, leading);
  int n = maxLinhas > 0 && novo > 0 ? (int)floorf((float)maxLinhas * leading / novo + .001f) : maxLinhas;
  return maxLinhas > 0 && n < 1 ? 1 : n;
}
float sinopse_bloco_corta(TxtEstilo original, const char *texto, int r, int g, int b,
                          float x, float y, float larg, float leading, float alpha, int maxLinhas) {
  int linhas = sinopse_linhas(original, leading, maxLinhas);
  if (maxLinhas > 0 && linhas <= 0) return 0;
  return txt_bloco_corta(sinopse_estilo(original), texto, r, g, b, x, y, larg,
                         sinopse_entrelinha(original, leading), alpha, linhas);
}
float sinopse_bloco(TxtEstilo original, const char *texto, int r, int g, int b,
                    float x, float y, float larg, float leading, float alpha, int maxLinhas) {
  if (!ajustes_sinopse_tamanho())
    return txt_bloco(original, texto, r, g, b, x, y, larg, leading, alpha, maxLinhas);
  return sinopse_bloco_corta(original, texto, r, g, b, x, y, larg, leading, alpha, maxLinhas);
}

// Copy metadata at open: a late catalog refresh cannot replace the text under
// the reader or transfer it to another account/profile visit.
static struct {
  int aberta;
  uint64_t sessao;
  char titulo[160], *texto, data[40];
  int comentario, nota, curtidas, respostas, oculto, posicao, total;
  float scroll, destino, max, pagina, passo;
  Uint32 tick;
  float largura, escala;
  TxtFamilia familia;
  TxtEstilo estilo;
  int nLinhas;
  struct SinopseLinha { size_t inicio, n; } *linhas;
#ifdef NV_TOUCH_UI
  ToqueRolagem toque;
#endif
} leitor;
void sinopse_fechar(void) {
  free(leitor.texto); free(leitor.linhas); memset(&leitor, 0, sizeof leitor);
}
int sinopse_comentario_aberto(void) { return sinopse_aberta() && leitor.comentario; }
int sinopse_aberta(void) {
  if (leitor.aberta && !perfis_sessao_atual(leitor.sessao)) sinopse_fechar();
  return leitor.aberta;
}
static void copiar(char *dst, size_t cap, const char *src) {
  size_t n = strlen(src);
  if (n >= cap) {
    n = cap - 1;
    while (n && ((unsigned char)src[n] & 0xc0) == 0x80) n--;
  }
  memcpy(dst, src, n); dst[n] = 0;
}
void sinopse_abrir(const char *titulo, const char *texto) {
  sinopse_fechar();
  if (!texto || !texto[0]) return;
  copiar(leitor.titulo, sizeof leitor.titulo, titulo ? titulo : "");
  leitor.texto = strdup(texto);
  if (!leitor.texto) return;
  leitor.sessao = perfis_sessao_geracao(); leitor.aberta = 1;
}
void sinopse_comentario(const char *autor, const char *texto, const char *data,
                        int nota, int curtidas, int respostas, int spoiler,
                        int posicao, int total) {
  sinopse_abrir(autor, texto);
  leitor.comentario = 1; leitor.nota = nota; leitor.curtidas = curtidas;
  leitor.respostas = respostas; leitor.oculto = spoiler;
  leitor.posicao = posicao; leitor.total = total;
  copiar(leitor.data, sizeof leitor.data, data ? data : "");
}
static void fecharPonteiro(int a, int b) { (void)a; (void)b; sinopse_fechar(); }
#ifdef NV_TOUCH_UI
static int rolar(const PonteiroRolagem *e) { return sinopse_aberta() && toquerol_evento(&leitor.toque, e); }
#endif
void sinopse_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (!sinopse_aberta() || e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  if (k == SDLK_ESCAPE || k == SDLK_AC_BACK || k == SDLK_BACKSPACE || k == SDLK_DELETE ||
      (!leitor.comentario && (k == SDLK_RETURN || k == SDLK_KP_ENTER)) || e->key.keysym.scancode == NV_SCANCODE_BACK) {
    sinopse_fechar(); return;
  }
  if (leitor.comentario && (k == SDLK_RETURN || k == SDLK_KP_ENTER)) {
    if (leitor.oculto) leitor.oculto = 0;
    else sinopse_fechar();
    return;
  }
  if (leitor.oculto) return;
#ifdef NV_TOUCH_UI
  if (toquerol_navegacao(e)) toquerol_limpar(&leitor.toque);
#endif
  float anterior = leitor.scroll;
  if (leitor.comentario) leitor.scroll = leitor.destino;
  if (k == SDLK_UP) leitor.scroll -= leitor.passo;
  else if (k == SDLK_DOWN) leitor.scroll += leitor.passo;
  else if (k == SDLK_PAGEUP) leitor.scroll -= leitor.pagina;
  else if (k == SDLK_PAGEDOWN) leitor.scroll += leitor.pagina;
  else if (k == SDLK_HOME) leitor.scroll = 0;
  else if (k == SDLK_END) leitor.scroll = leitor.max;
  if (leitor.scroll < 0) leitor.scroll = 0;
  if (leitor.scroll > leitor.max) leitor.scroll = leitor.max;
  if (leitor.comentario) { leitor.destino = leitor.scroll; leitor.scroll = anterior; }
}

// Complete UTF-8 wrapping, including a word longer than the viewport. Work in
// codepoints rather than cutting bytes or dropping the rest of a long token.
// The same pass measures and draws, so rotation/font changes cannot leave an
// unreachable tail. Metadata is already bounded by the catalog's own limits.
static size_t proximo(const char *p) {
  size_t n = 1;
  while (p[n] && ((unsigned char)p[n] & 0xc0) == 0x80) n++;
  return n;
}
static float textoCompleto(TxtEstilo estilo, float x, float y, float w, float lead, int desenhar) {
  char linha[2048];
  int i;
  if (!leitor.nLinhas || leitor.largura != w || leitor.estilo != estilo || leitor.escala != gfx_escala() || leitor.familia != txt_fonte_interface()) {
    const char *p = leitor.texto;
    size_t bytes = strlen(p);
    struct SinopseLinha *linhas = realloc(leitor.linhas, (bytes + 1) * sizeof *linhas);
    if (!linhas) return 0;
    leitor.linhas = linhas;
    leitor.nLinhas = 0; leitor.largura = w; leitor.estilo = estilo;
    leitor.escala = gfx_escala(); leitor.familia = txt_fonte_interface();
    while (*p) {
      size_t n = 0, quebra = 0;
      const char *inicio;
      while (*p == ' ' || *p == '\r') p++;
      if (!*p) break;
      inicio = p;
      while (*p && *p != '\n') {
        size_t k = proximo(p);
        if (n + k >= sizeof linha) break;
        memcpy(linha + n, p, k); linha[n + k] = 0;
        if (n && txt_largura(estilo, linha) > w) break;
        if (*p == ' ') quebra = n;
        n += k; p += k;
      }
      if (*p && *p != '\n' && quebra) { n = quebra; p = inicio + quebra; }
      i = leitor.nLinhas++;
      leitor.linhas[i].inicio = (size_t)(inicio - leitor.texto);
      leitor.linhas[i].n = n;
      if (*p == '\n') p++;
      // The minimum sheet width accommodates ordinary glyphs. In case of a
      // malformed/wider glyph, still advance and keep drawing inside scissor.
      if (p == inicio && *p) p += proximo(p);
    }
  }
  int primeira = lead > 0 ? (int)fmaxf(0, floorf(leitor.scroll / lead) - 1) : 0;
  int ultima = lead > 0 ? (int)ceilf((leitor.scroll + leitor.pagina) / lead) + 1 : leitor.nLinhas;
  if (ultima > leitor.nLinhas) ultima = leitor.nLinhas;
  if (desenhar) for (i = primeira; i < ultima; i++) {
    float top = (float)i * lead;
    size_t n = leitor.linhas[i].n;
    if (top + lead < leitor.scroll || top > leitor.scroll + leitor.pagina) continue;
    memcpy(linha, leitor.texto + leitor.linhas[i].inicio, n); linha[n] = 0;
    txt_desenhar_alpha(txt_linha(estilo, linha, 243, 242, 239, 255), x, y + top - leitor.scroll, 1);
  }
  return (float)leitor.nLinhas * lead;
}
void sinopse_desenhar(void) {
  float w, h, pad, x, y, lead, altura;
  GfxRect folha, texto, fechar, toqueFechar;
  TxtLinha cab, titulo, botao;
  TxtEstilo estilo = sinopse_estilo(TXT_DET_SIN);
  if (!sinopse_aberta()) return;
  ESCALA_INI();
  w = fminf(1100, NV_TELA_W - 48);
  h = leitor.comentario ? fminf(860, NV_TELA_H - 96) : NV_TELA_H - 48;
  pad = fminf(40, w * .06f); x = (NV_TELA_W - w) * .5f; y = (NV_TELA_H - h) * .5f;
  folha = (GfxRect){ x, y, w, h };
  cab = txt_linha_corta(TXT_AJ_INSP, leitor.comentario ? leitor.titulo : i18n("Ler sinopse"), 243, 242, 239, 255, w - 2 * pad - (leitor.comentario ? 240 : 0));
  titulo = txt_linha_corta(TXT_ILHA_SUB, leitor.comentario ? leitor.data : leitor.titulo, 243, 242, 239, 255, w - 2 * pad);
  botao = txt_linha_corta(TXT_PG_ROTULO, i18n("Fechar"), 243, 242, 239, 255, w - 2 * pad - 40);
  { float visualH = fmaxf(56, botao.h + 24), alvoH = visualH;
#ifdef NV_TOUCH_UI
    // The Mobile canvas normalizes the short edge to1080. Reserve48dp on a
    // 360dp phone, independently of UI zoom, without covering scrollable text.
    if (layout_modo_mobile()) alvoH = fmaxf(alvoH, 144.0f / gfx_escala());
#endif
    toqueFechar = (GfxRect){ x + pad, y + h - pad - alvoH, w - 2 * pad, alvoH };
    fechar = toqueFechar; fechar.y += (alvoH - visualH) * .5f; fechar.h = visualH;
  }
  texto = (GfxRect){ x + pad, y + pad + cab.h + 8 + titulo.h + 24, w - 2 * pad - 12, 0 };
  texto.h = toqueFechar.y - 24 - texto.y;
  if (texto.h < 1) texto.h = 1;
  lead = sinopse_entrelinha(TXT_DET_SIN, 40);
  leitor.pagina = texto.h; leitor.passo = lead;
  if (leitor.comentario) {
    texto.y = y + pad + 112; texto.h = toqueFechar.y - 48 - texto.y;
    Uint32 now = SDL_GetTicks();
    float dt = leitor.tick ? fminf((now - leitor.tick) * .001f, .08f) : 0;
    leitor.tick = now;
    leitor.scroll += (leitor.destino - leitor.scroll) *
      (ajustes_animacoes_reduzidas() ? 1 : 1 - expf(-18 * dt));
    if (fabsf(leitor.destino - leitor.scroll) < .1f) leitor.scroll = leitor.destino;
  }
  altura = leitor.oculto ? 0 : textoCompleto(estilo, texto.x, texto.y, texto.w, lead, 0);
  leitor.max = fmaxf(0, altura - texto.h);
  leitor.destino = fmaxf(0, fminf(leitor.max, leitor.destino));
  leitor.scroll = fmaxf(0, fminf(leitor.max, leitor.scroll));
  ponteiro_camada();
  ponteiro_alvo(0, 0, NV_TELA_W, NV_TELA_H, NULL, NULL, 0, 0);
  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0, 0, 0, 0, .65f);
  if (leitor.comentario) {
    gfx_cor(folha, 28 / fminf(w, h), .082f, .094f, .118f, 1);
    gfx_anel(folha, 28 / fminf(w, h), 1, 1, 1, 1, .10f);
    GfxRect avatar = {x + pad, y + pad, 64, 64};
    gfx_cor(avatar, .5f, .20f, .25f, .32f, 1);
    char inicial[8] = "";
    size_t n = leitor.titulo[0] ? proximo(leitor.titulo) : 0;
    if (n < sizeof inicial) memcpy(inicial, leitor.titulo, n);
    TxtLinha ini = txt_linha(TXT_ILHA_NOME, inicial, 243, 242, 239, 255);
    txt_desenhar_alpha(ini, avatar.x + (64 - ini.w) * .5f, avatar.y + (64 - ini.h) * .5f, 1);
    txt_desenhar_alpha(cab, x + pad + 84, y + pad, 1);
    txt_desenhar_alpha(titulo, x + pad + 84, y + pad + cab.h + 8, .65f);
    GLuint marca = tex_obter(extras_caminho_marca_nome("trakt_wordmark"));
    if (marca) gfx_rect((GfxRect){x + w - pad - 104, y + pad + 8, 104, 32}, marca, GFX_MARCA, 0, 0, 0, 0, 1, 1, 1, 1);
    else txt_desenhar_alpha(txt_linha(TXT_ILHA_NOME, "Trakt", 243, 242, 239, 255), x + w - pad - 84, y + pad + 8, 1);
    char stats[128], counter[32];
    snprintf(stats, sizeof stats, i18n("%d curtidas · %d respostas"), leitor.curtidas, leitor.respostas);
    txt_desenhar_alpha(txt_linha(TXT_ILHA_SUB, stats, 170, 179, 195, 255), texto.x, texto.y + texto.h + 12, 1);
    if (leitor.nota > 0) {
      gfx_icone((GfxRect){texto.x + texto.w - 112, texto.y + texto.h + 13, 24, 24}, "aj_heart", .9f, .45f, .5f, 1);
      snprintf(counter, sizeof counter, "%d/10", leitor.nota);
      txt_desenhar_alpha(txt_linha(TXT_ILHA_NOME, counter, 243, 242, 239, 255), texto.x + texto.w - 76, texto.y + texto.h + 10, 1);
    }
    snprintf(counter, sizeof counter, "%d / %d", leitor.posicao, leitor.total);
    TxtLinha pos = txt_linha(TXT_ILHA_SUB, counter, 170, 179, 195, 255);
    txt_desenhar_alpha(pos, x + w - pad - pos.w, y + pad + 62, 1);
  } else {
    plrui_material(folha, 28, 1, 1);
    txt_desenhar_alpha(cab, x + pad, y + pad, 1);
    txt_desenhar_alpha(titulo, x + pad, y + pad + cab.h + 8, .65f);
  }
  if (leitor.oculto) {
    TxtLinha aviso = txt_linha_corta(TXT_DET_SIN, i18n("Spoiler — OK para revelar"), 243, 242, 239, 255, texto.w);
    txt_desenhar_alpha(aviso, texto.x + (texto.w - aviso.w) * .5f, texto.y + (texto.h - aviso.h) * .5f, 1);
  } else {
    gfx_recorte(texto.x, texto.y, texto.w, texto.h);
    textoCompleto(estilo, texto.x, texto.y, texto.w, lead, 1);
    gfx_sem_recorte();
  }
  if (leitor.max > 0) {
    float bh = fmaxf(20, texto.h * texto.h / altura);
    gfx_cor((GfxRect){ x + w - pad - 4, texto.y + (texto.h - bh) * leitor.scroll / leitor.max, 4, bh }, .5f, 1, 1, 1, .4f);
  }
  plrui_linha_foco(fechar, 20, 1);
  txt_desenhar_alpha(botao, fechar.x + (fechar.w - botao.w) * .5f, fechar.y + (fechar.h - botao.h) * .5f, 1);
  ponteiro_alvo(toqueFechar.x, toqueFechar.y, toqueFechar.w, toqueFechar.h, NULL, fecharPonteiro, 0, 0);
#ifdef NV_TOUCH_UI
  toquerol_vincular(&leitor.toque, texto, gfx_escala(), 0, leitor.max, 1, &leitor.scroll);
  ponteiro_rolagem(rolar);
#endif
  ESCALA_FIM();
}
