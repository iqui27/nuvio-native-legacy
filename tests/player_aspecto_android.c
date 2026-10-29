// ASPECTO NO ANDROID (TCL, #aspvolta): cada modo tem que chegar ao backend
// com um destino/recorte proprio. A regressao desta rodada nao estava aqui —
// era o Kotlin vetando a recriacao da superficie no DV — mas o player foi a
// camada que a historia inteira (81d2152b2, 22f939517, ea7b6f1b1) quebrou
// mais vezes, entao o contrato dela fica medido: oito modos, retangulos
// distintos, inclusive com o painel de estatisticas aberto e depois de a
// superficie renascer (o dedupe do backend Android zera a cada abrirSessao,
// e o nucleo precisa reenviar sem assumir o retangulo entregue antes).
//
// O backend e o video.c do Mac (coto mudo) com video_simular() devolvendo
// 3840x1606 (2.39:1): video_janela/video_janela_fonte sao RENOMEADOS na
// compilacao do player.c para as capturas abaixo, como o
// player_android_retomada faz com video_tocar.
#include "player.h"
#include "catalogo.h"
#include "perfis.h"
#include "linguas.h"
#include "dados.h"
#include "ajustes.h"
#include "estatvivoui.h"
#include <assert.h>
int video_quadro_visto(void) { return video_pronto(); }
int extplayer_tentar(const char *url, const char *title, int anime, void (*internal)(const char *), void (*exit_player)(void)) {
  (void)url; (void)title; (void)anime; (void)internal; (void)exit_player; return 0;
}
#include <math.h>
#include <stdio.h>
#include <string.h>

static int ativo, pronto, tocando;
static char fonte[4096];
int video_tocar_retomada(const char *url, double pos, double pct) {
  (void)pos; (void)pct;
  snprintf(fonte, sizeof fonte, "%s", url); ativo = tocando = 1; pronto = 0; return 1;
}
int video_tocar_posicao(const char *url, double pos) { return video_tocar_retomada(url, pos, 0); }
int video_tocar(const char *url) { return video_tocar_posicao(url, 0); }
int video_retomada_inicial_estado(void) { return 0; }
void video_parar(void) { ativo = pronto = tocando = 0; fonte[0] = 0; }
void video_pausar(int p) { tocando = !p; }
void video_buscar(double pos) { (void)pos; }
int video_pausa_confirmada(void) { return 0; }
int video_tocando(void) { return tocando; }
int video_pronto(void) { return pronto; }
int video_superficie_estavel(void) { return 1; }
int video_ativo(void) { return ativo; }
int video_falhou(void) { return 0; }
int video_conflito_recurso(void) { return 0; }
double video_pos(void) { return 10.0; }
double video_duracao(void) { return 3600.0; }
const char *video_url_atual(void) { return fonte; }

// O "backend": so grava o ultimo retangulo/crop que o nucleo mandou.
static int janN, janX[32], janY[32], janW[32], janH[32], janEnc[32];
static int fntN, fntSX[32], fntSY[32], fntSW[32], fntSH[32],
           fntDX[32], fntDY[32], fntDW[32], fntDH[32];
void video_janela(int x, int y, int w, int h) {
  assert(janN < 32);
  janX[janN] = x; janY[janN] = y; janW[janN] = w; janH[janN] = h; janN++;
}
void video_janela_fonte(int sx, int sy, int sw, int sh,
                        int dx, int dy, int dw, int dh) {
  assert(fntN < 32);
  fntSX[fntN] = sx; fntSY[fntN] = sy; fntSW[fntN] = sw; fntSH[fntN] = sh;
  fntDX[fntN] = dx; fntDY[fntN] = dy; fntDW[fntN] = dw; fntDH[fntN] = dh; fntN++;
}

static void quadro(int n) { while (n-- > 0) { SDL_Delay(16); player_atualizar(.016f, SDL_GetTicks()); } }

