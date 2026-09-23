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
// ONE PIECE (tt0388629 / TMDB 37854): respostas reais de 23/09/2026,
// reduzidas aos campos que a reserva le (tests/fixtures/onepiece). O TMDB
// divide as temporadas de outro jeito e numera pelo absoluto; /episode/ de
// qualquer temporada >= 2 do Cinemeta responde 404, como na TV.
static int modoOP, pedidosOP, pedidosTemporadaOP;
static char *lerArquivo(const char *caminho) {
  FILE *f = fopen(caminho, "rb");
  long n;
  char *b;
  if (!f) return NULL;
  fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
  b = malloc((size_t)n + 1);
  if (b && fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); b = NULL; }
  if (b) b[n] = 0;
  fclose(f);
  return b;
}
static char *respostaOP(const char *url) {
  char cam[200];
  int t;
  pedidosOP++;
  if (strstr(url, "/find/tt0388629")) return strdup("{\"movie_results\":[],\"tv_results\":[{\"id\":37854}]}");
  if (strstr(url, "/find/")) return strdup("{\"movie_results\":[],\"tv_results\":[{\"id\":555}]}");
  if (strstr(url, "/episode/")) return NULL;                     // 404
  if (sscanf(url, "https://api.themoviedb.org/3/tv/37854/season/%d?", &t) == 1) {
    pedidosTemporadaOP++;
    snprintf(cam, sizeof cam, "tests/fixtures/onepiece/tmdb_tv_37854_s%d.json", t);
    return lerArquivo(cam);
  }
  if (strstr(url, "/3/tv/37854?")) return lerArquivo("tests/fixtures/onepiece/tmdb_tv_37854.json");
  return NULL;
}

