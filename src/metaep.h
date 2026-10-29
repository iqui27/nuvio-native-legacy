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
void metaep_scrobble_corpo(char *dst, size_t n, const char *showImdb,
                           int t, int e, const MetaEpInfo *ep, double pct);

#endif