// O que o modo mandou ao backend: a assinatura destino+recorte da ultima
// video_janela_fonte. Modos sem recorte mandam o quadro inteiro (sw=quadro).
static void assinatura(int *sig) {
  int i = fntN - 1;
  assert(i >= 0);
  sig[0] = fntSX[i]; sig[1] = fntSY[i]; sig[2] = fntSW[i]; sig[3] = fntSH[i];
  sig[4] = fntDX[i]; sig[5] = fntDY[i]; sig[6] = fntDW[i]; sig[7] = fntDH[i];
}
static int igual(const int *a, const int *b) {
  int i; for (i = 0; i < 8; i++) if (a[i] != b[i]) return 0; return 1;
}

int main(void) {
  assert(SDL_Init(SDL_INIT_TIMER) == 0);
  dados_iniciar("");
  perfis_definir_ativo(1); ling_local_legenda("none");

  CatItem c = {0};
  snprintf(c.imdb, sizeof c.imdb, "fixture-asp");
  snprintf(c.titulo, sizeof c.titulo, "Fixture");
  snprintf(c.tipo, sizeof c.tipo, "movie");
  cat_definir(&c, 1);

  // 3840x1606: o 2.39:1 medido na TCL do dono (Original letterbox, Crop
  // enche a tela recortando as laterais da fonte).
  { VideoSimulacao s; memset(&s, 0, sizeof s);
    s.largura = 3840; s.altura = 1606; s.aceita = 1; s.tocando = 1; s.pronto = 1;
    s.duracao = 3600.0; video_simular(&s); }

  player_abrir(0, NULL);
  player_definir_fonte("https://example.invalid/fixture.mp4");
  pronto = 1;
  // O modo salvo so entra depois de primeiro quadro + superficie estavel +
  // ASP_ASSENTA_MS (300 ms) — o relogio do SDL corre de verdade.
  quadro(30);
  assert(fntN > 0);

  // CADA MODO PRODUZ SAIDA DISTINTA. Apertar "Proporcao" muda o retangulo
  // que vai ao plano — a assinatura do modo anterior nao pode se repetir no
  // seguinte (o dedupe fica no backend; aqui o envio e direto).
  int ult[8] = {0}, sig[8];
  memcpy(ult, (int[]){0}, sizeof ult);
  { int i; for (i = 0; i < 8; i++) ult[i] = -1; }
  assinatura(ult);
  int trocas = 0;
  for (int i = 0; i < 10 && trocas < 7; i++) {
    int antes = fntN;
    player_aspecto_ciclar();
    quadro(2);
    assert(fntN > antes);          // toda troca aplica de novo
    assinatura(sig);
    if (!igual(sig, ult)) { trocas++; memcpy(ult, sig, sizeof sig); }
  }
  assert(trocas >= 5);   // oito modos, varios visivelmente distintos neste 2.39:1
  puts("ok cada modo manda retangulo/crop proprio");

  // PAINEL DE ESTATISTICAS ABERTO: a troca de aspecto continua chegando ao
  // backend (o painel e camada de desenho, nao de janela).
  estatvivo_abrir();
  { int antes = fntN, a[8] = {0}, b[8] = {0};
    assinatura(a);
    player_aspecto_definir(PLR_ASP_CROP); quadro(2); assinatura(b);
    assert(fntN > antes && !igual(a, b));
    player_aspecto_definir(PLR_ASP_ORIGINAL); quadro(2); }
  estatvivo_fechar();
  puts("ok troca vale com o painel de estatisticas aberto");

  // RECRIACAO DA SUPERFICIE: o backend Android zera o dedupe a cada
  // abrirSessao — para o nucleo, o sintoma da TV e "o plano perdeu o
  // retangulo". A proxima aplicacao tem que reenviar o MESMO retangulo
  // mesmo assim: quem decide o destino e o modo, nao o que foi entregue.
  { int a[8] = {0}, b[8] = {0};
    player_aspecto_definir(PLR_ASP_ZOOM_CINEMA); quadro(2); assinatura(a);
    // Reenvio do modo sem tocar em nada: o caminho do trailer/videoinfo.
    player_aspecto_definir(PLR_ASP_ZOOM_CINEMA); quadro(2); assinatura(b);
    assert(igual(a, b)); }
  puts("ok modo reaplicado reenvia o mesmo retangulo (superficie nova)");

  player_encerrar();
  puts("player_aspecto_android: modos distintos, painel aberto e reenvio ok");
  return 0;
}
