#include "login.h"
#include "iconeapp.h"
#include "logoapp.h"
#include "sessao.h"
#include "nuvem.h"
#include "qr.h"
#include "gfx.h"
#include "text.h"
#include "anim.h"
#include "layout.h"
#include "teclado.h"
#include "ponteiro.h"
#include "ajustes.h"
#include "idioma.h"
#include <SDL2/SDL_image.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// O QR e o caminho principal, e nao por gosto. MEDIDO na resposta do servidor:
// o codigo tem 32 digitos hexadecimais e a URL ~63 caracteres. Ninguem
// transcreve isso da TV para o celular sem errar — sem QR, esta tela nao
// funciona.
#define LG_QR_LADO       440.0f
#define LG_BLOCO_W      lgBlocoW()
#define LG_PILL_W        360.0f
#define LG_PILL_H         76.0f
#define LG_EMAIL_PILL_W  560.0f
#define LG_CAMPO_W       760.0f
#define LG_CAMPO_H        88.0f
#define LG_MOSTRAR_W     150.0f
// Zona de silencio: 4 modulos claros em volta, exigidos pela norma. Vao DENTRO
// da textura para que nenhum ajuste de layout possa comer a margem por
// acidente — sem ela, leitor nenhum acha o simbolo.
#define LG_QR_MARGEM       4
static float lgBlocoW(void) {
#ifdef NV_TOUCH_UI
  if (NV_TELA_H > NV_TELA_W) return NV_TELA_W - 2.0f * NV_MARGEM_X;
#endif
  return 1100.0f;
}

static float animBotao;
static float pulso;

// LOGIN POR E-MAIL E SENHA (#216). O QR continua sendo o caminho da TV; o
// e-mail entra como opcao ao lado dele, para quem esta num celular/tablet
// Android (escanear a propria tela nao da) ou nao tem outro aparelho. Os
// campos abrem a modal do teclado (teclado.h), que no Android chama o teclado
// do sistema e na TV oferece a grade, o ditado e o celular.
enum { LG_QR = 0, LG_EMAIL };
enum { LE_EMAIL = 0, LE_SENHA, LE_MOSTRAR, LE_ENTRAR, LE_QR, LE_N };
static int modo, foco, focoQr;
static int campoAberto = -1;
static char email[TECLADO_LONGO + 1], senha[TECLADO_LONGO + 1];
static char aviso[160];          // erro local ("preencha..."), traduzido no desenho
static float animFoco[LE_N], animFocoQr[2];
static int esperando;            // pedido de e-mail saiu, resposta ainda nao veio
static int senhaVisivel;         // "Mostrar" na senha (campo e modal)

// O e-mail: minusculas (o servidor compara sem caixa), digitos e o que um
// endereco usa. A senha: todo ASCII imprimivel, com espaco — e a senha que a
// pessoa criou no site, nao uma que o app escolhe.
static const char *ALFA_EMAIL = "abcdefghijklmnopqrstuvwxyz0123456789@._-+";
static const char *ALFA_SENHA =
  "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
  " .,_-@!#$%&*+=?/\\:;'\"()[]{}<>^~`|";

static void apagarTexto(char *p, size_t n) { volatile char *v = p; while (n--) *v++ = 0; }

// Quantos botoes a tela do QR tem agora: "Tentar de novo" so existe no erro.
static int qrTemRetry(void) {
  SesEstado st = sessao_estado();
  return st == SES_ERRO || st == SES_DESLOGADO;
}
static int qrN(void) { return nuvem_pronta() ? (qrTemRetry() ? 2 : 1) : 0; }
// Indice do botao "e-mail" na tela do QR.
static int qrEmail(void) { return qrTemRetry() ? 1 : 0; }

