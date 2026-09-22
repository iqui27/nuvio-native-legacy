// Reserva de arte pelo TMDB quando o metahub falha. Sem rede: rede_baixar e
// desc_chave_tmdb sao dubles, e o teste olha so o que arte_reserva_url faz
// com a URL e com a resposta do /find.
#include "artereserva.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *chave = "CHAVE";
static const char *resposta = NULL;      // resposta do /find
static const char *respostaEp = NULL;    // resposta do /tv/.../episode/...
static int respostasColisao;
static char ultimaUrl[400];
static int pedidos;
const char *desc_chave_tmdb(void) { return chave; }
const char *desc_chave_tmdb_reserva(void) { return chave; }
char *rede_baixar(const char *url, int segundos) {
  (void)segundos;
  pedidos++;
  snprintf(ultimaUrl, sizeof ultimaUrl, "%s", url);
  if (strstr(url, "/season/")) return respostaEp ? strdup(respostaEp) : NULL;
  if (respostasColisao) {
    if (strstr(url, "/find/tt0111161"))
      return strdup("{\"movie_results\":[{\"poster_path\":\"/one.jpg\",\"backdrop_path\":\"/one-bg.jpg\"}],\"tv_results\":[]}");
    if (strstr(url, "/find/tt0903747"))
      return strdup("{\"movie_results\":[{\"poster_path\":\"/two.jpg\",\"backdrop_path\":\"/two-bg.jpg\"}],\"tv_results\":[]}");
  }
  return resposta ? strdup(resposta) : NULL;
}

static int falhas = 0;
#define OK(cond, msg) do { if (!(cond)) { printf("FALHOU: %s\n", msg); falhas++; } } while (0)

static unsigned fixture_slot(const char *s) {
  unsigned h = 2166136261u;
  for (; *s; s++) { h ^= (unsigned char)*s; h *= 16777619u; }
  return h % 4096u;
}

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
  // Duas URLs distintas com o mesmo slot inicial exercitam o probing real.
  // Cada IMDb tem resposta diferente; assim o teste detecta tanto overwrite
  // quanto uma leitura que confia apenas no hash.
  {
    const char *urlA = "https://addon/poster/129.jpg";
    const char *urlB = "https://addon/background/144.jpg";
    respostasColisao = 1;
    OK(fixture_slot(urlA) == fixture_slot(urlB), "fixtures de colisao compartilham slot");
    OK(arte_reserva_registrar(urlA, "tt0111161", 1) == 1, "registra primeira colisao");
    OK(arte_reserva_registrar(urlB, "tt0903747", 0) == 1, "registra segunda colisao");
    pedidos = 0;
    OK(arte_reserva_url(urlA, s, sizeof s) == 1 &&
       !strcmp(s, "https://image.tmdb.org/t/p/w342/one.jpg"),
       "primeira URL colidida conserva seu IMDb");
    OK(arte_reserva_url(urlB, s, sizeof s) == 1 &&
       !strcmp(s, "https://image.tmdb.org/t/p/w1280/two-bg.jpg"),
       "segunda URL colidida conserva seu IMDb");
    OK(pedidos == 2, "cada URL colidida consulta seu proprio IMDb");
    OK(arte_reserva_registrar(urlA, "tt0111161", 0) == 1, "re-registro da mesma URL atualiza tipo");
    OK(arte_reserva_url(urlA, s, sizeof s) == 1 &&
       !strcmp(s, "https://image.tmdb.org/t/p/w1280/one-bg.jpg"),
       "re-registro nao cria entrada nem perde a chave");
    // URL nao registrada continua sem consulta: isto e uma negativa real,
    // nao uma segunda metade da colisao.
    pedidos = 0; ultimaUrl[0] = 0;
    OK(arte_reserva_url("https://addon/not-registered.jpg", s, sizeof s) == 0,
       "URL nao registrada nao recebe reserva");
    OK(pedidos == 0 && ultimaUrl[0] == 0, "negativa nao consulta TMDB");
    respostasColisao = 0;
  }
  ultimaUrl[0] = 0;
  chave = "";
  OK(arte_reserva_url("https://images.metahub.space/poster/medium/tt0111161/img", s, sizeof s) == 0, "sem chave");
  OK(ultimaUrl[0] == 0, "sem chave nao pede");
  // Rede falhou -> 0.
  chave = "CHAVE"; resposta = NULL;
  OK(arte_reserva_url("https://images.metahub.space/poster/medium/tt0111161/img", s, sizeof s) == 0, "rede falhou");
  // Still de episodio: /find da o id da serie, /tv/<id>/season/<s>/episode/<e> da o still.
  resposta = "{\"movie_results\":[],\"tv_results\":[{\"id\":1396,\"poster_path\":\"/bb.jpg\"}]}";
  respostaEp = "{\"air_date\":\"2008-01-20\",\"name\":\"Pilot\",\"still_path\":\"/pilot.jpg\",\"guest_stars\":[{\"name\":\"x\"}]}";
  pedidos = 0;
  OK(arte_reserva_url("https://episodes.metahub.space/tt0903747/1/1/w780.jpg", s, sizeof s) == 1, "still");
  OK(!strcmp(s, "https://image.tmdb.org/t/p/original/pilot.jpg"), "url do still");
  OK(pedidos == 2, "still custa duas viagens");
  OK(strstr(ultimaUrl, "/3/tv/1396/season/1/episode/1?api_key=CHAVE") != NULL, "url do episodio");
  respostaEp = "{\"still_path\":null}";
  OK(arte_reserva_url("https://episodes.metahub.space/tt0903747/1/2/w780.jpg", s, sizeof s) == 0, "still nulo");
  resposta = "{\"movie_results\":[],\"tv_results\":[]}"; pedidos = 0;
  OK(arte_reserva_url("https://episodes.metahub.space/tt0000001/1/1/w780.jpg", s, sizeof s) == 0, "serie desconhecida");
  OK(pedidos == 1, "sem id nao pede o episodio");
  OK(arte_reserva_url("https://episodes.metahub.space/tt0903747/x/1/w780.jpg", s, sizeof s) == 0, "still mal formado");
  // Limite explícito: URLs reais são compactadas na arena; repetir a mesma
  // chave atualiza a entrada e não consome espaço. Depois de encher a tabela,
  // uma nova chave é recusada de forma observável.
  {
    int aceitos = 0, recusados = 0, i;
    char url[96], imdb[24];
    for (i = 0; i < 4096; i++) {
      snprintf(url, sizeof url, "https://capacity/%04d.jpg", i);
      snprintf(imdb, sizeof imdb, "tt%07d", i);
      if (arte_reserva_registrar(url, imdb, 1)) aceitos++; else recusados++;
    }
    for (i = 0; i < 32; i++) {
      snprintf(url, sizeof url, "https://capacity-extra/%04d.jpg", i);
      if (!arte_reserva_registrar(url, "tt9999999", 1)) recusados++;
    }
    OK(aceitos > 4000, "registry bounded aceita milhares de entradas");
    OK(recusados > 0, "registry cheia recusa nova entrada");
    OK(arte_reserva_registrar("https://capacity/0000.jpg", "tt0000000", 0) == 1,
       "re-registro apos tabela cheia ainda atualiza chave existente");
  }
  printf("%s\n", falhas ? "artereserva: FALHOU" : "artereserva: ok");
  return falhas ? 1 : 0;
}
