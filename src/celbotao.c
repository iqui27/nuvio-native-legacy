// Ver celbotao.h para o porque.
#include "celbotao.h"
#include "celular.h"
#include "text.h"
#include "anim.h"
#include "layout.h"
#include "ajustes.h"
#include "idioma.h"
#include "qr.h"
#define NV_ESCALA_TELA   // o arquivo inteiro mede pela tela virtual (escala.h)
#include "escala.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// O CARTAO: 380 de largura cabe o QR de 248 com folga para o endereco numa
// linha em TXT_CAPTION ("192.168.100.200:65535/abcdefgh" mede ~330).
#define CB_W      400.0f
#define CB_PAD     30.0f
#define CB_QR     248.0f
#define CB_MOLDURA 12.0f
#define CB_VAO     14.0f   // entre o botao e o cartao

static int     dono, aberto;
// HOSPEDADO (celbotao.h): o dono desenha o QR dentro da propria superficie
// (celb_desenhar_em) e o cartao flutuante nao existe.
static int     embutido;
static float   animCartao;            // 0..1 (mola), entrando
static GfxRect ancora[CELB_N];        // ultimo botao desenhado de cada dono
static float   animBotao[CELB_N];
static int     focoBotao[CELB_N];     // o que o ultimo desenho pediu
static char    titulo[96];
static GfxRect cartao;

int celb_disponivel(void) { return celular_disponivel(); }
int celb_aberto(void) { return aberto; }
int celb_dono(void) { return aberto ? dono : CELB_NENHUM; }
GfxRect celb_cartao_rect(void) { return aberto ? cartao : (GfxRect){ 0, 0, 0, 0 }; }

void celb_atualizar(float dt) {
  int d;
  for (d = 1; d < CELB_N; d++) {
    float alvo = (focoBotao[d] || (aberto && dono == d)) ? 1.0f : 0.0f;
    animBotao[d] = ajustes_animacoes_reduzidas() ? alvo : anim_mola(animBotao[d], alvo, dt, NV_MOLA_FOCO);
    focoBotao[d] = 0;   // o proximo desenho repoe; botao que sumiu apaga
  }
  if (!aberto) { animCartao = 0.0f; return; }
  animCartao = ajustes_animacoes_reduzidas() ? 1.0f : anim_mola(animCartao, 1.0f, dt, NV_MOLA_TELA);
}

void celb_botao(int d, GfxRect r, int focado, PonteiroFn focar, int a, int b, float alpha) {
  float ar, ag, ab, k, esc;
  GfxRect c;
  int t;
  if (d <= CELB_NENHUM || d >= CELB_N || !celular_disponivel()) return;
  // Guardado na tela REAL: o botao pode estar numa camada ampliada (Spotlight)
  // ou nao (Busca, teclado); o cartao converte para a dele (escala.h).
  { float e = gfx_escala(); ancora[d] = (GfxRect){ r.x * e, r.y * e, r.w * e, r.h * e }; }
  // A mola anda em celb_atualizar; com o cartao aberto o botao fica aceso.
  focoBotao[d] = focado;
  k = animBotao[d];
  if (ponteiro_ativo()) ponteiro_alvo(r.x - 4.0f, r.y - 4.0f, r.w + 8.0f, r.h + 8.0f, focar, NULL, a, b);
  ajustes_acento(&ar, &ag, &ab);
  esc = 1.0f + 0.06f * k;
  c = (GfxRect){ r.x - r.w * (esc - 1.0f) * 0.5f, r.y - r.h * (esc - 1.0f) * 0.5f, r.w * esc, r.h * esc };
  // A MESMA FAMILIA DO FALAR (spotlight.c, busca.c, teclado.c): disco de
  // 0,2 de cinza em repouso, cheio na cor do realce no foco, com a mancha
  // difusa curta. Sem aro (ilha 2.0): o disco claro sobre a arte ja diz "botao".
  if (k > 0.01f)
    gfx_rect((GfxRect){ c.x - 12, c.y - 12, c.w + 24, c.h + 24 }, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f,
             ar, ag, ab, 0.30f * k * alpha);
  gfx_cor(c, 0.5f, anim_mistura(0.18f, ar, k), anim_mistura(0.185f, ag, k),
          anim_mistura(0.205f, ab, k), alpha);
  t = k > 0.5f ? ajustes_tinta_foco() : 224;
  gfx_icone((GfxRect){ c.x + c.w * 0.26f, c.y + c.h * 0.26f, c.w * 0.48f, c.h * 0.48f }, "aj_smartphone",
            t / 255.0f, t / 255.0f, t / 255.0f, alpha);
}