static void abrirEmail(void) {
  modo = LG_EMAIL; foco = email[0] ? LE_SENHA : LE_EMAIL; aviso[0] = 0;
}
static void voltarQr(void) {
  modo = LG_QR; focoQr = 0; aviso[0] = 0;
  apagarTexto(senha, sizeof senha);
  if (!sessao_logada() && sessao_estado() != SES_AGUARDANDO &&
      sessao_estado() != SES_PEDINDO && sessao_estado() != SES_EMAIL)
    sessao_login_comecar();
}
static void abrirCampo(int c) {
  campoAberto = c;
  if (c == LE_EMAIL) {
    teclado_abrir_com("E-mail", "O e-mail da sua conta Nuvio.", 120, ALFA_EMAIL, email);
    teclado_tipo(TECLADO_TIPO_EMAIL);
  } else {
    // A senha VOLTA para a modal (em pontos): errar uma letra no fim de uma
    // senha longa nao pode obrigar a redigitar tudo no D-pad.
    teclado_abrir_com("Senha", "A senha da sua conta Nuvio. Ela não aparece na tela.", 128, ALFA_SENHA, senha);
    teclado_tipo(TECLADO_TIPO_SENHA);
    teclado_mascarar(!senhaVisivel);
  }
}
static void enviar(void) {
  if (sessao_estado() == SES_EMAIL) return;
  if (!email[0] || !senha[0]) {
    snprintf(aviso, sizeof aviso, "%s", "Preencha o e-mail e a senha.");
    foco = email[0] ? LE_SENHA : LE_EMAIL;
    return;
  }
  aviso[0] = 0;
  sessao_login_email(email, senha);
  esperando = sessao_estado() == SES_EMAIL;
  // A senha sai da memoria da tela ja: errada, a pessoa digita de novo.
  apagarTexto(senha, sizeof senha);
}
static void okEmail(void) {
  switch (foco) {
    case LE_EMAIL: case LE_SENHA: abrirCampo(foco); break;
    case LE_MOSTRAR: senhaVisivel = !senhaVisivel; break;
    case LE_ENTRAR: enviar(); break;
    default: voltarQr(); break;
  }
}

// Ponteiro e dedo: o alvo poe o foco; o OK (do ponteiro) faz o resto.
static void ptFocoEmail(int i, int b) { (void)b; if (modo == LG_EMAIL) foco = i; }
static void ptFocoQr(int i, int b) { (void)b; if (modo == LG_QR) focoQr = i; }

static GLuint texQr;
static char   qrDe[512];

// FUNDO (1.7.2): a arte das listras, a mesma familia do splash. A abertura
// dissolve o logo por cima dela (abertura_fundo_fica), entao a tela de login
// nasce do splash sem corte. 1280x720 ampliada: e fundo, nao precisa de mais.
// Carregada no primeiro desenho e solta ao entrar (login_soltar).
static char   fundoCam[600];
static GLuint texFundo;
static int    tentouFundo;

void login_recursos(const char *dirArte) {
  snprintf(fundoCam, sizeof fundoCam, "%s/marcas/login-fundo.jpg", dirArte ? dirArte : ".");
}

static void carregarFundo(void) {
  SDL_Surface *s, *c;
  tentouFundo = 1;
  if (!fundoCam[0]) return;
  s = IMG_Load(fundoCam);
  if (!s) { printf("[login] sem o fundo (%s)\n", fundoCam); return; }
  c = SDL_ConvertSurfaceFormat(s, SDL_PIXELFORMAT_ABGR8888, 0);   // RGBA em bytes
  SDL_FreeSurface(s);
  if (!c) return;
  glGenTextures(1, &texFundo);
  glBindTexture(GL_TEXTURE_2D, texFundo);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, c->w, c->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, c->pixels);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  gfx_tex_esquecer(0);   // o gfx guarda a ultima textura ligada
  SDL_FreeSurface(c);
}

