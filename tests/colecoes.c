// Colecoes da conta no shape do web -> ColFolder, e a chave de fileira por id.
#include "../src/colecoes.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
const char *addons_base_por_id(const char *id) { return id && !strcmp(id, "org.x") ? "https://resolvido" : ""; }
int main(void) {
  const char *web =
    "[{\"collections_json\":{\"collections\":[{\"id\":\"c1\",\"title\":\"Streaming\",\"backdropImageUrl\":\"https://img/bg.jpg\","
    "\"folders\":[{\"id\":\"f1\",\"title\":\"Netflix\",\"coverImageUrl\":\"https://img/nf.jpg\",\"titleLogoUrl\":\"https://img/nf.png\",\"hideTitle\":true,"
    "\"sources\":[{\"provider\":\"addon\",\"addonId\":\"x\",\"addonBaseUrl\":\"https://addon/abc/manifest.json\",\"type\":\"movie\",\"catalogId\":\"nf_movies\",\"title\":\"Movies\",\"genre\":\"None\"},"
    "{\"provider\":\"tmdb\",\"tmdbSourceType\":\"DISCOVER\"},"
    "{\"addonBaseUrl\":\"https://addon/abc\",\"type\":\"series\",\"catalogId\":\"nf_series\",\"catalogName\":\"Series\"}]},"
    "{\"id\":\"f2\",\"title\":\"Vazia\",\"sources\":[]}]}]}}]";
  assert(col_definir_json(web) == 1);
  const ColFolder *f = col_folder(0);
  assert(f && !strcmp(f->group, "Streaming") && !strcmp(f->groupId, "c1") && !strcmp(f->id, "f1"));
  assert(!strcmp(f->hero, "https://img/bg.jpg") && !strcmp(f->cover, "https://img/nf.jpg") && f->hideTitle == 1);
  assert(f->nSources == 2);
  assert(!strcmp(f->sources[0].base, "https://addon/abc") && !strcmp(f->sources[0].catId, "nf_movies") && !f->sources[0].genre[0]);
  assert(!strcmp(f->sources[1].title, "Series") && !strcmp(f->sources[1].type, "series"));
  puts("ok  shape do web: linha da RPC, manifest.json cortado, tmdb fora, genre None vazio");

  char chave[192];
  col_chave_grupo("Streaming", chave, sizeof chave); assert(!strcmp(chave, "collection_c1"));
  col_chave_grupo("Outro", chave, sizeof chave);     assert(!strcmp(chave, "collection_Outro"));
  puts("ok  chave por id da colecao, nome quando nao ha id");

  // string escapada, como parseRemoteCollectionsPayload aceita
  const char *esc = "{\"collections_json\":\"{\\\"collections\\\":[{\\\"id\\\":\\\"c9\\\",\\\"title\\\":\\\"T\\\",\\\"folders\\\":[{\\\"id\\\":\\\"g\\\",\\\"title\\\":\\\"G\\\",\\\"sources\\\":[{\\\"addonBaseUrl\\\":\\\"https://a\\\",\\\"type\\\":\\\"movie\\\",\\\"catalogId\\\":\\\"k\\\"}]}]}]}\"}";
  assert(col_definir_json(esc) == 1 && !strcmp(col_folder(0)->groupId, "c9"));
  puts("ok  collections_json como string escapada");

  // vazio nao apaga
  assert(col_definir_json("{\"collections\":[]}") == 0 && col_n() == 1);
  puts("ok  vazio mantem o que havia");
  // fonte so com addonId (como a conta manda): entra, e a base resolve no acesso
  assert(col_definir_json("{\"collections\":[{\"id\":\"c\",\"title\":\"T\",\"folders\":[{\"id\":\"g\",\"title\":\"G\",\"sources\":[{\"provider\":\"addon\",\"addonId\":\"org.x\",\"type\":\"movie\",\"catalogId\":\"k\"}]}]}]}") == 1);
  assert(!strcmp(col_folder(0)->sources[0].base, "https://resolvido"));
  puts("ok  addonId sem URL resolve pela sonda");
  // a RPC real: collections_json e o array direto
  assert(col_definir_json("[{\"profile_id\":1,\"collections_json\":[{\"id\":\"r\",\"title\":\"R\",\"folders\":[{\"id\":\"g\",\"title\":\"G\",\"sources\":[{\"addonId\":\"a\",\"type\":\"movie\",\"catalogId\":\"k\"}]}]}],\"updated_at\":\"x\"}]") == 1);
  assert(!strcmp(col_folder(0)->groupId, "r"));
  puts("ok  linha da RPC com o array direto");
  // pacote + conta com o mesmo id: fica a arte local, grupo/titulo da conta
  { char dir[] = "/tmp/nuvio-col-XXXXXX"; char caminho[300]; FILE *f;
    assert(mkdtemp(dir));
    snprintf(caminho, sizeof caminho, "%s/collections.json", dir); f = fopen(caminho, "w");
    fputs("{\"groups\":[{\"id\":\"c1\",\"title\":\"Streaming\",\"folders\":[{\"id\":\"f1\",\"title\":\"Netflix\",\"cover\":\"collections/f1/cover.jpg\",\"hero\":\"collections/f1/hero.jpg\",\"frames\":12,\"sources\":[{\"title\":\"Movies\",\"base\":\"https://addon/abc\",\"type\":\"movie\",\"catId\":\"nf_movies\"}]}]}]}", f); fclose(f);
    assert(col_carregar(dir) == 1 && col_folder(0)->local && col_folder(0)->frames == 12);
    assert(col_definir_json("{\"collections\":[{\"id\":\"c1\",\"title\":\"Streaming Renomeado\",\"folders\":[{\"id\":\"f1\",\"title\":\"Netflix\",\"coverImageUrl\":\"https://cdn/nf.webp\",\"sources\":[{\"addonId\":\"x\",\"type\":\"movie\",\"catalogId\":\"nf_movies\"}]}]},{\"id\":\"c2\",\"title\":\"Nova\",\"folders\":[{\"id\":\"f9\",\"title\":\"Nova pasta\",\"coverImageUrl\":\"https://cdn/n.webp\",\"sources\":[{\"addonId\":\"x\",\"type\":\"movie\",\"catalogId\":\"k\"}]}]}]}") == 2);
    assert(strstr(col_folder(0)->hero, "/collections/f1/hero.jpg") && col_folder(0)->frames == 12 && col_folder(0)->local);
    assert(!strcmp(col_folder(0)->group, "Streaming Renomeado") && !strcmp(col_folder(0)->sources[0].base, "https://addon/abc"));
    assert(!strcmp(col_folder(1)->cover, "https://cdn/n.webp") && !col_folder(1)->local);
    puts("ok  pasta do pacote guarda arte e quadros; a conta da grupo, titulo e pastas novas"); }
  // OS QUATRO NIVEIS DE col_diagnostico. O que este teste guarda nao e a
  // funcao e sim a CAPACIDADE DE SEPARAR causas: os quatro casos abaixo
  // produzem hoje o mesmo sintoma na tela (fileira solta na home, #18) e o
  // nivel e a unica coisa que diz qual deles aconteceu. Se dois deles voltarem
  // a devolver o mesmo numero, o diagnostico volta a ser inutil e o relator
  // volta a mandar log que nao decide nada.
  { char g[64];
    assert(col_definir_json("{\"collections\":[{\"id\":\"c\",\"title\":\"Streaming\","
           "\"folders\":[{\"id\":\"g\",\"title\":\"G\",\"sources\":["
           "{\"addonBaseUrl\":\"https://x\",\"type\":\"movie\",\"catalogId\":\"top\"}]}]}]}") == 1);
    assert(col_diagnostico("https://outro", "movie", "top", g, sizeof g) == 0);
    assert(col_diagnostico("https://x", "series", "top", g, sizeof g) == 1);
    assert(col_diagnostico("https://x", "movie", "imdbRating", g, sizeof g) == 2);
    assert(col_diagnostico("https://x", "movie", "top", g, sizeof g) == 3);
    // O grupo sai junto: sem ele o nivel 3 diz "casou" e nao diz ONDE, e o
    // nivel 3 e exatamente o caso em que a pessoa precisa ir desocultar algo.
    assert(!strcmp(g, "Streaming"));
    // Base vazia nao casa com nada. Sem esta guarda uma fileira sem base
    // casaria com toda fonte ainda nao resolvida — o falso positivo que
    // col_por_catalogo ja evita, repetido aqui porque sao duas varreduras.
    assert(col_diagnostico("", "movie", "top", g, sizeof g) == 0);
    puts("ok  col_diagnostico separa os quatro motivos de nao engolir"); }

  puts("colecoes: tudo ok");
  return 0;
}