int celb_abrir(int d, const char *t) {
  if (d <= CELB_NENHUM || d >= CELB_N || !celular_disponivel()) return 0;
  snprintf(titulo, sizeof titulo, "%s", t ? t : "");
  dono = d;
  aberto = 1;
  embutido = 0;
  animCartao = 0.0f;
  // Sem rede o cartao abre assim mesmo e diz por que nao ha codigo: um OK que
  // nao faz nada pareceria botao quebrado.
  celular_abrir(titulo);
  printf("[celular] cartao aberto (dono %d)\n", d);
  return 1;
}

int celb_abrir_embutido(int d, const char *t) {
  if (!celb_abrir(d, t)) return 0;
  embutido = 1;
  return 1;
}
int celb_embutido(int d) { return aberto && embutido && dono == d; }

void celb_fechar(void) {
  if (!aberto) return;
  aberto = 0;
  embutido = 0;
  celular_fechar();
}
void celb_fechar_dono(int d) { if (aberto && dono == d) celb_fechar(); }

int celb_pegar(int d, char *dst, size_t n) {
  if (!aberto || dono != d) return 0;
  if (!celular_pegar(dst, n)) return 0;
  celb_fechar();
  return 1;
}

static void regerar(void) {
  if (aberto) celular_abrir(titulo);
}
static void fecharPonteiro(int a, int b) { (void)a; (void)b; celb_fechar(); }
static void regerarPonteiro(int a, int b) { (void)a; (void)b; regerar(); }

int celb_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (!aberto) return 0;
  if (embutido) {
    // HOSPEDADO: o QR e um pedaco da tela do dono, nao uma camada por cima —
    // so o Voltar e dele (fecha o QR, nao a tela); o resto segue para o dono,
    // que continua navegavel com o codigo a vista.
    if (e->type != SDL_KEYDOWN) return 0;
    k = e->key.keysym.sym;
    if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE || k == SDLK_DELETE ||
        e->key.keysym.scancode == NV_SCANCODE_BACK) {
      celb_fechar();
      return 1;
    }
    return 0;
  }
  if (e->type == SDL_TEXTINPUT || e->type == SDL_TEXTEDITING) return 1;
  if (e->type == SDL_KEYUP) return 1;
  if (e->type != SDL_KEYDOWN) return 0;
  k = e->key.keysym.sym;
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE || k == SDLK_DELETE ||
      e->key.keysym.scancode == NV_SCANCODE_BACK) {
    celb_fechar();
    return 1;
  }
  if ((k == SDLK_RETURN || k == SDLK_KP_ENTER) && !e->key.repeat) {
    // OK com o codigo na tela nao faz nada: quem aperta OK ali esta
    // "confirmando", e fechar levaria o codigo embora antes do envio.
    if (celular_estado() != CEL_ESPERANDO) regerar();
    return 1;
  }
  return 1;
}

