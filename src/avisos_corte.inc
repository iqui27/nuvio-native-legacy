// Corte do registro do envio automatico (#203). Incluido por avisos.c e tests/avisos_corte.c.
#include <stdio.h>
#include <string.h>
// Cabeca (1/4) + cauda, com uma linha "... N bytes omitidos ..." no meio. Corta
// em quebra de linha. Mexe no proprio buffer (texto tem nTexto+1 bytes, ja
// terminado em 0) e devolve o novo tamanho.
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
