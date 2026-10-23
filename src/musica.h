// Film/season soundtrack from NV_REC_URL/musica/v1, bounded iTunes previews,
// and the active profile's saved albums. Full albums open through the phone QR.
#ifndef NV_MUSICA_H
#define NV_MUSICA_H

#define MUS_FAIXAS_MAX 60
#define MUS_GUARDADAS_MAX 48

typedef struct { char imdb[16], titulo[128], artista[96]; int temp; } MusSalva;
int mus_listar(MusSalva *out, int max);
#define MUS_POR_PAGINA 7
// Seven tracks per TV page; clamp at both ends.
int mus_pagina(int atual, int faixas, int direcao);

typedef enum { MUS_NADA = 0, MUS_CARREGANDO, MUS_PRONTO, MUS_VAZIO, MUS_ERRO } MusEstado;

typedef struct {
  char titulo[128], artista[96], capa[256], apple[256];
  int  n, ano;
  struct { int n; char titulo[96], artista[64], previa[768]; int dur; } faixa[MUS_FAIXAS_MAX];
} MusAlbum;

// Bounded playlist and preview state, independent of platform decoding.
typedef struct {
  int ordem[MUS_FAIXAS_MAX], n, pos, faixa, ativa, pronta;
  unsigned desde;
} MusPrevia;
enum { MUS_PREV_CONTINUA, MUS_PREV_FIM, MUS_PREV_ERRO, MUS_PREV_OK,
       MUS_PREV_BACK, MUS_PREV_SAIR, MUS_PREV_OCUPADO };
int mus_previa_iniciar(MusPrevia *p, const MusAlbum *a, int faixa, int shuffle,
                       unsigned seed, unsigned agora, int ocupado);
int mus_previa_passo(MusPrevia *p, unsigned agora, double segundos, int pronta, int evento);
void mus_previa_parar(MusPrevia *p);
// Static spatial cover colors, RGBA32 input/output; caller owns 32x18x4 bytes.
void mus_cores(const unsigned char *px, int w, int h, int pitch, unsigned char *out);

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