int login_fundo_desenhar(float alfa) {
  if (!tentouFundo) carregarFundo();
  if (!texFundo) return 0;
  gfx_tex_aspect_atual = 0.0f;
  gfx_rect((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, texFundo, GFX_TEXTO,
           0, 0, 0, 0.0f, 1, 1, 1, alfa);
  return 1;
}

void login_soltar(void) {
  if (texFundo) { gfx_tex_esquecer(texFundo); glDeleteTextures(1, &texFundo); texFundo = 0; }
  tentouFundo = 0;
}   // conteudo ja desenhado, para nao refazer por quadro

// Sobe o simbolo como textura em vez de desenhar um retangulo por modulo: a
// versao 4 tem 33x33 = 1089 modulos, e mil chamadas de desenho por quadro
// custam mais que a tela inteira.
static void gerarTexQr(const char *texto) {
  Qr q;
  int n, lado, x, y;
  unsigned char *px;
  if (!texto || !texto[0]) return;
  if (!strcmp(qrDe, texto) && texQr) return;
  if (!qr_gerar(&q, texto)) { printf("[login] URL nao cabe num QR: %s\n", texto); return; }

  lado = q.lado + 2 * LG_QR_MARGEM;
  px = (unsigned char *)malloc((size_t)lado * lado * 3);
  if (!px) return;
  memset(px, 255, (size_t)lado * lado * 3);   // fundo claro, inclusive a margem
  for (y = 0; y < q.lado; y++)
    for (x = 0; x < q.lado; x++)
      if (qr_modulo(&q, x, y)) {
        size_t i = ((size_t)(y + LG_QR_MARGEM) * lado + (x + LG_QR_MARGEM)) * 3;
        px[i] = px[i + 1] = px[i + 2] = 0;
      }

  if (!texQr) glGenTextures(1, &texQr);
  glBindTexture(GL_TEXTURE_2D, texQr);
  // NEAREST, nao LINEAR: um modulo borrado com o vizinho e o jeito mais rapido
  // de tornar o simbolo ilegivel numa camera de celular.
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, lado, lado, 0, GL_RGB, GL_UNSIGNED_BYTE, px);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  free(px);
  n = snprintf(qrDe, sizeof qrDe, "%s", texto);
  (void)n;
}

void login_iniciar(void) {
  animBotao = 0.0f;
  pulso = 0.0f;
  modo = LG_QR; foco = LE_EMAIL; focoQr = 0; campoAberto = -1; aviso[0] = 0; esperando = 0;
  senhaVisivel = 0;
  apagarTexto(senha, sizeof senha);
  // Pedir o codigo JA, sem esperar o OK: a pessoa que acabou de instalar o app
  // nao tem nada para decidir nesta tela, e um botao "entrar" antes do codigo
  // so acrescenta um toque e uns segundos de espera depois dele.
  if (!sessao_logada()) sessao_login_comecar();
}

