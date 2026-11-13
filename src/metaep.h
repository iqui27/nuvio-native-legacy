#ifndef NV_METAEP_H
#define NV_METAEP_H
#include <stddef.h>

typedef struct {
  int temporada, episodio;
  char tvdb[24];
  char tmdb[24];
  char trakt[24];
} MetaEpInfo;

int metaep_temporada(const char *video, const char *fim);
int metaep_tvdb_video(const char *video, const char *fim);
int metaep_existe(const char *corpo, int t, int e);
int metaep_buscar(const char *corpo, int t, int e, MetaEpInfo *out);
// Finds one unambiguous episode identity even when the metadata provider uses
// a different season layout from Trakt. Repeated copies of the same pair are
// accepted; assigning the ID to different pairs is not.
int metaep_buscar_tvdb(const char *corpo, int tvdb, MetaEpInfo *out);
int metaep_proximo(const char *corpo, int t, int e, MetaEpInfo *out);
int metaep_nome(const char *corpo, int t, int e, char *dst, size_t n);
int metaep_lancamento(const char *corpo, int t, int e, char *dst, size_t n);
// TVDB per-season numbers onto a list that numbers episodes absolutely
// (Nuvio's One Piece: S2 runs E62-E77, TVDB's S2E1 is S2E62). Built once per
// metadata body, then each lookup is arithmetic: the anime import asks for
// every watched episode, and scanning the body each time took ~10 s for One
// Piece on an LG.
typedef struct {
  int temporada, primeiro, ultimo;
  int contiguos;  // primeiro..primeiro+contiguos-1 each appear exactly once
} MetaTempFaixa;
typedef struct {
  MetaTempFaixa *faixas;
  int n, falsas;
} MetaTvdbIndice;
int metaep_tvdb_indice(const char *corpo, MetaTvdbIndice *ix);
// TVDB season `temporada` (>1), episode `episodio`, on that list: 1 and
// out->temporada/episodio when the season continues the previous one's
// numbers with no gap or repeat up to that episode.
int metaep_tvdb_global(const MetaTvdbIndice *ix, int temporada, int episodio,
                       MetaEpInfo *out);
void metaep_tvdb_indice_liberar(MetaTvdbIndice *ix);
void metaep_scrobble_corpo(char *dst, size_t n, const char *showImdb,
                           int t, int e, const MetaEpInfo *ep, double pct);

#endif
