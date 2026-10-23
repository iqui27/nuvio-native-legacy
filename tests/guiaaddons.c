// Filtro de add-ons do guia (#283): ler, gravar, alternar, por perfil.
#include "guiaaddons.h"
#include <stdio.h>
#include <string.h>

static int falhas;
#define CHECA(c) do { if (!(c)) { printf("FALHOU: %s (linha %d)\n", #c, __LINE__); falhas++; } } while (0)

int main(void) {
  GuiaAddonsOcultos o, o2;
  char buf[4096], nome[48];
  memset(&o, 0, sizeof o);
  // Padrao: nada escondido, todos aparecem.
  CHECA(!guiaaddons_oculto(&o, "https://a.example"));
  guiaaddons_ler(&o, NULL);
  CHECA(o.n == 0);
  // Esconder, conferir, mostrar de novo.
  CHECA(guiaaddons_alternar(&o, "https://a.example") == 1);
  CHECA(guiaaddons_alternar(&o, "https://b.example/x") == 1);
  CHECA(guiaaddons_oculto(&o, "https://a.example"));
  CHECA(!guiaaddons_oculto(&o, "https://c.example"));
  CHECA(guiaaddons_alternar(&o, "https://a.example") == 0);
  CHECA(!guiaaddons_oculto(&o, "https://a.example") && guiaaddons_oculto(&o, "https://b.example/x"));
  // Ida e volta pelo texto do arquivo (CRLF e repetidas toleradas).
  guiaaddons_alternar(&o, "https://a.example");
  CHECA(guiaaddons_texto(&o, buf, sizeof buf) > 0);
  guiaaddons_ler(&o2, buf);
  CHECA(o2.n == 2 && guiaaddons_oculto(&o2, "https://a.example") && guiaaddons_oculto(&o2, "https://b.example/x"));
  guiaaddons_ler(&o2, "https://a.example\r\n\r\nhttps://a.example\n");
  CHECA(o2.n == 1);
  // Base vazia nunca entra; teto respeitado sem estourar.
  CHECA(guiaaddons_alternar(&o, "") == 0 && !guiaaddons_oculto(&o, ""));
  memset(&o, 0, sizeof o);
  for (int i = 0; i < GUIAADDONS_MAX; i++) {
    char b[64]; snprintf(b, sizeof b, "https://h%d.example", i);
    CHECA(guiaaddons_alternar(&o, b) == 1);
  }
  CHECA(guiaaddons_alternar(&o, "https://extra.example") == 0 && o.n == GUIAADDONS_MAX);
  // Texto que nao cabe: devolve 0 e buffer vazio, nunca meia lista.
  CHECA(guiaaddons_texto(&o, buf, 40) == 0 && buf[0] == 0);
  // Um arquivo por perfil.
  CHECA(!strcmp(guiaaddons_arquivo(1, nome, sizeof nome), "guia-addons-p1.txt"));
  CHECA(!strcmp(guiaaddons_arquivo(3, nome, sizeof nome), "guia-addons-p3.txt"));
  CHECA(!strcmp(guiaaddons_arquivo(0, nome, sizeof nome), "guia-addons-p1.txt"));
  printf(falhas ? "%d falha(s)\n" : "guiaaddons: ok\n", falhas);
  return falhas ? 1 : 0;
}
