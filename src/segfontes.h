// FONTES DE SEGMENTOS DE ANIME COMO NO NUVIO TV / PC (feat/mkv-chapter-segments).
//
// Duas pecas que intro.c nao tinha, portadas de SimklIdResolver.kt e
// SkipIntroRepository.kt (Namer03/NuvioTV, Namer03/NuvioDesktop):
//
//   SIMKL: IMDb (ou kitsu:/mal:) -> o id do MyAnimeList e do AniList DA
//   TEMPORADA certa (mapped_tvdb_seasons) e o numero do episodio dentro dela
//   (TVDB T/E -> episodio do anime). Usa o client_id Simkl que o app ja embute
//   (nuvem_simkl_cliente); vazio = Simkl desligado e intro.c cai na Fribb.
//
//   ANIME-SKIP: GraphQL em api.anime-skip.com com o Client ID da PROPRIA pessoa
//   (ajuste "ID do Anime-Skip", tambem sincronizado da conta Nuvio). Sem ID, nada.
//
// Tudo BLOQUEIA: chamar do fio de intro.c, nunca do fio do player.
#ifndef NV_SEGFONTES_H
#define NV_SEGFONTES_H
#include <stddef.h>
#include "intro.h"

typedef struct {
  long simkl;        // id Simkl da entrada usada (a da temporada, quando houve)
  long mal;          // 0 = sem MyAnimeList
  long anilist;      // 0 = sem AniList
  int  ep;           // episodio DENTRO da entrada do anime (pode diferir do TVDB)
  int  anime;        // 1 = Simkl diz que e anime
} SegIds;

// `fonte`: "imdb", "mal" ou "kitsu". `t`/`e`: temporada e episodio no TVDB
// (imdb) ou o episodio do item (mal/kitsu: `t` e ignorado). Devolve 1 quando
// resolveu algo (mesmo sem mal/anilist), 0 sem resposta util ou sem client_id.
// Cache em memoria por id.
int segfontes_simkl(const char *fonte, const char *id, int t, int e, SegIds *out);

// Anime-Skip. `clientId` vazio/NULL = 0 sem rede. `*falha` = 1 em erro de
// rede/servidor (nao em "nao conhece"). Devolve quantos trechos.
int segfontes_animeskip(long anilist, int ep, const char *clientId,
                        IntroTrecho *out, int max, int *falha);

// Leitores puros (testes).
int  segfontes_animeskip_shows(const char *json, char ids[][48], int max);
int  segfontes_animeskip_ep(const char *json, int ep, IntroTrecho *out, int max);
long segfontes_simkl_busca(const char *json, char *tipo, size_t ntipo);   // id simkl, tipo "anime"/"tv"/"movies"
int  segfontes_simkl_detalhes(const char *json, long *mal, long *anilist, int *temporada);
long segfontes_simkl_temporada(const char *json, int tvdbT);              // simkl_id da temporada
int  segfontes_simkl_ep(const char *json, int tvdbT, int tvdbE);          // episodio do anime, 0 = nao ha

// Dubles de rede (testes). NULL volta ao padrao (rede_pedir).
void segfontes_definir_buscador(int (*get)(const char *url, char **corpo, int *status));
void segfontes_definir_postador(int (*post)(const char *url, const char *corpo,
                                            const char *clientId, char **resp, int *status));
void segfontes_limpar_cache(void);

// O Client ID do Anime-Skip em uso: o do ajuste, senao o da conta Nuvio.
// Definidos por ajustes.c / sync.c; aqui so guardamos.
void segfontes_animeskip_id_conta(const char *id);   // sync.c (credential_json.client_id)
void segfontes_animeskip_id_local(const char *id);   // ajuste digitado (vazio = usa a conta)
const char *segfontes_animeskip_id(char *buf, size_t n);
#endif
