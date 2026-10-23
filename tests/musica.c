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
  CK(page == 2 && mus_pagina(page, 18, 1) == 2);
  CK(mus_pagina(page, 18, -1) == 1 && mus_pagina(0, 18, -1) == 0);
  CK(mus_pagina(8, 60, 1) == 8);
  CK(mus_pagina(0, 0, 1) == 0 && mus_pagina(0, 14, 8) == 1);
  MusPrevia p = {0};
  memset(&a, 0, sizeof a); a.n = 14;
  for (int i = 0; i < a.n; i++) if (i != 4) strcpy(a.faixa[i].previa, "https://preview/audio.m4a");
  CK(mus_previa_iniciar(&p, &a, -1, 0, 123, 100, 1) == -1 && !p.ativa);
  CK(mus_previa_iniciar(&p, &a, -1, 0, 123, 100, 0) == 0 && p.n == 13);
  CK(mus_previa_passo(&p, 200, 0, 1, MUS_PREV_CONTINUA) == 0);
  CK(mus_previa_passo(&p, 30200, 0, 1, MUS_PREV_CONTINUA) == 1);
  for (int event = MUS_PREV_ERRO; event <= MUS_PREV_OCUPADO; event++) {
    mus_previa_iniciar(&p, &a, 2, 0, 0, 0, 0);
    CK(mus_previa_passo(&p, 10, 1, 1, event) == -1 && !p.ativa);
  }
  mus_previa_iniciar(&p, &a, 2, 0, 0, 0, 0);
  CK(mus_previa_passo(&p, 1, 30, 1, MUS_PREV_CONTINUA) == -1 && !p.ativa);
  mus_previa_iniciar(&p, &a, 2, 0, 0, 0, 0);
  CK(mus_previa_passo(&p, 15001, 0, 0, MUS_PREV_CONTINUA) == -1);
  CK(mus_previa_iniciar(&p, &a, 4, 0, 0, 0, 0) == -1);
  for (unsigned seed = 1; seed < 20; seed++) {
    int seen[14] = {0}, changed = 0;
    mus_previa_iniciar(&p, &a, -1, 1, seed, 0, 0);
    CK(p.n == 13);
    for (int i = 0; i < p.n; i++) { CK(p.ordem[i] != 4 && !seen[p.ordem[i]]++); changed |= p.ordem[i] != i; }
    CK(changed);
  }
  unsigned char pixels[8*8*4], ambient[32*18*4];
  for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++) {
    unsigned char *q = pixels + (y*8+x)*4;
    q[0] = x < 4 ? 0 : 255; q[1] = 0; q[2] = x < 4 ? 255 : 0; q[3] = 255;
  }
  mus_cores(pixels, 8, 8, 32, ambient);
  CK(ambient[2] > ambient[0] && ambient[31*4] > ambient[31*4+2]);
  CK(ambient[3] == 255 && ambient[(32*17)*4+2] < ambient[2]);
  mus_cores(NULL, 0, 0, 0, ambient); CK(ambient[3] == 255);
  char url[300];
  const char *ok = "{\"album\":{\"titulo\":\"Dune (Original Motion Picture Soundtrack)\",\"artista\":\"Hans Zimmer\","
    "\"capa\":\"https://x/600x600bb.jpg\",\"apple\":\"https://music.apple.com/a\"},"
    "\"faixas\":[{\"n\":1,\"titulo\":\"Dream of Arrakis\",\"artista\":\"Hans Zimmer\",\"dur\":65,\"previa\":\"https://audio/x.m4a\"},"
    "{\"n\":2,\"titulo\":\"Ripples in the Sand\",\"artista\":\"Hans Zimmer\",\"dur\":190}]}";
  CK(mus_interpretar(ok, &a));
  CK(a.n == 2 && a.faixa[1].dur == 190 && !strcmp(a.faixa[0].titulo, "Dream of Arrakis"));
  CK(!strcmp(a.artista, "Hans Zimmer") && a.capa[0]);
  CK(!strcmp(a.faixa[0].previa, "https://audio/x.m4a") && !a.faixa[1].previa[0]);
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
