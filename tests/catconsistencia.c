// Identidade do historico, respostas tardias e mutacoes do vetor publicado.
#include "catalogo.h"
#include "progresso.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

int ajustes_idioma_ingles(void) { return 0; }
int ajustes_idioma(void) { return 0; }
const char *i18n(const char *s) { return s; }
const char *idioma_mes_data(int m, const char *s) { (void)m; return s; }
const char *dados_dir(void) { return ""; }
const char *sessao_usuario(void) { return ""; }
int perfis_ativo(void) { return 1; }
const char *desc_genero_pt(const char *s) { return s; }
int prog_ler(ProgRegistro *v, int n) { (void)v; (void)n; return 0; }
int prog_gravar_local(const char *id, int t, int e, double p, double d, int tocou) {
  (void)id; (void)t; (void)e; (void)p; (void)d; (void)tocou; return 0;
}

static int gancho;
static CatItem lote[40];
static void publicar(int qtd, const char *id) {
  memset(lote, 0, sizeof lote);
  for (int i = 0; i < qtd; i++) {
    snprintf(lote[i].imdb, sizeof lote[i].imdb, "%s", id);
    snprintf(lote[i].tipo, sizeof lote[i].tipo, "movie");
  }
  cat_definir(lote, qtd);
}
void nv_cat_teste_antes_trava(void) {
  if (gancho) { gancho = 0; publicar(1, "ttnovo"); cat_quadro(); }
}

static void *mutar(void *p) {
  (void)p;
  CatItem copia = {0};
  snprintf(copia.imdb, sizeof copia.imdb, "ttteste");
  snprintf(copia.tipo, sizeof copia.tipo, "movie");
  for (int i = 0; i < 1500; i++) {
    cat_definir_na_lista(30, i & 1);
    cat_definir_na_lista_imdb("ttteste", i & 1);
    cat_imdb_na_lista("ttteste");
    cat_atualizar_item(30, &copia);
  }
  return NULL;
}
static void *historicoEscrever(void *p) {
  long fio = (long)p;
  char id[32];
  unsigned long long g = cat_historico_geracao();
  for (int i = 0; i < 800; i++) {
    snprintf(id, sizeof id, "tt%07ld", fio * 800 + i);
    assert(cat_historico_definir_se_geracao(id, "movie", 1, g));
    assert(cat_historico_estado_id(id, "movie") == 1);
  }
  return NULL;
}
int main(void) {
  CatItem filme = {0}, serie = {0}, edit;
  pthread_t fios[8];
  snprintf(filme.imdb, sizeof filme.imdb, "tt1234567");
  snprintf(filme.tipo, sizeof filme.tipo, "movie");
  filme.progresso = 90; serie = filme;
  snprintf(serie.tipo, sizeof serie.tipo, "series");
  cat_historico_contexto("conta A", 1);
  unsigned long long a = cat_historico_geracao();
  assert(cat_visto(&filme) && !cat_visto(&serie));
  cat_historico_definir_id(filme.imdb, "movie", 0);
  assert(!cat_visto(&filme));
  cat_historico_definir_id(filme.imdb, "series", 1);
  assert(cat_visto(&serie));
  cat_historico_contexto("conta A", 1);
  assert(a == cat_historico_geracao() && !cat_visto(&filme) && cat_visto(&serie));
  // Ausencia/remocao Trakt nao varre registros que vieram de outra fonte.
  cat_historico_definir_id("tt7654321", "movie", 1);
  cat_historico_definir_id(filme.imdb, "movie", 0);
  assert(cat_historico_estado_id("tt7654321", "movie") == 1);
  cat_historico_contexto("conta A", 2);
  assert(cat_historico_estado_id(filme.imdb, "movie") == -1);
  assert(!cat_historico_definir_se_geracao(filme.imdb, "movie", 1, a));
  unsigned long long perfil2 = cat_historico_geracao();
  cat_historico_definir_id(filme.imdb, "movie", 1);
  cat_historico_contexto("conta B", 2);
  assert(cat_historico_estado_id(filme.imdb, "movie") == -1);
  assert(!cat_historico_definir_se_geracao(filme.imdb, "movie", 1, perfil2));
  unsigned long long b = cat_historico_geracao();
  cat_historico_contexto("", 0); // logout
  assert(!cat_historico_definir_se_geracao(filme.imdb, "movie", 1, b));
  cat_historico_contexto("conta A", 1); // A->B->A nao ressuscita resposta A
  assert(!cat_historico_definir_se_geracao(filme.imdb, "movie", 1, a));
  assert(cat_visto(&filme) && !cat_visto(&serie));

  for (long i = 0; i < 8; i++) assert(!pthread_create(&fios[i], NULL, historicoEscrever, (void *)i));
  for (int i = 0; i < 8; i++) pthread_join(fios[i], NULL);
  for (int i = 0; i < 6400; i++) {
    char id[32]; snprintf(id, sizeof id, "tt%07d", i);
    assert(cat_historico_estado_id(id, "movie") == 1);
  }
  puts("ok historico: conta, perfil, logout, A-B-A, provas locais, fallback e 6400 insercoes concorrentes");

  publicar(40, "ttvelho"); edit = *cat_item(30);
  snprintf(edit.titulo, sizeof edit.titulo, "resposta antiga");
  gancho = 1; cat_definir_na_lista(30, 1); // bloco encolheu antes da trava
  assert(!cat_item(0)->naLista);
  publicar(40, "ttvelho"); gancho = 1;
  assert(cat_definir_na_lista_imdb("ttvelho", 1) == 0);
  publicar(40, "ttvelho"); cat_definir_na_lista(0, 1); gancho = 1;
  assert(!cat_imdb_na_lista("ttvelho"));
  publicar(40, "ttvelho"); gancho = 1; cat_atualizar_item(30, &edit);
  assert(!strcmp(cat_item(0)->imdb, "ttnovo"));
  publicar(40, "ttnovo"); cat_atualizar_item(30, &edit); // mesmo indice, outro id
  assert(!strcmp(cat_item(30)->imdb, "ttnovo") && !cat_item(30)->titulo[0]);
  edit = *cat_item(30); snprintf(edit.titulo, sizeof edit.titulo, "resposta atual");
  cat_atualizar_item(30, &edit); assert(!strcmp(cat_item(30)->titulo, "resposta atual"));
  publicar(40, "ttteste");
  for (int i = 0; i < 3; i++) assert(!pthread_create(&fios[i], NULL, mutar, NULL));
  for (int i = 0; i < 250; i++) { publicar(i & 1 ? 4 : 40, "ttteste"); cat_quadro(); }
  for (int i = 0; i < 3; i++) pthread_join(fios[i], NULL);
  puts("catconsistencia: PASS (mutacao segura, indice antigo rejeitado, publicacao concorrente)");
  return 0;
}
