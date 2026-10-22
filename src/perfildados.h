// Snapshot independente de UI para Perfil e Stats. Produtores (Trakt, Simkl)
// preenchem esta estrutura em workers; perfil.c apenas guarda e desenha.
#ifndef NV_PERFILDADOS_H
#define NV_PERFILDADOS_H

#include "arteurl.h"
#include <stdint.h>

// QUATRO, e nao oito e seis. A tela nao rola mais: cabem quatro cards de
// destaque e quatro pastilhas de genero, e buscar mais do que se desenha so
// produz campo morto no snapshot.
#define PERFIL_MAX_GENEROS    4
#define PERFIL_MAX_DESTAQUES  4
#define PERFIL_MAX_DIAS       42

typedef struct {
  char nome[40];
  int  quantidade;
  uint32_t cor;              // 0xRRGGBB; zero usa a paleta da tela
} PerfilGenero;

typedef struct {
  char id[64];               // imdb/trakt/slug, devolvido sem interpretacao
  char titulo[128];
  char detalhe[96];          // ex.: "T2E3 · Solo" ou "Filme"
  char poster[NV_ARTE_URL_MAX];
  char backdrop[768];
  int  plays;
  int  minutos;
} PerfilDestaque;

typedef struct PerfilDados {
  char nome[96];
  char usuario[64];
  char avatar[768];
  char periodo[48];          // ex.: "SETEMBRO 2026"

  int minutos;
  int plays;
  int filmes;
  int episodios;

  int streakAtual;
  // streakAnterior FOI REMOVIDO: era declarado, zerado por memset e nunca lido
  // nem escrito por ninguem — nem pelo produtor (trakt.c), nem pela tela.
  int diasAtivosMes;
  int diasAtivosAno;
  int primeiroDiaSemana;     // 0=domingo..6=sabado
  int nDias;
  unsigned short atividade[PERFIL_MAX_DIAS];

  int nGeneros;
  PerfilGenero generos[PERFIL_MAX_GENEROS];
  int nDestaques;
  PerfilDestaque destaques[PERFIL_MAX_DESTAQUES];
  // Zero e o padrao seguro para produtores antigos. O snapshot mensal nao
  // comprova cobertura anual nem uma sequencia que cruza a virada do mes.
  int parcial;
  int anoCompleto;
  int streakCompleto;
  char aviso[160];
} PerfilDados;

#endif
