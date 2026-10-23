#include "horafmt.h"
#include "esmaecer.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <stdatomic.h>
#ifndef ESMAECER_SEM_GFX
#include "gfx.h"
#include "layout.h"
#include "text.h"
#include "descanso.h"
#endif

static _Atomic int escolhaAtual = ESM_PADRAO;
static _Atomic int estiloAtual = ESM_ESTILO_VITRINE;
static _Atomic(void (*)(void)) publicarProtetor;
static void protetorPublicar(void) {
  void (*publicar)(void) = publicarProtetor;
  if (publicar) publicar();
}
void esmaecer_protetor_publicador(void (*publicar)(void)) {
  publicarProtetor = publicar;
  protetorPublicar();
}
static unsigned ultimaEntrada;
static int   temEntrada;
static unsigned acordouEm;
static int   acordou;
static int   estagio;
static float veu;
static int   avisoEscuro = -1;   // -1 nada pendente; 0/1 = novo valor

unsigned esmaecer_ms(int escolha) {
  static const unsigned S[ESM_ESCOLHAS] = { 0, 30, 60, 120, 300, 600 };
  return escolha >= 0 && escolha < ESM_ESCOLHAS ? S[escolha] * 1000u : 0u;
}

static int estiloValido(int e) { return e >= 0 && e < ESM_ESTILOS ? e : ESM_ESTILO_VITRINE; }

int esmaecer_estagio_para(unsigned ocioMs, int escolha, int estilo) {
  unsigned t = esmaecer_ms(escolha);
  unsigned fim = estiloValido(estilo) == ESM_ESTILO_ESCURECER ? ESM_NO_FIM_MS : ESM_DESCANSO_MS;
  if (!t) return ESM_ACESO;
  if (ocioMs < t) return ESM_ACESO;
  if (ocioMs < t + fim) return ESM_VEU;
  return ESM_ESCURO;
}

float esmaecer_alfa_do_estagio(int e, int estilo) {
  if (e == ESM_ESCURO)
    return estiloValido(estilo) == ESM_ESTILO_ESCURECER ? ESM_ALFA_ESCURO : ESM_ALFA_DESCANSO;
  return e == ESM_VEU ? ESM_ALFA_VEU : 0.0f;
}

void esmaecer_reiniciar(void) {
  ultimaEntrada = 0; temEntrada = 0; acordou = 0; acordouEm = 0;
  estagio = ESM_ACESO; veu = 0.0f; avisoEscuro = -1;
}

void esmaecer_escolha(int escolha) {
  escolhaAtual = escolha >= 0 && escolha < ESM_ESCOLHAS ? escolha : ESM_PADRAO;
  protetorPublicar();
}
void esmaecer_estilo(int estilo) {
  estiloAtual = estiloValido(estilo);
  protetorPublicar();
}
int  esmaecer_estilo_atual(void) { return estiloAtual; }

static void definirEstagio(int e) {
  if (e == estagio) return;
  if ((e == ESM_ESCURO) != (estagio == ESM_ESCURO)) avisoEscuro = e == ESM_ESCURO;
  estagio = e;
}

int esmaecer_entrada(unsigned agora, int consumivel) {
  int dimmed = estagio != ESM_ACESO || veu > 0.001f;
  ultimaEntrada = agora; temEntrada = 1;
  if (dimmed) {
    // ACORDA NA HORA e engole a tecla que acordou.
    definirEstagio(ESM_ACESO);
    veu = 0.0f;
    acordou = 1; acordouEm = agora;
    return consumivel;
  }
  // A repeticao da MESMA tecla segurada logo depois de acordar tambem nao age.
  if (acordou && consumivel && agora - acordouEm < 300u) return 1;
  return 0;
}

void esmaecer_quadro(unsigned agora, float dt, int reproduzindo) {
  float alvo;
  if (!temEntrada) { ultimaEntrada = agora; temEntrada = 1; }
  // Tocando de verdade: o relogio da ociosidade fica parado em "agora". Ao
  // pausar, os N minutos contam da pausa.
  if (reproduzindo) ultimaEntrada = agora;
  definirEstagio(esmaecer_estagio_para(agora - ultimaEntrada, escolhaAtual, estiloAtual));
  alvo = esmaecer_alfa_do_estagio(estagio, estiloAtual);
  if (dt < 0.0f) dt = 0.0f;
  if (dt > 0.1f) dt = 0.1f;
  if (veu < alvo) { veu += ESM_VELOCIDADE * dt; if (veu > alvo) veu = alvo; }
  else if (veu > alvo) { veu = alvo; }   // acordar e instantaneo
}

