// Upload trimming. Included by avisos.c and standalone regression tests.
#include <stdio.h>
#include <string.h>
// Read a native session without allocating or scanning the entire log.
// Keep startup through the first displayed frame, capped at 16 KB, then tail.
// The caller provides max + 1 bytes, including the terminating zero.
static size_t lerRegistroCabecaCauda(FILE *f, char *texto, size_t max) {
  long tam, de;
  size_t cab, cau, n;
  char marca[64], *fim;
  int m;
  texto[0] = 0;
  if (fseek(f, 0, SEEK_END) != 0 || (tam = ftell(f)) < 0) return 0;
  rewind(f);
  if ((unsigned long)tam <= max) {
    n = fread(texto, 1, max, f);
    texto[n] = 0;
    return n;
  }
  cab = fread(texto, 1, 16 * 1024, f);
  texto[cab] = 0;
  fim = strstr(texto, "primeiro quadro na tela");
  if (fim && (fim = strchr(fim, '\n')) != NULL) cab = (size_t)(fim + 1 - texto);
  else {
    while (cab && texto[cab - 1] != '\n') cab--;
  }
  cau = max - cab - sizeof marca;
  de = tam - (long)cau;
  if (fseek(f, de, SEEK_SET) != 0) return 0;
  n = fread(texto + cab + sizeof marca, 1, cau, f);
  fim = memchr(texto + cab + sizeof marca, '\n', n);
  if (fim) {
    size_t salto = (size_t)(fim + 1 - (texto + cab + sizeof marca));
    memmove(texto + cab + sizeof marca, fim + 1, n - salto);
    de += (long)salto;
    n -= salto;
  }
  m = snprintf(marca, sizeof marca, "\n[...] %lu KB omitidos\n",
               (unsigned long)(de - (long)cab) / 1024);
  memmove(texto + cab + m, texto + cab + sizeof marca, n);
  memcpy(texto + cab, marca, (size_t)m);
  n += cab + (size_t)m;
  texto[n] = 0;
  return n;
}

// In-memory automatic uploads: keep 1/4 head + tail on line boundaries.
// The buffer contains n + 1 bytes, including the terminating zero.
static size_t cortarCabecaCauda(char *texto, size_t n, size_t max) {
  size_t cab, cau, a, b;
  char marca[64];
  int m;
  if (!texto || n <= max) return n;
  cab = max / 4;
  cau = max - cab - 64;
  a = cab;
  while (a > cab / 2 && texto[a] != '\n') a--;
  if (texto[a] != '\n') a = cab;
  b = n - cau;
  while (b < n && texto[b] != '\n') b++;
  if (b >= n - cau / 2 || b >= n) b = n - cau; else b++;
  m = snprintf(marca, sizeof marca, "\n... %lu bytes omitidos ...\n", (unsigned long)(b - a));
  memmove(texto + a + m, texto + b, n - b);
  memcpy(texto + a, marca, (size_t)m);
  n = a + (size_t)m + (n - b);
  texto[n] = 0;
  return n;
}
