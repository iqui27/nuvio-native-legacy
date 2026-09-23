// Reserva de arte pelo TMDB quando o metahub falha. Sem rede: rede_baixar e
// desc_chave_tmdb sao dubles, e o teste olha so o que arte_reserva_url faz
// com a URL e com a resposta do /find.
#include "artereserva.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

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

// Trakt: chave publica e busca por id sao dubles, como o /find acima.
static const char *respostaTrakt = NULL;
static int temChaveTrakt = 1;
static char ultimaUrlTrakt[300];
static int pedidosTrakt;
int trakt_cabecalhos_publicos(const char **cab, char *k, size_t nK) {
  if (!temChaveTrakt) return 0;
  snprintf(k, nK, "trakt-api-key: X");
  cab[0] = "trakt-api-version: 2"; cab[1] = k; cab[2] = NULL;
  return 1;
}
char *rede_baixar_com(const char *url, int segundos, const char *const *cab) {
  (void)segundos; (void)cab;
  pedidosTrakt++;
  snprintf(ultimaUrlTrakt, sizeof ultimaUrlTrakt, "%s", url);
  return respostaTrakt ? strdup(respostaTrakt) : NULL;
}

static int falhas = 0;
#define OK(cond, msg) do { if (!(cond)) { printf("FALHOU: %s\n", msg); falhas++; } } while (0)

static unsigned long long agoraFalso;
static unsigned long long relogioFalso(void) { return agoraFalso; }

