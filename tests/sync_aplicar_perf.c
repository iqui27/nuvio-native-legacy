// CUSTO DO CICLO DE SYNC NO FIO PRINCIPAL (colecoes da conta), no Mac.
//
// Registros da TCL Smart TV Pro do dono (Android 14, Mali-G52, 04/10/2026):
// a cada cinco minutos um quadro com `upd=59..95 ms` e des ~2 ms, sempre no
// segundo em que o log mostra "[sync] blob de ajustes" -> "[colecoes] 129
// pastas vindas da conta" -> "[contalib] ...". Este teste monta uma resposta
// com o mesmo tamanho (129 pastas em ~20 colecoes, URLs longas, embrulhada em
// collections_json como a RPC manda) e mede, repetido, o que sync_passo faz
// com ela: colfileiras_receber (valida + col_definir_json) e
// colfileiras_sincronizar. A segunda volta e a de todo ciclo: conteudo igual.
//
//   bash tests/sync_aplicar_perf.sh
#include "colfileiras.h"
#include "colecoes.h"
#include "catordem.h"
#include "fileiras.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

const char *addons_base_por_id(const char *id) { (void)id; return "https://fixture.example"; }
int addons_n(void) { return 1; }
int addons_base_desligada(const char *b) { (void)b; return 0; }   // 203-desligados
const char *addons_base(int i) { (void)i; return "https://fixture.example"; }
const char *addons_id_manifesto(int i) { (void)i; return "addon.demo"; }
const char *sessao_usuario(void) { return "account-A"; }
char *dados_caminho(char *dst, unsigned n, const char *nome) {
  snprintf(dst, n, "%s/%s", getenv("NV_T_DIR"), nome); return dst;
}
int dados_gravar(const char *nome, const char *texto) {
  char path[1024]; dados_caminho(path, sizeof path, nome);
  FILE *f = fopen(path, "w"); if (!f) return 0;
  int ok = fputs(texto, f) >= 0;
  return fclose(f) == 0 && ok;
}

static int cmpd(const void *a, const void *b) {
  double x = *(const double *)a, y = *(const double *)b;
  return x < y ? -1 : x > y;
}
static double agoraMs(void) {
  struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}

// Texto JSON escapado para dentro de uma string JSON (collections_json).
static void escapar(char *dst, const char *src) {
  for (; *src; src++) { if (*src == '"' || *src == '\\') *dst++ = '\\'; *dst++ = *src; }
  *dst = 0;
}

int main(void) {
  size_t cap = 4u << 20, k = 0;
  char *in = malloc(cap), *out = malloc(cap * 2 + 64);
  int c, f, s, pasta = 0, i;
  assert(in && out);
  k += (size_t)snprintf(in + k, cap - k, "[");
  for (c = 0; c < 20; c++) {
    k += (size_t)snprintf(in + k, cap - k, "%s{\"id\":\"col-%02d-6f1c2b9e\",\"title\":\"Colecao %d\","
         "\"backdropImageUrl\":\"https://cdn.xperience-app.com/covers/default/colecao-%02d.backdrop.webp?v=1759500000\","
         "\"folders\":[", c ? "," : "", c, c, c);
    for (f = 0; f < 7 && pasta < 129; f++, pasta++) {
      k += (size_t)snprintf(in + k, cap - k, "%s{\"id\":\"folder-%03d-1b2c3d4e\",\"title\":\"Pasta %d\","
           "\"coverImageUrl\":\"https://cdn.xperience-app.com/covers/default/pasta-%03d.cover.webp?v=1759500000&w=640\","
           "\"heroBackdropUrl\":\"https://cdn.xperience-app.com/covers/default/pasta-%03d.backdrop.webp?v=1759500000\","
           "\"titleLogoUrl\":\"https://cdn.xperience-app.com/logos/pasta-%03d.logo.png?v=1759500000\","
           "\"focusGifUrl\":\"%s\",\"focusGifEnabled\":true,\"hideTitle\":false,\"tileShape\":\"poster\","
           "\"sources\":[", f ? "," : "", pasta, pasta, pasta, pasta, pasta,
           pasta % 3 ? "" : "https://media.giphy.com/media/abcdefghijklmnop/giphy.gif");
      for (s = 0; s < 4; s++)
        k += (size_t)snprintf(in + k, cap - k, "%s{\"provider\":\"addon\",\"addonId\":\"addon.demo\","
             "\"addonBaseUrl\":\"https://fixture.example/manifest.json\",\"type\":\"%s\","
             "\"catalogId\":\"cat-%03d-%d\",\"title\":\"Catalogo %d/%d\",\"genre\":\"None\"}",
             s ? "," : "", s % 2 ? "series" : "movie", pasta, s, pasta, s);
      k += (size_t)snprintf(in + k, cap - k, "]}");
    }
    k += (size_t)snprintf(in + k, cap - k, "]}");
  }
  k += (size_t)snprintf(in + k, cap - k, "]");
  strcpy(out, "[{\"collections_json\":\"");
  escapar(out + strlen(out), in);
  strcat(out, "\"}]");
  printf("resposta: %zu bytes (%d pastas)\n", strlen(out), pasta);

  fil_definir_perfil(1); fil_definir_limite(40);
  int voltas = getenv("NV_VOLTAS") ? atoi(getenv("NV_VOLTAS")) : 60;
  static double tv[4096], tr[4096], ts[4096];
  if (voltas > 4096) voltas = 4096;
  for (i = 0; i < voltas; i++) {
    double t0 = agoraMs(), t1, t2, t3;
    int ok = col_resposta_valida(out);
    t1 = agoraMs();
    int n = colfileiras_receber(out);
    t2 = agoraMs();
    colfileiras_sincronizar();
    t3 = agoraMs();
    tv[i] = t1 - t0; tr[i] = t2 - t1; ts[i] = t3 - t2;
    if (i < 2) printf("volta %d: valida=%.2f ms receber=%.2f ms sincronizar=%.2f ms (ok=%d pastas=%d)\n",
           i, t1 - t0, t2 - t1, t3 - t2, ok, n);
  }
  qsort(tv, (size_t)voltas, sizeof *tv, cmpd);
  qsort(tr, (size_t)voltas, sizeof *tr, cmpd);
  qsort(ts, (size_t)voltas, sizeof *ts, cmpd);
  printf("mediana de %d voltas: valida=%.2f ms receber=%.2f ms sincronizar=%.2f ms | p95 receber=%.2f ms\n",
         voltas, tv[voltas / 2], tr[voltas / 2], ts[voltas / 2], tr[(int)(0.95 * (voltas - 1))]);
  return 0;
}
