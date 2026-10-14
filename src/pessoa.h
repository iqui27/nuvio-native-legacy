// Person metadata shared by the title page and the 2.1 filmography.
#ifndef NV_PESSOA_H
#define NV_PESSOA_H
#include "filmomodelo.h"
#define PES_MAX FM_MAX
void pessoa_pedir(long tmdbId, const char *nomeConhecido, const char *foto);
int pessoa_pronta(void);
long pessoa_id(void);
unsigned pessoa_revisao(void);
const char *pessoa_nome(void);
const char *pessoa_foto(void);
const char *pessoa_bio(void);
const char *pessoa_area(void);
const char *pessoa_nascimento(void);
const char *pessoa_morte(void);
const char *pessoa_lugar(void);
const char *pessoa_outros(void);
const char *pessoa_wikidata(void);
int pessoa_bio_ingles(void);
const FmCredito *pessoa_creditos(void);
int pessoa_n_creditos(void);
const char *pessoa_credito_titulo(int i);
const char *pessoa_credito_papel(int i);
const char *pessoa_credito_ano(int i);
const char *pessoa_credito_poster(int i);
const char *pessoa_credito_imdb(int i);
long pessoa_credito_tmdb(int i);
const char *pessoa_credito_tipo(int i);
// Parser also used by the offline screenshot harness.
int pessoa_ler(long id, const char *json, const char *hoje);
#endif
