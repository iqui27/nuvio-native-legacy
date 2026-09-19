// Reserva de arte pelo TMDB quando o metahub falha. Sem rede: rede_baixar e
// desc_chave_tmdb sao dubles, e o teste olha so o que arte_reserva_url faz
// com a URL e com a resposta do /find.
#include "artereserva.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *chave = "CHAVE";
static const char *resposta = NULL;
static char ultimaUrl[400];
const char *desc_chave_tmdb(void) { return chave; }
char *rede_baixar(const char *url, int segundos) {
  (void)segundos;
  snprintf(ultimaUrl, sizeof ultimaUrl, "%s", url);
  return resposta ? strdup(resposta) : NULL;
}

static int falhas = 0;
#define OK(cond, msg) do { if (!(cond)) { printf("FALHOU: %s\n", msg); falhas++; } } while (0)

int main(void) {
  char s[400];
  // Filme: poster w342 pelo movie_results.
  resposta = "{\"movie_results\":[{\"id\":278,\"poster_path\":\"/abc.jpg\",\"backdrop_path\":\"/fundo.jpg\"}],\"tv_results\":[]}";
  OK(arte_reserva_url("https://images.metahub.space/poster/medium/tt0111161/img", s, sizeof s) == 1, "poster de filme");
  OK(!strcmp(s, "https://image.tmdb.org/t/p/w342/abc.jpg"), "url do poster");
  OK(strstr(ultimaUrl, "/3/find/tt0111161?api_key=CHAVE&external_source=imdb_id") != NULL, "url do /find");
  OK(arte_reserva_url("https://images.metahub.space/background/medium/tt0111161/img", s, sizeof s) == 1, "fundo de filme");
  OK(!strcmp(s, "https://image.tmdb.org/t/p/w1280/fundo.jpg"), "url do fundo");
  // Serie: movie_results vazio, cai no tv_results.
  resposta = "{\"movie_results\":[],\"tv_results\":[{\"id\":1396,\"poster_path\":\"/bb.jpg\"}]}";
  OK(arte_reserva_url("https://images.metahub.space/poster/small/tt0903747/img", s, sizeof s) == 1, "poster de serie");
  OK(!strcmp(s, "https://image.tmdb.org/t/p/w342/bb.jpg"), "url do poster de serie");
  // Sem imagem no TMDB (null) -> 0.
  resposta = "{\"movie_results\":[{\"id\":1,\"poster_path\":null}],\"tv_results\":[]}";
  OK(arte_reserva_url("https://images.metahub.space/poster/medium/tt0000001/img", s, sizeof s) == 0, "poster nulo");
  // Titulo desconhecido -> 0.
  resposta = "{\"movie_results\":[],\"tv_results\":[]}";
  OK(arte_reserva_url("https://images.metahub.space/poster/medium/tt9999999/img", s, sizeof s) == 0, "desconhecido");
  // Logo nao tem reserva; outro host nao e metahub; sem chave nada.
  resposta = "{\"movie_results\":[{\"poster_path\":\"/x.jpg\"}]}";
  ultimaUrl[0] = 0;
  OK(arte_reserva_url("https://images.metahub.space/logo/medium/tt0111161/img", s, sizeof s) == 0, "logo");
  OK(arte_reserva_url("https://image.tmdb.org/t/p/w342/abc.jpg", s, sizeof s) == 0, "outro host");
  OK(arte_reserva_url("https://images.metahub.space/poster/medium/12345/img", s, sizeof s) == 0, "id sem tt");
  OK(ultimaUrl[0] == 0, "nenhum pedido feito nos casos recusados");
  chave = "";
  OK(arte_reserva_url("https://images.metahub.space/poster/medium/tt0111161/img", s, sizeof s) == 0, "sem chave");
  OK(ultimaUrl[0] == 0, "sem chave nao pede");
  // Rede falhou -> 0.
  chave = "CHAVE"; resposta = NULL;
  OK(arte_reserva_url("https://images.metahub.space/poster/medium/tt0111161/img", s, sizeof s) == 0, "rede falhou");
  printf("%s\n", falhas ? "artereserva: FALHOU" : "artereserva: ok");
  return falhas ? 1 : 0;
}
