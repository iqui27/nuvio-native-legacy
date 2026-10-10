// O APP SOBREVIVE AO SERVIDOR DA CONTA FORA DO AR (#215).
//
// MEDIDO em 02/10/2026: api.nuvio.tv em 504 (depois 502) por horas. O log da
// TCL: "[sync] leitura de addons: HTTP 504", colecoes/biblioteca/vistos 504,
// "ordem de catalogos: HTTP 429", "[desc] 0 catalogos declarados pelos addons"
// — home vazia. Este teste roda o sync.c REAL (fio, sync_iniciar/sync_passo/
// sync_periodico) contra um servidor dublado que responde 200, 504, 429 ou
// nada, com o disco dublado numa pasta de verdade (contacache.c de verdade):
//
//   1. sem copia: nenhum addon inventado, o estado diz "servidor fora", e o
//      ciclo para de perguntar depois dos addons;
//   2. servidor bom: a resposta vira copia em disco;
//   3. servidor fora (504 / 429 / sem resposta): os addons da COPIA chegam a
//      addons_definir_lista, e colecoes/biblioteca/vistos tambem saem dela;
//   4. o servidor volta: a volta seguinte sai em SYNC_RETENTA_MS e o estado
//      volta a "respondeu";
//   5. a copia e da CONTA: outra conta nao a usa; logout apaga.
//
// Cada sessao e um processo (as estaticas de sync.c nao voltam a zero).
#include "sync.h"
#include "addons.h"
#include "catordem.h"
#include "contacache.h"
#include "traktauth.h"
#include "perfis.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// ------------------------------------------------------------ disco dublado

static const char *pasta(void) {
  const char *d = getenv("NV_T_DIR");
  return d && *d ? d : "/tmp";
}
static void caminho(char *dst, size_t tam, const char *nome) {
  snprintf(dst, tam, "%s/%s", pasta(), nome);
}
const char *dados_dir(void) { return pasta(); }
void dados_uuid(char *dst, unsigned tam) {
  static unsigned seq;
  snprintf(dst, tam, "%08x-0000-4000-8000-%012x", (unsigned)getpid(), ++seq);
}
char *dados_ler(const char *nome) {
  char c[600];
  FILE *f;
  long n;
  char *b;
  caminho(c, sizeof c, nome);
  f = fopen(c, "rb");
  if (!f) return NULL;
  fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
  b = malloc((size_t)n + 1);
  n = (long)fread(b, 1, (size_t)n, f);
  b[n] = 0;
  fclose(f);
  return b;
}
int dados_gravar(const char *nome, const char *conteudo) {
  char c[600];
  FILE *f;
  caminho(c, sizeof c, nome);
  f = fopen(c, "wb");
  if (!f) return 0;
  fputs(conteudo ? conteudo : "", f);
  fclose(f);
  return 1;
}
int dados_apagar(const char *nome) {
  char c[600];
  caminho(c, sizeof c, nome);
  return remove(c) == 0;
}
const char *dados_cliente_id(void) { return "tv-teste"; }

// ------------------------------------------------------------ perfis dublados

int  perfis_ativo(void)          { return 1; }
int  perfis_ativo_addons(void)   { return 1; }
int  perfis_n(void)              { return 1; }
int  perfis_precisa_escolher(void) { return 0; }
const char *perfis_dono(void)    { return "dono-a"; }
void perfis_esquecer(void)       { }
const ContaPerfil *perfis_item(int i) { (void)i; return NULL; }
const char *i18n(const char *s) { return s; }
int ajustes_social(void) { return 1; }
int ajustes_hist_conta(void) { return 1; }
void ajustes_perfil_guardar(int perfil) { (void)perfil; }
int  ajustes_perfil_restaurar(int perfil) { (void)perfil; return 0; }
void ajustes_perfil_esquecer(void) { }
void arteesc_esquecer(void) { }
void sorg_esquecer(void) { }
int  perfis_puxar(void) { return 1; }

// ------------------------------------------------------------ servidor dublado
// `modo`: 200 responde; 504/429/502 falha com esse HTTP; 0 = sem resposta.

static int modo = 200;
// "parcial": a tabela de addons responde, as RPCs dao 429 (so a ordem e as
// so-leitura caem — caem uma a uma, cada uma para a sua copia).
static int rpcModo = -1;
static const char *usuario = "conta-a";
static int rpcs, tabelas;

static const char *ADDONS =
  "[{\"url\":\"https://a.exemplo/manifest.json\",\"name\":\"A\",\"enabled\":true},"
  "{\"url\":\"https://b.exemplo/manifest.json\",\"name\":\"B\",\"enabled\":false}]";

