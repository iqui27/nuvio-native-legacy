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
// As regras (geracao, vencimento, "Novo codigo", espera) moram em ParearCtl
// (parear.c, testadas em tests/parear.c); aqui so a trava, o fio e a pagina.
static SDL_SpinLock trava;
static ParearCtl ctl;
static int  fioVivo;
static char urlAtual[420];
static char pagina[200];
static char codigoTela[16], paginaTela[200];   // copias do laco de desenho

typedef struct { unsigned geracao; int novo; char url[420]; } Pedido;

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
  // "Novo codigo": o servidor apaga o codigo vivo deste endereco e da outro.
  if (p->novo) jsw_ci(&w, "novo", 1);
  jsw_obj_fim(&w);
  r = rede_postar_st(destino, PAREAR_TEMPO_REDE, cab, jsw_texto_final(&w), &st);
  jsw_livre(&w);
  ok = parear_ler_resposta(st, r, cod, sizeof cod, pag, sizeof pag, &validade);
  free(r);
  // O endereco e o codigo nao vao para o log: quem le o registro nao entra na TV.
  printf("[parear] HTTP %d%s%s\n", st, p->novo ? " (novo)" : "", ok ? " codigo recebido" : "");
  fflush(stdout);
  SDL_AtomicLock(&trava);
  if (ok && p->geracao == ctl.geracao) parear_pagina_curta(pag, pagina, sizeof pagina);
  parear_ctl_resposta(&ctl, p->geracao, ok, cod, validade, st, (long)time(NULL));
  fioVivo = 0;
  SDL_AtomicUnlock(&trava);
  free(p);
  return 0;
}

static void zerar(void) {
  parear_ctl_zerar(&ctl);
  pagina[0] = urlAtual[0] = 0;
}

void parear_rede_esquecer(void) {
  SDL_AtomicLock(&trava);
  zerar();
  SDL_AtomicUnlock(&trava);
}

int parear_rede_renovar(void) {
  int r = 0;
  SDL_AtomicLock(&trava);
  // So ha o que renovar com um endereco em uso (QR na tela, ajuste ligado).
  if (urlAtual[0]) r = parear_ctl_renovar(&ctl, (long)time(NULL));
  SDL_AtomicUnlock(&trava);
  // O pedido sai no proximo parear_rede_passo (no maximo um quadro depois),
  // ou assim que o fio de um pedido antigo terminar (a resposta dele e jogada fora).
  return r;
}

void parear_rede_passo(const char *urlLogin, int ligado) {
  Pedido *p = NULL;
  long agora = (long)time(NULL);
  if (!ligado || !base()[0] || !parear_endereco_ok(urlLogin)) {
    SDL_AtomicLock(&trava);
    if (urlAtual[0] || ctl.codigo[0]) zerar();
    SDL_AtomicUnlock(&trava);
    return;
  }
  SDL_AtomicLock(&trava);
  if (strcmp(urlAtual, urlLogin)) {
    zerar();
    snprintf(urlAtual, sizeof urlAtual, "%s", urlLogin);
  }
  if (parear_ctl_quer_pedir(&ctl, agora, fioVivo)) {
    p = malloc(sizeof *p);
    if (p) {
      p->geracao = ctl.geracao;
      p->novo = ctl.novo;
      snprintf(p->url, sizeof p->url, "%s", urlAtual);
      fioVivo = 1;
    }
  }
  SDL_AtomicUnlock(&trava);
  if (p) {
    SDL_Thread *t = SDL_CreateThread(fio, "parear", p);
    if (t) SDL_DetachThread(t);
    else {
      SDL_AtomicLock(&trava); fioVivo = 0; ctl.proxima = -1; SDL_AtomicUnlock(&trava);
      free(p);
    }
  }
}

const char *parear_rede_codigo(void) {
  char c[16];
  SDL_AtomicLock(&trava);
  snprintf(c, sizeof c, "%s", ctl.codigo);
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
