// Legenda de imagem do Blu-ray (PGS, S_HDMV/PGS no Matroska) decodificada em
// software: nenhum backend desta app (.tpk Samsung incluso) sabe desenhar isso,
// e ffmpeg nao entra no .tpk. C puro, sem SDL, sem rede.
//
// Entrada: o payload de UM bloco Matroska, que e a sequencia de segmentos SEM
// o cabecalho "PG"+PTS/DTS do .sup (tipo 1 byte, tamanho 2 bytes BE, dados).
// Um bloco = um display set (PCS, WDS, PDS, ODS..., END). O decodificador guarda
// paletas e objetos da epoca, entao display sets "normais" que so trocam a
// paleta ou reaproveitam objeto funcionam na ordem. Fora de ordem (a colheita
// do mkvass anda por Cues), o que referencia objeto ausente e descartado — nunca
// confundido com "limpar a tela".
#ifndef NV_PGS_H
#define NV_PGS_H
#include <stddef.h>
#include <stdint.h>

#define PGS_MAX_OBJ 2

typedef struct {
  int x, y, w, h;      // posicao e tamanho na tela (tela_w x tela_h)
  int forcado;         // flag "forced" do objeto na composicao
  uint8_t *rgba;       // w*h*4, RGBA nao premultiplicado; dono: o quadro
} PgsObjeto;

typedef struct {
  int tela_w, tela_h;
  int n;               // 0 = composicao vazia: limpa a tela
  PgsObjeto obj[PGS_MAX_OBJ];
} PgsQuadro;

typedef struct Pgs Pgs;
Pgs *pgs_novo(void);
void pgs_liberar(Pgs *);
// 1 = `q` valido (n pode ser 0), libere com pgs_quadro_liberar. 0 = bloco sem
// composicao utilizavel (truncado, lixo, objeto que nao chegou).
int pgs_decodificar(Pgs *, const uint8_t *buf, size_t n, PgsQuadro *q);
void pgs_quadro_liberar(PgsQuadro *);
#endif
