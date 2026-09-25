// Ver vazao.h. Aritmetica pura: tests/vazao.sh compila isto sozinho.
#include "vazao.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int menor(const void *a, const void *b) {
  int x = *(const int *)a, y = *(const int *)b;
  return x < y ? -1 : x > y;
}

int vazao_resumir(const int *kbps, int n, VazaoResumo *r) {
  int v[VAZAO_AMOSTRAS_MAX], m = 0, i, k;
  if (r) memset(r, 0, sizeof *r);
  if (!kbps || !r) return 0;
  for (i = 0; i < n && m < VAZAO_AMOSTRAS_MAX; i++)
    if (kbps[i] >= 0) v[m++] = kbps[i];
  if (!m) return 0;
  qsort(v, (size_t)m, sizeof v[0], menor);
  r->n = m;
  r->medianaKbps = (m & 1) ? v[m / 2] : (int)(((long)v[m / 2 - 1] + v[m / 2] + 1) / 2);
  // Posicao mais proxima: ceil(0,2 m) - 1, em inteiro. Com 5 amostras e a
  // 1a; com 24, a 5a — nunca o minimo absoluto de uma amostra grande, que e
  // um segundo isolado e nao o "trecho ruim" tipico.
  k = (m * 20 + 99) / 100 - 1;
  if (k < 0) k = 0;
  r->p20Kbps = v[k];
  r->otimoKbps = (int)(((long)r->p20Kbps * 75 + 50) / 100);
  r->maximoKbps = (int)(((long)r->medianaKbps * 90 + 50) / 100);
  return 1;
}

double vazao_gb(int kbps, int segundos) {
  if (kbps <= 0 || segundos <= 0) return 0.0;
  // kbps / 1000 = Mbps; Mbps x s / 8 = MB; / 1000 = GB.
  return (double)kbps * (double)segundos / 8.0 / 1000000.0;
}

static void fmtUmaCasa(char *dst, size_t n, double x, char sep) {
  char *p;
  if (!dst || !n) return;
  if (x < 0) x = 0;
  if (x < 9.95) snprintf(dst, n, "%.1f", x);
  else snprintf(dst, n, "%.0f", x);
  p = strchr(dst, '.');
  if (p) *p = sep;
}

void vazao_fmt_mbps(char *dst, size_t n, int kbps, char sep) {
  fmtUmaCasa(dst, n, (double)kbps / 1000.0, sep);
}

void vazao_fmt_gb(char *dst, size_t n, double gb, char sep) {
  fmtUmaCasa(dst, n, gb, sep);
}

int vazao_url_aviso(const char *u) {
  if (!u) return 0;
  return strstr(u, "downloading.mp4") || strstr(u, "/slate") ||
         strstr(u, "slate.mp4") || strstr(u, "slate.m3u8") ||
         strstr(u, "slate.elfhosted.com") || strstr(u, "static.debridio.com") ? 1 : 0;
}

int vazao_host(const char *u, char *h, size_t n) {
  const char *p = u ? strstr(u, "://") : NULL;
  size_t k;
  if (h && n) h[0] = 0;
  if (!p || !h || n == 0) return 0;
  k = (size_t)(p + 3 - u) + strcspn(p + 3, "/?#");
  if (k >= n) k = n - 1;
  memcpy(h, u, k);
  h[k] = 0;
  return 1;
}

// Degraus pelo bitrate MEDIO tipico de cada tipo de arquivo (o que um filme
// de 2 h ocupa dividido pela duracao): remux 4K 50-60 Mbps, remux 1080p
// 25-30, WEB-DL 4K 15-25, WEB-DL 1080p 5-10. O degrau usa o OTIMO, que ja
// tem a folga para o pico de cena.
const char *vazao_dica(int otimoKbps) {
  if (otimoKbps >= 60000) return "Remux 4K deve tocar sem parar.";
  if (otimoKbps >= 30000) return "4K WEB-DL e remux 1080p sem parar; remux 4K pode pausar.";
  if (otimoKbps >= 15000) return "4K WEB-DL leve ou 1080p; evite remux.";
  if (otimoKbps >= 6000) return "Prefira 1080p WEB-DL; 4K pode pausar.";
  return "Prefira 720p ou 1080p leve.";
}
