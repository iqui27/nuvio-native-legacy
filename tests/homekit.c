// Kit de Home (homekit.c): a regra pura. Texto -> pastas -> texto, sem rede,
// sem disco e sem colecoes.c.
#include "homekit.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int soCinemeta(const char *id) { return !strcmp(id, "com.linvo.cinemeta"); }

static const char *KIT =
  "{\"nuvioKit\":1,\"name\":\"Kit \\\"teste\\\"\",\"author\":\"alguem\","
  "\"collections\":["
  "{\"id\":\"c1\",\"title\":\"Streamings\",\"backdropImageUrl\":\"https://img.invalid/fundo.jpg\","
  "\"folders\":["
  "{\"id\":\"f1\",\"title\":\"Netflix\",\"coverImageUrl\":\"https://img.invalid/n.png\",\"tileShape\":\"POSTER\","
  "\"sources\":[{\"provider\":\"tmdb\",\"title\":\"Séries\",\"tmdbSourceType\":\"NETWORK\",\"tmdbId\":213,\"mediaType\":\"TV\",\"sortBy\":\"popularity.desc\",\"filters\":{\"withGenres\":\"18\"}}]},"
  "{\"id\":\"f2\",\"title\":\"Top\",\"coverImageUrl\":\"javascript:alert(1)\",\"hideTitle\":true,"
  "\"sources\":[{\"provider\":\"addon\",\"addonBaseUrl\":\"https://ladrao.invalid/CHAVE\",\"addonId\":\"com.linvo.cinemeta\",\"type\":\"movie\",\"catalogId\":\"top\"},"
  "{\"provider\":\"addon\",\"addonId\":\"outro.addon\",\"type\":\"movie\",\"catalogId\":\"x\"},"
  "{\"provider\":\"addon\",\"addonBaseUrl\":\"https://ladrao.invalid/CHAVE\",\"type\":\"movie\",\"catalogId\":\"y\"},"
  "{\"provider\":\"desconhecido\"}]},"
  "{\"id\":\"f3\",\"title\":\"Sem fonte\",\"sources\":[]},"
  "{\"id\":\"f4\",\"sources\":[{\"provider\":\"trakt\",\"traktListId\":5}]}"
  "]},"
  "{\"title\":\"Listas\",\"folders\":[{\"title\":\"Oscar\",\"sources\":[{\"provider\":\"trakt\",\"traktListId\":42,\"mediaType\":\"MOVIE\",\"sortBy\":\"rank\",\"sortHow\":\"asc\"},{\"provider\":\"trakt\",\"traktListId\":0}]}]}"
  "]}";

static HkKit a, b;

static void formato(void) {
  assert(hk_ler(KIT, &a, soCinemeta) == HK_OK);
  assert(!strcmp(a.nome, "Kit \"teste\"") && !strcmp(a.autor, "alguem"));
  assert(a.nGrupos == 2 && a.nPastas == 3);
  // pasta 0: TMDB, arte e forma
  assert(!strcmp(a.pastas[0].title, "Netflix") && !strcmp(a.pastas[0].group, "Streamings"));
  assert(!strcmp(a.pastas[0].groupId, "kit_home_0") && !strncmp(a.pastas[0].id, "kit_", 4));
  assert(!strcmp(a.pastas[0].cover, "https://img.invalid/n.png"));
  assert(!strcmp(a.pastas[0].groupBackdrop, "https://img.invalid/fundo.jpg"));
  assert(a.pastas[0].forma == COL_FORMA_POSTER && a.pastas[0].nSources == 1);
  assert(a.pastas[0].sources == a.fontes[0]);
  assert(!strcmp(a.fontes[0][0].prov, "tmdb") && a.fontes[0][0].tmdbId == 213);
  assert(!strcmp(a.fontes[0][0].filtros, "{\"withGenres\":\"18\"}"));
  // pasta 1: so o addon instalado entra, e SEM a URL que o kit trouxe
  assert(!strcmp(a.pastas[1].title, "Top") && a.pastas[1].nSources == 1 && a.pastas[1].hideTitle);
  assert(!a.fontes[1][0].prov[0] && !strcmp(a.fontes[1][0].addonId, "com.linvo.cinemeta"));
  assert(!a.fontes[1][0].base[0]);
  assert(!a.pastas[1].cover[0]);                     // javascript: recusado
  assert(a.pastas[1].forma == COL_FORMA_QUADRADO);   // ausente = quadrado, como o web
  // pasta 2: segunda colecao, lista do Trakt
  assert(!strcmp(a.pastas[2].group, "Listas") && !strcmp(a.pastas[2].groupId, "kit_home_1"));
  assert(a.grupoDe[2] == 1 && a.fontes[2][0].traktLista == 42);
  // o relatorio do que ficou de fora
  assert(a.pulAddon == 2);   // addon nao instalado + addon so com URL
  assert(a.pulFonte == 2);   // provedor desconhecido + trakt sem id
  assert(a.pulPasta == 2);   // sem fonte + sem titulo
  assert(a.pulArte == 1);
  // A URL de terceiro nao sobrevive em lugar nenhum do que e guardado.
  { char *s = hk_serializar(&a); assert(s && !strstr(s, "ladrao")); free(s); }
  printf("formato: ok\n");
}

