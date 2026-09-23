#include "artereserva.h"
#include <pthread.h>
#include "descoberta.h"
#include "rede.h"
#include "js.h"
#include "trakt.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// images.metahub.space/<tipo>/<tamanho>/<ttNNN>/img  ->  tipo e id.
// So poster e background: logo nao tem par no /find.
static int lerMetahub(const char *url, char *tipo, size_t tTipo, char *id, size_t tId) {
  static const char PREFIXO[] = "images.metahub.space/";
  const char *p = strstr(url, PREFIXO), *barra;
  size_t n;
  if (!p) return 0;
  p += sizeof PREFIXO - 1;
  barra = strchr(p, '/');
  if (!barra) return 0;
  n = (size_t)(barra - p);
  if (n + 1 > tTipo) return 0;
  memcpy(tipo, p, n); tipo[n] = 0;
  if (strcmp(tipo, "poster") && strcmp(tipo, "background")) return 0;
  p = strchr(barra + 1, '/');               // pula o tamanho (small/medium/large)
  if (!p) return 0;
  p++;
  barra = strchr(p, '/');
  n = barra ? (size_t)(barra - p) : strlen(p);
  if (n < 3 || n + 1 > tId || p[0] != 't' || p[1] != 't') return 0;
  memcpy(id, p, n); id[n] = 0;
  return 1;
}

// episodes.metahub.space/<ttNNN>/<temp>/<ep>/<tam>.jpg  ->  id, temporada, episodio.
static int lerStill(const char *url, char *id, size_t tId, int *temp, int *ep) {
  static const char PREFIXO[] = "episodes.metahub.space/";
  const char *p = strstr(url, PREFIXO), *barra;
  size_t n;
  if (!p) return 0;
  p += sizeof PREFIXO - 1;
  barra = strchr(p, '/');
  if (!barra) return 0;
  n = (size_t)(barra - p);
  if (n < 3 || n + 1 > tId || p[0] != 't' || p[1] != 't') return 0;
  memcpy(id, p, n); id[n] = 0;
  if (sscanf(barra + 1, "%d/%d/", temp, ep) != 2 || *temp < 0 || *ep < 1) return 0;
  return 1;
}

// Still de episodio: duas viagens (o id do TMDB pela /find, depois o
// episodio). So depois de o metahub falhar, e o still e o card de Continue
// Watching inteiro — vale as duas.
static int reservaStill(const char *chave, const char *id, int temp, int ep,
                        char *saida, size_t tam) {
  char api[300], caminho[128] = "";
  char *resp;
  const char *v;
  long idTv = 0;
  snprintf(api, sizeof api,
           "https://api.themoviedb.org/3/find/%s?api_key=%s&external_source=imdb_id", id, chave);
  resp = rede_baixar(api, 8);
  if (!resp) return 0;
  v = js_array(resp, NULL, "tv_results");
  if (v) idTv = (long)js_num(v, js_fim(v), "id", 0.0);
  free(resp);
  if (idTv <= 0) return 0;
  snprintf(api, sizeof api,
           "https://api.themoviedb.org/3/tv/%ld/season/%d/episode/%d?api_key=%s", idTv, temp, ep, chave);
  resp = rede_baixar(api, 8);
  if (!resp) return 0;
  js_texto_raiz(resp, "still_path", caminho, sizeof caminho);
  free(resp);
  if (caminho[0] != '/') return 0;
  // Na Samsung nunca `original`: o decode do navegador devolve o still inteiro
  // (3840 px em varias series) e foi um fundo desses que zerou o heap no
  // registro 1450 — ver fundoOriginal() em artehero.c.
#ifdef __EMSCRIPTEN__
  snprintf(saida, tam, "https://image.tmdb.org/t/p/w1280%s", caminho);
#else
  snprintf(saida, tam, "https://image.tmdb.org/t/p/original%s", caminho);
#endif
  printf("[tex] reserva do TMDB para still de %s S%dE%d\n", id, temp, ep);
  fflush(stdout);
  return 1;
}

