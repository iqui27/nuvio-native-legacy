// plugins.c (F09) de ponta a ponta contra tests/plugins_server.py:
//   * desligado por padrao; ligado, manifesto -> codigo -> QuickJS -> Stream;
//   * ACK do sync: edicao marca pendente, confirmar so com o mesmo rev e
//     geracao, edicao durante o push mantem a pendencia, remover o ULTIMO
//     repositorio gera retrato vazio pendente, lista vazia da conta limpa;
//   * leitura da conta: nao-array nao e lista; pendencia local vence;
//   * troca de perfil no meio de uma consulta: geracao nova, consulta -1 e
//     nenhum aviso tardio; conta/perfil isolados no disco.
#include "plugins.h"
#include "pluginjs.h"
#include "addons.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static int falhas;
#define CONFERE(c, m) do { if (!(c)) { printf("FALHOU: %s\n", m); falhas++; } else printf("ok: %s\n", m); } while (0)

// ---- dublês do resto do app ------------------------------------------------
static char dirDados[512];
static int perfil = 1;
static char usuario[64] = "conta-a";
int perfis_ativo_addons(void) { return __atomic_load_n(&perfil, __ATOMIC_ACQUIRE); }
const char *sessao_usuario(void) { return usuario; }
const char *desc_chave_tmdb_reserva(void) { return ""; }
static OrigemExtra origemReg;
void addons_definir_origem_extra(OrigemExtra f, int (*a)(void)) { (void)a; origemReg = f; }
static pthread_mutex_t dm = PTHREAD_MUTEX_INITIALIZER;
static int gravar(const char *nome, const char *c) {
  char p[700]; FILE *f;
  snprintf(p, sizeof p, "%s/%s", dirDados, nome);
  pthread_mutex_lock(&dm);
  f = fopen(p, "wb");
  if (f) { fputs(c, f); fclose(f); }
  pthread_mutex_unlock(&dm);
  return f != NULL;
}
int dados_gravar(const char *nome, const char *c) { return gravar(nome, c); }
int dados_gravar_leve(const char *nome, const char *c) { return gravar(nome, c); }
int dados_apagar(const char *nome) { char p[700]; snprintf(p, sizeof p, "%s/%s", dirDados, nome); return unlink(p) == 0; }
char *dados_ler(const char *nome) {
  char p[700]; FILE *f; long n; char *b;
  snprintf(p, sizeof p, "%s/%s", dirDados, nome);
  pthread_mutex_lock(&dm);
  f = fopen(p, "rb");
  if (!f) { pthread_mutex_unlock(&dm); return NULL; }
  fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
  b = malloc((size_t)n + 1);
  if (b && fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); b = NULL; }
  if (b) b[n] = 0;
  fclose(f);
  pthread_mutex_unlock(&dm);
  return b;
}

static void dormir(int ms) { struct timespec t = { ms / 1000, (ms % 1000) * 1000000L }; nanosleep(&t, NULL); }
static unsigned long agoraMs(void) {
  struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
  return (unsigned long)ts.tv_sec * 1000UL + (unsigned long)ts.tv_nsec / 1000000UL;
}
static void esperarManifestos(void) {
  for (int k = 0; k < 100 && plugins_atualizando(); k++) dormir(50);
}

typedef struct { int avisos2, avisosTarde, ger; unsigned gerInicio; } Ouvinte;
static void ouvir(void *u, int k, const char *nome, const char *id, int estado, const void *f, int n) {
  Ouvinte *o = u; (void)k; (void)nome; (void)f; (void)n; (void)id;
  if (estado >= 2) { __atomic_add_fetch(&o->avisos2, 1, __ATOMIC_RELAXED);
    if (plugins_geracao() != o->gerInicio) __atomic_add_fetch(&o->avisosTarde, 1, __ATOMIC_RELAXED); }
}

