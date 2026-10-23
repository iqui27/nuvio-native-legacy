// Musicas: interpreta a resposta do Worker, monta a URL do QR e guarda por perfil.
#include "../src/musica.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int perfil = 1;
static char *arq[4]; static char nomeArq[4][48];
int perfis_ativo(void) { return perfil; }
void sync_proteger_ajustes_locais(void) {}
char *rede_baixar(const char *u, int s) { (void)u; (void)s; return NULL; }
char *dados_ler(const char *n) {
  int i; for (i = 0; i < 4; i++) if (arq[i] && !strcmp(nomeArq[i], n)) return strdup(arq[i]);
  return NULL;
}
int dados_gravar(const char *n, const char *c) {
  int i, v = -1;
  for (i = 0; i < 4; i++) { if (arq[i] && !strcmp(nomeArq[i], n)) { v = i; break; } if (!arq[i] && v < 0) v = i; }
  free(arq[v]); arq[v] = strdup(c); snprintf(nomeArq[v], 48, "%s", n); return 1;
}
static int falhas;
#define CK(c) do { if (!(c)) { printf("FAIL %s\n", #c); falhas++; } } while (0)

int main(void) {
  MusAlbum a;
  int page = 0;
  for (int i = 0; i < 8; i++) page = mus_pagina(page, 18, 1);
  CK(page == 8 && mus_pagina(page, 18, 1) == 8);
  CK(mus_pagina(page, 18, -1) == 7 && mus_pagina(0, 18, -1) == 0);
  CK(mus_pagina(29, 60, 1) == 29);
  char url[300];
  const char *ok = "{\"album\":{\"titulo\":\"Dune (Original Motion Picture Soundtrack)\",\"artista\":\"Hans Zimmer\","
    "\"capa\":\"https://x/600x600bb.jpg\",\"apple\":\"https://music.apple.com/a\"},"
    "\"faixas\":[{\"n\":1,\"titulo\":\"Dream of Arrakis\",\"artista\":\"Hans Zimmer\",\"dur\":65},"
    "{\"n\":2,\"titulo\":\"Ripples in the Sand\",\"artista\":\"Hans Zimmer\",\"dur\":190}]}";
  CK(mus_interpretar(ok, &a));
  CK(a.n == 2 && a.faixa[1].dur == 190 && !strcmp(a.faixa[0].titulo, "Dream of Arrakis"));
  CK(!strcmp(a.artista, "Hans Zimmer") && a.capa[0]);
  CK(!mus_interpretar("{\"vazio\":true}", &a));
  CK(!mus_interpretar("lixo", &a));
  CK(!mus_interpretar(NULL, &a));
  mus_url_celular(url, sizeof url, "tt1160419", "Dune: Part Two", 2024, 0);
  CK(url[0] == 0); // sem NV_REC_URL
  CK(!mus_guardado("tt1", 0));
  CK(mus_alternar_guardado("tt1", 0, "Album A", "Art") == 1);
  CK(mus_guardado("tt1", 0) && !mus_guardado("tt1", 2));
  CK(mus_alternar_guardado("tt1", 2, "Album B", "Art") == 1);
  CK(mus_alternar_guardado("tt1", 0, "Album A", "Art") == 0);
  CK(!mus_guardado("tt1", 0) && mus_guardado("tt1", 2));
  MusSalva saved[48];
  CK(mus_listar(saved, 48) == 1);
  CK(!strcmp(saved[0].imdb, "tt1") && saved[0].temp == 2 && !strcmp(saved[0].titulo, "Album B"));
  perfil = 2; CK(!mus_guardado("tt1", 2)); // outro perfil nao ve
  CK(mus_listar(saved, 48) == 0);
  printf(falhas ? "musica: %d falhas\n" : "musica: ok\n", falhas);
  return falhas != 0;
}
