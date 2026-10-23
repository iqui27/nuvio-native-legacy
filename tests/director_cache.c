// Sem rede real: confirma que a busca nao aceita homonimo de ator e publica
// a ficha somente depois de validar o nome e o departamento no TMDB.
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../src/diretor.c"

const char *desc_chave_tmdb(void) { return "test-key"; }
const char *desc_tmdb_idioma(void) { return "pt-BR"; }
const char *i18n(const char *s) { return s; }
void desc_data_extenso(const char *iso, char *dst, size_t tam) {
  snprintf(dst, tam, "%s", iso);
}

char *rede_baixar(const char *url, int segundos) {
  (void)segundos;
  if (strstr(url, "/search/person?")) {
    const char *json =
      "{\"results\":["
      "{\"id\":11,\"name\":\"Alex Silva\",\"known_for_department\":\"Acting\",\"profile_path\":\"/wrong.jpg\"},"
      "{\"id\":22,\"name\":\"Alex Silva\",\"known_for_department\":\"Directing\",\"profile_path\":\"/right.jpg\",\"known_for\":[{\"title\":\"Filme\",\"backdrop_path\":\"/hero.jpg\"}]}]}";
    return strdup(json);
  }
  if (strstr(url, "/person/22?")) {
    return strdup("{\"known_for_department\":\"Directing\",\"biography\":\"Uma ficha real.\",\"birthday\":\"1970-01-01\",\"place_of_birth\":\"Brasil\"}");
  }
  return NULL;
}

static int esperar_ficha(const char *nome) {
  for (int i = 0; i < 100; i++) {
    if (diretor_pronto(nome)) return 1;
    usleep(10000);
  }
  return 0;
}

int main(void) {
  diretor_pedir("alex silva");
  assert(esperar_ficha("Alex Silva"));
  assert(strstr(diretor_foto("Alex Silva"), "/right.jpg"));
  assert(strcmp(diretor_foto("Alex Silva"), "https://image.tmdb.org/t/p/w500/right.jpg") == 0);
  assert(strcmp(diretor_hero("Alex Silva"), "https://image.tmdb.org/t/p/w1280/hero.jpg") == 0);
  assert(strstr(diretor_bio("Alex Silva"), "ficha real"));
  puts("director cache: PASS (identity, department, published ficha)");
  return 0;
}