static char *falha(int *st) {
  *st = modo;
  if (!modo) return NULL;
  return strdup(modo == 429 ? "{\"message\":\"Too Many Requests\"}" : "error code: 504");
}
char *sessao_rpc(const char *funcao, const char *corpo, int *st) {
  (void)corpo;
  rpcs++;
  if (rpcModo >= 0 && rpcModo != 200) { int m = modo; char *r; modo = rpcModo; r = falha(st); modo = m; return r; }
  if (modo != 200) return falha(st);
  *st = 200;
  if (!strcmp(funcao, "sync_pull_collections")) return strdup("[{\"marca\":\"colecao-conta\"}]");
  if (!strcmp(funcao, "sync_pull_library"))     return strdup("[{\"marca\":\"bib-conta\"}]");
  if (!strcmp(funcao, "sync_pull_watched_items")) return strdup("[{\"marca\":\"visto-conta\"}]");
  return strdup("[]");
}
char *sessao_tabela(const char *t, const char *q, int *st) {
  (void)q;
  tabelas++;
  if (modo != 200) return falha(st);
  *st = 200;
  return strdup(!strcmp(t, "addons") ? ADDONS : "[]");
}
int  sessao_logada(void)          { return 1; }
const char *sessao_usuario(void)  { return usuario; }
int  nuvem_freio_ativo(void)      { return 0; }
int  nuvem_erro_ausente(const char *c) { (void)c; return 0; }
const char *nuvem_trakt_cliente(void) { return ""; }
void nuvem_url_escapar(const char *v, char *d, unsigned t) { snprintf(d, t, "%s", v); }

// ------------------------------------------------------------ o app dublado

static int nAplicados, ativosAplicados;
static char primeiraUrl[600];
// #201 (sessao "longa"): a conta manda um addon de URL com 1500 caracteres e
// outro acima do teto de leitura da conta (SY_URL_LEITURA, 16 KB desde o #203:
// de 2 a 16 KB a URL vira apelido, addonurl.h). O primeiro tem de chegar
// INTEIRO ao app; o segundo nao pode chegar, nem cortado.
#define URL_GRANDE 17000
static char urlLonga[1501], urlGrande[URL_GRANDE + 1], contaLonga[URL_GRANDE + 3000];
static int longaIntacta, grandeChegou;
static void montarLonga(void) {
  size_t k, i;
  k = (size_t)snprintf(urlLonga, sizeof urlLonga, "https://longo.exemplo/SEGREDO");
  for (i = 0; k < 1500 - 14; i++) urlLonga[k++] = (char)('a' + (int)((i * 7) % 26));
  snprintf(urlLonga + k, 15, "/manifest.json");
  k = (size_t)snprintf(urlGrande, sizeof urlGrande, "https://grande.exemplo/SEGREDO");
  for (i = 0; k < URL_GRANDE - 14; i++) urlGrande[k++] = (char)('a' + (int)((i * 5) % 26));
  snprintf(urlGrande + k, 15, "/manifest.json");
  snprintf(contaLonga, sizeof contaLonga,
           "[{\"url\":\"%s\",\"name\":\"Longo\",\"enabled\":true},"
           "{\"url\":\"%s\",\"name\":\"Grande\",\"enabled\":true},"
           "{\"url\":\"https://b.exemplo/manifest.json\",\"name\":\"B\",\"enabled\":false}]",
           urlLonga, urlGrande);
}
int addons_definir_lista(const AddonRemoto *l, int n) {
  int i;
  nAplicados = n;
  ativosAplicados = 0;
  for (i = 0; i < n; i++) ativosAplicados += l[i].ativo;
  snprintf(primeiraUrl, sizeof primeiraUrl, "%s", n > 0 ? l[0].url : "");
  longaIntacta = n > 0 && !strcmp(l[0].url, urlLonga);
  for (i = 0; i < n; i++) if (strstr(l[i].url, "grande.exemplo")) grandeChegou = 1;
  return 1;
}
static int colConta, bibConta, vistoConta;
int  col_definir_json(const char *j) { if (j && strstr(j, "colecao-conta")) colConta++; return 0; }
int  contalib_ler_biblioteca(const char *j) { if (j && strstr(j, "bib-conta")) bibConta++; return 0; }
int  contalib_ler_vistos(const char *j) { if (j && strstr(j, "visto-conta")) vistoConta++; return 0; }
void desc_remontar_fileiras(void) {}
// 1.8 sync.c also talks to plugins.c, colfileiras.c and colecoes.c revision/validation;
// none of them is under test here, so they are inert: no plugin edit pending, no
// collection-row revision change.
#include "plugins.h"
#include "colecoes.h"
#include "colfileiras.h"
void plugins_retrato(PlugRetrato *s) { memset(s, 0, sizeof *s); }
int  plugins_confirmar(unsigned rev, unsigned g) { (void)rev; (void)g; return 1; }
int  plugins_definir_da_conta(const PlugRepo *l, int n, unsigned g) { (void)l; (void)n; (void)g; return 0; }
int  plugins_ler_conta(const char *j, PlugRepo *o, int max) { (void)j; (void)o; (void)max; return 0; }
void plugins_perfil_mudou(void) {}
void plugins_esquecer(void) {}
int  col_resposta_valida(const char *j) { return j && *j; }
unsigned col_revisao(void) { return 0; }
void colfileiras_sincronizar(void) {}
int  colfileiras_receber(const char *j) { return col_definir_json(j); }   // real one forwards the blob to colecoes.c
void colfileiras_contexto(void) {}
void desc_repetir_silencioso(void) {}
void desc_repetir(void) {}
void desc_repetir_addons(void) {}
void desc_refazer_continuar(void) {}
void desc_loc_apagar(void) {}
void desc_esquecer(void) {}
void desc_tmdb_definir(const char *c) { (void)c; }

