/* Reuses the real discovery/catalogue/snapshot fixture, with controlled
 * response completion instead of external network timing. */
#define main montagem_cedo_main
#include "montagem_cedo.c"
#include "jellyfin_stub.inc"
#include "nvtmp.h"
#undef main

static unsigned long long inicio, primeiraPronta, primeiraPublicada, lentaAcabou;
static int modo, mudou;
static void conferirProntas(void) {
  CatItem c; CatFileira f;
  if (modo == 1) {
    assert(cat_n_fileiras() == 5);
    assert(cat_copiar_por_id("ttOlda", "movie", &c));
    assert(cat_copiar_por_id("ttOldb", "movie", &c));
    assert(cat_copiar_por_id("ttOldc", "movie", &c));
  } else if (modo == 2) {
    assert(cat_copiar_por_id("ttWReady", "movie", &c) && c.naLista);
  } else if (modo == 3) {
    assert(cat_copiar_fileira("col:ready", &c, 1, &f) == 1 && !f.base[0]);
    assert(!strcmp(c.imdb, "ttCollection"));
  }
}
static void observarMarco(const char *s) {
  if (!strcmp(s, "montar: inicio")) inicio = agoraMs();
  else if (!strcmp(s, "primeira fileira da rede na tela")) {
    CatItem c; CatFileira f;
    assert(cat_copiar_por_id("tta1", "movie", &c));
    assert(cat_copiar_fileira(AID "_movie_cat_a", &c, 1, &f) == 1);
    assert(!strcmp(f.catId, "cat_a") && !strcmp(c.imdb, "tta1"));
    if (!primeiraPronta) primeiraPronta = agoraMs();
    if (!primeiraPublicada) primeiraPublicada = agoraMs();
  } else if (!strcmp(s, "primeira fileira da rede pronta (publicacao pendente)")) {
    if (!primeiraPronta) primeiraPronta = agoraMs();
    conferirProntas();
  } else if (!strcmp(s, "continuar assistindo na tela")) {
    conferirProntas();
  } else if (!strcmp(s, "catalogo da rede publicado") && !primeiraPublicada) primeiraPublicada = agoraMs();
}
static void atrasar(const char *id) {
  if (modo == 4) {
    if (!strcmp(id, "cat_a")) usleep(60000);
    if (!strcmp(id, "cat_b") && !__sync_lock_test_and_set(&mudou, 1)) {
      usleep(10000);
      __atomic_store_n(&fixtureDono, "dono-new", __ATOMIC_RELEASE);
      desc_repetir();
    }
    return;
  }
  if (!strcmp(id, "cat_b")) {
    usleep(350000);
    __atomic_store_n(&lentaAcabou, agoraMs(), __ATOMIC_RELEASE);
  }
}
static void item(CatItem *c, const char *id, int lista) {
  memset(c, 0, sizeof *c);
  snprintf(c->imdb, sizeof c->imdb, "%s", id);
  snprintf(c->tipo, sizeof c->tipo, "movie");
  snprintf(c->titulo, sizeof c->titulo, "%s", id);
  snprintf(c->poster, sizeof c->poster, "p.jpg");
  c->naLista = lista;
}
int main(int argc, char **argv) {
  static CatItem seed[8]; static CatFileira f[8];
  int n = 2, nf = 2, k;
  const char *tmp = getenv("TMPDIR");
  assert(argc == 2); modo = atoi(argv[1]);
  snprintf(dirDados, sizeof dirDados, "%snuvio-home-progressiva-%d",
           nvt("/"), (int)getpid());
  assert(!mkdir(dirDados, 0700)); desc_tmdb(dirDados); homeestado_iniciar();
  fixtureCW = fixtureSocial = 1;
  item(&seed[0], "ttCW", 0); seed[0].progresso = 20;
  item(&seed[1], "ttSocial", 0);
  snprintf(f[0].chave, sizeof f[0].chave, "continue_watching"); f[0].ini = 0; f[0].n = 1;
  snprintf(f[1].chave, sizeof f[1].chave, "social_activity"); f[1].ini = 1; f[1].n = 1; f[1].socialGeracao = 1;
  if (modo == 1) { /* warm catalogue: old rows must never shrink */
    for (k = 0; k < 3; k++) {
      char id[32]; snprintf(id, sizeof id, "ttOld%c", 'a' + k);
      item(&seed[n], id, 0);
      snprintf(f[nf].chave, sizeof f[nf].chave, "%s_movie_cat_%c", AID, 'a' + k);
      snprintf(f[nf].catId, sizeof f[nf].catId, "cat_%c", 'a' + k);
      snprintf(f[nf].base, sizeof f[nf].base, "%s", BASE);
      f[nf].ini = n++; f[nf++].n = 1;
    }
  } else if (modo == 2) { /* Trakt list only, no row/base. Preserve early */
    n = 1; nf = 0;
    item(&seed[0], "ttWReady", 1); nListaWl = 1;
    snprintf(listaWl[0], sizeof listaWl[0], "ttWReady");
  } else if (modo == 3) { /* collection row with an empty base remains warm */
    item(&seed[n], "ttCollection", 0);
    snprintf(f[nf].chave, sizeof f[nf].chave, "col:ready");
    f[nf].ini = n++; f[nf++].n = 1;
  }
  cat_definir_tudo(seed, n, f, nf); homeestado_salvar(f, nf);
  fixtureMarco = observarMarco; fixtureCatalogo = atrasar;
  desc_iniciar(); esperarQuieto();
  assert(primeiraPronta && primeiraPublicada);
  printf("home_progressiva mode=%d: ready=%llu ms, visible=%llu ms\n", modo,
         primeiraPronta - inicio, primeiraPublicada - inicio);
  fflush(stdout);
  if (modo == 0) {
    assert(primeiraPublicada < __atomic_load_n(&lentaAcabou, __ATOMIC_ACQUIRE));
    assert(cat_n_fileiras() == 5); /* 3 catalogue quota + 2 fixed */
    assert(imdbNaTela("ttCW", NULL) == 1 && imdbNaTela("ttSocial", NULL) == 1);
  } else if (modo == 4) {
    assert(mudou && montagens == 2 && !strcmp(sessao_usuario(), "dono-new"));
    assert(cat_n_fileiras() == 5);
  } else {
    assert(primeiraPublicada >= __atomic_load_n(&lentaAcabou, __ATOMIC_ACQUIRE));
    if (modo == 2) assert(imdbNaTela("ttWReady", ehNaLista) == 1);
  }
  printf("home_progressiva mode=%d: ready=%llu ms, visible=%llu ms; PASS\n", modo,
         primeiraPronta - inicio, primeiraPublicada - inicio);
  return 0;
}
