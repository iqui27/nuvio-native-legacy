// SAIDA PELA TV NAO E QUEDA NO .tpk (tpkdesp.h).
//
// As sequencias abaixo sao os finais de sessao medidos no D1 (1.7.0, tizen-tpk,
// 6 h) que a sessao seguinte contava como "anterior caiu". Cada uma tem de
// deixar "oculto" no despedida.txt; uma sessao que esconde e VOLTA tem de
// apagar; e a saida limpa ("fim") nunca e sobrescrita.
#include "../src/tpkdesp.h"
#include "nvtmp.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char dir[256], arq[300];

static const char *ler(void) {
  static char b[32];
  FILE *f = fopen(arq, "rb");
  size_t r;
  if (!f) return "";
  r = fread(b, 1, sizeof b - 1, f); b[r] = 0; fclose(f);
  return b;
}
static void zerar(void) { remove(arq); tpkdesp_iniciar(dir); }

int main(void) {
  snprintf(dir, sizeof dir, "%s/nuvio-tpkdesp-%d", nvt_dir(), (int)getpid());
  snprintf(arq, sizeof arq, "%s/despedida.txt", dir);
  { char cmd[300]; snprintf(cmd, sizeof cmd, "mkdir -p %s", dir); assert(system(cmd) == 0); }

  // 1. Exit: Janela/Etapa da saida (host OnTerminate -> SoltaTudo).
  zerar();
  tpkdesp_linha("[etapa] host note main-window visible=False t=146.0s calls=7145 frames=7015 paused=True @145.96s");
  tpkdesp_tecla("XF86Exit");
  tpkdesp_linha("[etapa] host note terminate t=146.7s calls=7145 frames=7015 paused=True @146.65s");
  tpkdesp_linha("[janela] saida: principal escondida t=146.7s");
  assert(!strcmp(ler(), "oculto\n"));
  printf("ok  Exit + terminate grava oculto\n");

  // 2. Sessao longa: as Etapa passaram do teto de 400, so a Janela chega.
  zerar();
  tpkdesp_linha("[janela] principal visivel=False t=22.7s");
  assert(!strcmp(ler(), "oculto\n"));
  printf("ok  so a linha de janela (sem Etapa) grava oculto\n");

  // 3. Desligar: so a tecla chega antes de a TV matar o processo.
  zerar();
  tpkdesp_tecla("XF86PowerOff");
  tpkdesp_linha("[etapa] host note gl-window focus=False t=120.1s calls=5887 frames=5757 paused=False @120.05s");
  assert(!strcmp(ler(), "oculto\n"));
  printf("ok  PowerOff grava oculto\n");

  // 4. Escondeu e voltou: a marca sai (uma queda depois disso e queda).
  zerar();
  tpkdesp_linha("[etapa] host note pause t=363.9s calls=16824 frames=16614 paused=True @363.89s");
  tpkdesp_linha("[janela] principal visivel=True t=400.0s");
  assert(!strcmp(ler(), ""));
  tpkdesp_tecla("XF86Exit");
  tpkdesp_tecla("Down");                     // Exit sem fechar: seguiu em uso
  assert(!strcmp(ler(), ""));
  printf("ok  voltar a janela ou outra tecla apaga o oculto\n");

  // 5. Saida limpa gravada pelo app vence.
  zerar();
  { FILE *f = fopen(arq, "wb"); fputs("fim\n", f); fclose(f); }
  tpkdesp_linha("[janela] saida: principal escondida t=231.1s");
  tpkdesp_linha("[janela] principal visivel=True t=232.0s");
  assert(!strcmp(ler(), "fim\n"));
  printf("ok  fim nunca e sobrescrito\n");

  // 6. Linhas comuns nao mexem em nada.
  zerar();
  tpkdesp_linha("[janela] gl visivel=True t=1286.6s");
  tpkdesp_linha("[tv] modelo=X tizen=9.0 host=api11");
  assert(!strcmp(ler(), ""));
  printf("ok  linhas neutras nao gravam\n");

  remove(arq); rmdir(dir);
  printf("tpkdesp: tudo ok\n");
  return 0;
}
