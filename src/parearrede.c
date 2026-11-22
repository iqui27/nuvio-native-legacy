#include "parearrede.h"
#include "parear.h"
#include "rede.h"
#include "jsw.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef NV_REC_URL
#define NV_REC_URL ""
#endif
#define PAREAR_TEMPO_REDE 12

// Tudo abaixo e lido pelo laco de desenho e escrito pelo fio: sob a trava.
// `geracao` muda quando o endereco muda ou a tela esquece; resposta de um
// pedido antigo e descartada.
static SDL_SpinLock trava;
static unsigned geracao;
static int  fioVivo, tentativa;
static long expira, proxima;          // segundos; proxima = -1: desistiu
static char urlAtual[420];
static char codigo[16], pagina[200];
static char codigoTela[16], paginaTela[200];   // copias do laco de desenho

typedef struct { unsigned geracao; char url[420]; } Pedido;

static const char *base(void) {
#ifdef NV_LINUX_DESKTOP
  // So na previa do Linux: aponta para um servidor local de teste.
  const char *e = getenv("NUVIO_PAREAR_URL");
  if (e && *e) return e;
#endif
  return NV_REC_URL;
}

static int fio(void *u) {
  Pedido *p = u;
  const char *cab[] = { "Content-Type: application/json", NULL };
  char destino[600], cod[16], pag[200];
  int st = 0, validade = 0, ok;
  char *r;
  Jsw w;
  snprintf(destino, sizeof destino, "%s/v1/parear", base());
  jsw_iniciar(&w);
  jsw_obj_ini(&w);
  jsw_cs(&w, "url", p->url);
  jsw_obj_fim(&w);
  r = rede_postar_st(destino, PAREAR_TEMPO_REDE, cab, jsw_texto_final(&w), &st);
  jsw_livre(&w);
  ok = parear_ler_resposta(st, r, cod, sizeof cod, pag, sizeof pag, &validade);
  free(r);
  // O endereco e o codigo nao vao para o log: quem le o registro nao entra na TV.
  printf("[parear] HTTP %d%s\n", st, ok ? " codigo recebido" : "");
  fflush(stdout);
  SDL_AtomicLock(&trava);
  if (p->geracao == geracao) {
    long agora = (long)time(NULL);
    if (ok) {
      snprintf(codigo, sizeof codigo, "%s", cod);
      parear_pagina_curta(pag, pagina, sizeof pagina);
      expira = agora + validade;
      tentativa = 0;
      proxima = 0;
    } else {
      int espera = parear_espera(st, tentativa);
      codigo[0] = 0;
      proxima = espera < 0 ? -1 : agora + espera;
    }
  }
  fioVivo = 0;
  SDL_AtomicUnlock(&trava);
  free(p);
  return 0;
}

static void zerar(void) {
  geracao++;
  codigo[0] = pagina[0] = urlAtual[0] = 0;
  expira = proxima = 0;
  tentativa = 0;
}

void parear_rede_esquecer(void) {
  SDL_AtomicLock(&trava);
  zerar();
  SDL_AtomicUnlock(&trava);
}

void parear_rede_passo(const char *urlLogin, int ligado) {
  Pedido *p = NULL;
  long agora = (long)time(NULL);
  if (!ligado || !base()[0] || !parear_endereco_ok(urlLogin)) {
    SDL_AtomicLock(&trava);
    if (urlAtual[0] || codigo[0]) zerar();
    SDL_AtomicUnlock(&trava);
    return;
  }
  SDL_AtomicLock(&trava);
  if (strcmp(urlAtual, urlLogin)) {
    zerar();
    snprintf(urlAtual, sizeof urlAtual, "%s", urlLogin);
  }
  // Codigo vencendo: some da tela e pede outro (o servidor devolve um novo).
  if (codigo[0] && parear_vencendo(agora, expira)) { codigo[0] = 0; proxima = 0; }
  if (!codigo[0] && !fioVivo && proxima >= 0 && agora >= proxima) {
    p = malloc(sizeof *p);
    if (p) {
      p->geracao = geracao;
      snprintf(p->url, sizeof p->url, "%s", urlAtual);
      fioVivo = 1;
      tentativa++;
    }
  }
  SDL_AtomicUnlock(&trava);
  if (p) {
    SDL_Thread *t = SDL_CreateThread(fio, "parear", p);
    if (t) SDL_DetachThread(t);
    else {
      SDL_AtomicLock(&trava); fioVivo = 0; proxima = -1; SDL_AtomicUnlock(&trava);
      free(p);
    }
  }
}

const char *parear_rede_codigo(void) {
  char c[16];
  SDL_AtomicLock(&trava);
  snprintf(c, sizeof c, "%s", codigo);
  SDL_AtomicUnlock(&trava);
  parear_formatar(c, codigoTela, sizeof codigoTela);
  return codigoTela;
}

const char *parear_rede_pagina(void) {
  SDL_AtomicLock(&trava);
  snprintf(paginaTela, sizeof paginaTela, "%s", pagina);
  SDL_AtomicUnlock(&trava);
  return paginaTela;
}
