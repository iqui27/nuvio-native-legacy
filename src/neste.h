// NESTE EPISODIO: o elenco convidado do episodio (TMDB /tv/{id}/season/{s}/
// episode/{e}/credits, guest_stars), sem sincronia com o video. Um painel no
// player lista quem aparece; o dado vem de um fio proprio, 1 pedido por episodio.
#ifndef NV_NESTE_H
#define NV_NESTE_H

#define NESTE_MAX 8

typedef enum { NESTE_NADA = 0, NESTE_CARREGANDO, NESTE_PRONTO, NESTE_VAZIO, NESTE_ERRO } NesteEstado;

typedef struct { int n; struct { char nome[64], personagem[64]; } p[NESTE_MAX]; } NesteElenco;

// Pede (idempotente) o elenco de imdb T`temp`E`ep`.
void        neste_pedir(const char *imdb, int temp, int ep);
// Copia para `out` (pode ser NULL) e devolve o estado.
NesteEstado neste_estado(const char *imdb, int temp, int ep, NesteElenco *out);
// Interpreta a resposta do TMDB (guest_stars); devolve quantos nomes achou.
int         neste_interpretar(const char *json, NesteElenco *out);

#endif