// ------------------------------------------------------------ o resto, mudo

void addons_marcar_da_conta(int perfil) { (void)perfil; }
void addons_esquecer(void) {}
int  addons_exportar(AddonRemoto *s, int m) { (void)s; (void)m; return 0; }
void agenda_esquecer(void) {}
void lembrete_esquecer_todos(void) {}
int  ajustes_aplicar_blob(const char *j) { (void)j; return 0; }
void ajustes_definir_ocultar_nao_lancados(int l) { (void)l; }
int  ajustes_mesclar_blob(const char *b, char **s) { (void)b; *s = NULL; return 0; }
void ajustes_tmdb_idioma_relatar(const char *b) { (void)b; }
void ajustes_idiomas_da_conta(const char *b) { (void)b; }
void buscasrec_esquecer(void) {}
void cachearte_limpar_referencias(void) {}
int  cat_apagar_cache(void) { return 0; }
int  contalib_aplicar_catalogo(void) { return 0; }
int  contalib_aplicar_vistos(void) { return 0; }
void contalib_esquecer(void) {}
unsigned contalib_vistos_revisao(void) { return 0; }   // #199
int  simkl_ativo(void) { return 0; }
void contalib_reconciliar(void) {}
void debrid_definir_chave(const char *s, const char *c) { (void)s; (void)c; }
void debrid_esquecer(void) {}
void extras_definir_chave(const char *c) { (void)c; }
void fontepref_esquecer(void) {}
void legmem_esquecer(void) {}
void homeestado_esquecer(void) {}
void mapa_esquecer(void) {}
void prog_esquecer_tudo(void) {}
void perfilcont_esquecer(void) {}
void psparede_esquecer(void) {}   // sync.c 2.0 chama no logout; faltava aqui e o teste nao ligava
void recomenda_esquecer(void) {}
void salvos_esquecer(void) {}
void stalker_esquecer(void) {}
int  syncprog_aplicar(int *c) { (void)c; return 0; }
int  syncprog_empurrar(void) { return 0; }
void syncprog_esquecer(void) {}
int  syncprog_puxadas(void) { return 0; }
int  syncprog_puxar(void) { return 0; }
int  trakt_ativo(void) { return 0; }
int  trakt_definir(const char *t, const char *c) { (void)t; (void)c; return 0; }
int  trakt_credencial_igual(const char *t, const char *c) { (void)t; (void)c; return 0; }
void trakt_esquecer(void) {}
TraEstado traktauth_estado(void) { return TRA_LIGADO; }
void vistoep_esquecer(void) {}
void xtream_esquecer(void) {}
void servidores_esquecer_todos(void) {}

// ------------------------------------------------------------ roteiro

static int falhas;
static unsigned agora = 1000;
static void confere(const char *d, int ok) {
  printf("  %-66s %s\n", d, ok ? "ok" : "FALHOU");
  if (!ok) falhas++;
}
static void ateTerminar(void) {
  int i;
  for (i = 0; i < 4000 && sync_estado() == SYNC_RODANDO; i++) { sync_passo(agora += 16); usleep(1000); }
  sync_passo(agora += 16);
  sync_passo(agora += 16);
}
static int existe(const char *nome) {
  char *b = dados_ler(nome);
  int ok = b != NULL;
  free(b);
  return ok;
}

