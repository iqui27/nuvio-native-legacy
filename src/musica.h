// MUSICAS do titulo: a trilha sonora original (filme ou temporada) vinda do
// servico de recomendacoes (NV_REC_URL/musica/v1, MusicBrainz + iTunes) e a
// lista "Guardadas" do perfil. Sem Spotify/Apple Music: o app so MOSTRA a
// trilha e entrega um QR para abrir a busca no celular (NV_REC_URL/m/).
#ifndef NV_MUSICA_H
#define NV_MUSICA_H

#define MUS_FAIXAS_MAX 60
#define MUS_GUARDADAS_MAX 48

typedef struct { char imdb[16], titulo[128], artista[96]; int temp; } MusSalva;
int mus_listar(MusSalva *out, int max);
// Two tracks per TV page; clamp at both ends.
int mus_pagina(int atual, int faixas, int direcao);

typedef enum { MUS_NADA = 0, MUS_CARREGANDO, MUS_PRONTO, MUS_VAZIO, MUS_ERRO } MusEstado;

typedef struct {
  char titulo[128], artista[96], capa[256], apple[256];
  int  n;
  struct { int n; char titulo[96], artista[64]; int dur; } faixa[MUS_FAIXAS_MAX];
} MusAlbum;

// Pede (fio proprio, idempotente) a trilha de imdb; `temp` 0 = filme, >0 = temporada.
void      mus_pedir(const char *imdb, const char *titulo, int ano, int temp);
// Copia o album para `out` (pode ser NULL) e devolve o estado.
MusEstado mus_estado(const char *imdb, int temp, MusAlbum *out);

// Interpreta a resposta do Worker; 1 se tem album (testavel sem rede).
int       mus_interpretar(const char *json, MusAlbum *out);
// URL do QR: NV_REC_URL/m/#id=..&t=..&y=..&s=.. ; "" sem NV_REC_URL.
void      mus_url_celular(char *dst, unsigned tam, const char *imdb,
                          const char *titulo, int ano, int temp);

// Lista "Guardadas" do perfil ativo (musicas-p<N>.txt).
int       mus_guardado(const char *imdb, int temp);
// Liga/desliga; devolve o novo estado (1 = guardado).
int       mus_alternar_guardado(const char *imdb, int temp, const char *album,
                                const char *artista);
void      mus_esquecer(void); // memoria apenas (teste / troca de perfil)

#endif
