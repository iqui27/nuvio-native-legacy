#include "artereserva.h"
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

int arte_reserva_url(const char *url, char *saida, size_t tam) {
  char tipo[16], id[24], api[300], caminho[128] = "";
  const char *chave, *corpo, *v;
  char *resp;
  int poster;
  if (!url || !saida || tam < 80) return 0;
  if (!lerMetahub(url, tipo, sizeof tipo, id, sizeof id)) return 0;
  chave = desc_chave_tmdb();
  if (!chave[0]) return 0;
  poster = !strcmp(tipo, "poster");
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
