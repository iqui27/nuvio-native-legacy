// A versao maior do webOS vem de UM lugar: webos_release do nyx, depois o
// starfish-release, e "desconhecida" (0) quando nenhum dos dois diz com
// clareza. TV de 2017 (webOS 3.9) so tem o nyx.
#include "webosver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)
static void grava(const char *caminho, const char *txt) {
  FILE *f = fopen(caminho, "wb");
  if (f) { fputs(txt, f); fclose(f); }
}
static int com(const char *dir, const char *nyx, const char *star) {
  char a[256], b[256];
  snprintf(a, sizeof a, "%s/os_info.json", dir);
  snprintf(b, sizeof b, "%s/starfish-release", dir);
  remove(a); remove(b);
  if (nyx) grava(a, nyx);
  if (star) grava(b, star);
  nv_webos_testar(a, b);
  return nv_webos_major();
}
int main(int argc, char **argv) {
  const char *d = argc > 1 ? argv[1] : "/tmp";
  // O caso real: 65SJ800V / OLED55B7P, sem starfish-release.
  CHECK(com(d, "{\"webos_release\":\"3.9.3\",\"webos_name\":\"x\"}", NULL) == 3);
  CHECK(!strcmp(nv_webos_fonte(), "nyx"));
  CHECK(com(d, "{ \"webos_release\" : \"4.10.0\" }", NULL) == 4);
  CHECK(com(d, "{\"webos_release\":\"11.2.0-5\"}", NULL) == 11);
  CHECK(com(d, "{\"webos_release\":\"6.0.0\"}", "Rockhopper release 4.10.2-31 (x)\n") == 6); // nyx manda
  // sem nyx: starfish
  CHECK(com(d, NULL, "Rockhopper release 5.1.0-2 (x)\n") == 5);
  CHECK(!strcmp(nv_webos_fonte(), "starfish"));
  { char l[64]; nv_webos_starfish_linha(l, sizeof l); CHECK(!strncmp(l, "Rockhopper release 5", 20)); }
  // nyx malformado cai no starfish
  CHECK(com(d, "{\"webos_release\":null}", "x release 4.5.1\n") == 4);
  // nenhum dos dois: desconhecida
  CHECK(com(d, NULL, NULL) == 0);
  CHECK(!strcmp(nv_webos_fonte(), "-"));
  // malformados (cada um = desconhecido)
  CHECK(com(d, "{\"webos_release\":\"3", NULL) == 0);                 // cortado
  CHECK(com(d, "{\"webos_release\":\"3garbage\"}", NULL) == 0);
  CHECK(com(d, "{\"webos_release\":\"3\"}", NULL) == 0);              // sem minor
  CHECK(com(d, "{\"webos_release\":null,\"3\":\"x\"}", NULL) == 0);   // nome de outro campo
  CHECK(com(d, "{\"webos_release\":3.9}", NULL) == 0);                // nao e string
  CHECK(com(d, "{\"webos_release\":\"-3.9.0\"}", NULL) == 0);         // negativo
  CHECK(com(d, "{\"webos_release\":\"3.9.0", NULL) == 0);            // string aberta
  CHECK(com(d, "{\"webos_release\":\"99999999999.1\"}", NULL) == 0);  // fora da faixa
  CHECK(com(d, "{\"webos_release\":\"0.0.0\"}", NULL) == 0);
  CHECK(com(d, "{\"webos_release\":\"3.9\\u0030\"}", NULL) == 0);     // escape
  CHECK(com(d, "{\"x\":\"webos_release\"}", NULL) == 0);
  CHECK(nv_webos_parse_nyx(NULL) == 0);
  {  // valor cortado exatamente no limite de leitura
    static char grande[NV_WEBOS_LEITURA + 64];
    int n = snprintf(grande, sizeof grande, "{\"pad\":\"");
    memset(grande + n, 'a', NV_WEBOS_LEITURA - 1 - n - 20);
    n = NV_WEBOS_LEITURA - 1 - 20;
    snprintf(grande + n, sizeof grande - n, "\",\"webos_release\":\"3.9.3\"}");
    CHECK(com(d, grande, NULL) == 0);   // o valor cai alem de 4095 bytes
  }
  // starfish malformado
  CHECK(com(d, NULL, "release abc\n") == 0);
  CHECK(com(d, NULL, "no release here\nRockhopper release 4.10.2-31\n") == 4);
  // 1: major.minor exato
  CHECK(com(d, "{\"webos_release\":\"3.9\"}", NULL) == 3);
  CHECK(com(d, "{\"webos_release\":\"4.10\"}", NULL) == 4);
  // 2: so a chave do nivel 1, documento fechado, duplicata conflitante = desconhecido
  CHECK(com(d, "{\"old\":{\"webos_release\":\"5.0.0\"},\"webos_release\":\"3.9.3\"}", NULL) == 3);
  CHECK(com(d, "{\"old\":{\"webos_release\":\"5.0.0\"}}", NULL) == 0);
  CHECK(com(d, "{\"a\":[{\"webos_release\":\"5.0.0\"}],\"b\":\"x\"}", NULL) == 0);
  CHECK(com(d, "{\"webos_release\":\"3.9.3\"", NULL) == 0);                    // sem fechar
  CHECK(com(d, "{\"webos_release\":\"3.9.3\"} lixo", NULL) == 0);             // sobra
  CHECK(com(d, "{\"webos_release\":\"3.9.3\"}  \n", NULL) == 3);              // so espaco
  CHECK(com(d, "{\"webos_release\":\"3.9.3\",\"webos_release\":\"5.0.0\"}", NULL) == 0); // conflito
  CHECK(com(d, "{\"webos_release\":\"3.9.3\",\"webos_release\":\"3.9.3\"}", NULL) == 3); // igual
  CHECK(com(d, "{\"webos_release\":\"3.9.3\",\"webos_release\":null}", NULL) == 0);
  CHECK(com(d, "{\"x\":\"a\\\"}b{\",\"webos_release\":\"3.9.3\"}", NULL) == 3); // aspas escapadas
  CHECK(com(d, "[\"webos_release\",\"3.9.3\"]", NULL) == 0);
  // 5: starfish com fronteira de palavra e terminador valido
  CHECK(com(d, NULL, "prerelease 5.0\n") == 0);
  CHECK(com(d, NULL, "Rockhopper release 5garbage\n") == 0);
  CHECK(com(d, NULL, "Rockhopper release 5\n") == 5);
  CHECK(com(d, NULL, "Rockhopper release 4.10.2-31 (x)\n") == 4);
  {
    static char longa[400];
    memset(longa, 'a', sizeof longa); longa[sizeof longa - 1] = 0;
    memcpy(longa + 250, "release 5.0", 11);       // numero cortado pelo limite da linha
    CHECK(com(d, NULL, longa) == 0);
  }
  // JSON malformado em valor que nao e o nosso: o documento todo e desconhecido
  CHECK(com(d, "{\"x\":[},\"webos_release\":\"5.0\"}", NULL) == 0);        // fechador de outro tipo
  CHECK(com(d, "{\"x\":{]},\"webos_release\":\"5.0\"}", NULL) == 0);
  CHECK(com(d, "{\"x\":,\"webos_release\":\"5.0\"}", NULL) == 0);          // valor ausente
  CHECK(com(d, "{\"x\":tru,\"webos_release\":\"5.0\"}", NULL) == 0);       // primitivo invalido
  CHECK(com(d, "{\"x\":[tru],\"webos_release\":\"5.0\"}", NULL) == 0);
  CHECK(com(d, "{\"x\":[1,],\"webos_release\":\"5.0\"}", NULL) == 0);      // virgula sobrando
  CHECK(com(d, "{\"x\":[1 2],\"webos_release\":\"5.0\"}", NULL) == 0);     // virgula faltando
  CHECK(com(d, "{\"x\" 1,\"webos_release\":\"5.0\"}", NULL) == 0);         // dois-pontos faltando
  CHECK(com(d, "{\"a\":1,,\"webos_release\":\"5.0\"}", NULL) == 0);
  CHECK(com(d, "{\"webos_release\":\"5.0\",}", NULL) == 0);
  CHECK(com(d, "{\"x\":-,\"webos_release\":\"5.0\"}", NULL) == 0);
  CHECK(com(d, "{\"x\":01,\"webos_release\":\"5.0\"}", NULL) == 0);
  CHECK(com(d, "{\"x\":1.,\"webos_release\":\"5.0\"}", NULL) == 0);
  CHECK(com(d, "{\"a\":true,\"b\":false,\"c\":null,\"d\":-1.5e3,\"e\":[1,{\"y\":[]},\"s\"],\"webos_release\":\"5.0\"}", NULL) == 5);
  // chave com barra no nivel 1: documento desconhecido (nao decodificamos), em qualquer ordem
  CHECK(com(d, "{\"webos_release\":\"5.0.0\",\"webos\\u005frelease\":\"3.9.3\"}", NULL) == 0);
  CHECK(com(d, "{\"webos\\u005frelease\":\"3.9.3\",\"webos_release\":\"5.0.0\"}", NULL) == 0);
  CHECK(com(d, "{\"webos\\u005frelease\":\"3.9.3\"}", NULL) == 0);
  // 4/6: sem caminho e sem hook nao le nada; getters devolvem copia
  nv_webos_testar(NULL, NULL);
#ifndef NV_WEBOS
  CHECK(nv_webos_major() == 0);
#endif
  { char l[64]; com(d, NULL, "Rockhopper release 5.1.0\n"); nv_webos_starfish_linha(l, sizeof l); CHECK(!strncmp(l, "Rockhopper", 10)); }
  { // erro de leitura (diretorio onde devia haver arquivo): desconhecida e NAO guardada
    char dir[300], arq[300];
    snprintf(dir, sizeof dir, "%s/os_info.json", d);
    snprintf(arq, sizeof arq, "%s/starfish-release", d);
    remove(dir); remove(arq);
    mkdir(dir, 0700);
    nv_webos_testar(dir, arq);
    CHECK(nv_webos_major() == 0);
    rmdir(dir);
    grava(dir, "{\"webos_release\":\"3.9.3\"}");
    CHECK(nv_webos_major() == 3);        // sem nv_webos_testar: tentou de novo
  }
  { // Copilot no #408: o nyx valido nao pode ser descartado por erro de E/S no
    // starfish (fonte de menor prioridade); senao o video.c guarda o palpite 4/5.
    char nyx[300], star[300];
    snprintf(nyx, sizeof nyx, "%s/os_info.json", d);
    snprintf(star, sizeof star, "%s/starfish-release", d);
    remove(nyx); remove(star);
    grava(nyx, "{\"webos_release\":\"3.9.3\"}");
    mkdir(star, 0700);
    nv_webos_testar(nyx, star);
    CHECK(nv_webos_major() == 3);
    CHECK(!strcmp(nv_webos_fonte(), "nyx"));
    rmdir(star);
  }
  if (!fails) puts("webosver ok");
  return fails != 0;
}
