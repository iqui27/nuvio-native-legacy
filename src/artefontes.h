// LEITURA DAS RESPOSTAS DAS FONTES DE ARTE DO DESTAQUE (23/09/2026).
//
// Sem rede, sem SDL e sem estado: recebe o corpo que a API devolveu e escolhe
// a url. Mora aqui, e nao em artereserva.c, para os testes lerem fixtures
// (tests/artefontes.sh) sem dublar rede nenhuma. Quem pergunta a rede e
// artereserva.c (arte_fonte_resolver).
//
// O QUE CADA FONTE ENTREGA (medido com curl em 23/09):
//   TMDB /images   dezenas de backdrops por titulo (Um Sonho de Liberdade: 68),
//                  com iso_639_1 null = sem texto. O `backdrop_path` padrao e so
//                  um deles.
//   Apple TV       a busca da UTS (a mesma do trailer, trailerapple.c) traz
//                  images.shelfImageBackground: arte-chave 3840x2160 SEM titulo
//                  escrito, servida pelo mzstatic em qualquer tamanho
//                  ({w}x{h}.{f}). 1920x1080.jpg = ~300 KB.
//   fanart.tv      moviebackground/showbackground 1920x1080, so com chave.
//   Kitsu          coverImage e FAIXA larga (large = 3360x800), nao 16:9.
//   AniList        bannerImage tambem e faixa (1900x400).
#ifndef NV_ARTEFONTES_H
#define NV_ARTEFONTES_H
#include <stddef.h>

// Texto para um segmento de url virtual: [A-Za-z0-9-_.~] passam, o resto vira
// %XX (a barra inclusive). `dst` sempre termina em 0; corta no limite.
void af_codificar(const char *s, char *dst, size_t n);
// O inverso, lendo `s` ate a proxima '/' ou o fim. Devolve o ponteiro para
// onde parou (a '/' ou o 0).
const char *af_decodificar(const char *s, char *dst, size_t n);

// TMDB. `corpo` e a resposta de /{movie|tv}/{id}?append_to_response=images
// (tem `backdrop_path` na raiz e images.backdrops) ou de /{movie|tv}/{id}/images
// (so os backdrops). `padrao` recebe o backdrop_path da raiz (vazio se nao
// veio). `outro` recebe o MELHOR backdrop que nao e o padrao nem `evitar`
// (caminho "/x.jpg", pode ser NULL): sem texto primeiro, depois maior nota,
// depois mais votos. Devolve 1 se `outro` saiu, 0 se so o padrao (ou nada).
int af_tmdb_fundos(const char *corpo, const char *evitar,
                   char *padrao, size_t np, char *outro, size_t no);

// fanart.tv v3 (/movies/{id} ou /tv/{tvdb}). Fundo sem idioma ("" ou "00")
// antes do com idioma; entre iguais, mais likes. 1 se achou.
int af_fanart_fundo(const char *corpo, int serie, char *url, size_t n);

// Kitsu (/anime?filter[text]=... em lista, ou /anime/{id} em objeto): a
// coverImage `large` do primeiro anime do tipo certo (filme = subtype movie;
// serie = qualquer outro) com ano +-1 de `ano` (0 = nao confere). 1 se achou.
int af_kitsu_capa(const char *corpo, int ano, int serie, char *url, size_t n);

// AniList GraphQL: data.Media.bannerImage, conferindo startDate.year +-1
// quando `ano` > 0. 1 se achou.
int af_anilist_banner(const char *corpo, int ano, char *url, size_t n);

// Apple (mzstatic): troca o "{w}x{h}.{f}" do modelo por "<larg>x<alt>.jpg",
// 16:9. 1 se o modelo tinha o marcador.
int af_apple_tamanho(const char *modelo, int larg, char *url, size_t n);

#endif
