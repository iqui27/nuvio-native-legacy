// Real rede.c/libcurl against the loopback Range server; no external URL.
#include "mkvass.h"
#include "imgsub.h"
#include "dados.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
int main(int argc, char **argv) {
  assert(argc==3); dados_iniciar("."); mkvass_aceitar_imagem(1);
  mkvass_iniciar(argv[1],6); mkvass_folga(3); mkvass_passo(600.5);
  int ok=atoi(argv[2]); ImgsubQuadro *q=NULL;
  for (int i=0; i<5000 && !mkvass_nogo(); i++) {
    q=imgsub_pegar(600.5); if(q) break; usleep(1000);
  }
  MkvassDiagnostico d; mkvass_diagnostico(&d);
  if(ok) {
    assert(q && q->n==1 && q->ini==600 && q->obj[0].x==100 && q->obj[0].y==900);
    assert(d.network_bytes>0 && d.decoded>0);
  } else assert(!q && d.state>=MKVASS_NOGO);
  imgsub_soltar(q); mkvass_parar();
  for(int i=0;i<1000 && mkvass_ocupado();i++) usleep(1000);
  assert(!mkvass_ocupado()); puts("PGS loopback HTTP: ok");
}