int   esmaecer_estagio(void) { return estagio; }
float esmaecer_veu(void) { return veu; }
int   esmaecer_apagado(void) {
  return estagio == ESM_ESCURO && veu >= esmaecer_alfa_do_estagio(ESM_ESCURO, estiloAtual) - 0.001f;
}
int   esmaecer_segura_protetor_tv(void) {
  return esmaecer_ms(escolhaAtual) > 0 && estiloAtual != ESM_ESTILO_ESCURECER;
}
int   esmaecer_descanso(void) { return estiloAtual != ESM_ESTILO_ESCURECER && esmaecer_apagado(); }
int   esmaecer_mudou_escuro(int *escuro) {
  if (avisoEscuro < 0) return 0;
  if (escuro) *escuro = avisoEscuro;
  avisoEscuro = -1;
  return 1;
}

float esmaecer_brilho_base(int escolha) {
  static const float B[4] = { 1.0f, 0.80f, 0.65f, 0.50f };
  return escolha >= 0 && escolha < 4 ? B[escolha] : 0.80f;
}

float esmaecer_brilho_osd(int escolha, unsigned parado_ms, int tocando) {
  float f = esmaecer_brilho_base(escolha);
  if (tocando && parado_ms > BRILHO_AUTO_MS) {
    float t = (float)(parado_ms - BRILHO_AUTO_MS) / (float)BRILHO_AUTO_RAMPA;
    if (t > 1.0f) t = 1.0f;
    t = t * t * (3.0f - 2.0f * t);
    f *= 1.0f - (1.0f - BRILHO_AUTO_PASSO) * t;
  }
  return f < 0.40f ? 0.40f : f;
}

float esmaecer_brilho_hdr(const char *hdr, int dv) {
  if (dv) return HDR_UI_BRILHO;
  if (!hdr || !*hdr || !strcasecmp(hdr, "none") || !strcasecmp(hdr, "desconhecido")) return 1.0f;
  return HDR_UI_BRILHO;
}

#ifndef ESMAECER_SEM_GFX
void esmaecer_desenhar(unsigned agora) {
  GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
  float g = gfx_opacidade_grupo, m = gfx_osd_mult;
  if (veu <= 0.002f) return;
  gfx_opacidade_grupo = 1.0f; gfx_osd_mult = 1.0f;
  gfx_cor(tela, 0, 0, 0, 0, veu);
  // TELA DE DESCANSO: vitrine ou relogio, por cima do preto. O estilo
  // "so escurecer" segue com o relogio pequeno abaixo.
  if (estiloAtual != ESM_ESTILO_ESCURECER) {
    if (esmaecer_descanso()) descanso_desenhar(agora);
    gfx_opacidade_grupo = g; gfx_osd_mult = m;
    return;
  }
  // O RELOGIO que anda: so quando o veu ja passou de 80% (nunca por cima de
  // uma tela ainda legivel). Cada eixo com um periodo diferente, para a rota
  // nao repetir; pouco brilho (cinza escuro), nada parado no mesmo pixel.
  if (veu > 0.80f) {
    static char horaTxt[8];
    static TxtLinha l;
    static time_t seg;
    time_t t = time(NULL);
    if (t != seg || !l.tex) {
      struct tm lt;
      seg = t;
      if (localtime_r(&t, &lt)) {
        char h[12];
        hora_tela(h, sizeof h, &lt);
        if (!l.tex || strcmp(h, horaTxt)) {
          snprintf(horaTxt, sizeof horaTxt, "%s", h);
          l = txt_linha(TXT_BODY, horaTxt, 120, 120, 120, 255);
        }
      }
    }
    if (l.tex) {
      // Muda de lugar a cada minuto, em pixel inteiro, e parado no resto
      // (05/10: andar a cada quadro deixava o texto tremendo). A troca apaga e
      // acende devagar, 1,5 s cada, como a tela de descanso.
      unsigned n = agora / 60000u, f = agora % 60000u;
      float s = (float)n * 60.0f, k;
      float px = 0.5f + 0.5f * sinf(s / 41.0f), py = 0.5f + 0.5f * sinf(s / 67.0f + 1.3f);
      float x = floorf(120.0f + px * (NV_TELA_W - 240.0f - (float)l.w));
      float y = floorf(120.0f + py * (NV_TELA_H - 240.0f - (float)l.h));
      k = f < 1500u ? (float)f / 1500.0f : f > 58500u ? (float)(60000u - f) / 1500.0f : 1.0f;
      k = k * k * (3.0f - 2.0f * k);
      txt_desenhar_alpha(l, x, y, k * (veu - 0.80f) / 0.12f);
    }
  }
  gfx_opacidade_grupo = g; gfx_osd_mult = m;
}
#endif
