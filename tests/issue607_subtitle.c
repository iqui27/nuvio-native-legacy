#define main subtitle_fixture_main
#include "addons_legendas.c"
#undef main

static void *replace(void *unused) {
  (void)unused;
  pthread_mutex_lock(&legTrava);
  snprintf(legs[0].url, sizeof legs[0].url, "new subtitle");
  pthread_mutex_unlock(&legTrava);
  const Legenda *other = addons_legenda(0);
  assert(other && !strcmp(other->url, "new subtitle"));
  return NULL;
}

int teste_merge_selecionada(const Legenda *l, int indice);

static void automatic_languages(void) {
  const char *languages[] = {"por", "eng"};
  memset(legs, 0, sizeof legs);
  nLegs = 2;
  snprintf(legId, sizeof legId, "tt123");
  for (int i = 0; i < nLegs; i++) {
    snprintf(legs[i].idioma, sizeof legs[i].idioma, "%s", languages[i]);
    snprintf(legs[i].url, sizeof legs[i].url, "https://fixture.invalid/%s.srt", languages[i]);
  }
  for (int i = 0; i < nLegs; i++) {
    ling_local_legenda(languages[i]);
    prazo_auto_iniciar();
    prazo_auto_avancar(1000);
    assert(teste_merge_selecionada(&legs[i], i));
  }
  puts("PASS: automatic selection preserves Portuguese and English addon languages");
}

int main(void) {
  pthread_t worker;
  nLegs = 1;
  snprintf(legs[0].url, sizeof legs[0].url, "original subtitle");
  const Legenda *copy = addons_legenda(0);
  assert(copy);
  assert(!pthread_create(&worker, NULL, replace, NULL));
  pthread_join(worker, NULL);
  assert(!strcmp(copy->url, "original subtitle"));
  assert(!addons_legenda(-1) && !addons_legenda(1));
  assert(!strcmp(addons_legenda(0)->url, "new subtitle"));
  puts("PASS: subtitle copy survives worker replacement and another thread's getter");
  automatic_languages();
  return 0;
}