// --- desenho ------------------------------------------------------------------
// QR como TEXTURA (como login.c): a versao 3-4 tem ~1000 modulos, e um
// retangulo por modulo por quadro custaria mais que o cartao inteiro. NEAREST:
// modulo borrado e o jeito mais rapido de a camera nao ler.
static GLuint texQr;
static char   texQrDe[96];
static void qrTextura(const char *u) {
  Qr q;
  int lado, x, y;
  unsigned char *px;
  if (!u[0] || (texQr && !strcmp(texQrDe, u))) return;
  if (!qr_gerar(&q, u)) return;
  lado = q.lado + 4;
  px = (unsigned char *)malloc((size_t)lado * lado * 3);
  if (!px) return;
  memset(px, 255, (size_t)lado * lado * 3);
  for (y = 0; y < q.lado; y++)
    for (x = 0; x < q.lado; x++)
      if (qr_modulo(&q, x, y)) {
        size_t i = ((size_t)(y + 2) * lado + (x + 2)) * 3;
        px[i] = px[i + 1] = px[i + 2] = 0;
      }
  if (!texQr) glGenTextures(1, &texQr);
  glBindTexture(GL_TEXTURE_2D, texQr);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, lado, lado, 0, GL_RGB, GL_UNSIGNED_BYTE, px);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  free(px);
  snprintf(texQrDe, sizeof texQrDe, "%s", u);
}

// A moldura branca e o QR, com o canto de cima em (x, y): o MESMO desenho no
// cartao e hospedado na tela do dono.
static void qrDesenhar(const char *u, float x, float y, float a) {
  qrTextura(u);
  if (!texQr) return;
  gfx_cor((GfxRect){ x, y, CB_QR + 2 * CB_MOLDURA, CB_QR + 2 * CB_MOLDURA }, 0.06f, 1.0f, 1.0f, 1.0f, a);
  gfx_tex_aspect_atual = 0.0f;
  gfx_rect((GfxRect){ x + CB_MOLDURA, y + CB_MOLDURA, CB_QR, CB_QR }, texQr, GFX_SNAP, 0, 0.0f, 0.0f, 0.0f, 0, 0, 0, a);
}
void celb_desenhar_qr_em(int d, GfxRect r, float alpha) {
  if (!celb_embutido(d)) return;
  const char *u = celular_url();
  if (!u[0]) {
    txt_bloco(TXT_CAPTION2, i18n("Sem rede local. Conecte a TV ao Wi-Fi e aperte OK."),
              190, 190, 200, r.x, r.y, r.w, 26, alpha, 4);
    return;
  }
  qrTextura(u);
  if (!texQr) return;
  gfx_tex_aspect_atual = 0;
  gfx_rect(r, texQr, GFX_SNAP, 0, 0, 0, 0, 0, 0, 0, alpha);
}
static const char *FRASE_QR = "Aponte a câmera do celular. Mesma rede Wi-Fi, vale por 5 minutos.";
static const char *msgSemQr(int est) {
  return est == CEL_FALHOU ? "Endereço bloqueado por tentativas erradas. Aperte OK para gerar outro."
       : est == CEL_EXPIROU ? "O endereço expirou. Aperte OK para gerar outro."
       : "Sem rede local. Conecte a TV ao Wi-Fi e aperte OK.";
}

// Altura do miolo, pela mesma conta que o desenho usa.
#define CB_TITULO_H   40.0f
#define CB_LINHA_H    26.0f
static float alturaCartao(int temQr) {
  if (!temQr) return CB_PAD + CB_TITULO_H + 16.0f + 3 * CB_LINHA_H + CB_PAD;
  return CB_PAD + CB_TITULO_H + 18.0f + CB_QR + 2 * CB_MOLDURA + 18.0f + 30.0f + 10.0f +
         3 * CB_LINHA_H + CB_PAD - 6.0f;
}