typedef struct { const char *id, *tipo; Stream *l; int n; Ouvinte o; } Consulta;
static void *fioConsulta(void *u) {
  Consulta *c = u;
  c->o.gerInicio = plugins_geracao();
  c->n = plugins_consultar(c->id, c->tipo, NULL, NULL, ouvir, &c->o, &c->l);
  return NULL;
}

int main(int argc, char **argv) {
  char base[200], man[260];
  PlugRetrato s;
  PlugRepo conta[2];
  Stream *l = NULL;
  int n;
  if (argc < 3) { printf("uso: base dir\n"); return 2; }
  snprintf(base, sizeof base, "%s", argv[1]);
  snprintf(dirDados, sizeof dirDados, "%s", argv[2]);
  snprintf(man, sizeof man, "%s/manifest.json", base);

  plugins_iniciar();
  plugins_ligar_aos_addons();
  CONFERE(origemReg != NULL, "ligado como origem extra dos addons");
  CONFERE(!plugins_ligado(), "desligado por padrao");
  CONFERE(plugins_disponivel(), "motor + transporte disponiveis no host");
  CONFERE(plugins_consultar("tmdb:603", "movie", NULL, NULL, NULL, NULL, &l) == 0, "desligado: nada roda");

  // adicionar -> pendente com rev novo
  CONFERE(plugins_adicionar_repo(man) == 1, "repositorio adicionado");
  CONFERE(plugins_adicionar_repo(man) == -1, "duplicado recusado");
  CONFERE(plugins_adicionar_repo("file:///etc") == 0, "so http(s)");
  esperarManifestos();
  CONFERE(plugins_repo_scrapers(0, NULL) == 2 && plugins_repo_estado(0) == 1, "manifesto lido: 2 scrapers");
  plugins_retrato(&s);
  CONFERE(s.pendente && s.n == 1, "edicao fica pendente");
  // edicao durante o push: confirmar com o rev velho nao limpa
  plugins_alternar_repo(0); plugins_alternar_repo(0);
  CONFERE(!plugins_confirmar(s.rev, s.geracao) && plugins_pendente(), "ACK com rev velho mantem pendencia");
  plugins_retrato(&s);
  CONFERE(plugins_confirmar(s.rev, s.geracao) && !plugins_pendente(), "ACK com rev atual limpa");
  // pendencia local vence a leitura da conta
  plugins_alternar_repo(0); plugins_alternar_repo(0);
  plugins_retrato(&s);
  CONFERE(!plugins_definir_da_conta(NULL, 0, s.geracao) && plugins_n_repos() == 1, "conta nao passa por cima de pendencia");
  plugins_confirmar(s.rev, s.geracao);
  // resposta que nao e array nao e lista
  CONFERE(plugins_ler_conta("{\"message\":\"x\"}", conta, 2) == -1, "objeto de erro nao e lista");
  CONFERE(plugins_ler_conta("<html>", conta, 2) == -1, "HTML nao e lista");
  CONFERE(plugins_ler_conta(" [ ] ", conta, 2) == 0, "array vazio e lista vazia valida");
  CONFERE(plugins_ler_conta("[{\"url\":\"https://a/manifest.json\",\"name\":\"A\",\"enabled\":false}]", conta, 2) == 1 &&
          !conta[0].ativo, "linha da conta lida");

  // ligado: consulta de ponta a ponta (so o scraper rapido para filme+serie)
  plugins_definir_ligado(1);
  { unsigned long t0 = agoraMs();
    Consulta c = { "tmdb:603:1:2", "series", NULL, 0, {0} };
    fioConsulta(&c);
    CONFERE(c.n == 1 && c.l && strstr(c.l[0].provedor, "Bom"), "serie: 1 fonte do scraper Bom");
    CONFERE(c.n == 1 && strstr(c.l[0].descricao, "42 603 tv"), "argumentos e fetch do scraper");
    CONFERE(agoraMs() - t0 < 10000, "consulta termina");
    free(c.l); }

  // troca de perfil NO MEIO da consulta (o scraper Lento espera 4 s)
  { pthread_t t;
    Consulta c = { "tmdb:603", "movie", NULL, 0, {0} };
    unsigned long t0 = agoraMs();
    pthread_create(&t, NULL, fioConsulta, &c);
    dormir(700);
    __atomic_store_n(&perfil, 2, __ATOMIC_RELEASE);
    plugins_perfil_mudou();
    pthread_join(t, NULL);
    CONFERE(c.n == -1 && !c.l, "troca de perfil descarta a consulta em voo");
    CONFERE(agoraMs() - t0 < 2500, "troca de perfil interrompe em menos de 2 s");
    CONFERE(c.o.avisosTarde == 0, "nenhum aviso depois da troca");
    CONFERE(!plugins_ligado() && plugins_n_repos() == 0, "perfil 2 tem estado proprio (vazio, desligado)"); }
  // volta ao perfil 1: estado dele no disco
  __atomic_store_n(&perfil, 1, __ATOMIC_RELEASE);
  plugins_perfil_mudou();
  CONFERE(plugins_ligado() && plugins_n_repos() == 1, "perfil 1 relido do disco");

  // remover o ULTIMO repositorio: retrato vazio PENDENTE (sobe vazio)
  CONFERE(plugins_remover_repo(0) && plugins_n_repos() == 0, "ultimo repositorio removido");
  plugins_retrato(&s);
  CONFERE(s.pendente && s.n == 0, "retrato vazio pendente para o push");
  // troca de perfil entre o retrato e o ACK: confirmar nao vale
  __atomic_store_n(&perfil, 2, __ATOMIC_RELEASE); plugins_perfil_mudou();
  __atomic_store_n(&perfil, 1, __ATOMIC_RELEASE); plugins_perfil_mudou();
  CONFERE(!plugins_confirmar(s.rev, s.geracao) && plugins_pendente(), "ACK de geracao velha nao limpa (e sobrevive em disco)");
  plugins_retrato(&s);
  CONFERE(plugins_confirmar(s.rev, s.geracao) && !plugins_pendente(), "ACK do vazio limpa");
  // conta com 1 repositorio, depois conta com lista VAZIA: limpa
  memset(conta, 0, sizeof conta);
  snprintf(conta[0].url, sizeof conta[0].url, "%s", man); conta[0].ativo = 1;
  CONFERE(plugins_definir_da_conta(conta, 1, plugins_geracao()) && plugins_n_repos() == 1, "lista da conta aplicada");
  CONFERE(!plugins_definir_da_conta(conta, 1, plugins_geracao() - 1), "lista de geracao velha ignorada");
  CONFERE(plugins_definir_da_conta(conta, 0, plugins_geracao()) && plugins_n_repos() == 0, "lista vazia da conta limpa a local");
  // outra conta no mesmo aparelho nao herda nada; logout apaga o da conta
  plugins_adicionar_repo(man);
  snprintf(usuario, sizeof usuario, "conta-b");
  plugins_perfil_mudou();
  CONFERE(plugins_n_repos() == 0 && !plugins_ligado(), "conta B nao ve os repositorios da conta A");
  snprintf(usuario, sizeof usuario, "conta-a");
  plugins_perfil_mudou();
  CONFERE(plugins_n_repos() == 1, "conta A volta com os dela");
  plugins_esquecer();
  plugins_perfil_mudou();
  CONFERE(plugins_n_repos() == 0, "logout apagou o estado da conta");
  esperarManifestos();
  for (int k = 0; k < 60 && (pj_orcamento_rede_uso() || pj_orcamento_heap_uso()); k++) dormir(100);
  CONFERE(pj_orcamento_rede_uso() == 0 && pj_orcamento_heap_uso() == 0, "orcamento global devolvido");
  (void)n;
  printf(falhas ? "plugins_fontes: %d falha(s)\n" : "plugins_fontes: ok\n", falhas);
  return falhas != 0;
}