// Tabela (url -> imdb) para arte de host que nao carrega o id na URL. O hash
// e apenas o ponto inicial de um probing linear: a chave completa (bytes e
// tamanho) continua sendo comparada, portanto uma colisao nunca escolhe a
// reserva TMDB de outro titulo.
//
// A arena guarda somente os bytes das URLs efetivamente registradas, sem
// duplicar esses bytes em cada celula da tabela. O teto de 2 MiB continua
// reservado no BSS por desenho (a tabela acrescenta cerca de 144 KiB), para
// manter o uso previsivel no WASM/Tizen; nao e uma alocacao sob demanda.
// O registry vive pela sessao do processo e nao tem reset: depois de 4096
// URLs distintas, ou antes disso se a arena for preenchida por URLs longas,
// novas reservas sao omitidas de forma explicita.
#define AR_REG 4096u
#define AR_URL_MAX 512u
#define AR_TEXT_MAX (2u * 1024u * 1024u)
typedef struct {
  uint32_t h;
  uint32_t off;
  uint16_t len;
  unsigned char poster, usado;
  char imdb[24];
} ArteRegistro;
static ArteRegistro reg[AR_REG];
static char regTexto[AR_TEXT_MAX];
static uint32_t regTextoN;
static pthread_mutex_t regTrava = PTHREAD_MUTEX_INITIALIZER;
static int regAvisouCheia, regAvisouTexto, regAvisouUrlLonga;

static uint32_t hashUrl(const char *u) {
  uint32_t h = 2166136261u;
  for (; *u; u++) { h ^= (unsigned char)*u; h *= 16777619u; }
  return h;
}

static int regSlot(const char *url, size_t n, uint32_t h, int *achou) {
  unsigned i = (unsigned)(h % AR_REG), passo;
  for (passo = 0; passo < AR_REG; passo++) {
    ArteRegistro *r = &reg[i];
    if (!r->usado) { *achou = 0; return (int)i; }
    if (r->h == h && r->len == n && !memcmp(regTexto + r->off, url, n)) {
      *achou = 1; return (int)i;
    }
    i = (i + 1u) % AR_REG;
  }
  *achou = 0;
  return -1;
}

int arte_reserva_registrar(const char *url, const char *imdb, int poster) {
  uint32_t h;
  size_t n;
  int achou, slot;
  if (!url || !url[0] || !imdb || strncmp(imdb, "tt", 2)) return 0;
  if (strstr(url, "images.metahub.space") || strstr(url, "image.tmdb.org") ||
      strstr(url, "episodes.metahub.space")) return 0; // esses ja se resolvem sozinhos
  n = strlen(url);
  if (n >= AR_URL_MAX) {
    pthread_mutex_lock(&regTrava);
    if (!regAvisouUrlLonga) {
      regAvisouUrlLonga = 1;
      fprintf(stderr, "[tex] reserva addon: URL excede limite de %u bytes\n", AR_URL_MAX - 1u);
    }
    pthread_mutex_unlock(&regTrava);
    return 0;
  }
  h = hashUrl(url);
  pthread_mutex_lock(&regTrava);
  slot = regSlot(url, n, h, &achou);
  if (slot < 0) {
    if (!regAvisouCheia) {
      regAvisouCheia = 1;
      fprintf(stderr, "[tex] reserva addon: tabela cheia (%u entradas), fallback omitido\n", AR_REG);
    }
    pthread_mutex_unlock(&regTrava);
    return 0;
  }
  if (!achou) {
    if (regTextoN + n > AR_TEXT_MAX) {
      if (!regAvisouTexto) {
        regAvisouTexto = 1;
        fprintf(stderr, "[tex] reserva addon: arena de URLs cheia (%u bytes), fallback omitido\n", AR_TEXT_MAX);
      }
      pthread_mutex_unlock(&regTrava);
      return 0;
    }
    reg[slot].h = h;
    reg[slot].off = regTextoN;
    reg[slot].len = (uint16_t)n;
    memcpy(regTexto + regTextoN, url, n);
    regTextoN += (uint32_t)n;
    reg[slot].usado = 1;
  }
  reg[slot].poster = poster ? 1 : 0;
  snprintf(reg[slot].imdb, sizeof reg[slot].imdb, "%.*s", (int)strcspn(imdb, ":"), imdb);
  pthread_mutex_unlock(&regTrava);
  return 1;
}

