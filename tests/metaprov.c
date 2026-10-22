// metaprov: montagem de URL (idioma, ids, busca) e decisao de reserva, com
// respostas falsas. Sem rede.
#include "metaprov.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned int SDL_GetTicks(void) { return 0; }
void marco(const char *m) { (void)m; }
static const char *idiomaFalso = "pt-BR";
const char *ajustes_tmdb_idioma(void) { return idiomaFalso; }
char *rede_baixar(const char *url, int seg) {
  (void)url; (void)seg; return NULL;
}

static int falhas;
#define CHECK(c) do { if (!(c)) { printf("FALHOU %s:%d  %s\n", __FILE__, __LINE__, #c); falhas++; } } while (0)

// Rede falsa: roteia por host; cada chamada fica registrada.
static int nNuvio, nCine, segNuvio, segCine;
static char ultimaNuvio[600];
static int  stNuvio = 200, stCine = 200;
static const char *corpoNuvio, *corpoCine;
static char *falso(const char *url, int seg, int *st, void *ctx) {
  (void)ctx;
  if (strstr(url, "catalog.nuvio.tv")) {
    nNuvio++; segNuvio = seg; snprintf(ultimaNuvio, sizeof ultimaNuvio, "%s", url);
    *st = stNuvio;
    return stNuvio == 200 && corpoNuvio ? strdup(corpoNuvio) : NULL;
  }
  nCine++; segCine = seg; *st = stCine;
  return stCine == 200 && corpoCine ? strdup(corpoCine) : NULL;
}
static void zera(void) {
  nNuvio = nCine = 0; stNuvio = stCine = 200; corpoNuvio = corpoCine = NULL;
  metaprov_zerar_pausa();
}

static const char *META_OK = "{\"meta\":{\"id\":\"tt1375666\",\"name\":\"A Origem\",\"videos\":[]}}";
static const char *META_CINE = "{\"meta\":{\"id\":\"tt1375666\",\"name\":\"Inception\"}}";
static const char *BUSCA_OK = "{\"metas\":[{\"id\":\"tt1375666\",\"name\":\"A Origem\"}]}";
static const char *BUSCA_VAZIA = "{\"metas\":[]}";