static void idaEVolta(void) {
  char *s = hk_serializar(&a), *s2;
  assert(s);
  assert(hk_ler(s, &b, NULL) == HK_OK);
  assert(b.nGrupos == a.nGrupos && b.nPastas == a.nPastas);
  assert(!b.pulAddon && !b.pulFonte && !b.pulPasta && !b.pulArte);
  for (int i = 0; i < a.nPastas; i++) {
    assert(!strcmp(a.pastas[i].title, b.pastas[i].title) && !strcmp(a.pastas[i].id, b.pastas[i].id));
    assert(!strcmp(a.pastas[i].groupId, b.pastas[i].groupId) && a.pastas[i].forma == b.pastas[i].forma);
    assert(a.pastas[i].nSources == b.pastas[i].nSources && a.pastas[i].hideTitle == b.pastas[i].hideTitle);
    assert(!memcmp(a.fontes[i], b.fontes[i], sizeof a.fontes[i]));
  }
  s2 = hk_serializar(&b);
  assert(s2 && !strcmp(s, s2));
  free(s); free(s2);
  printf("ida e volta: ok\n");
}

static void recusas(void) {
  static char grande[HK_CORPO_MAX + 64];
  assert(hk_ler(NULL, &b, NULL) == HK_ERR_JSON);
  assert(hk_ler("", &b, NULL) == HK_ERR_JSON);
  assert(hk_ler("<html>404</html>", &b, NULL) == HK_ERR_JSON);
  assert(hk_ler("{\"foo\":1}", &b, NULL) == HK_ERR_JSON);
  assert(hk_ler("{\"collections\":[]}", &b, NULL) == HK_ERR_VAZIO);
  assert(hk_ler("{\"nuvioKit\":2,\"collections\":[]}", &b, NULL) == HK_ERR_VERSAO);
  // download cortado no meio: nunca meio kit
  { char *m = strdup(KIT); m[strlen(m) - 40] = 0; assert(hk_ler(m, &b, NULL) == HK_ERR_JSON); assert(!b.nPastas); free(m); }
  { char *m = malloc(strlen(KIT) + 8); sprintf(m, "%s lixo", KIT); assert(hk_ler(m, &b, NULL) == HK_ERR_JSON); free(m); }
  memset(grande, ' ', sizeof grande - 1);
  assert(hk_ler(grande, &b, NULL) == HK_ERR_GRANDE);
  // sem addon nenhum instalado e so fonte de addon: vazio, e diz por que
  assert(hk_ler("[{\"title\":\"A\",\"folders\":[{\"title\":\"B\",\"sources\":[{\"addonId\":\"x\",\"type\":\"movie\",\"catalogId\":\"c\"}]}]}]", &b, soCinemeta) == HK_ERR_VAZIO);
  assert(b.pulAddon == 1);
  // o mesmo array sozinho (export do web) entra quando o addon existe
  assert(hk_ler("[{\"title\":\"A\",\"folders\":[{\"title\":\"B\",\"sources\":[{\"addonId\":\"com.linvo.cinemeta\",\"type\":\"movie\",\"catalogId\":\"top\"}]}]}]", &b, soCinemeta) == HK_OK);
  printf("recusas: ok\n");
}