static int lerRegistro(const char *url, char *id, size_t nId, int *poster) {
  uint32_t h = hashUrl(url);
  size_t n = strlen(url);
  int achou, slot;
  int ok = 0;
  pthread_mutex_lock(&regTrava);
  slot = regSlot(url, n, h, &achou);
  if (slot >= 0 && achou) {
    snprintf(id, nId, "%s", reg[slot].imdb); *poster = reg[slot].poster; ok = 1;
  }
  pthread_mutex_unlock(&regTrava);
  return ok;
}

int arte_reserva_url(const char *url, char *saida, size_t tam) {
  char tipo[16], id[24], api[300], caminho[128] = "";
  const char *chave, *corpo, *v;
  char *resp;
  int poster, temp = 0, ep = 0;
  if (!url || !saida || tam < 80) return 0;
  chave = desc_chave_tmdb_reserva();
  if (!chave[0]) {
    // UMA VEZ: e o unico jeito de saber pelo log que a reserva existe e nao
    // pode agir (#67: "the fallback didn't help" sem uma linha para provar).
    static int avisou;
    if (!avisou) { avisou = 1; printf("[tex] reserva do TMDB indisponivel: sem chave do TMDB neste pacote\n"); fflush(stdout); }
    return 0;
  }
  if (lerStill(url, id, sizeof id, &temp, &ep)) return reservaStill(chave, id, temp, ep, saida, tam);
  if (lerMetahub(url, tipo, sizeof tipo, id, sizeof id)) poster = !strcmp(tipo, "poster");
  else if (lerRegistro(url, id, sizeof id, &poster)) snprintf(tipo, sizeof tipo, "%s", poster ? "poster" : "background");
  else return 0;
  snprintf(api, sizeof api,
           "https://api.themoviedb.org/3/find/%s?api_key=%s&external_source=imdb_id",
           id, chave);
  // 8 s, como a imagem: isto roda no fio de decode e ja e o segundo pedido de
  // uma arte que acabou de falhar.
  resp = rede_baixar(api, 8);
  if (!resp) return 0;
  corpo = resp;
  // O /find responde os dois vetores sempre; o cheio diz o tipo do titulo, que
  // a URL do metahub nao carrega.
  v = js_array(corpo, NULL, "movie_results");
  if (!v) v = js_array(corpo, NULL, "tv_results");
  if (v) js_texto(v, js_fim(v), poster ? "poster_path" : "backdrop_path",
                  caminho, sizeof caminho);
  free(resp);
  if (caminho[0] != '/') return 0;
  // w342 e o que a fileira desenha (o card de 288 promove ate 2x sem perder);
  // w1280 cobre o heroi e o detalhe pelo mesmo decode escalado do card.
  snprintf(saida, tam, "https://image.tmdb.org/t/p/%s%s", poster ? "w342" : "w1280", caminho);
  printf("[tex] reserva do TMDB para %s de %s\n", tipo, id);
  fflush(stdout);
  return 1;
}

