// XMLTV EM FLUXO (guia de TV, M4).
//
// O parser antigo (epg.c) abria o XML INTEIRO na memoria: grade de provedor
// acima de 32 MB de rede / 128 MB de XML era "maior que o teto, ignorada" —
// 42% das tentativas nos logs da 2.0.x. Aqui o arquivo entra em pedacos de
// qualquer tamanho (gzip detectado pelos dois primeiros bytes e aberto com zlib
// tambem em pedacos) e so UM elemento <channel> ou <programme> de cada vez fica
// num buffer de XMLTV_ELEM bytes. Elemento maior que isso (descricao enorme) e
// pulado inteiro e contado. Memoria de pico: o buffer + a janela do zlib.
//
// Quem chama decide o que reter (os canais da lista, a janela de tempo): os
// retornos de chamada recebem os campos ja desentidados.
#ifndef NV_XMLTV_H
#define NV_XMLTV_H
#include <stddef.h>
#include <zlib.h>

#define XMLTV_ELEM (32 * 1024)
#define XMLTV_NOMES 4

typedef struct {
  // <channel id="..."><display-name>...</display-name>...</channel>
  void (*canal)(const char *id, const char *const *nomes, int nNomes, void *u);
  // <icon src="..."/> do mesmo <channel>, logo depois de `canal`. Opcional.
  void (*icone)(const char *id, const char *src, void *u);
  // <programme channel start stop><title>...</title></programme>
  void (*programa)(const char *canal, const char *ini, const char *fim,
                   const char *titulo, void *u);
  void *u;
  // estado
  int  gz, inicio, zOk, erroZ;
  z_stream z;
  char nome[16]; int nNome, lendoNome;   // nome da tag depois de '<'
  int  dentro;                           // 0 fora, 1 channel, 2 programme
  int  pulando;                          // elemento grande demais: descarta
  char *buf; size_t n;
  char cauda[16];                        // ultimos bytes do elemento
  long long bytesXml, bytesBrutos;
  int  nCanais, nProgramas, nPulados;
} Xmltv;

int  xmltv_iniciar(Xmltv *x);            // 0 = sem memoria
// Bytes crus (XML ou gzip). 0 = o gzip quebrou (o resto e ignorado).
int  xmltv_alimentar(Xmltv *x, const char *dados, size_t n);
void xmltv_fim(Xmltv *x);                // solta o buffer e o zlib

// "20250914223000 -0300" -> epoch UTC; 0 quando nao e data XMLTV.
long long xmltv_tempo(const char *s);
// &amp; &lt; &#233; ... no lugar (o texto so encolhe).
void xmltv_desentidade(char *s);

#endif