// argv: <modo HTTP> [<conta>] [volta|sair]
//   volta = depois do ciclo, o servidor volta a 200 e sync_periodico tem de
//           partir em SYNC_RETENTA_MS;
//   sair  = depois do ciclo, sync_esquecer_usuario (logout).
// argv[2] = "-" mantem a conta padrao. O que se espera vem do .sh.
int main(int argc, char **argv) {
  const char *extra = argc > 3 ? argv[3] : "";
  int copia;
  setvbuf(stdout, NULL, _IOLBF, 0);
  modo = argc > 1 ? atoi(argv[1]) : 200;
  if (argc > 1 && !strcmp(argv[1], "parcial")) { modo = 200; rpcModo = 429; }
  if (argc > 1 && !strcmp(argv[1], "longa")) { modo = 200; montarLonga(); ADDONS = contaLonga; }
  if (argc > 2 && strcmp(argv[2], "-")) usuario = argv[2];
  printf("-- sessao: servidor %d, conta %s%s%s\n", modo, usuario, *extra ? ", " : "", extra);
  copia = existe("conta-addons-p1.json");
  sync_iniciar();
  ateTerminar();

  if (rpcModo == 429) {
    confere("parcial: addons da conta (resposta nova)", nAplicados == 2);
    confere("parcial: colecoes, biblioteca e vistos da copia", colConta && bibConta && vistoConta);
    confere("parcial: estado diz servidor fora (429) e usando copia",
            sync_servidor_fora() == 429 && sync_usando_copia());
    confere("parcial: os addons NAO sao da copia (sem aviso de addons)", sync_addons_fora() == 0);
  } else if (modo == 200) {
    confere("servidor bom: os 2 addons da conta aplicados", nAplicados == 2 && ativosAplicados == 1);
    confere("estado: o servidor respondeu", sync_servidor_fora() == 0 && !sync_usando_copia());
    confere("a resposta virou copia em disco (addons, colecoes, biblioteca, vistos)",
            existe("conta-addons-p1.json") && existe("conta-colecoes-p1.json") &&
            existe("conta-biblioteca-p1.json") && existe("conta-vistos-p1.json"));
    { char *c = contacache_ler(CC_ADDONS, 1, usuario, NULL);
      confere("a copia tem dono e perfil (outra conta nao le)",
              c && !contacache_ler(CC_ADDONS, 1, "outra-conta", NULL));
      free(c); }
  } else {
    int esperado = modo ? modo : -1;
    confere("estado: servidor fora com o HTTP certo", sync_servidor_fora() == esperado);
    confere("o ciclo parou de perguntar depois dos addons", rpcs == 0 && tabelas == 1);
    if (copia && !strcmp(usuario, "conta-a")) {
      confere("com copia: os addons DA COPIA chegaram ao app",
              nAplicados == 2 && ativosAplicados == 1 &&
              !strcmp(primeiraUrl, "https://a.exemplo/manifest.json"));
      confere("com copia: colecoes, biblioteca e vistos tambem", colConta && bibConta && vistoConta);
      confere("estado: usando a copia", sync_usando_copia() && sync_copia_quando() > 0);
      confere("estado: addons da copia (aviso da ilha: usando seus addons salvos)", sync_addons_fora() == 1);
    } else {
      confere("sem copia (ou copia de outra conta): nenhum addon inventado", nAplicados == 0);
      confere("estado: fora e SEM copia", !sync_usando_copia() && sync_addons_fora() == 2);
    }
  }

  if (ADDONS == contaLonga) {
    confere("#201: a URL de 1500 caracteres chegou inteira ao app", longaIntacta);
    confere("#201: a URL acima do limite nao chegou, nem cortada", nAplicados == 2 && !grandeChegou);
    // A lista desta TV ficou menor que a da conta: uma edicao daqui NAO sobe
    // (o push substituiria a lista da conta e apagaria o addon que nao coube).
    sync_sujar_addons();
    confere("#201: edicao com a lista incompleta nao vira pendencia em disco",
            !existe("conta-addons-pend-636f6e74612d61-p1.txt"));
  }
  if (!strcmp(extra, "volta")) {
    int r;
    modo = 200;
    r = sync_periodico(agora + 1000);
    confere("antes de SYNC_RETENTA_MS nao repete", r == 0);
    r = sync_periodico(agora + SYNC_RETENTA_MS + 1);
    confere("servidor fora: volta seguinte em SYNC_RETENTA_MS, nao em 5 min", r == 1);
    ateTerminar();
    confere("o servidor voltou: estado volta a respondeu",
            sync_servidor_fora() == 0 && !sync_usando_copia() && sync_addons_fora() == 0);
    confere("e a lista da conta substitui a copia", nAplicados == 2);
  }
  if (!strcmp(extra, "sair")) {
    sync_esquecer_usuario();
    confere("logout apaga as copias da conta",
            !existe("conta-addons-p1.json") && !existe("conta-colecoes-p1.json") &&
            !existe("conta-biblioteca-p1.json") && !existe("conta-vistos-p1.json"));
    confere("logout zera o estado de servidor fora", sync_servidor_fora() == 0);
  }
  printf("%s\n", falhas ? "FALHOU" : "PASSOU");
  return falhas ? 1 : 0;
}

void cat_historico_contexto(const char *u, int p) { (void)u; (void)p; }

__attribute__((weak)) int addons_perfil_da_lista(void) { return 0; }