static void celb_desenharCorpo_(void);
// Camada ampliada (escala.h): o corpo desenha na tela virtual.
void celb_desenhar(void) {
  ESCALA_INI();
  celb_desenharCorpo_();
  ESCALA_FIM();
}
static void celb_desenharCorpo_(void) {
  float a, x, y, w, h, ar, ag, ab;
  GfxRect r;
  int est, temQr;
  const char *u, *curta;
  if (!aberto || embutido) return;   // hospedado: quem desenha e o dono
  a = anim_suave(animCartao);
  est = celular_estado();
  u = celular_url();
  temQr = est == CEL_ESPERANDO && u[0];
  r = ancora[dono];
  { float e = gfx_escala_ui(); r.x /= e; r.y /= e; r.w /= e; r.h /= e; }
  w = CB_W; h = alturaCartao(temQr);
  // ANCORADO NO BOTAO: abaixo dele, com a borda direita alinhada a dele; sem
  // espaco embaixo, em cima; nunca fora da tela.
  x = r.x + r.w - w;
  y = r.y + r.h + CB_VAO;
  if (y + h > NV_TELA_H - 24.0f) y = r.y - CB_VAO - h;
  if (y < 24.0f) y = 24.0f;
  if (x < 32.0f) x = 32.0f;
  if (x + w > NV_TELA_W - 32.0f) x = NV_TELA_W - 32.0f - w;
  y += (1.0f - a) * -10.0f;
  cartao = (GfxRect){ x, y, w, h };

  // CAMADA: clique fora fecha, clique no cartao vencido gera outro codigo.
  ponteiro_camada();
  if (ponteiro_ativo()) {
    ponteiro_alvo(0, 0, NV_TELA_W, NV_TELA_H, NULL, fecharPonteiro, 0, 0);
    ponteiro_alvo(x, y, w, h, NULL, temQr ? NULL : regerarPonteiro, 0, 0);
  }
  ajustes_acento(&ar, &ag, &ab);
  // ILHA 2.0 (a mesma de salvospainel.c): sombra curta, miolo de vidro ou
  // solido, a luz do canto de cima e NENHUM aro.
  { const int vid = ajustes_vidro();
    const float raio = 28.0f / h;
    gfx_rect((GfxRect){ x - 18.0f, y - 8.0f, w + 36.0f, h + 40.0f }, 0, GFX_SOMBRA,
             1.0f, 0, 0, 0.5f, 0, 0, 0, 0.42f * a);
    if (vid) gfx_vidro_material(cartao, raio, GFX_VIDRO_DENS, a);
    else gfx_cor(cartao, raio, .098f, .102f, .118f, .99f * a);
    if (!vid) gfx_luz_canto(cartao, raio, w * .25f, -h * .25f, w * .9f, ar, ag, ab, .08f * a); }

  { TxtLinha t = txt_linha_corta(TXT_CALLOUT, i18n("Digitar pelo celular"), 245, 248, 255, 255,
                                 w - 2 * CB_PAD - 40.0f);
    float ty = y + CB_PAD + (CB_TITULO_H - t.h) * 0.5f;
    gfx_icone((GfxRect){ x + CB_PAD, y + CB_PAD + (CB_TITULO_H - 28.0f) * 0.5f, 28.0f, 28.0f }, "aj_smartphone",
              ar, ag, ab, a);
    txt_desenhar_alpha(t, x + CB_PAD + 40.0f, ty, a); }
  y += CB_PAD + CB_TITULO_H;

  if (!temQr) {
    txt_bloco(TXT_BODY, i18n(msgSemQr(est)), 200, 204, 212, x + CB_PAD, y + 16.0f, w - 2 * CB_PAD, CB_LINHA_H + 6.0f, a, 3);
    return;
  }
  y += 18.0f;
  qrDesenhar(u, x + (w - CB_QR) * 0.5f - CB_MOLDURA, y, a);
  y += CB_QR + 2 * CB_MOLDURA + 18.0f;
  // ENDERECO CURTO, para quem nao tem camera: sem "http://" (o navegador do
  // celular completa sozinho), centrado.
  curta = !strncmp(u, "http://", 7) ? u + 7 : u;
  { TxtLinha t = txt_linha_corta(TXT_CAPTION, curta, 232, 236, 244, 255, w - 2 * CB_PAD);
    txt_desenhar_alpha(t, x + (w - t.w) * 0.5f, y, a); }
  y += 30.0f + 10.0f;
  txt_bloco(TXT_CAPTION2, i18n(FRASE_QR), 170, 174, 184, x + CB_PAD, y, w - 2 * CB_PAD, CB_LINHA_H, a * 0.9f, 3);
}

