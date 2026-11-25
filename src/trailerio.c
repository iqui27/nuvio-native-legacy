#ifdef NV_WEBOS
#include "trailerio.h"
#include "js.h"
#include "rede.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
  RedeJob *job;
  int cancelado, pronto;
  char pedido[96];
  char id[32], tipo[8];
  TrailerioLista *lista;
} TrailerioPedido;

static TrailerioPedido *atual;
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;

int trailerio_id(const char *id, const char *tipo) {
  return tipo && (!strcmp(tipo, "movie") || !strcmp(tipo, "series")) &&
    id && !strncmp(id, "tt", 2) && id[2] && strlen(id) < 32 &&
    id[2 + strspn(id + 2, "0123456789")] == 0;
}

static int formato(const char *url) {
  if (strncmp(url, "https://", 8)) return 0;
  const char *host = url + 8;
  const char *path = host + strspn(host, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-");
  if (path == host || *path != '/' || strpbrk(url, "\"\\#")) return 0;
  for (const unsigned char *p = (const unsigned char *)url; *p; p++)
    if (*p <= ' ' || *p == 127) return 0;
  const size_t n = strcspn(path, "?");
  if (n >= 4 && !strncmp(path + n - 4, ".mp4", 4)) return 1;
  if (n >= 5 && !strncmp(path + n - 5, ".m3u8", 5)) return 2;
  return 0;
}

int trailerio_ler(const char *json, const char *id, const char *tipo, TrailerioLista *lista) {
  char metaId[32], metaTipo[8];
  lista->n = 0;
  if (!trailerio_id(id, tipo)) return 0;
  const char *meta = js_valor_raiz_em(json, NULL, "meta");
  if (!meta || *meta != '{') return 0;
  const char *metaFim = js_fim(meta);
  if (metaFim[-1] != '}' ||
      !js_texto_raiz_em(meta, metaFim, "id", metaId, sizeof metaId) || strcmp(metaId, id) ||
      !js_texto_raiz_em(meta, metaFim, "type", metaTipo, sizeof metaTipo) || strcmp(metaTipo, tipo)) return 0;
  const char *links = js_valor_raiz_em(meta, metaFim, "links");
  if (!links || *links != '[') return 0;
  const char *fimLinks = js_fim(links);
  if (fimLinks > metaFim || fimLinks[-1] != ']') return 0;
  for (const char *p = js_raiz_array(links); p && p < fimLinks && lista->n < TRAILERIO_MAX; ) {
    const char *fim = js_fim(p);
    if (fim <= p || fim >= fimLinks) break;
    TrailerioFonte *fonte = &lista->fontes[lista->n];
    if (js_texto_raiz_em(p, fim, "trailers", fonte->url, sizeof fonte->url)) {
      const int tipoUrl = formato(fonte->url);
      int repetido = 0;
      for (int i = 0; i < lista->n; i++)
        if (!strcmp(lista->fontes[i].url, fonte->url)) repetido = 1;
      if (tipoUrl && !repetido) {
        fonte->hls = tipoUrl == 2;
        lista->n++;
      }
    }
    p = js_prox(fim);
  }
  return lista->n;
}

static void liberar(TrailerioPedido *pedido) {
  rede_job_soltar(pedido->job);
  free(pedido->lista);
  free(pedido);
}

static void *buscar(void *argumento) {
  TrailerioPedido *pedido = argumento;
  RedePedido carga = { .url = pedido->pedido, .prazo_ms = 8000,
                      .max_bytes = 128 * 1024, .job = pedido->job };
  RedeResposta resposta = {0};
  if (rede_pedir(&carga, &resposta) && resposta.status == 200) {
    pedido->lista = malloc(sizeof *pedido->lista);
    if (pedido->lista) trailerio_ler(resposta.corpo, pedido->id, pedido->tipo, pedido->lista);
  }
  rede_resposta_limpar(&resposta);

  pthread_mutex_lock(&trava);
  const int cancelado = pedido->cancelado;
  if (!cancelado) pedido->pronto = 1;
  pthread_mutex_unlock(&trava);
  if (cancelado) liberar(pedido);
  return NULL;
}

int trailerio_iniciar(const char *id, const char *tipo) {
  if (atual || !trailerio_id(id, tipo)) return 0;
  TrailerioPedido *pedido = calloc(1, sizeof *pedido);
  if (!pedido) return 0;
  const int n = snprintf(pedido->pedido, sizeof pedido->pedido,
                         "https://trailerio.cc/meta/%s/%s.json", tipo, id);
  if (n < 0 || (size_t)n >= sizeof pedido->pedido) { free(pedido); return 0; }
  RedeGrupo *grupo = rede_grupo_criar();
  pedido->job = rede_job_criar(grupo);
  rede_grupo_soltar(grupo);
  strcpy(pedido->id, id);
  strcpy(pedido->tipo, tipo);
  pthread_t thread;
  if (!pedido->job || pthread_create(&thread, NULL, buscar, pedido)) {
    liberar(pedido);
    return 0;
  }
  pthread_detach(thread);
  atual = pedido;
  return 1;
}

int trailerio_resultado(TrailerioLista **lista) {
  TrailerioPedido *pedido = atual;
  *lista = NULL;
  if (!pedido) return -1;
  pthread_mutex_lock(&trava);
  const int pronto = pedido->pronto;
  pthread_mutex_unlock(&trava);
  if (!pronto) return 0;
  if (pedido->lista && pedido->lista->n > 0) {
    *lista = pedido->lista;
    pedido->lista = NULL;
  }
  atual = NULL;
  liberar(pedido);
  return *lista ? 1 : -1;
}

void trailerio_cancelar(void) {
  TrailerioPedido *pedido = atual;
  if (!pedido) return;
  atual = NULL;
  pthread_mutex_lock(&trava);
  const int pronto = pedido->pronto;
  if (!pronto) {
    pedido->cancelado = 1;
    rede_job_cancelar(pedido->job);
  }
  pthread_mutex_unlock(&trava);
  if (pronto) liberar(pedido);
}
#endif
