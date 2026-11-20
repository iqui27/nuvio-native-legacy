#!/bin/bash
# #799: dois avisos de ciclo antes do consumo nao podem se sobrescrever.
# Exercita nv_tpk_ciclo / nv_tpk_retomou / appEsperaVez REAIS de src/tpk.c
# (extraidos pelo python abaixo) com a fila SDL e o callback do player stubados.
set -eu
cd "$(dirname "$0")/.."
tmp=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-tpk-ciclo.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
python3 - "$tmp" <<'PYTHON'
from pathlib import Path
import re, sys
out = Path(sys.argv[1])
src = Path("src/tpk.c").read_text()
tokens = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*[\s\S]*?\*/|[{};]')
def funcao(assinatura):
    for inicio in re.finditer(r'(?m)^' + re.escape(assinatura), src):
        aberta = None; nivel = 0
        for token in tokens.finditer(src, inicio.start()):
            v = token[0]
            if aberta is None:
                if v == ';': break
                if v != '{': continue
                aberta = token.start()
            if v == '{': nivel += 1
            elif v == '}':
                nivel -= 1
                if nivel == 0:
                    return src[inicio.start():token.end()]
        if aberta is not None:
            return src[inicio.start():]
    raise SystemExit("funcao nao achada: " + assinatura)
ciclo = funcao("void nv_tpk_ciclo(int memoria) {")
retomou = funcao("void nv_tpk_retomou(void) {")
espera = funcao("static void appEsperaVez(int entregou) {")
# helpers e estado da fila FIFO (#799): o bloco entre o marcador e appEsperaVez
ini = src.index("#define CICLO_FILA")
fim = src.index("static void appEsperaVez")
fila = src[ini:fim]
harness = r'''
#include <pthread.h>
#include <stdio.h>
#include <string.h>
typedef unsigned Uint32;
#define SDL_APP_WILLENTERBACKGROUND 0x102
#define SDL_APP_LOWMEMORY           0x104
#define SDL_APP_DIDENTERFOREGROUND  0x106
typedef struct { Uint32 type; } SDL_Event;
enum { VEZ_NUI = 0, VEZ_APP = 1 };
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  mudou = PTHREAD_COND_INITIALIZER;
static int vez = VEZ_APP, pendente, appEsperando;
static Uint32 eventos[16]; static int nEventos;
static int foregroundFila;
static void player_ciclo_evento(Uint32 t) { if (nEventos < 16) eventos[nEventos++] = t; }
static int SDL_PushEvent(SDL_Event *e) { if (e->type == SDL_APP_DIDENTERFOREGROUND) foregroundFila++; return 0; }
'''
# strip the visibility attribute line before each function (harness has no dllexport)
ciclo = re.sub(r'__attribute__\(\(visibility\("default"\)\)\)\s*', '', ciclo)
retomou = re.sub(r'__attribute__\(\(visibility\("default"\)\)\)\s*', '', retomou)
main = r'''
static int falhas;
static void ok(int c, const char *o) { printf("%s %s\n", c?"ok   ":"FALHA", o); if(!c) falhas++; }
static void drenar(void) { Uint32 t; while (cicloTirar(&t)) { } }
static void reset(void) { drenar(); nEventos=0; foregroundFila=0; vez=VEZ_APP; }
static void consumir(void) { appEsperaVez(0); }
int main(void) {
  // resume isolado
  reset(); nv_tpk_retomou(); consumir();
  ok(nEventos==1 && eventos[0]==SDL_APP_DIDENTERFOREGROUND && foregroundFila==1,
     "resume isolado: callback + fila SDL");
  // pausa -> resume antes do consumo: ambos devem chegar, pausa primeiro
  reset(); nv_tpk_ciclo(0); nv_tpk_retomou(); consumir();
  ok(nEventos==2 && eventos[0]==SDL_APP_WILLENTERBACKGROUND &&
     eventos[1]==SDL_APP_DIDENTERFOREGROUND && foregroundFila==1,
     "pausa+resume antes do consumo: pausa NAO perdida, ordem preservada (#799)");
  // pouca memoria -> resume
  reset(); nv_tpk_ciclo(1); nv_tpk_retomou(); consumir();
  ok(nEventos==2 && eventos[0]==SDL_APP_LOWMEMORY &&
     eventos[1]==SDL_APP_DIDENTERFOREGROUND && foregroundFila==1,
     "pouca memoria+resume: lowmemory NAO perdido (#799)");
  // resume -> pouca memoria: foreground nao pode sumir
  reset(); nv_tpk_retomou(); nv_tpk_ciclo(1); consumir();
  ok(nEventos==2 && foregroundFila==1 &&
     ((eventos[0]==SDL_APP_LOWMEMORY && eventos[1]==SDL_APP_DIDENTERFOREGROUND) ||
      (eventos[0]==SDL_APP_DIDENTERFOREGROUND && eventos[1]==SDL_APP_LOWMEMORY)),
     "resume+pouca memoria: foreground NAO perdido (#799)");
  // pausa + pouca memoria + resume: tres avisos, nenhum sobrescrito
  reset(); nv_tpk_ciclo(0); nv_tpk_ciclo(1); nv_tpk_retomou(); consumir();
  ok(nEventos==3 && foregroundFila==1, "tres avisos antes do consumo: todos entregues");
  // Cenario do revisor: retomada seguida de pausa NAO pode inverter a ordem.
  reset(); nv_tpk_retomou(); nv_tpk_ciclo(0); consumir();
  ok(nEventos==2 && eventos[0]==SDL_APP_DIDENTERFOREGROUND &&
     eventos[1]==SDL_APP_WILLENTERBACKGROUND,
     "retomada->pausa antes da vez: ordem de chegada (FIFO), nao prioridade (#799)");
  // Dois ciclos completos antes da vez: quatro avisos, dois foreground.
  reset(); nv_tpk_ciclo(0); nv_tpk_retomou(); nv_tpk_ciclo(0); nv_tpk_retomou(); consumir();
  ok(nEventos==4 && foregroundFila==2 &&
     eventos[0]==SDL_APP_WILLENTERBACKGROUND && eventos[1]==SDL_APP_DIDENTERFOREGROUND &&
     eventos[2]==SDL_APP_WILLENTERBACKGROUND && eventos[3]==SDL_APP_DIDENTERFOREGROUND,
     "pausa->retomada->pausa->retomada: nenhum ciclo perdido (#799)");
  printf("\n%s\n", falhas?"FALHOU":"tpk_ciclo_fila: ok");
  return falhas!=0;
}
'''
(out/"test.c").write_text(harness + fila + "\n" + ciclo + "\n" + retomou + "\n" + espera + "\n" + main)
PYTHON
cc -Wall -O1 -pthread "$tmp/test.c" -o "$tmp/test"
"$tmp/test"
