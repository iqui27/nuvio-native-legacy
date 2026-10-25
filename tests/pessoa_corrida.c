// #205: the latest cast selection wins while the previous fetch is blocked.
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#define NV_PERFIL_SESSAO_FORTE
const char *desc_chave_tmdb(void) { return "k"; }
const char *desc_tmdb_idioma(void) { return "pt-BR"; }
int ajustes_tmdb_elenco(void) { return 1; }
static pthread_mutex_t portao = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t sinal = PTHREAD_COND_INITIALIZER;
static int aberto, entrou, falhar, cenario, perfis;

char *rede_baixar(const char *u, int t) {
  (void)t;
  if (cenario) {
    if (strstr(u, "/find/"))
      return cenario == 1 ? NULL : strdup("{\"movie_results\":[{\"id\":42}]}");
    if (strstr(u, "/movie/42/credits"))
      return cenario == 2 ? NULL : strdup("{\"cast\":[{\"id\":8,\"name\":\"Ator B\"}]}");
    if (strstr(u, "/search/person"))
      return strdup("{\"results\":[{\"id\":99,\"name\":\"Ator B\"},{\"id\":8,\"name\":\"ATOR B\"}]}");
    if (strstr(u, "/person/")) {
      perfis++;
      return strdup("{\"name\":\"Ator B\",\"biography\":\"Bio\",\"combined_credits\":{\"cast\":[{\"id\":103,\"media_type\":\"movie\",\"title\":\"Wrong film\"}]}}");
    }
    return NULL;
  }
  if (strstr(u, "/person/7")) {
    pthread_mutex_lock(&portao);
    entrou = 1;
    pthread_cond_signal(&sinal);
    while (!aberto) pthread_cond_wait(&sinal, &portao);
    pthread_mutex_unlock(&portao);
    return strdup("{\"name\":\"Ator A\",\"combined_credits\":{\"cast\":[{\"id\":101,\"media_type\":\"movie\",\"title\":\"Filme A\"}]}}");
  }
  if (strstr(u, "/search/person") && strstr(u, "Ator+B"))
    return strdup("{\"results\":[{\"id\":8,\"name\":\"Ator B\"}]}");
  if (strstr(u, "/person/8") && !falhar)
    return strdup("{\"name\":\"Ator B\",\"combined_credits\":{\"cast\":[{\"id\":102,\"media_type\":\"movie\",\"title\":\"Filme B\"}]}}");
  return NULL;
}
char *rede_baixar_com(const char *u, int t, const char *const *c) {
  (void)c; return rede_baixar(u, t);
}
#include "../src/pessoa.c"

static void esperar(void) {
  for (int i = 0; i < 2000; i++) {
    pthread_mutex_lock(&trava);
    int vivo = fioVivo;
    pthread_mutex_unlock(&trava);
    if (!vivo) return;
    usleep(1000);
  }
  assert(!"worker timed out");
}

int main(void) {
  pessoa_pedir(7, "Ator A", "a.jpg");
  pthread_mutex_lock(&portao);
  while (!entrou) pthread_cond_wait(&sinal, &portao);
  pthread_mutex_unlock(&portao);
  pessoa_pedir(9, "Skipped", "skip.jpg");
  pessoa_pedir(0, "Ator B", "b.jpg");
  pthread_mutex_lock(&trava);
  assert(!strcmp(nome, "Ator B"));
  assert(!strcmp(foto, "b.jpg") && nCred == 0);
  pthread_mutex_unlock(&trava);
  pthread_mutex_lock(&portao);
  aberto = 1;
  pthread_cond_signal(&sinal);
  pthread_mutex_unlock(&portao);
  esperar();
  assert(pessoa_pronta() && !strcmp(pessoa_nome(), "Ator B"));
  assert(pessoa_n_creditos() == 1);
  assert(!strcmp(pessoa_credito_titulo(0), "Filme B"));
  assert(!strcmp(pessoa_foto(), "b.jpg"));
  puts("ok #205: latest actor replaces blocked actor and owns filmography");
  pessoa_pedir(0, "Unknown", "unknown.jpg");
  esperar();
  assert(pessoa_falhou() && !pessoa_pronta() && pessoa_n_creditos() == 0);
  falhar = 1;
  pessoa_pedir(8, "Ator B", "b.jpg");
  esperar();
  assert(pessoa_falhou());
  falhar = 0;
  pessoa_pedir(8, "Ator B", "b.jpg");
  esperar();
  assert(!pessoa_falhou() && pessoa_pronta() && pessoa_n_creditos() == 1);
  puts("ok #205: unresolved names and network failures allow retry");
  pessoa_contexto_titulo("tt0000042", "movie");
  for (cenario = 1; cenario <= 2; cenario++) {
    pessoa_pedir(0, "Ator B", "b.jpg");
    esperar();
    assert(pessoa_falhou() && !pessoa_pronta() && pessoa_n_creditos() == 0);
    assert(perfis == 0);
  }
  puts("ok #205: ambiguous missing-ID cast rejected after title lookup or credits failure");
  cenario = 3;
  pessoa_pedir(0, "Ator B", "b.jpg");
  esperar();

  assert(pessoa_pronta() && !pessoa_falhou() && idPedido == 8 && perfis == 1);
  puts("ok #205: title credits confirm cast identity despite ambiguous name search");
  return 0;
}

const char *i18n(const char *s) { return s; }
