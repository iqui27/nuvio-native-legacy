// TOCTOU da persistencia do backend URL (achado sol-f31r2, #742 volta 4).
//
// O revisor pausou o worker da descoberta ANTES de dados_gravar (por
// --wrap=dados_gravar) e mostrou que a gravacao atrasada chegava ao disco
// DEPOIS da gravacao do usuario: limpar ou trocar a URL nesse meio-tempo
// deixava nuvio-backend.txt com a URL descoberta velha, que voltava no
// arranque. rc 6 nos tres sanitizadores.
//
// Aqui o mesmo cenario roda contra o modulo com a serializacao nova: a
// gravacao do usuario vai num fio a parte (ela espera a do worker e grava por
// ultimo), e no fim o que esta no disco — e o que o arranque rele — tem de
// ser a escolha do usuario.
//
//   ./teste limpa     usuario limpa o campo    -> memoria=""   disco=""
//   ./teste troca     usuario grava outra URL  -> memoria=disco="https://novo.invalid"
//   ./teste reinicia  troca + nova descoberta  -> memoria=disco="https://novo-desc.invalid"
#include "nvbackend.h"
#include "dados.h"
#include "rede.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed = PTHREAD_COND_INITIALIZER;
static pthread_key_t old_worker;
static int arrived, released, finished;

static void checkpoint(void) {
  pthread_mutex_lock(&mutex);
  arrived = 1;
  pthread_cond_broadcast(&changed);
  while (!released) pthread_cond_wait(&changed, &mutex);
  pthread_mutex_unlock(&mutex);
}

static void wait_flag(int *flag) {
  pthread_mutex_lock(&mutex);
  while (!*flag) pthread_cond_wait(&changed, &mutex);
  pthread_mutex_unlock(&mutex);
}

static void worker_exit(void *ignored) {
  (void)ignored;
  pthread_mutex_lock(&mutex);
  finished = 1;
  pthread_cond_broadcast(&changed);
  pthread_mutex_unlock(&mutex);
}

char *rede_baixar_st(const char *url, int timeout, const char *const *headers, int *status) {
  (void)timeout;
  (void)headers;
  *status = 200;
  if (!strcmp(url, "https://novo.invalid/.well-known/nuvio"))
    return strdup("{\"backend_url\":\"https://novo-desc.invalid\"}");
  assert(!strcmp(url, "https://antigo.invalid/.well-known/nuvio"));
  assert(!pthread_setspecific(old_worker, &finished));
  return strdup("{\"backend_url\":\"https://antigo-desc.invalid\"}");
}

// O worker da descoberta para DENTRO de dados_gravar, antes da chamada real —
// a janela exata do achado: conferencia da geracao ja passou, gravacao ainda
// nao chegou ao disco. So a URL descoberta pausa; as gravacoes do usuario
// (e o apagar) passam direto.
int __real_dados_gravar(const char *name, const char *contents);
int __wrap_dados_gravar(const char *name, const char *contents) {
  if (!strcmp(contents ? contents : "", "https://antigo-desc.invalid")) checkpoint();
  return __real_dados_gravar(name, contents);
}

static int poll_state(void) {
  for (int attempt = 0; attempt < 5000; ++attempt) {
    int state = nvbackend_descobrir_estado();
    if (state != 1) return state;
    nanosleep(&(struct timespec){.tv_nsec = 1000000}, NULL);
  }
  abort();
}

static const char *modo;
static int tChamou, tFim, tOk;

static void *fio_usuario(void *ignored) {
  (void)ignored;
  int r;
  pthread_mutex_lock(&mutex);
  tChamou = 1;
  pthread_cond_broadcast(&changed);
  pthread_mutex_unlock(&mutex);
  // Com a serializacao esta chamada espera a gravacao do worker terminar e
  // grava por ultimo: o disco nunca fica com a URL descoberta atrasada.
  if (!strcmp(modo, "limpa")) r = nvbackend_definir_url("");
  else r = nvbackend_definir_url("https://novo.invalid");
  if (!strcmp(modo, "reinicia") && r == 1) {
    // Variante do probe: depois de trocar, uma descoberta NOVA termina com
    // sucesso — o fio destacado nao pode sobrescrever o resultado dela.
    assert(nvbackend_descobrir_iniciar("https://novo.invalid"));
    assert(poll_state() == 2);
  }
  pthread_mutex_lock(&mutex);
  tOk = r;
  tFim = 1;
  pthread_cond_broadcast(&changed);
  pthread_mutex_unlock(&mutex);
  return NULL;
}

int main(int argc, char **argv) {
  pthread_t t;
  assert(argc == 2);
  modo = argv[1];
  assert(!strcmp(modo, "limpa") || !strcmp(modo, "troca") || !strcmp(modo, "reinicia"));
  dados_iniciar(getenv("NUVIO_DADOS"));
  assert(!pthread_key_create(&old_worker, worker_exit));

  assert(nvbackend_definir_url("https://antigo.invalid"));
  assert(nvbackend_descobrir_iniciar("https://antigo.invalid"));
  wait_flag(&arrived);                    // worker parado dentro de dados_gravar
  assert(!pthread_create(&t, NULL, fio_usuario, NULL));
  wait_flag(&tChamou);                    // a gravacao do usuario foi iniciada
  nanosleep(&(struct timespec){.tv_nsec = 30000000}, NULL);  // tempo de travar
  pthread_mutex_lock(&mutex);
  released = 1;
  pthread_cond_broadcast(&changed);
  pthread_mutex_unlock(&mutex);
  assert(!pthread_join(t, NULL));
  wait_flag(&finished);                   // fio destacado terminou de vez
  int state = poll_state();
  nvbackend_descobrir_limpar();

  char memory[400];
  nv_rec_url_copiar(memory, sizeof memory);
  char *disk = dados_ler("nuvio-backend.txt");   // o que o arranque carrega
  const char *expected =
    !strcmp(modo, "limpa") ? "" :
    !strcmp(modo, "troca") ? "https://novo.invalid" : "https://novo-desc.invalid";
  printf("%s estado=%d tOk=%d memória=<%s> disco=<%s>\n",
         modo, state, tOk, memory, disk ? disk : "");
  int result = (!tOk || strcmp(memory, expected) || strcmp(disk ? disk : "", expected)) ? 6 : 0;
  free(disk);
  assert(!pthread_key_delete(old_worker));
  return result;
}
