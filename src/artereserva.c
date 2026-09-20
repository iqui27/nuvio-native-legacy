#include "artereserva.h"
#include <pthread.h>
#include "descoberta.h"
#include "rede.h"
#include "js.h"
#include <stdio.h>
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
  snprintf(saida, tam, "https://image.tmdb.org/t/p/original%s", caminho);
  printf("[tex] reserva do TMDB para still de %s S%dE%d\n", id, temp, ep);
  fflush(stdout);
  return 1;
}

// Tabela (url -> imdb) para arte de host que nao carrega o id na URL. Chave
// pelo hash FNV da URL; 1024 posicoes cobrem a biblioteca e a home (que sao
// os lugares onde uma arte de addon aparece), e uma colisao so custa uma
// reserva errada numa arte que ja tinha falhado.
#define AR_REG 1024
static struct { unsigned long h; char imdb[24]; unsigned char poster, usado; } reg[AR_REG];
static pthread_mutex_t regTrava = PTHREAD_MUTEX_INITIALIZER;

static unsigned long hashUrl(const char *u) {
  unsigned long h = 2166136261ul;
  for (; *u; u++) { h ^= (unsigned char)*u; h *= 16777619ul; h &= 0xffffffffUL; }
  return h;
}

void arte_reserva_registrar(const char *url, const char *imdb, int poster) {
  unsigned long h;
  if (!url || !url[0] || !imdb || strncmp(imdb, "tt", 2)) return;
  if (strstr(url, "images.metahub.space") || strstr(url, "image.tmdb.org") ||
      strstr(url, "episodes.metahub.space")) return;   // esses ja se resolvem sozinhos
  h = hashUrl(url);
  pthread_mutex_lock(&regTrava);
  { unsigned i = (unsigned)(h % AR_REG);
    reg[i].h = h; reg[i].poster = poster ? 1 : 0; reg[i].usado = 1;
    snprintf(reg[i].imdb, sizeof reg[i].imdb, "%.*s", (int)strcspn(imdb, ":"), imdb); }
  pthread_mutex_unlock(&regTrava);
}

static int lerRegistro(const char *url, char *id, size_t nId, int *poster) {
  unsigned long h = hashUrl(url);
  unsigned i = (unsigned)(h % AR_REG);
  int ok = 0;
  pthread_mutex_lock(&regTrava);
  if (reg[i].usado && reg[i].h == h) {
    snprintf(id, nId, "%s", reg[i].imdb); *poster = reg[i].poster; ok = 1;
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