// PRIMEIRA URL DE UMA LISTA DE IMAGENS do Trakt ("fanart":["media.trakt.tv/..."]).
// Copia curta do imagemTrakt de trakt.c, que e static e vive num modulo que
// este nao pode arrastar para o teste sem rede. MEDIDO em 22/09 com curl em
// /search/imdb/tt0111161?extended=full,images: o bloco vem dentro de
// movie/show, a url vem SEM esquema e em /medium/ (.jpg.webp — webp.c le).
static int fanartTrakt(const char *corpo, char *dst, size_t n) {
  const char *p = strstr(corpo, "\"fanart\""), *ini;
  size_t L;
  if (!p) return 0;
  p = strchr(p + 8, '[');
  if (!p) return 0;
  while (*++p && (unsigned char)*p <= ' ') { }
  if (*p != '"') return 0;               // lista vazia: nao inventar arte
  ini = ++p;
  while (*p && *p != '"') p++;
  L = (size_t)(p - ini);
  if (!L || L + 9 >= n) return 0;
  if (!strncmp(ini, "http", 4)) snprintf(dst, n, "%.*s", (int)L, ini);
  else                          snprintf(dst, n, "https://%.*s", (int)L, ini);
  return 1;
}

int arte_fonte_resolver(const char *url, char *saida, size_t tam) {
  static const char PRE[] = ARTE_VIRTUAL_PREFIXO;
  char fonte[8], tamanho[12], id[24];
  const char *p;
  if (!url || strncmp(url, PRE, sizeof PRE - 1)) return 0;
  if (!saida || tam < 80) return -1;
  p = url + sizeof PRE - 1;
  if (sscanf(p, "%7[^/]/%11[^/]/%23[^/]", fonte, tamanho, id) != 3 ||
      strncmp(id, "tt", 2)) return -1;
#ifdef __EMSCRIPTEN__
  // Samsung: nunca o fundo de 3840 (fundoOriginal() em artehero.c). A url
  // virtual ja nasce pequena la; isto e a segunda trava, para uma url gravada
  // por outra build ou montada por outro caminho.
  if (!strcmp(tamanho, "original")) snprintf(tamanho, sizeof tamanho, "w1280");
  if (!strcmp(tamanho, "full"))     snprintf(tamanho, sizeof tamanho, "medium");
#endif
  if (!strcmp(fonte, "tmdb")) {
    char api[300], caminho[128] = "";
    const char *chave = desc_chave_tmdb_reserva(), *v;
    char *resp;
    if (strcmp(tamanho, "w1280") && strcmp(tamanho, "original")) return -1;
    if (!chave[0]) return -1;
    snprintf(api, sizeof api,
             "https://api.themoviedb.org/3/find/%s?api_key=%s&external_source=imdb_id",
             id, chave);
    resp = rede_baixar(api, 8);
    if (!resp) return -1;
    v = js_array(resp, NULL, "movie_results");
    if (!v) v = js_array(resp, NULL, "tv_results");
    if (v) js_texto(v, js_fim(v), "backdrop_path", caminho, sizeof caminho);
    free(resp);
    if (caminho[0] != '/') return -1;
    snprintf(saida, tam, "https://image.tmdb.org/t/p/%s%s", tamanho, caminho);
  } else if (!strcmp(fonte, "trakt")) {
    // Chave PUBLICA do aplicativo, sem token: a busca por id nao precisa de
    // conta vinculada, e o fundo do Trakt nao pode depender de login.
    const char *cab[4];
    char chave[160], api[200], fan[400];
    char *resp;
    int ok;
    if (strcmp(tamanho, "medium") && strcmp(tamanho, "full")) return -1;
    if (!trakt_cabecalhos_publicos(cab, chave, sizeof chave)) return -1;
    snprintf(api, sizeof api,
             "https://api.trakt.tv/search/imdb/%s?type=movie,show&extended=full,images", id);
    resp = rede_baixar_com(api, 8, cab);
    if (!resp) return -1;
    ok = fanartTrakt(resp, fan, sizeof fan);
    free(resp);
    if (!ok) return -1;
    { char *m = strstr(fan, "/medium/");
      if (m && !strcmp(tamanho, "full") && strlen(fan) + 2 < tam)
        snprintf(saida, tam, "%.*s/full/%s", (int)(m - fan), fan, m + 8);
      else snprintf(saida, tam, "%s", fan); }
  } else return -1;
  printf("[tex] fundo %s de %s: %.90s\n", fonte, id, saida);
  fflush(stdout);
  return 1;
}
