#include <assert.h>
#include <stdlib.h>
#include <string.h>
#define pthread_create test_thread_create
#define pthread_detach test_thread_detach
#define osp1_derivar test_derivar
#define osp1_prova_cliente test_prova_cliente
#define osp1_prova_servidor test_prova_servidor
#include "../src/openseek.c"
#undef pthread_create
#undef pthread_detach

static void *(*worker)(void *);
static void *worker_arg;
int test_thread_create(pthread_t *t, const pthread_attr_t *a, void *(*fn)(void *), void *arg) {
  (void)a; *t = pthread_self(); worker = fn; worker_arg = arg; return 0;
}
int test_thread_detach(pthread_t t) { (void)t; return 0; }
int test_derivar(const char *a,const char *b,const char *c,const char *d,long e,long f,unsigned char k[32],int (*g)(void *),void *h) {
  (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;(void)g;(void)h; memset(k,0,32); return 1;
}
void test_prova_cliente(const unsigned char *k,const char *a,const char *b,char out[65]) { (void)k;(void)a;(void)b; memset(out,'a',64); out[64]=0; }
void test_prova_servidor(const unsigned char *k,const char *a,const char *b,char out[65]) { test_prova_cliente(k,a,b,out); }
static int saved_token;
char *dados_ler(const char *name) { (void)name; return NULL; }
int dados_gravar(const char *name,const char *text) { (void)name; saved_token = strstr(text, "token=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa") != NULL; return 1; }
int dados_apagar(const char *name) { (void)name; saved_token = 0; return 1; }
int rede_pedir(const RedePedido *p, RedeResposta *r) {
  const char *start = "{\"pair_id\":\"00112233445566778899aabbccddeeff\",\"salt\":\"0f1e2d3c4b5a69788796a5b4c3d2e1f0\",\"spki_pin\":\"sha256//AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=\",\"iterations\":200000}";
  const char *claim = "{\"device_id\":\"0123456789abcdef\",\"token\":\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\",\"server_proof\":\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"}";
  r->status = 200; r->corpo = strdup(strstr(p->url,"/start") ? start : claim); return 1;
}
void rede_resposta_limpar(RedeResposta *r) { free(r->corpo); r->corpo = NULL; }
static void run_worker(void) { void *(*fn)(void *) = worker; void *arg = worker_arg; worker = NULL; fn(arg); }
int main(int argc, char **argv) {
  assert(argc == 2);
  assert(openseek_definir_endereco("https://a.example"));
  assert(openseek_parear("ABCDEFGH"));
  if (!strcmp(argv[1], "forget")) openseek_esquecer();
  else if (!strcmp(argv[1], "aba")) { assert(openseek_definir_endereco("https://b.example")); assert(openseek_definir_endereco("https://a.example")); }
  run_worker();
  if (!strcmp(argv[1], "normal")) { assert(openseek_pareado()); assert(saved_token); assert(openseek_status(NULL, 0) == OSU_PAREADO); }
  else { assert(!openseek_pareado()); assert(!saved_token); assert(openseek_status(NULL, 0) == OSU_NAO_PAREADO); }
  return 0;
}
