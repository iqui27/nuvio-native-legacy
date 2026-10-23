// #412: a publicacao de um lote ASS nao pode segurar o passo do quadro.
#include "../src/mkvass.c"
#include <assert.h>
#include <stdatomic.h>
static atomic_int publicando;
int legenda_ligada_em(unsigned g) { return g == 7; }
unsigned legenda_geracao(void) { return 7; }
unsigned legenda_definir_corpo_se(const char *c, unsigned g) { (void)c; return g; }
void assrender_limpar_fontes(void) {}
int assrender_adicionar_fonte(const char *n, const void *d, size_t t) { (void)n; (void)d; (void)t; return 1; }
int legenda_atualizar_corpo_se(const char *c, unsigned g) {
  (void)c; atomic_store(&publicando, 1); usleep(200000); return g == 7;
}
static void *entregarTeste(void *arg) { assert(entregarCorpoSeAtual(arg, "ASS")); return NULL; }
int main(void) {
  Fio f = {0}; pthread_t fio;
  S.geracao = f.g = 1; f.legG = 7; f.entregas = 1; S.estado = MKVASS_COLHENDO;
  assert(!pthread_create(&fio, NULL, entregarTeste, &f));
  while (!atomic_load(&publicando)) usleep(1000);
  long inicio = agoraMs();
  mkvass_passo(123.0); mkvass_folga(5.0); assert(mkvass_estado() == MKVASS_COLHENDO);
  long ms = agoraMs() - inicio;
  pthread_join(fio, NULL);
  printf("quadro durante publicacao ASS: %ld ms\n", ms);
  assert(ms < 50 && S.pos == 123.0 && f.entregas == 2);
  S.geracao++;
  assert(!entregarCorpoSeAtual(&f, "obsoleto"));
  puts("ok: quadro livre e geracao antiga descartada");
}
