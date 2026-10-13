// P2 da rodada 2: carga silenciosa, conclusao duravel e ordem dos instantes.
#include "catalogo.h"
#include "conquistasdados.h"
#include "progresso.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static long long agora = 1000;
static int perfil = 1;
static const char *conta = "regressao";
static long long relogio(void) { return agora; }
int nv_test_clock_gettime(clockid_t id, struct timespec *ts) {
  (void)id; ts->tv_sec = agora / 1000; ts->tv_nsec = (agora % 1000) * 1000000;
  return 0;
}
int perfis_ativo(void) { return perfil; }
const char *sessao_usuario(void) { return conta; }
int ajustes_conquistas(void) { return 1; }
int ajustes_idioma_ingles(void) { return 0; }
int ajustes_idioma(void) { return 0; }
const char *i18n(const char *s) { return s; }
const char *idioma_mes_data(int m, const char *s) { (void)m; return s; }
const char *desc_genero_pt(const char *s) { return s; }
const char *dados_dir(void) { return "."; }
char *dados_ler(const char *nome) {
  FILE *f = fopen(nome, "rb");
  if (!f) return NULL;
  assert(!fseek(f, 0, SEEK_END));
  long n = ftell(f); assert(n >= 0); rewind(f);
  char *s = calloc((size_t)n + 1, 1); assert(s);
  assert(fread(s, 1, (size_t)n, f) == (size_t)n); fclose(f);
  return s;
}
int dados_gravar(const char *nome, const char *txt) {
  FILE *f = fopen(nome, "wb"); assert(f);
  assert(fputs(txt, f) >= 0); return fclose(f) == 0;
}
int dados_apagar(const char *nome) { return remove(nome) == 0; }

static CqEstado estado(void) {
  CqEstado s; cqd_carregar(&s); return s;
}
static int vistos(void) {
  CqEstado s = estado();
  const CqEntrada *e = cqregra_buscar(&s, "trilha:oscar-melhor-filme");
  return e ? e->vistos : 0;
}

static void carga_silenciosa(void) {
  cqd_atualizar_local(); // primeira abertura antes de chegar o historico
  cat_historico_definir_id("tt0018578", "movie", 1);
  cqd_atualizar_local();
  assert(vistos() == 1);
  cat_historico_contexto(conta, perfil); // mapa descartado: conclusao importada fica
  cqd_atualizar_local(); assert(vistos() == 1);
  agora = 2000;
  cqd_concluir_local("tt9999999"); // outro filme nao libera premio retroativo
  assert(prog_gravar_local("tt9999999", 0, 0, 1000, 1000, 1));
  cqd_atualizar_local();
  CqEstado s = estado();
  assert(cqregra_pendentes(&s) == 0);
  // Uma conclusao nova ainda avisa, inclusive antes da primeira abertura.
  perfil = 2; cat_historico_contexto(conta, perfil);
  cqd_concluir_local("tt0018578");
  assert(prog_gravar_local("tt0018578", 0, 0, 1000, 1000, 1));
  s = estado(); assert(vistos() == 1 && cqregra_pendentes(&s) == 1);
  cqd_atualizar_local(); s = estado();
  assert(cqregra_pendentes(&s) == 1);
}

static void reassistida_parcial(void) {
  assert(prog_gravar_local("tt0018578", 0, 0, 1000, 1000, 1));
  cqd_atualizar_local(); assert(vistos() == 1);
  agora = 2000;
  assert(prog_gravar_local("tt0018578", 0, 0, 20, 1000, 1));
  prog_invalidar(); // releitura do disco, sem depender da sessao anterior
  cqd_atualizar_local(); assert(vistos() == 1);
  // Outro titulo parcial nao herda a conclusao, e seek nao a fabrica.
  assert(prog_gravar_local("tt0019729", 0, 0, 20, 1000, 1));
  assert(prog_gravar_local("tt0020629", 0, 0, 1000, 1000, 0));
  cqd_atualizar_local(); assert(vistos() == 1);
  perfil = 2; cqd_atualizar_local(); assert(vistos() == 0);
  perfil = 1; conta = "outra";
  cqd_atualizar_local(); assert(vistos() == 0);
}

static void conclusao_mais_nova(void) {
  cat_historico_definir_id("tt0018578", "movie", 0);
  agora = 2000;
  assert(prog_gravar_local("tt0018578", 0, 0, 1000, 1000, 1));
  cqd_atualizar_local(); assert(vistos() == 1);
  // Reaplicar a mesma marca nao renova seu instante.
  agora = 3000;
  cat_historico_definir_id("tt0018578", "movie", 0);
  cat_historico_definir_tempo("tt0018578", "movie", 0, 1000);
  cqd_atualizar_local(); assert(vistos() == 1);
  // Desmarcacao realmente nova continua ganhando.
  cat_historico_definir_tempo("tt0018578", "movie", 0, 3000);
  cqd_atualizar_local(); assert(vistos() == 0);
  // A resposta antiga nao vence uma marca mais nova; empate desmarca.
  cat_historico_definir_tempo("tt0018578", "movie", 1, 1000);
  cqd_atualizar_local(); assert(vistos() == 0);
  cat_historico_definir_tempo("tt0018578", "movie", 0, 4000);
  agora = 4000; cqd_concluir_local("tt0018578");
  cqd_atualizar_local(); assert(vistos() == 0);
  agora = 5000; cqd_concluir_local("tt0018578");
  cqd_atualizar_local(); assert(vistos() == 1);
}

int main(int argc, char **argv) {
  assert(argc == 2);
  prog_definir_relogio(relogio);
  switch (atoi(argv[1])) {
    case 1: carga_silenciosa(); break;
    case 2: reassistida_parcial(); break;
    case 3: conclusao_mais_nova(); break;
    default: return 2;
  }
  printf("PASS: conquistas_regressao P2-%s\n", argv[1]);
}
