// HASH DAS COLECOES NO FIO DA MONTAGEM x SYNC NO LACO PRINCIPAL (#203).
//
// homeestado.c (fio `montar` da descoberta) fazia o hash das colecoes lendo
// col_folder(i)->sources[s] sem trava. Ao mesmo tempo o sync do laco
// principal chama col_definir_json (fecharMontagem: free do bloco de fontes)
// e col_esquecer_perfil (free tambem). O ponteiro de col_folder sobrevive
// (blocos nunca liberados), mas `sources` aponta para o bloco que acabou de ser
// liberado: leitura de memoria liberada.
//
// Agora o hash e col_hash_estrutura(), calculado dentro de colecoes.c sob
// colTrava. Este teste roda 4 fios fazendo o hash em laco enquanto o fio
// principal troca as colecoes; cada hash visto tem de ser um dos tres
// conjuntos validos (A, B, vazio), e o ASAN nao pode acusar nada.
// -DANTIGO usa o hash do jeito velho (col_folder + sources sem trava): com
// ASAN aborta em heap-use-after-free.
#include "../src/colecoes.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *addons_base_por_id(const char *id) { return id && !strcmp(id, "org.x") ? "https://resolvido" : ""; }

// Copia fiel do hashColecoes de homeestado.c em 0fcfa601.
static unsigned hashBytes(unsigned h, const void *data, size_t n) {
  const unsigned char *p = (const unsigned char *)data;
  while (n--) h = (h ^ *p++) * 16777619u;
  return h;
}
static unsigned hashTexto(unsigned h, const char *s) {
  if (s) h = hashBytes(h, s, strlen(s));
  return (h ^ 0xffu) * 16777619u;
}
static unsigned hashVelho(void) {
  unsigned h = 2166136261u;
  int i;
  h = hashBytes(h, &(int){col_n()}, sizeof(int));
  for (i = 0; i < col_n(); i++) {
    const ColFolder *f = col_folder(i);
    int s;
    if (!f) continue;
    h = hashTexto(h, f->group);
    h = hashTexto(h, f->id);
    h = hashBytes(h, &f->nSources, sizeof f->nSources);
    for (s = 0; s < f->nSources; s++) {
      const ColSource *src = &f->sources[s];
      h = hashTexto(h, src->prov); h = hashTexto(h, src->addonId);
      h = hashTexto(h, src->base); h = hashTexto(h, src->type);
      h = hashTexto(h, src->catId); h = hashTexto(h, src->tmdbTipo);
      h = hashBytes(h, &src->tmdbId, sizeof src->tmdbId);
      h = hashTexto(h, src->midia); h = hashTexto(h, src->ordenar);
      h = hashTexto(h, src->ordem); h = hashBytes(h, &src->traktLista, sizeof src->traktLista);
    }
  }
  return h;
}
#ifdef ANTIGO
static unsigned hashAgora(void) { return hashVelho(); }
#else
static unsigned hashAgora(void) { return col_hash_estrutura(); }
#endif

static char *json(int pastas, int fontes, const char *tag) {
  size_t cap = 4096 + (size_t)pastas * fontes * 160;
  char *b = malloc(cap);
  size_t n = 0;
  n += snprintf(b + n, cap - n, "{\"collections\":[{\"id\":\"g%s\",\"title\":\"G\",\"folders\":[", tag);
  for (int i = 0; i < pastas; i++) {
    n += snprintf(b + n, cap - n, "%s{\"id\":\"%s%d\",\"title\":\"P%d\",\"sources\":[", i ? "," : "", tag, i, i);
    for (int s = 0; s < fontes; s++)
      n += snprintf(b + n, cap - n, "%s{\"addonId\":\"org.x\",\"type\":\"movie\",\"catalogId\":\"%s_%d_%d\"}",
                    s ? "," : "", tag, i, s);
    n += snprintf(b + n, cap - n, "]}");
  }
  snprintf(b + n, cap - n, "]}]}");
  return b;
}

static int parar, ruins, vistos;
static unsigned hA, hB, hE;
static void *leitor(void *u) {
  (void)u;
  while (!__atomic_load_n(&parar, __ATOMIC_SEQ_CST)) {
    unsigned h = hashAgora();
    if (h != hA && h != hB && h != hE) __atomic_add_fetch(&ruins, 1, __ATOMIC_RELAXED);
    __atomic_add_fetch(&vistos, 1, __ATOMIC_RELAXED);
  }
  return NULL;
}

int main(void) {
  char *a = json(40, 6, "a"), *b = json(70, 9, "b");
  pthread_t t[4];
  int i, falhas = 0;
  setvbuf(stdout, NULL, _IOLBF, 0);
  // Os tres estados validos, medidos sem concorrencia.
  if (col_definir_json(a) <= 0) { printf("json A recusado\n"); return 2; }
  hA = hashAgora();
  if (col_definir_json(b) <= 0) { printf("json B recusado\n"); return 2; }
  hB = hashAgora();
  col_esquecer_perfil();
  hE = hashAgora();
  col_definir_json(a);
  if (hashAgora() != hA) { printf("hash instavel sem concorrencia\n"); return 2; }
  // O valor tem de ser o MESMO do hash antigo: senao todo snapshot da Home ja
  // gravado na TV seria descartado uma vez na atualizacao.
  if (hashVelho() != hA) { printf("  col_hash_estrutura != hash antigo: FALHOU\n"); falhas++; }
  else printf("  mesmo valor do hash antigo (sem concorrencia): ok\n");
  for (i = 0; i < 4; i++) pthread_create(&t[i], NULL, leitor, NULL);
  for (i = 0; i < 400; i++) {
    col_definir_json(i % 2 ? a : b);
    if (i % 7 == 0) col_esquecer_perfil();
  }
  __atomic_store_n(&parar, 1, __ATOMIC_SEQ_CST);
  for (i = 0; i < 4; i++) pthread_join(t[i], NULL);
  printf("  hashes vistos=%d fora dos tres estados=%d\n", vistos, ruins);
  if (ruins) { printf("  hash de um conjunto pela metade: FALHOU\n"); falhas++; }
  free(a); free(b);
  printf("colhash_corrida: %s\n", falhas ? "FALHOU" : "ok");
  return falhas ? 1 : 0;
}