// --- hospedado: o mesmo QR, dentro da superficie do dono ----------------------
// TRES ARRANJOS, do mais largo ao mais apertado. O QR nunca encolhe: 248 px e
// o tamanho medido para a camera a 3 m, e e o que o cartao usa.
//   LADO    QR a esquerda, a frase a direita, o endereco embaixo (coluna larga)
//   PILHA   QR, endereco, frase
//   CURTO   QR e endereco (quando nem a pilha cabe na altura que o dono tem)
// O endereco fica sempre numa linha propria, da largura toda: e a saida de
// quem nao tem camera, e cortado nao serve para nada.
#define CB_E_QR     (CB_QR + 2 * CB_MOLDURA)
#define CB_E_VAO    22.0f
#define CB_E_LADO_MIN 230.0f   // largura minima da frase ao lado do QR
#define CB_E_URL_H  (14.0f + 30.0f)
enum { CB_E_MSG = 0, CB_E_LADO, CB_E_PILHA, CB_E_CURTO };
static int embArranjo(float w, float hMax, float *h) {
  int est = celular_estado();
  float frase;
  if (!(est == CEL_ESPERANDO && celular_url()[0])) {
    *h = txt_bloco(TXT_BODY, i18n(msgSemQr(est)), 0, 0, 0, 0, 0, w, CB_LINHA_H + 6.0f, 0.0f, 3);
    return CB_E_MSG;
  }
  if (w >= CB_E_QR + CB_E_VAO + CB_E_LADO_MIN) { *h = CB_E_QR + CB_E_URL_H; return CB_E_LADO; }
  frase = txt_bloco(TXT_CAPTION2, i18n(FRASE_QR), 0, 0, 0, 0, 0, w, CB_LINHA_H, 0.0f, 3);
  *h = CB_E_QR + CB_E_URL_H + 8.0f + frase;
  if (*h <= hMax) return CB_E_PILHA;
  *h = CB_E_QR + CB_E_URL_H;
  return CB_E_CURTO;
}
float celb_embutido_altura(int d, float w, float hMax) {
  float h = 0.0f;
  if (!celb_embutido(d)) return 0.0f;
  embArranjo(w, hMax, &h);
  return h;
}
void celb_desenhar_em(int d, GfxRect r, float alpha) {
  float a, h, y = r.y;
  int arr;
  const char *u, *curta;
  if (!celb_embutido(d)) return;
  a = anim_suave(animCartao) * alpha;
  arr = embArranjo(r.w, r.h, &h);
  cartao = (GfxRect){ r.x, r.y, r.w, h };
  if (arr == CB_E_MSG) {
    // Vencido, bloqueado ou sem rede: o clique na frase gera outro, como o OK.
    if (ponteiro_ativo()) ponteiro_alvo(r.x, r.y, r.w, h, NULL, regerarPonteiro, 0, 0);
    txt_bloco(TXT_BODY, i18n(msgSemQr(celular_estado())), 200, 204, 212, r.x, y, r.w, CB_LINHA_H + 6.0f, a, 3);
    return;
  }
  u = celular_url();
  qrDesenhar(u, r.x, y, a);
  if (arr == CB_E_LADO) {
    float tx = r.x + CB_E_QR + CB_E_VAO, tw = r.w - CB_E_QR - CB_E_VAO;
    float fh = txt_bloco(TXT_CAPTION2, i18n(FRASE_QR), 0, 0, 0, 0, 0, tw, CB_LINHA_H, 0.0f, 5);
    txt_bloco(TXT_CAPTION2, i18n(FRASE_QR), 170, 174, 184, tx, y + (CB_E_QR - fh) * 0.5f, tw, CB_LINHA_H, a * 0.9f, 5);
  }
  y += CB_E_QR + 14.0f;
  curta = !strncmp(u, "http://", 7) ? u + 7 : u;
  { TxtLinha t = txt_linha_corta(TXT_CAPTION, curta, 232, 236, 244, 255, r.w);
    txt_desenhar_alpha(t, r.x, y, a); }
  y += 30.0f + 8.0f;
  if (arr == CB_E_PILHA)
    txt_bloco(TXT_CAPTION2, i18n(FRASE_QR), 170, 174, 184, r.x, y, r.w, CB_LINHA_H, a * 0.9f, 3);
}