static void *resolverEmFio(void *arg) {
  char s[400], url[96];
  int i, *erros = arg;
  for (i = 0; i < 2000; i++) {
    snprintf(url, sizeof url, "https://nuvio.invalid/arte/tmdb/w1280/tt%07d", i % 300);
    if (arte_fonte_resolver(url, s, sizeof s) != 1 || strncmp(s, "https://image.tmdb.org/t/p/w1280/", 33)) (*erros)++;
  }
  return NULL;
}

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
  // URL VIRTUAL DE FONTE (ajuste "Background do hero"): o fundo do TMDB ou do
  // Trakt de um titulo que so trouxe o do Cinemeta.
  resposta = "{\"movie_results\":[{\"id\":278,\"backdrop_path\":\"/fundo.jpg\"}],\"tv_results\":[]}";
  OK(arte_fonte_resolver("https://image.tmdb.org/t/p/w1280/x.jpg", s, sizeof s) == 0, "url real nao e virtual");
  OK(arte_fonte_resolver("https://nuvio.invalid/arte/tmdb/w1280/tt0111161", s, sizeof s) == 1 &&
     !strcmp(s, "https://image.tmdb.org/t/p/w1280/fundo.jpg"), "virtual TMDB -> backdrop w1280");
  OK(strstr(ultimaUrl, "/3/find/tt0111161?api_key=CHAVE&external_source=imdb_id") != NULL, "virtual TMDB pede o /find");
  OK(arte_fonte_resolver("https://nuvio.invalid/arte/tmdb/original/tt0111161", s, sizeof s) == 1 &&
     !strcmp(s, "https://image.tmdb.org/t/p/original/fundo.jpg"), "virtual TMDB original fora do Tizen");
  resposta = "{\"movie_results\":[],\"tv_results\":[{\"id\":1396,\"backdrop_path\":\"/serie.jpg\"}]}";
  OK(arte_fonte_resolver("https://nuvio.invalid/arte/tmdb/w1280/tt0903747", s, sizeof s) == 1 &&
     !strcmp(s, "https://image.tmdb.org/t/p/w1280/serie.jpg"), "virtual TMDB de serie");
  resposta = "{\"movie_results\":[],\"tv_results\":[]}";
  OK(arte_fonte_resolver("https://nuvio.invalid/arte/tmdb/w1280/tt9999999", s, sizeof s) == -1, "virtual TMDB desconhecido falha");
  OK(arte_fonte_resolver("https://nuvio.invalid/arte/tmdb/w9999/tt0111161", s, sizeof s) == -1, "tamanho fora da escada");
  OK(arte_fonte_resolver("https://nuvio.invalid/arte/tmdb/w1280/12345", s, sizeof s) == -1, "virtual sem tt");
  // Formato MEDIDO com curl em 22/09 (/search/imdb/tt0111161?extended=full,images).
  respostaTrakt = "[{\"type\":\"movie\",\"movie\":{\"ids\":{\"imdb\":\"tt0111161\"},"
    "\"images\":{\"logo\":[\"media.trakt.tv/images/movies/000/000/234/logos/medium/l.png.webp\"],"
    "\"fanart\":[\"media.trakt.tv/images/movies/000/000/234/fanarts/medium/d0.jpg.webp\"],"
    "\"poster\":[]}}}]";
  OK(arte_fonte_resolver("https://nuvio.invalid/arte/trakt/medium/tt0111161", s, sizeof s) == 1 &&
     !strcmp(s, "https://media.trakt.tv/images/movies/000/000/234/fanarts/medium/d0.jpg.webp"), "virtual Trakt -> fanart");
  OK(strstr(ultimaUrlTrakt, "api.trakt.tv/search/imdb/tt0111161?") != NULL &&
     strstr(ultimaUrlTrakt, "images") != NULL, "virtual Trakt pede a busca por id com imagens");
  OK(arte_fonte_resolver("https://nuvio.invalid/arte/trakt/full/tt0111161", s, sizeof s) == 1 &&
     !strcmp(s, "https://media.trakt.tv/images/movies/000/000/234/fanarts/full/d0.jpg.webp"), "virtual Trakt full");
  respostaTrakt = "[{\"type\":\"movie\",\"movie\":{\"images\":{\"fanart\":[]}}}]";
  OK(arte_fonte_resolver("https://nuvio.invalid/arte/trakt/medium/tt1", s, sizeof s) == -1, "fanart vazio falha");
  temChaveTrakt = 0; respostaTrakt = NULL;
  arte_fonte_cache_limpar();
  OK(arte_fonte_resolver("https://nuvio.invalid/arte/trakt/medium/tt0111161", s, sizeof s) == -1, "sem chave do Trakt falha");
  chave = "";
  OK(arte_fonte_resolver("https://nuvio.invalid/arte/tmdb/w1280/tt0111161", s, sizeof s) == -1, "sem chave do TMDB falha");
  // MEMORIA DO RESOLVEDOR (resolve_ms de 408-640 ms na C9): a segunda
  // pergunta pelo mesmo titulo e fonte nao vai a rede — nem em outro tamanho.
  arte_fonte_cache_limpar();
  arte_fonte_cache_relogio(relogioFalso);
  agoraFalso = 1000;
  chave = "CHAVE"; temChaveTrakt = 1;
  resposta = "{\"movie_results\":[{\"id\":278,\"backdrop_path\":\"/fundo.jpg\"}],\"tv_results\":[]}";
  pedidos = 0;
  OK(arte_fonte_resolver("https://nuvio.invalid/arte/tmdb/w1280/tt0111161", s, sizeof s) == 1 && pedidos == 1, "memoria: primeira vai a rede");
  resposta = NULL;   // rede "caiu": so a memoria pode responder
  OK(arte_fonte_resolver("https://nuvio.invalid/arte/tmdb/w1280/tt0111161", s, sizeof s) == 1 &&
     !strcmp(s, "https://image.tmdb.org/t/p/w1280/fundo.jpg") && pedidos == 1, "memoria: repeticao sem rede");
  OK(arte_fonte_resolver("https://nuvio.invalid/arte/tmdb/original/tt0111161", s, sizeof s) == 1 &&
     !strcmp(s, "https://image.tmdb.org/t/p/original/fundo.jpg") && pedidos == 1, "memoria: outro tamanho, mesma consulta");
  respostaTrakt = "[{\"movie\":{\"images\":{\"fanart\":[\"media.trakt.tv/f/medium/a.jpg.webp\"]}}}]";
  pedidosTrakt = 0;
  OK(arte_fonte_resolver("https://nuvio.invalid/arte/trakt/medium/tt0111161", s, sizeof s) == 1 && pedidosTrakt == 1, "memoria: fonte e parte da chave");
  respostaTrakt = NULL;
  OK(arte_fonte_resolver("https://nuvio.invalid/arte/trakt/full/tt0111161", s, sizeof s) == 1 &&
     !strcmp(s, "https://media.trakt.tv/f/full/a.jpg.webp") && pedidosTrakt == 1, "memoria: Trakt full sai da medium guardada");
  // Negativa: a API respondeu sem fundo -> vale 5 min, depois pergunta de novo.
  resposta = "{\"movie_results\":[],\"tv_results\":[]}";
  pedidos = 0;
  OK(arte_fonte_resolver("https://nuvio.invalid/arte/tmdb/w1280/tt7777777", s, sizeof s) == -1 && pedidos == 1, "negativa: primeira pergunta");
  agoraFalso += 4 * 60 * 1000;
  OK(arte_fonte_resolver("https://nuvio.invalid/arte/tmdb/w1280/tt7777777", s, sizeof s) == -1 && pedidos == 1, "negativa: em vigor nao pede");
  agoraFalso += 2 * 60 * 1000;
  resposta = "{\"movie_results\":[{\"backdrop_path\":\"/novo.jpg\"}],\"tv_results\":[]}";
  OK(arte_fonte_resolver("https://nuvio.invalid/arte/tmdb/w1280/tt7777777", s, sizeof s) == 1 &&
     !strcmp(s, "https://image.tmdb.org/t/p/w1280/novo.jpg") && pedidos == 2, "negativa: vencida pergunta de novo");
  // Falha de rede e falta de chave NAO viram negativa.
  resposta = NULL; pedidos = 0;
  OK(arte_fonte_resolver("https://nuvio.invalid/arte/tmdb/w1280/tt6666666", s, sizeof s) == -1 && pedidos == 1, "rede falhou");
  resposta = "{\"movie_results\":[{\"backdrop_path\":\"/volta.jpg\"}],\"tv_results\":[]}";
  OK(arte_fonte_resolver("https://nuvio.invalid/arte/tmdb/w1280/tt6666666", s, sizeof s) == 1 && pedidos == 2, "rede falha nao fica guardada");
  chave = ""; pedidos = 0;
  OK(arte_fonte_resolver("https://nuvio.invalid/arte/tmdb/w1280/tt5555555", s, sizeof s) == -1 && pedidos == 0, "sem chave nao pede");
  chave = "CHAVE";
  OK(arte_fonte_resolver("https://nuvio.invalid/arte/tmdb/w1280/tt5555555", s, sizeof s) == 1 && pedidos == 1, "sem chave nao fica guardado");
  // Limite: 192 celulas, sai a usada ha mais tempo. tt0111161 e tocado no
  // meio e sobrevive; tt6666666 (o mais antigo intocado) sai.
  { int i;
    char url[96];
    for (i = 0; i < 300; i++) {
      snprintf(url, sizeof url, "https://nuvio.invalid/arte/tmdb/w1280/tt%07d", 1000000 + i);
      arte_fonte_resolver(url, s, sizeof s);
      if (i % 50 == 0) arte_fonte_resolver("https://nuvio.invalid/arte/tmdb/w1280/tt0111161", s, sizeof s);
    }
    resposta = NULL; pedidos = 0;
    OK(arte_fonte_resolver("https://nuvio.invalid/arte/tmdb/w1280/tt0111161", s, sizeof s) == 1 && pedidos == 0, "LRU guarda o usado");
    OK(arte_fonte_resolver("https://nuvio.invalid/arte/tmdb/w1280/tt6666666", s, sizeof s) == -1 && pedidos == 1, "LRU tira o antigo");
    OK(arte_fonte_resolver("https://nuvio.invalid/arte/tmdb/w1280/tt1000299", s, sizeof s) == 1 && pedidos == 1, "LRU guarda o recente");
  }
  // Varios fios ao mesmo tempo (os dois de rede do tex_cache): sem corrida.
  { pthread_t f[4];
    int erros = 0, i;
    resposta = "{\"movie_results\":[{\"backdrop_path\":\"/f.jpg\"}],\"tv_results\":[]}";
    arte_fonte_cache_limpar();
    for (i = 0; i < 4; i++) pthread_create(&f[i], NULL, resolverEmFio, &erros);
    for (i = 0; i < 4; i++) pthread_join(f[i], NULL);
    OK(erros == 0, "fios concorrentes resolvem certo");
  }
  arte_fonte_cache_relogio(NULL);
  printf("%s\n", falhas ? "artereserva: FALHOU" : "artereserva: ok");
  return falhas ? 1 : 0;
}