static void tetos(void) {
  static char j[200000];
  int n = 0, c, f, s;
  n += sprintf(j + n, "{\"collections\":[");
  for (c = 0; c < HK_GRUPOS_MAX + 2; c++) {
    n += sprintf(j + n, "%s{\"title\":\"C%d\",\"folders\":[", c ? "," : "", c);
    for (f = 0; f < 5; f++) {
      n += sprintf(j + n, "%s{\"title\":\"P%d\",\"sources\":[", f ? "," : "", f);
      for (s = 0; s < HK_FONTES_PASTA + 3; s++)
        n += sprintf(j + n, "%s{\"provider\":\"trakt\",\"traktListId\":%d}", s ? "," : "", s + 1);
      n += sprintf(j + n, "]}");
    }
    n += sprintf(j + n, "]}");
  }
  n += sprintf(j + n, "]}");
  assert(hk_ler(j, &b, NULL) == HK_OK);
  assert(b.nGrupos == HK_GRUPOS_MAX && b.nPastas == HK_PASTAS_MAX);
  assert(b.pastas[0].nSources == HK_FONTES_PASTA);
  assert(b.pulPasta == (HK_GRUPOS_MAX + 2) * 5 - HK_PASTAS_MAX);
  // texto com aspas, barra e controle sobrevive a ida e volta
  assert(hk_ler("{\"name\":\"a\\\\b\\n\\\"c\\\"\",\"collections\":[{\"title\":\"T\\t1\",\"folders\":[{\"title\":\"Çá \\\"x\\\"\",\"sources\":[{\"provider\":\"trakt\",\"traktListId\":1}]}]}]}", &b, NULL) == HK_OK);
  { char *s1 = hk_serializar(&b); assert(s1 && hk_ler(s1, &a, NULL) == HK_OK);
    assert(!strcmp(a.pastas[0].title, b.pastas[0].title) && !strcmp(a.nome, b.nome)); free(s1); }
  printf("tetos: ok\n");
}

static void urls(void) {
  assert(hk_url_valida("https://exemplo.com/kit.json"));
  assert(hk_url_valida("http://192.168.1.5:8000/k.json?x=1"));
  assert(!hk_url_valida("ftp://x/y") && !hk_url_valida("https://") && !hk_url_valida("https:///x"));
  assert(!hk_url_valida("file:///etc/passwd") && !hk_url_valida("https://a b") && !hk_url_valida(NULL));
  printf("urls: ok\n");
}

// O exemplo publicado em docs/plans/kits-home.md tem de ser um kit valido.
static void exemplo(void) {
  static char j[16384];
  FILE *f = fopen("tests/fixtures/kit-home.json", "rb");
  size_t n;
  assert(f);
  n = fread(j, 1, sizeof j - 1, f);
  fclose(f);
  j[n] = 0;
  assert(hk_ler(j, &b, soCinemeta) == HK_OK);
  assert(b.nGrupos == 2 && b.nPastas == 5);
  assert(!b.pulAddon && !b.pulFonte && !b.pulPasta && !b.pulArte);
  assert(b.pastas[0].nSources == 2 && b.pastas[0].forma == COL_FORMA_PAISAGEM);
  printf("exemplo: ok\n");
}

int main(void) {
  exemplo();
  formato();
  idaEVolta();
  recusas();
  tetos();
  urls();
  printf("homekit: ok\n");
  return 0;
}