int main(void) {
  char u[600], k[200], *c;
  int prov;

  // --- URLs ---
  metaprov_url_meta(u, sizeof u, METAPROV_NUVIO, "movie", "tt1375666", "pt-BR");
  CHECK(!strcmp(u, "https://catalog.nuvio.tv/%7B%22language%22%3A%22pt-BR%22%7D/meta/movie/tt1375666.json"));
  metaprov_url_meta(u, sizeof u, METAPROV_NUVIO, "series", "tt0944947", "en-US");
  CHECK(!strcmp(u, "https://catalog.nuvio.tv/%7B%22language%22%3A%22en-US%22%7D/meta/series/tt0944947.json"));
  metaprov_url_meta(u, sizeof u, METAPROV_NUVIO, "movie", "tmdb:27205", "ja-JP");
  CHECK(strstr(u, "/meta/movie/tmdb:27205.json") != NULL);
  metaprov_url_meta(u, sizeof u, METAPROV_NUVIO, "movie", "tt1/../x y", "pt-BR");
  CHECK(strstr(u, "x%20y") && !strstr(u, "/../"));          // id nunca escapa do caminho
  metaprov_url_meta(u, sizeof u, METAPROV_NUVIO, "movie", "tt1", "pt\"BR}");
  CHECK(strstr(u, "en-US") && !strchr(u + 8, '"'));         // idioma estranho -> en-US
  metaprov_url_meta(u, sizeof u, METAPROV_CINEMETA, "series", "tt0944947", "pt-BR");
  CHECK(!strcmp(u, "https://v3-cinemeta.strem.io/meta/series/tt0944947.json"));

  metaprov_url_busca(u, sizeof u, METAPROV_NUVIO, "movie", "the invite", "pt-BR");
  CHECK(!strcmp(u, "https://catalog.nuvio.tv/%7B%22language%22%3A%22pt-BR%22%7D/catalog/movie/popular-movies/search=the%20invite.json"));
  metaprov_url_busca(u, sizeof u, METAPROV_NUVIO, "series", "caf\xc3\xa9 & co/?#", "en-US");
  CHECK(strstr(u, "/catalog/series/popular-series/search=caf%C3%A9%20%26%20co%2F%3F%23.json") != NULL);
  metaprov_url_busca(u, sizeof u, METAPROV_CINEMETA, "series", "a b", NULL);
  CHECK(!strcmp(u, "https://v3-cinemeta.strem.io/catalog/series/top/search=a%20b.json"));

  // --- idioma e chave de cache ---
  idiomaFalso = "pt-BR";
  CHECK(!strcmp(metaprov_idioma(), "pt-BR"));
  metaprov_chave(k, sizeof k, "series", "tt1");
  CHECK(!strcmp(k, "nuvio:pt-BR/series/tt1"));
  idiomaFalso = "en-US";
  { char k2[200], k3[200];
    metaprov_chave(k2, sizeof k2, "series", "tt1");
    metaprov_chave(k3, sizeof k3, "movie", "tt1");
    CHECK(strcmp(k, k2) != 0);                 // idioma novo = chave nova
    CHECK(strcmp(k2, k3) != 0); }              // tipo continua na chave
  idiomaFalso = "";
  CHECK(!strcmp(metaprov_idioma(), "en-US"));
  idiomaFalso = "pt-BR";

  // --- validade ---
  CHECK(metaprov_meta_valido(META_OK));
  CHECK(!metaprov_meta_valido("{\"meta\":{}}"));
  CHECK(!metaprov_meta_valido("{\"meta\": null}"));
  CHECK(!metaprov_meta_valido("{\"meta\":{\"id\":\"tt1\"}}"));   // sem name
  CHECK(!metaprov_meta_valido("not json"));
  CHECK(!metaprov_meta_valido(""));
  CHECK(!metaprov_meta_valido(NULL));
  CHECK(metaprov_busca_valida(BUSCA_VAZIA));
  CHECK(!metaprov_busca_valida("{\"err\":1}"));

  // --- meta: Nuvio responde ---
  zera(); corpoNuvio = META_OK; corpoCine = META_CINE;
  c = metaprov_meta_com("movie", "tt1375666", 10, falso, NULL, &prov);
  CHECK(c && strstr(c, "A Origem") && prov == METAPROV_NUVIO && nNuvio == 1 && nCine == 0);
  CHECK(segNuvio == METAPROV_NUVIO_S);
  CHECK(strstr(ultimaNuvio, "pt-BR") != NULL);
  free(c);

  // --- meta: Nuvio vazio/invalido/404 -> Cinemeta, sem pausar (nao e queda) ---
  zera(); corpoNuvio = "{\"meta\":{}}"; corpoCine = META_CINE;
  c = metaprov_meta_com("movie", "tt1", 10, falso, NULL, &prov);
  CHECK(c && prov == METAPROV_CINEMETA && nNuvio == 1 && nCine == 1 && segCine == 10);
  free(c);
  corpoNuvio = "<html>"; nNuvio = nCine = 0;
  c = metaprov_meta_com("movie", "tt1", 10, falso, NULL, &prov);
  CHECK(c && prov == METAPROV_CINEMETA && nNuvio == 1);
  free(c);
  stNuvio = 404; nNuvio = nCine = 0;
  c = metaprov_meta_com("movie", "tt1", 10, falso, NULL, &prov);
  CHECK(c && prov == METAPROV_CINEMETA && nNuvio == 1);
  free(c);
  nNuvio = 0;
  c = metaprov_meta_com("movie", "tt1", 10, falso, NULL, &prov);     // 404 nao pausa
  CHECK(c && nNuvio == 1);
  free(c);

  // --- meta: queda de rede/5xx -> Cinemeta e Nuvio descansa ---
  zera(); stNuvio = 0; corpoCine = META_CINE;
  c = metaprov_meta_com("series", "tt2", 10, falso, NULL, &prov);
  CHECK(c && prov == METAPROV_CINEMETA && nNuvio == 1 && nCine == 1);
  free(c);
  c = metaprov_meta_com("series", "tt3", 10, falso, NULL, &prov);
  CHECK(c && nNuvio == 1 && nCine == 2);                              // nao tentou o Nuvio de novo
  free(c);
  metaprov_zerar_pausa(); stNuvio = 503; nNuvio = 0;
  c = metaprov_meta_com("series", "tt2", 10, falso, NULL, &prov);
  free(c);
  c = metaprov_meta_com("series", "tt3", 10, falso, NULL, &prov);
  CHECK(nNuvio == 1);                                                 // 5xx tambem pausa
  free(c);

  // --- meta: prazo do Nuvio nunca passa do prazo do chamador ---
  zera(); corpoNuvio = META_OK;
  c = metaprov_meta_com("movie", "tt1", 3, falso, NULL, &prov);
  CHECK(segNuvio == 3);
  free(c);

  // --- meta: nada responde ---
  zera(); stNuvio = 0; stCine = 0;
  c = metaprov_meta_com("movie", "tt1", 10, falso, NULL, &prov);
  CHECK(c == NULL && prov == -1);

  // --- busca ---
  zera(); corpoNuvio = BUSCA_OK; corpoCine = BUSCA_OK;
  c = metaprov_busca_com("movie", "origem", 5, 6, falso, NULL, &prov);
  CHECK(c && prov == METAPROV_NUVIO && nCine == 0);
  CHECK(strstr(ultimaNuvio, "popular-movies/search=origem.json") != NULL);
  free(c);
  zera(); corpoNuvio = BUSCA_VAZIA; corpoCine = BUSCA_OK;               // vazio e resposta valida
  c = metaprov_busca_com("series", "zzzz", 5, 6, falso, NULL, &prov);
  CHECK(c && prov == METAPROV_NUVIO && nCine == 0);
  free(c);
  zera(); stNuvio = 0; corpoCine = BUSCA_OK;                            // Nuvio fora -> Cinemeta
  c = metaprov_busca_com("series", "origem", 5, 6, falso, NULL, &prov);
  CHECK(c && prov == METAPROV_CINEMETA && nNuvio == 1 && segCine == 6);
  free(c);
  zera(); corpoNuvio = "garbage"; corpoCine = BUSCA_OK;                 // JSON invalido -> Cinemeta
  c = metaprov_busca_com("movie", "origem", 5, 6, falso, NULL, &prov);
  CHECK(c && prov == METAPROV_CINEMETA);
  free(c);
  zera(); stNuvio = 0; stCine = 0;
  c = metaprov_busca_com("movie", "x", 5, 6, falso, NULL, &prov);
  CHECK(c == NULL);

  // Id fora dos idPrefixes do Nuvio (anime "kitsu:") vai direto ao Cinemeta e
  // nunca pausa o Nuvio.
  zera(); corpoCine = META_CINE; stNuvio = 0;
  c = metaprov_meta_com("series", "kitsu:1376", 10, falso, NULL, &prov);
  CHECK(nNuvio == 0 && nCine == 1); free(c);
  corpoNuvio = META_OK; stNuvio = 200;
  c = metaprov_meta_com("movie", "tt1375666", 10, falso, NULL, &prov);
  CHECK(c && prov == METAPROV_NUVIO); free(c);

  // Duracao: "148m" (filme do Nuvio), "25min" (episodio), "148 min" (Cinemeta).
  { const char *in[] = { "148m", "25min", "148 min", "54 min", "1h 30m", "", "abc", "0m" };
    const char *out[] = { "148 min", "25 min", "148 min", "54 min", "1h 30m", "", "abc", "0m" };
    int i; char s[24];
    for (i = 0; i < 8; i++) { snprintf(s, sizeof s, "%s", in[i]); metaprov_duracao(s, sizeof s);
                              CHECK(!strcmp(s, out[i])); } }

  if (falhas) { printf("%d falha(s)\n", falhas); return 1; }
  puts("metaprov: ok");
  return 0;
}