char *rede_baixar(const char *url, int segundos) {
  (void)segundos;
  if (modoOP) return respostaOP(url);
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

// Com codigo HTTP: corpo = 200, sem corpo = 404 (o duble nao simula queda de
// rede aqui; `redeCaiuSt` faz isso).
static int redeCaiuSt;
char *rede_baixar_st(const char *url, int segundos, const char *const *cab, int *st) {
  char *r;
  (void)cab;
  if (redeCaiuSt) { if (st) *st = 0; return NULL; }
  r = rede_baixar(url, segundos);
  if (st) *st = r ? 200 : 404;
  return r;
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

// AniList e POST (GraphQL). Duble como os outros.
static const char *respostaPost = NULL;
static char ultimoCorpoPost[600];
char *rede_postar(const char *url, int segundos, const char *const *cab, const char *corpo) {
  (void)url; (void)segundos; (void)cab;
  snprintf(ultimoCorpoPost, sizeof ultimoCorpoPost, "%s", corpo ? corpo : "");
  return respostaPost ? strdup(respostaPost) : NULL;
}

// Apple: o main registra trailerapple_arte; aqui um duble que conta.
static char appleTitulo[200];
static int appleAno, appleSerie, appleChamadas;
static int appleDuble(const char *imdb, const char *titulo, int ano, int serie, char *m, size_t n) {
  (void)imdb;
  appleChamadas++;
  snprintf(appleTitulo, sizeof appleTitulo, "%s", titulo);
  appleAno = ano; appleSerie = serie;
  snprintf(m, n, "https://is1-ssl.mzstatic.com/image/thumb/X/{w}x{h}.{f}");
  return 1;
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
  // O still guardou o id do TMDB (tmdbid/tt0903747) na memoria do resolvedor;
  // os testes abaixo querem o /find de um titulo nunca visto.
  arte_fonte_cache_limpar();
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
  // Limite: 512 celulas, sai a usada ha mais tempo. tt0111161 e tocado no
  // meio e sobrevive; tt6666666 (o mais antigo intocado) sai.
  { int i;
    char url[96];
    for (i = 0; i < 700; i++) {
      snprintf(url, sizeof url, "https://nuvio.invalid/arte/tmdb/w1280/tt%07d", 1000000 + i);
      arte_fonte_resolver(url, s, sizeof s);
      if (i % 50 == 0) arte_fonte_resolver("https://nuvio.invalid/arte/tmdb/w1280/tt0111161", s, sizeof s);
    }
    resposta = NULL; pedidos = 0;
    OK(arte_fonte_resolver("https://nuvio.invalid/arte/tmdb/w1280/tt0111161", s, sizeof s) == 1 && pedidos == 0, "LRU guarda o usado");
    OK(arte_fonte_resolver("https://nuvio.invalid/arte/tmdb/w1280/tt6666666", s, sizeof s) == -1 && pedidos == 1, "LRU tira o antigo");
    OK(arte_fonte_resolver("https://nuvio.invalid/arte/tmdb/w1280/tt1000699", s, sizeof s) == 1 && pedidos == 1, "LRU guarda o recente");
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
  // ---- 23/09: TMDB pelo id, o OUTRO backdrop, Apple, fanart.tv e anime.
  arte_fonte_cache_limpar();
  chave = "CHAVE";
  // Com o id do TMDB na virtual: UM pedido, sem /find, e ele enche padrao e
  // outro de uma vez (append_to_response=images).
  resposta = "{\"backdrop_path\":\"/pad.jpg\",\"id\":278,\"images\":{\"backdrops\":["
             "{\"file_path\":\"/pad.jpg\",\"iso_639_1\":null,\"vote_average\":7}," 
             "{\"file_path\":\"/txt.jpg\",\"iso_639_1\":\"en\",\"vote_average\":9},"
             "{\"file_path\":\"/alt.jpg\",\"iso_639_1\":null,\"vote_average\":6}]}}";
  pedidos = 0;
  OK(arte_fonte_resolver("https://nuvio.invalid/arte/tmdb/w1280/tt0111161/m278", s, sizeof s) == 1 &&
     !strcmp(s, "https://image.tmdb.org/t/p/w1280/pad.jpg") && pedidos == 1, "tmdb com id: um pedido");
  OK(strstr(ultimaUrl, "/3/movie/278?append_to_response=images&include_image_language=null,en&api_key=CHAVE") != NULL &&
     !strstr(ultimaUrl, "/find/"), "tmdb com id: /movie/{id}, sem /find");
  resposta = NULL;
  OK(arte_fonte_resolver("https://nuvio.invalid/arte/tmdbalt/w780/tt0111161/m278", s, sizeof s) == 1 &&
     !strcmp(s, "https://image.tmdb.org/t/p/w780/alt.jpg") && pedidos == 1, "tmdb outro: veio no mesmo pedido, sem texto");
  OK(arte_fonte_resolvida("https://nuvio.invalid/arte/tmdbalt/w1280/tt0111161", s, sizeof s) == 1 &&
     !strcmp(s, "https://image.tmdb.org/t/p/w1280/alt.jpg"), "resolvida: sem rede, da memoria");
  OK(arte_fonte_resolvida("https://nuvio.invalid/arte/tmdb/w1280/tt0000042", s, sizeof s) == 0, "resolvida: nao sabe = 0");
  // Serie sem id: /find (padrao + id), depois /tv/{id}/images para o outro.
  resposta = "{\"movie_results\":[],\"tv_results\":[{\"id\":1396,\"backdrop_path\":\"/bb.jpg\"}],"
             "\"backdrops\":[{\"file_path\":\"/bb.jpg\"},{\"file_path\":\"/bb2.jpg\"}]}";
  pedidos = 0;
  OK(arte_fonte_resolver("https://nuvio.invalid/arte/tmdbalt/w1280/tt0903747", s, sizeof s) == 1 &&
     !strcmp(s, "https://image.tmdb.org/t/p/w1280/bb2.jpg") && pedidos == 2, "tmdb outro sem id: /find + /images");
  OK(strstr(ultimaUrl, "/3/tv/1396/images?include_image_language=null,en&api_key=CHAVE") != NULL, "tmdb outro: /tv/{id}/images");
  resposta = NULL;
  OK(arte_fonte_resolver("https://nuvio.invalid/arte/tmdb/w1280/tt0903747", s, sizeof s) == 1 &&
     !strcmp(s, "https://image.tmdb.org/t/p/w1280/bb.jpg") && pedidos == 2, "o /find ja guardou o padrao");
  OK(arte_fonte_resolver("https://nuvio.invalid/arte/tmdb/w500/tt0903747", s, sizeof s) == -1, "w500 nao e tamanho de fundo");
  // Titulo que o TMDB nao conhece: o outro vira negativa e nao repete o /find.
  resposta = "{\"movie_results\":[],\"tv_results\":[]}"; pedidos = 0;
  OK(arte_fonte_resolver("https://nuvio.invalid/arte/tmdbalt/w1280/tt0000404", s, sizeof s) == -1 && pedidos == 1, "outro desconhecido: um /find");
  OK(arte_fonte_resolver("https://nuvio.invalid/arte/tmdbalt/w1280/tt0000404", s, sizeof s) == -1 && pedidos == 1, "outro desconhecido: negativa guardada");
  // Apple: pelo registro do main (trailerapple_arte), com titulo e ano.
  OK(arte_fonte_resolver("https://nuvio.invalid/arte/apple/1920/tt15239678/m/2024/Dune%3A%20Part%20Two", s, sizeof s) == -1,
     "apple sem registro: nao existe");
  arte_fonte_definir_apple(appleDuble);
  OK(arte_fonte_resolver("https://nuvio.invalid/arte/apple/1920/tt15239678/m/2024/Dune%3A%20Part%20Two", s, sizeof s) == 1 &&
     !strcmp(s, "https://is1-ssl.mzstatic.com/image/thumb/X/1920x1080.jpg"), "apple -> mzstatic 1920x1080");
  OK(!strcmp(appleTitulo, "Dune: Part Two") && appleAno == 2024 && !appleSerie, "apple recebe titulo decodificado, ano e tipo");
  appleChamadas = 0;
  OK(arte_fonte_resolver("https://nuvio.invalid/arte/apple/1280/tt15239678/m/2024/Dune%3A%20Part%20Two", s, sizeof s) == 1 &&
     strstr(s, "/1280x720.jpg") && appleChamadas == 0, "apple 1280 sai da memoria");
  OK(arte_fonte_resolver("https://nuvio.invalid/arte/apple/1920/tt1/m/0/X", s, sizeof s) == -1, "apple sem ano: malformada");
  // fanart.tv: sem chave nao pergunta; com chave, filme pelo tt.
  pedidos = 0;
  OK(arte_fonte_resolver("https://nuvio.invalid/arte/fanart/full/tt0111161/m/278", s, sizeof s) == -1 && pedidos == 0,
     "fanart sem chave: nada");
  arte_fonte_chave_fanart("PESSOAL");
  resposta = "{\"moviebackground\":[{\"url\":\"https://assets.fanart.tv/fanart/movies/278/moviebackground/a.jpg\",\"lang\":\"\",\"likes\":\"3\"}]}";
  OK(arte_fonte_resolver("https://nuvio.invalid/arte/fanart/full/tt0111161/m/278", s, sizeof s) == 1 &&
     !strcmp(s, "https://assets.fanart.tv/fanart/movies/278/moviebackground/a.jpg"), "fanart filme");
  OK(strstr(ultimaUrl, "webservice.fanart.tv/v3/movies/tt0111161?api_key=PESSOAL") != NULL, "fanart: v3/movies pelo tt");
  // Serie: o tvdb vem do TMDB (external_ids); com o id do TMDB, sem /find.
  resposta = "{\"tvdb_id\":81189,\"showbackground\":[{\"url\":\"https://assets.fanart.tv/fanart/tv/81189/showbackground/b.jpg\",\"lang\":\"\"}]}";
  pedidos = 0;
  OK(arte_fonte_resolver("https://nuvio.invalid/arte/fanart/full/tt0903747/s/1396", s, sizeof s) == 1 &&
     strstr(s, "/showbackground/b.jpg") && pedidos == 2, "fanart serie: external_ids + v3/tv");
  OK(strstr(ultimaUrl, "webservice.fanart.tv/v3/tv/81189?api_key=PESSOAL") != NULL, "fanart serie pelo tvdb");
  arte_fonte_chave_fanart("");
  // Anime: kitsu:N direto; tt pela busca do Kitsu, AniList se ela nao casar.
  resposta = "{\"data\":{\"id\":\"7442\",\"attributes\":{\"subtype\":\"TV\",\"coverImage\":{\"large\":\"https://media.kitsu.app/anime/cover_images/7442/large.jpg\"}}}}";
  OK(arte_fonte_resolver("https://nuvio.invalid/arte/anime/large/kitsu:7442/s/2013/Attack%20on%20Titan", s, sizeof s) == 1 &&
     strstr(s, "/7442/large.jpg") && strstr(ultimaUrl, "kitsu.io/api/edge/anime/7442?"), "anime kitsu:N");
  resposta = "{\"data\":[]}";
  respostaPost = "{\"data\":{\"Media\":{\"bannerImage\":\"https:\\/\\/s4.anilist.co\\/b.jpg\",\"startDate\":{\"year\":2013}}}}";
  OK(arte_fonte_resolver("https://nuvio.invalid/arte/anime/large/tt2560140/s/2013/Attack%20on%20Titan", s, sizeof s) == 1 &&
     !strcmp(s, "https://s4.anilist.co/b.jpg") && strstr(ultimoCorpoPost, "Attack on Titan"), "anime tt: Kitsu vazio, AniList");
  OK(arte_fonte_resolver("https://nuvio.invalid/arte/anime/large/mal:1/s/0/", s, sizeof s) == 1 &&
     strstr(ultimoCorpoPost, "idMal:1"), "anime mal:N pelo AniList");
  OK(arte_fonte_resolver("https://nuvio.invalid/arte/tmdb/w1280/kitsu:1", s, sizeof s) == -1, "kitsu: so vale no anime");
  respostaPost = NULL;
  // MESMA IMAGEM: url real igual (virtual resolvida) ou mesmos bytes.
  arte_bytes_registrar("https://images.metahub.space/background/medium/tt0111161/img", "ABCDEFGH", 8);
  arte_bytes_registrar("https://catalogo/bg.jpg", "ABCDEFGH", 8);
  arte_bytes_registrar("https://image.tmdb.org/t/p/w1280/alt.jpg", "OUTRA", 5);
  OK(arte_mesma_imagem("https://catalogo/bg.jpg", "https://images.metahub.space/background/medium/tt0111161/img"),
     "mesma imagem: bytes iguais em urls diferentes");
  OK(!arte_mesma_imagem("https://nuvio.invalid/arte/tmdbalt/w1280/tt0111161", "https://catalogo/bg.jpg"),
     "mesma imagem: virtual resolvida com bytes diferentes");
  OK(arte_mesma_imagem("https://nuvio.invalid/arte/tmdbalt/w1280/tt0111161", "https://image.tmdb.org/t/p/w1280/alt.jpg"),
     "mesma imagem: virtual resolvida para a mesma url");
  OK(!arte_mesma_imagem("https://nao/sei.jpg", "https://catalogo/bg.jpg"), "mesma imagem: sem assinatura = nao sabe");
  // ONE PIECE: a numeracao do Cinemeta nao e a do TMDB (ver respostaOP).
  {
    char *cine = lerArquivo("tests/fixtures/onepiece/cinemeta_tt0388629.json");
    OK(cine != NULL, "fixture do Cinemeta");
    arte_fonte_cache_limpar();
    modoOP = 1;
    OK(arte_reserva_episodios("tt0388629", cine) == 1179, "1179 episodios de temporada > 0 registrados");
    free(cine);
    // S2E3 do Cinemeta = absoluto 11 = TMDB S1E11 (mesma data, 2000-01-26).
    OK(arte_reserva_url("https://episodes.metahub.space/tt0388629/2/3/w780.jpg", s, sizeof s) == 1 &&
       !strcmp(s, "https://image.tmdb.org/t/p/original/b08je5NHtJwFnU5zXAH242fEuPT.jpg"),
       "One Piece S2E3 -> TMDB episodio 11");
    // S22E1: absoluto 1085 no Cinemeta, mas a data (2023-12-03) e a do 1086.
    OK(arte_reserva_url("https://episodes.metahub.space/tt0388629/22/1/w780.jpg", s, sizeof s) == 1 &&
       !strcmp(s, "https://image.tmdb.org/t/p/original/iZVtHVlZTwvler8zxIlBuhoPRWN.jpg"),
       "One Piece S22E1 -> 1086 pela data, nao 1085 pelo absoluto");
    // S23E1: absoluto 1155 = ultimo da temporada 22 do TMDB; a data
    // (2026-04-05) passa do fim dela e casa na 23 (episodio 1156).
    OK(arte_reserva_url("https://episodes.metahub.space/tt0388629/23/1/w780.jpg", s, sizeof s) == 1 &&
       !strcmp(s, "https://image.tmdb.org/t/p/original/39legAikT7IK7yCWHzYQWHdEFPo.jpg"),
       "One Piece S23E1 -> temporada vizinha do TMDB");
    // Rolar de novo: nada volta a rede.
    pedidosOP = 0;
    OK(arte_reserva_url("https://episodes.metahub.space/tt0388629/2/3/w780.jpg", s, sizeof s) == 1 &&
       strstr(s, "/b08je5NHtJwFnU5zXAH242fEuPT.jpg") && pedidosOP == 0, "still casado fica na memoria");
    // Outro episodio da mesma temporada do TMDB: so o 404 do /episode/, a
    // temporada ja esta guardada (nem /find, nem /tv, nem /season de novo).
    pedidosOP = 0; pedidosTemporadaOP = 0;
    OK(arte_reserva_url("https://episodes.metahub.space/tt0388629/21/11/w780.jpg", s, sizeof s) == 1 &&
       !strcmp(s, "https://image.tmdb.org/t/p/original/zuwp8uIf5qZ5nXRAjGPBVDVWa9N.jpg"),
       "One Piece S21E11 -> 902 pela data");
    OK(pedidosOP == 1 && pedidosTemporadaOP == 0, "temporada do TMDB guardada: um pedido so");
    // Serie sem episodios registrados e sem o numero no TMDB: 0, e a segunda
    // vez nao pede nada (antes: /find + 404 a cada volta do recuo).
    pedidosOP = 0;
    OK(arte_reserva_url("https://episodes.metahub.space/tt9999998/3/1/w780.jpg", s, sizeof s) == 0, "sem registro e 404: sem still");
    OK(pedidosOP == 2, "find + episode");
    pedidosOP = 0;
    OK(arte_reserva_url("https://episodes.metahub.space/tt9999998/3/1/w780.jpg", s, sizeof s) == 0 && pedidosOP == 0,
       "404 guardado: a rolagem nao repete o pedido");
    // Rede fora no /episode/: nada e guardado, a proxima volta pergunta.
    redeCaiuSt = 1; pedidosOP = 0;
    OK(arte_reserva_url("https://episodes.metahub.space/tt9999998/3/2/w780.jpg", s, sizeof s) == 0, "rede fora: sem still");
    redeCaiuSt = 0;
    OK(arte_reserva_url("https://episodes.metahub.space/tt9999998/3/2/w780.jpg", s, sizeof s) == 0 && pedidosOP == 1,
       "rede fora nao vira negativa");
    modoOP = 0;
    arte_fonte_cache_limpar();
  }
  arte_fonte_cache_relogio(NULL);
  printf("%s\n", falhas ? "artereserva: FALHOU" : "artereserva: ok");
  return falhas ? 1 : 0;
}