void login_evento(const SDL_Event *e) {
  SDL_Keycode k;
  int ok, voltar;
  if (teclado_aberto()) { teclado_evento(e); return; }
  if (e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  ok = (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) && !e->key.repeat;
  voltar = k == SDLK_AC_BACK || k == SDLK_ESCAPE || e->key.keysym.scancode == NV_SCANCODE_BACK;
  if (modo == LG_EMAIL) {
    if (voltar) { voltarQr(); return; }
    if (sessao_estado() == SES_EMAIL) return;   // esperando o servidor
    // Mostrar fica a direita da senha: cima/baixo pulam ele.
    if (k == SDLK_UP)
      foco = foco == LE_QR ? LE_ENTRAR : foco == LE_ENTRAR ? LE_SENHA : LE_EMAIL;
    else if (k == SDLK_DOWN)
      foco = foco == LE_EMAIL ? LE_SENHA : foco == LE_ENTRAR || foco == LE_QR ? LE_QR : LE_ENTRAR;
    else if (k == SDLK_RIGHT && foco == LE_SENHA) foco = LE_MOSTRAR;
    else if (k == SDLK_LEFT && foco == LE_MOSTRAR) foco = LE_SENHA;
    else if (ok) okEmail();
    return;
  }
  if (k == SDLK_UP && focoQr > 0) focoQr--;
  else if (k == SDLK_DOWN && focoQr + 1 < qrN()) focoQr++;
  else if (ok && qrN()) {
    if (focoQr >= qrN()) focoQr = qrN() - 1;
    if (focoQr == qrEmail()) { abrirEmail(); return; }
    // "Tentar de novo". OK so faz sentido quando ha o que refazer. Com o
    // codigo na tela o unico botao e o do e-mail: reiniciar o fluxo trocaria o
    // codigo que a pessoa acabou de digitar no celular.
    if (sessao_estado() == SES_ERRO || sessao_estado() == SES_DESLOGADO)
      sessao_login_comecar();
  }
}

void login_atualizar(float dt, Uint32 agora) {
  int i, r;
  sessao_passo((unsigned)agora);
  animBotao = anim_mola(animBotao, 1.0f, dt, NV_MOLA_FOCO);
  pulso += dt;
  teclado_atualizar(dt, agora);
  r = teclado_resultado();
  if (r != TECLADO_NADA && campoAberto >= 0) {
    int seguir = 0;
    if (campoAberto == LE_SENHA) senhaVisivel = !teclado_mascarado();
    if (r == TECLADO_PRONTO) {
      // PRONTO/ENTER AVANCA (#216): do e-mail direto para a senha, da senha
      // para Entrar. So com o controle, sao dois OK a menos.
      if (campoAberto == LE_EMAIL) {
        snprintf(email, sizeof email, "%s", teclado_texto());
        foco = LE_SENHA;
        seguir = 1;
      } else {
        snprintf(senha, sizeof senha, "%s", teclado_texto());
        foco = LE_ENTRAR;
      }
      aviso[0] = 0;
    }
    teclado_esquecer();
    campoAberto = -1;
    if (seguir) abrirCampo(LE_SENHA);
  }
  // Falhou: o foco vai para onde a pessoa conserta.
  // (O estado muda no fio do pedido, entre quadros: por isso a bandeira.)
  if (esperando && sessao_estado() != SES_EMAIL) {
    esperando = 0;
    if (modo == LG_EMAIL && !sessao_logada()) foco = LE_SENHA;
  }
  if (focoQr >= qrN() && qrN()) focoQr = qrN() - 1;
  for (i = 0; i < LE_N; i++)
    animFoco[i] = anim_mola(animFoco[i], modo == LG_EMAIL && foco == i && !teclado_aberto() ? 1.0f : 0.0f,
                            dt, NV_MOLA_FOCO);
  for (i = 0; i < 2; i++)
    animFocoQr[i] = anim_mola(animFocoQr[i], modo == LG_QR && focoQr == i ? 1.0f : 0.0f, dt, NV_MOLA_FOCO);
}

// Pilula de botao: cheia e clara no foco, vidro discreto fora dele.
static void pilula(float cx, float y, float w, const char *rot, float k, float alpha,
                   PonteiroFn focar, int i) {
  GfxRect pill = { cx - w * 0.5f, y, w, LG_PILL_H };
  TxtLinha t;
  int tom = k >= 0.5f ? 24 : 236;
  gfx_cor(pill, NV_RAIO_PILL, 1.0f, 1.0f, 1.0f, anim_mistura(0.12f, 0.92f, k) * alpha);
  t = txt_linha_corta(TXT_BODY, rot, tom, tom, tom + 2 > 255 ? 255 : tom + 2, 255, w - 48.0f);
  txt_desenhar_alpha(t, cx - t.w * 0.5f, y + (LG_PILL_H - t.h) * 0.5f, alpha);
  if (focar) ponteiro_alvo(pill.x, pill.y, pill.w, pill.h, focar, NULL, i, 0);
}

static void linhaCentrada(TxtEstilo est, const char *s, int r, int g, int b,
                          float y, float alpha) {
  TxtLinha l = txt_linha_corta(est, s, r, g, b, 255, LG_BLOCO_W);
  txt_desenhar_alpha(l, (NV_TELA_W - l.w) * 0.5f, y, alpha);
}
static float tituloLogin(float y) {
#ifdef NV_TOUCH_UI
  const char *s = i18n("Entrar na sua conta");
  if (NV_TELA_H > NV_TELA_W && txt_largura(TXT_TITULO1, s) > LG_BLOCO_W)
    return txt_bloco_corta(TXT_TITULO1, s, 255, 255, 255,
                          (NV_TELA_W - LG_BLOCO_W) * 0.5f, y, LG_BLOCO_W,
                          86.0f, 1.0f, 2) + 42.0f;
#endif
  linhaCentrada(TXT_TITULO1, "Entrar na sua conta", 255, 255, 255, y, 1.0f);
  return 118.0f;
}

// Campo da tela de e-mail: rotulo em cima, caixa com o valor (ou pontos).
static void campo(float y, int i, const char *rotulo, const char *valor, int oculto) {
  float x = (NV_TELA_W - LG_CAMPO_W) * 0.5f, k = animFoco[i];
  GfxRect cx = { x, y + 42.0f, LG_CAMPO_W, LG_CAMPO_H };
  float ar, ag, ab;
  TxtLinha r = txt_linha(TXT_CAPTION, rotulo, 176, 178, 186, 255);
  txt_desenhar_alpha(r, x + 4.0f, y, 1.0f);
  ajustes_acento(&ar, &ag, &ab);
  gfx_cor(cx, NV_RAIO_CARD, 1.0f, 1.0f, 1.0f, anim_mistura(0.07f, 0.13f, k));
  if (k > 0.01f)
    gfx_vidro_aro((GfxRect){ cx.x - 6, cx.y - 6, cx.w + 12, cx.h + 12 }, NV_RAIO_CARD, 2.5f,
                  ar, ag, ab, 0.95f * k);
  ponteiro_alvo(cx.x, y, cx.w, cx.h + 42.0f, ptFocoEmail, NULL, i, 0);
  if (valor[0]) {
    char pontos[TECLADO_LONGO * 3 + 1];
    TxtLinha t;
    float larg = cx.w - 48.0f - (i == LE_SENHA ? LG_MOSTRAR_W + 12.0f : 0.0f);
    if (oculto) {
      size_t j, n = strlen(valor);
      for (j = 0; j < n && j < TECLADO_LONGO; j++) memcpy(pontos + j * 3, "\xE2\x80\xA2", 3);
      pontos[j * 3] = 0;
    }
    // O valor e da pessoa; txt_linha procura traducao, e um e-mail nunca casa.
    t = txt_linha(TXT_HEADLINE, oculto ? pontos : valor, 240, 242, 248, 255);
    gfx_recorte(cx.x + 24.0f, cx.y, larg, cx.h);
    txt_desenhar_alpha(t, cx.x + 24.0f - (t.w > larg ? t.w - larg : 0.0f),
                       cx.y + (cx.h - t.h) * 0.5f, 1.0f);
    gfx_sem_recorte();
  }
}

static void desenharEmail(float y, Uint32 agora) {
  SesEstado st = sessao_estado();
  const char *falha = sessao_erro_email();
  (void)agora;
  linhaCentrada(TXT_BODY, "Use o e-mail e a senha da sua conta Nuvio.", 176, 178, 186, y, 1.0f);
  y += 84.0f;
  campo(y, LE_EMAIL, "E-mail", email, 0);
  y += 42.0f + LG_CAMPO_H + 30.0f;
  campo(y, LE_SENHA, "Senha", senha, !senhaVisivel);
  // MOSTRAR/OCULTAR dentro do campo, a direita (como nos sites).
  { float x = (NV_TELA_W + LG_CAMPO_W) * 0.5f - LG_MOSTRAR_W - 12.0f;
    float yb = y + 42.0f + (LG_CAMPO_H - 60.0f) * 0.5f, k = animFoco[LE_MOSTRAR];
    GfxRect b = { x, yb, LG_MOSTRAR_W, 60.0f };
    int tom = k >= 0.5f ? 24 : 220;
    TxtLinha t = txt_linha(TXT_CAPTION, senhaVisivel ? "Ocultar" : "Mostrar", tom, tom, tom, 255);
    gfx_cor(b, NV_RAIO_PILL, 1.0f, 1.0f, 1.0f, anim_mistura(0.10f, 0.92f, k));
    txt_desenhar_alpha(t, b.x + (b.w - t.w) * 0.5f, b.y + (b.h - t.h) * 0.5f, 1.0f);
    ponteiro_alvo(b.x, b.y, b.w, b.h, ptFocoEmail, NULL, LE_MOSTRAR, 0); }
  y += 42.0f + LG_CAMPO_H + 48.0f;
  pilula(NV_TELA_W * 0.5f, y, LG_EMAIL_PILL_W, st == SES_EMAIL ? "Entrando…" : "Entrar",
         animFoco[LE_ENTRAR], 1.0f, ptFocoEmail, LE_ENTRAR);
  y += LG_PILL_H + 24.0f;
  pilula(NV_TELA_W * 0.5f, y, LG_EMAIL_PILL_W, "Usar o código QR",
         animFoco[LE_QR], 1.0f, ptFocoEmail, LE_QR);
  y += LG_PILL_H + 36.0f;
  if (aviso[0]) linhaCentrada(TXT_BODY, aviso, 236, 108, 108, y, 1.0f);
  else if (st != SES_EMAIL && falha[0]) {
    // Ja vem traduzida de sessao.c (formato com i18n): nao passa de novo.
    TxtLinha l = txt_linha_corta(TXT_BODY, falha, 236, 108, 108, 255, LG_BLOCO_W);
    txt_desenhar_alpha(l, (NV_TELA_W - l.w) * 0.5f, y, 1.0f);
  }
}

void login_desenhar(Uint32 agora) {
  SesEstado st = sessao_estado();
  float y;
  (void)agora;

  { GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
    if (!login_fundo_desenhar(1.0f))
      gfx_cor(tela, 0.0f, NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, 1.0f); }
  // A tela inteira e a camada: nada de tras recebe toque.
  ponteiro_camada();
  ponteiro_alvo(0, 0, NV_TELA_W, NV_TELA_H, NULL, NULL, 0, 0);

  logoapp_marca((GfxRect){ NV_MARGEM_X, NV_MARGEM_Y, 72.0f, 72.0f }, 1.0f);

  y = 118.0f;
  y += tituloLogin(y);

  if (modo == LG_EMAIL && nuvem_pronta()) {
    desenharEmail(y, agora);
    teclado_desenhar(agora);
    return;
  }

  if (!nuvem_pronta()) {
    // Este caso e de COMPILACAO, nao do usuario: o pacote saiu sem a
    // configuracao do servidor. Dizer "erro ao entrar" mandaria a pessoa tentar
    // de novo para sempre contra algo que nunca vai funcionar.
    linhaCentrada(TXT_HEADLINE, "Este pacote foi montado sem servidor.",
                  236, 108, 108, y, 1.0f);
    linhaCentrada(TXT_BODY,
                  "Quem gerou o .ipk precisa informar a URL e a chave do projeto.",
                  176, 178, 186, y + 62.0f, 1.0f);
    return;
  }

  switch (st) {
    case SES_PEDINDO:
      linhaCentrada(TXT_HEADLINE, "Preparando o código…", 210, 212, 220, y, 1.0f);
      pilula(NV_TELA_W * 0.5f, y + 110.0f, LG_EMAIL_PILL_W, "Entrar com e-mail e senha",
             animFocoQr[0], 1.0f, ptFocoQr, 0);
      break;

    case SES_AGUARDANDO: {
      const char *url = sessao_url_login();
      linhaCentrada(TXT_BODY, "Aponte a câmera do celular para o código:",
                    176, 178, 186, y, 1.0f);
      y += 62.0f;

      gerarTexQr(url);
      if (texQr) {
        // Moldura clara um pouco maior que o simbolo: sobre o fundo escuro da
        // tela, a zona de silencio da textura sozinha ja bastaria, mas a
        // moldura arredondada faz o bloco ler como um cartao e nao como um
        // buraco branco no meio da tela.
        GfxRect moldura = { (NV_TELA_W - LG_QR_LADO - 32.0f) * 0.5f, y - 16.0f,
                            LG_QR_LADO + 32.0f, LG_QR_LADO + 32.0f };
        GfxRect r = { (NV_TELA_W - LG_QR_LADO) * 0.5f, y, LG_QR_LADO, LG_QR_LADO };
        gfx_cor(moldura, 0.06f, 1.0f, 1.0f, 1.0f, 1.0f);
        gfx_tex_aspect_atual = 0.0f;   // 1:1, sem recorte
        gfx_rect(r, texQr, GFX_SNAP, 0, 0.0f, 0.0f, 0.0f, 0, 0, 0, 1.0f);
      } else {
        linhaCentrada(TXT_HEADLINE, "não consegui desenhar o código",
                      236, 108, 108, y + 100.0f, 1.0f);
      }
      y += LG_QR_LADO + 42.0f;

      // O endereco em texto e a saida de emergencia de quem nao tem camera —
      // nao e o caminho principal, e por isso vem em corpo pequeno.
      if (url[0]) linhaCentrada(TXT_CAPTION, url, 150, 152, 160, y, 1.0f);
      y += 46.0f;

      // Sinal de vida. Sem ele a tela fica parada por minutos e parece travada
      // — e a pessoa reinicia o app no meio do login. Respiracao lenta (ciclo
      // de 2s), nao piscada: piscar em texto de espera le como alerta.
      { float a = 0.5f + 0.5f * sinf(pulso * 3.14159f);
        linhaCentrada(TXT_CAPTION, "Aguardando a autorização…",
                      150, 152, 160, y, 0.45f + 0.40f * a); }
      y += 64.0f;
      pilula(NV_TELA_W * 0.5f, y, LG_EMAIL_PILL_W, "Entrar com e-mail e senha",
             animFocoQr[0], 1.0f, ptFocoQr, 0);
      break;
    }

    case SES_TROCANDO:
      linhaCentrada(TXT_HEADLINE, "Autorizado. Entrando…", 210, 212, 220, y, 1.0f);
      break;

    case SES_LOGADO:
      linhaCentrada(TXT_HEADLINE, "Pronto.", 210, 212, 220, y, 1.0f);
      break;

    case SES_ERRO:
    case SES_DESLOGADO:
    default: {
      const char *msg = sessao_erro();
      linhaCentrada(TXT_HEADLINE, msg[0] ? msg : "Não consegui falar com o servidor.",
                    236, 108, 108, y, 1.0f);
      y += 96.0f;
      pilula(NV_TELA_W * 0.5f, y, LG_EMAIL_PILL_W, "Tentar de novo",
             animFocoQr[0], animBotao, ptFocoQr, 0);
      pilula(NV_TELA_W * 0.5f, y + LG_PILL_H + 24.0f, LG_EMAIL_PILL_W, "Entrar com e-mail e senha",
             animFocoQr[1], animBotao, ptFocoQr, 1);
      break;
    }
  }
}

int login_concluido(void) { return sessao_logada(); }
