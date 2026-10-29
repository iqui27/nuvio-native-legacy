// Quadros de legenda de imagem por tempo: o ultimo com ini <= t manda, quadro
// vazio limpa, duracao desconhecida expira, limpar() zera, o que o desenho
// segura sobrevive a limpar().
#include "imgsub.h"
#include <stdio.h>
#include <stdlib.h>

static int falhas;
#define CHECA(c) do { if (!(c)) { printf("FALHOU %s:%d: %s\n", __FILE__, __LINE__, #c); falhas++; } } while (0)

static unsigned donoT = 1;
static void poe(double ini, double fim, int x, int vazio) {
  PgsQuadro p = {0};
  p.tela_w = 1920; p.tela_h = 1080;
  if (!vazio) {
    p.n = 1; p.obj[0].x = x; p.obj[0].w = 4; p.obj[0].h = 2;
    p.obj[0].rgba = calloc(4 * 4 * 2, 1);
  }
  imgsub_por(donoT, ini, fim, &p);
  CHECA(!p.obj[0].rgba && p.n == 0);
}
static int xEm(double t) {
  ImgsubQuadro *f = imgsub_pegar(t);
  int x = !f ? -1 : f->n == 0 ? -2 : f->obj[0].x;
  imgsub_soltar(f);
  return x;
}

int main(void) {
  ImgsubQuadro *seg;
  int i;
  // fora de ordem, como a colheita do mkvass
  poe(20, 22, 300, 0);
  poe(10, 12, 100, 0);
  poe(12, -1, 0, 1);        // limpa em 12
  poe(15, -1, 200, 0);      // sem duracao: expira em 20 (IMGSUB_DUR_PADRAO)
  CHECA(xEm(9.9) == -1);
  CHECA(xEm(10) == 100);
  CHECA(xEm(11.99) == 100);
  CHECA(xEm(12) == -2);     // marca de limpar (quadro vazio, nao ausencia)
  CHECA(xEm(14) == -2);
  CHECA(xEm(15) == 200);
  CHECA(xEm(19.99) == 200);
  CHECA(xEm(20) == 300);    // o proximo quadro troca antes do padrao
  // limpar sem fim, no meio da janela: vale ate o proximo quadro, nao +5 s
  poe(16, -1, 0, 1);
  CHECA(xEm(15.9) == 200);
  CHECA(xEm(16) == -2);
  CHECA(xEm(19.9) == -2);
  CHECA(xEm(22) == -1);     // fim explicito
  // Limpar DENTRO da janela do quadro anterior (fim 15, clear em 12).
  imgsub_limpar();
  poe(10, 15, 100, 0);
  poe(12, -1, 0, 1);
  CHECA(xEm(11.5) == 100);
  CHECA(xEm(12) == -2);     // limpa no meio da janela do quadro anterior
  CHECA(xEm(14) == -2);
  poe(10, 12, 111, 0);      // mesma fala de novo: substitui
  CHECA(xEm(11) == 111);
  seg = imgsub_pegar(11);
  CHECA(seg != NULL);
  imgsub_limpar();
  CHECA(xEm(11) == -1);
  CHECA(seg && seg->obj[0].rgba && seg->obj[0].x == 111);   // o desenho ainda tem o dele
  imgsub_soltar(seg);
  // teto: nao passa de 512 quadros e fica o que esta perto do playhead
  for (i = 0; i < 600; i++) poe(i * 3.0, i * 3.0 + 2.0, i, 0);
  CHECA(xEm(0) == 0 && xEm(300.5) == 100 && xEm(1800.5) == -1);
  imgsub_soltar(imgsub_pegar(900));
  poe(901, 903, 7, 0);
  CHECA(xEm(901.5) == 7);
  // dono: quadro de geracao mais velha e descartado; mais nova expulsa a antiga
  imgsub_limpar();
  donoT = 5; poe(10, 12, 1, 0);
  donoT = 4; poe(20, 22, 2, 0);
  CHECA(xEm(21) == -1 && xEm(11) == 1 && imgsub_dono_atual() == 5);
  donoT = 6; poe(30, 32, 3, 0);
  CHECA(xEm(11) == -1 && xEm(31) == 3 && imgsub_dono_atual() == 6);
  imgsub_limpar();
  if (falhas) { printf("imgsub: %d falha(s)\n", falhas); return 1; }
  puts("imgsub: ok");
  return 0;
}
