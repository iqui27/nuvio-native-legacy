// Legendas de imagem (PGS) ja decodificadas, indexadas por tempo. Quem escreve
// e a thread do mkvass, que colhe a frente do playhead e fora de ordem; quem le
// e a thread de desenho. Cada quadro e imutavel e contado por referencia: o
// desenho segura o quadro que esta na tela sem travar a colheita.
#ifndef NV_IMGSUB_H
#define NV_IMGSUB_H
#include "pgs.h"

typedef struct ImgsubQuadro {
  double ini, fim;           // fim < 0: duracao desconhecida
  int tela_w, tela_h, n;     // n == 0: marca "limpar a tela"
  PgsObjeto obj[PGS_MAX_OBJ];
  int refs;
  unsigned id;               // unico por quadro: chave do cache de textura do desenho
} ImgsubQuadro;

// Guarda o quadro decodificado (leva os bitmaps: q->obj[i].rgba fica NULL).
void imgsub_por(unsigned dono, double ini, double fim, PgsQuadro *q);
void imgsub_limpar(void);
// Geracao da legenda (legenda_geracao) a que os quadros pertencem: o desenho so
// mostra se a legenda ainda e dessa geracao. imgsub_limpar zera.
unsigned imgsub_dono_atual(void);
// Quadro que deve estar na tela em `t`, ou NULL. Quem recebe devolve com soltar.
ImgsubQuadro *imgsub_pegar(double t);
void imgsub_soltar(ImgsubQuadro *);
// Quanto um quadro de duracao desconhecida fica na tela sem outro que o troque.
#define IMGSUB_DUR_PADRAO 5.0
// Desenha o quadro de `t` no retangulo do video (x,y,w,h); 1 se desenhou. So
// mostra quando a legenda ainda e do dono do imgsub. Fora de imgsub.c: precisa de GL.
int imgsub_desenhar(double t, float x, float y, float w, float h, float alpha);
// Solta o cache de texturas no fio de desenho, com contexto GL atual.
void imgsub_gl_limpar(void);
#endif
