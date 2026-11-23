// HIST-SEMLOGIN — sem login, trocar de perfil nao trocava o contexto do
// historico em memoria: o perfil 2 via os assistidos do perfil 1.
//
// O relato (Poke) e a hipotese por leitura: sync_iniciar (src/sync.c) chamava
// cat_historico_contexto("",0) sempre que sessao_logada()==0, e o bloco de
// reabertura de sessao de app.c so rodava com login — deslogado, nenhuma das
// duas portas levava o perfil ativo ao mapa.
//
// Este teste roda o sync.c REAL (sync_iniciar/sync_abrir_sessao de verdade),
// o catalogo.c REAL (a tabela de historico), o contapend.c REAL (o jornal
// conta-pend-anon.txt, que ja grava o gesto deslogado quando ha tracker no
// destino) e o vistoep.c REAL, com sessao, perfis, rede e disco dublados —
// o disco e uma pasta de verdade (NV_T_DIR), como em tests/syncordem.c.
//
//   1. deslogado, perfil 1: "marcar assistido" escreve o espelho local
//      (cat_historico_definir_id, o que ctxmenu.c faz) e o gesto entra no
//      jornal com perfil=1 (contapend_gesto, o que visto_titulo chama).
//   2. troca para o perfil 2 chamando so sync_iniciar — o que o app.c da base
//      oferecia deslogado: o historico do 2 TEM de estar vazio.
//   3. volta ao perfil 1 pela reabertura de sessao (o que o app.c corrigido
//      chama): o jornal re-marca o que e do 1 — "o dele volta".
//   4. o 2, que nunca marcou nada, continua vazio.
//
// Na base e16157090: o passo 2 falha — o contexto fica ("",0) qualquer que
// seja o perfil, e a marca do 1 fica visivel no mapa do 2. O passo 3 passa
// mesmo na base porque a leitura (cat_historico_estado_tempo) deriva de novo
// pelo jornal por perfil — o jornal NUNCA vaza, so o mapa vazava.
// Ver tests/hist_semlogin.sh.
#include "sync.h"
#include "catalogo.h"
#include "contapend.h"
#include "vistoep.h"
#include "visto.h"
#include "nvtmp.h"
#include "addons.h"
#include "catordem.h"
#include "catordemcache.h"
#include "progresso.h"
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
  return d && *d ? d : nvt_dir();
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
  char c[600], tmp[608];
  FILE *f;
  caminho(c, sizeof c, nome);
  snprintf(tmp, sizeof tmp, "%s.tmp", c);
  f = fopen(tmp, "wb");
  if (!f) return 0;
  int ok = fputs(conteudo ? conteudo : "", f) >= 0;
  if (fclose(f)) ok = 0;
  if (ok && !rename(tmp, c)) return 1;
  remove(tmp); return 0;
}
int dados_apagar(const char *nome) {
  char c[600];
  caminho(c, sizeof c, nome);
  return remove(c) == 0;
}
const char *dados_cliente_id(void) { return "tv-teste"; }

// ------------------------------------------------------------ perfis dublados
// Mesma semantica de perfis.c: `ativo` e a escolha e a geracao de sessao anda
// a cada troca (perfis_definir_ativo -> sessaoAvancar).

static int ativo = 1, escolhido;
static uint64_t geracaoSessao = 1;
int  perfis_ativo(void)          { return ativo; }
int  perfis_ativo_addons(void)   { return ativo; }
int  perfis_n(void)              { return 2; }
int  perfis_precisa_escolher(void) { return !escolhido; }
const char *perfis_dono(void)    { return ""; }
void perfis_esquecer(void)       { ativo = 1; escolhido = 0; }
const ContaPerfil *perfis_item(int i) { (void)i; return NULL; }
const char *i18n(const char *s) { return s; }
const char *idioma_mes_data(int m, const char *s) { (void)m; return s; }
int  ajustes_idioma(void) { return 0; }
int  ajustes_idioma_ingles(void) { return 0; }
int  ajustes_social(void) { return 1; }
int  ajustes_hist_conta(void) { return 1; }
void ajustes_perfil_guardar(int perfil) { (void)perfil; }
int  ajustes_perfil_restaurar(int perfil) { (void)perfil; return 0; }
void ajustes_perfil_esquecer(void) { }
void arteesc_esquecer(void) { }
void sorg_esquecer(void) { }
static void escolher(int indice) {
  char l[16];
  ativo = indice;
  escolhido = 1;
  geracaoSessao++;
  snprintf(l, sizeof l, "%d\n", indice);
  dados_gravar("perfil.txt", l);
  printf("[perfis] perfil ativo: %d\n", indice);
}

uint64_t perfis_sessao_geracao(void) { return geracaoSessao; }
int perfis_sessao_atual(uint64_t g) { return g == geracaoSessao; }
int perfis_sessao_travar(uint64_t g) { return g == geracaoSessao; }
void perfis_sessao_destravar(void) {}
void perfis_invalidar_sessao(void) { geracaoSessao++; }
int perfis_puxar(void) { return 2; }
int perfis_puxar_sessao(uint64_t s, const char *u, const char *t) { (void)s;(void)u;(void)t; return perfis_puxar(); }

// ------------------------------------------------------------ rede dublada

char *sessao_rpc(const char *funcao, const char *corpo, int *st) {
  (void)funcao; (void)corpo; *st = 200;
  return strdup("[]");
}
char *sessao_tabela(const char *t, const char *q, int *st) {
  (void)t; (void)q; *st = 200;
  return strdup("[]");
}
static const char *usuario = "";
static int logada;
int  sessao_logada(void)          { return logada; }
const char *sessao_usuario(void)  { return usuario; }
int  sessao_token_copiar(char *j, size_t nj) { snprintf(j, nj, "tok"); return 1; }
int  sessao_identidade_local_copiar(char *u, size_t nu, char *j, size_t nj) {
  snprintf(u, nu, "%s", sessao_usuario()); return sessao_token_copiar(j, nj);
}
void sessao_token_recusado(const char *t) { (void)t; }
int  nuvem_freio_ativo(void)      { return 0; }
int  nuvem_erro_ausente(const char *c) { (void)c; return 0; }
const char *nuvem_trakt_cliente(void) { return ""; }
void nuvem_url_escapar(const char *v, char *d, unsigned t) { snprintf(d, t, "%s", v); }
char *nuvem_rpc_com(const char *f, const char *c, const char *b, int *s) { return sessao_rpc(f, c, s); }
char *nuvem_tabela(const char *f, const char *c, const char *b, int *s) { return sessao_tabela(f, c, s); }

// ------------------------------------------------------------ home dublada

void desc_remontar_fileiras(void) {}
void desc_repetir_silencioso(void) {}
void desc_repetir(void) {}
void desc_repetir_addons(void) {}
void desc_refazer_continuar(void) {}
void desc_loc_apagar(void) {}
void desc_esquecer(void) {}
void desc_tmdb_definir(const char *c) { (void)c; }
void desc_abrir_sessao(void) {}
void desc_sessao_passo(void) {}
int  desc_sessao_preparada(void) { return 1; }
const char *desc_genero_pt(const char *s) { return s; }
#include "plugins.h"
void plugins_retrato(PlugRetrato *r) { memset(r, 0, sizeof *r); }
int  plugins_confirmar(unsigned rev, unsigned geracao) { (void)rev; (void)geracao; return 0; }
int  plugins_definir_da_conta(const PlugRepo *l, int n, unsigned g) { (void)l; (void)n; (void)g; return 0; }
int  plugins_ler_conta(const char *json, PlugRepo *saida, int max) { (void)json; (void)saida; (void)max; return -1; }
void plugins_esquecer(void) {}
void plugins_perfil_mudou(void) {}

// ------------------------------------------------------------ o resto, mudo

int  addons_definir_lista(const AddonRemoto *l, int n) { (void)l; (void)n; return 0; }
void addons_marcar_da_conta(int perfil) { (void)perfil; }
void addons_esquecer(void) {}
int  addons_exportar(AddonRemoto *s, int m) { (void)s; (void)m; return 0; }
void addons_diferenca(const AddonRemoto *a, int na, const AddonRemoto *d, int nd,
                      int *e, int *s, int *m) {
  (void)a; (void)na; (void)d; (void)nd;
  if (e) *e = 0; if (s) *s = 0; if (m) *m = 0;
}
void agenda_esquecer(void) {}
void lembrete_esquecer_todos(void) {}
int  ajustes_aplicar_blob(const char *j) { (void)j; return 0; }
void ajustes_idiomas_da_conta(const char *b) { (void)b; }
void ajustes_definir_ocultar_nao_lancados(int l) { (void)l; }
int  ajustes_mesclar_blob(const char *b, char **s) { (void)b; *s = NULL; return 0; }
void ajustes_tmdb_idioma_relatar(const char *b) { (void)b; }
void buscasrec_esquecer(void) {}
void cachearte_limpar_referencias(void) {}
int  col_definir_json(const char *j) { (void)j; return 0; }
unsigned col_revisao(void) { return 0; }
int  col_resposta_valida(const char *j) { return j && *j; }
void colfileiras_sincronizar(void) {}
int  colfileiras_receber(const char *j) { return col_definir_json(j); }
void colfileiras_contexto(void) {}
int  contalib_aplicar_catalogo(void) { return 0; }
int  contalib_aplicar_vistos(void) { return 0; }
void contalib_esquecer(void) {}
int  contalib_n(void) { return 0; }
const void *contalib_item(int i) { (void)i; return NULL; }
int  contalib_ler_biblioteca(const char *j) { (void)j; return 0; }
int  contalib_ler_vistos(const char *j) { (void)j; return 0; }
unsigned contalib_vistos_revisao(void) { return 0; }
int  simkl_ativo(void) { return 0; }
void contalib_reconciliar(void) {}
void debrid_definir_chave(const char *s, const char *c) { (void)s; (void)c; }
void debrid_esquecer(void) {}
void debrid_conta_iniciar(void *p) { (void)p; }
void debrid_conta_definir(const char *j) { (void)j; }
void debrid_aplicar_chaves_conta(void) {}
void extras_definir_chave(const char *c) { (void)c; }
void extras_abrir_sessao(void) {}
void fontepref_esquecer(void) {}
void legmem_esquecer(void) {}
void audiomem_esquecer(void) {}
void homeestado_esquecer(void) {}
void mapa_esquecer(void) {}
void prog_esquecer_tudo(void) {}
int  prog_ler(ProgRegistro *v, int n) { (void)v; (void)n; return 0; }
int  prog_gravar_local(const char *id, int t, int e, double p, double d, int tocou) {
  (void)id; (void)t; (void)e; (void)p; (void)d; (void)tocou; return 0;
}
void perfilcont_esquecer(void) {}
void recomenda_esquecer(void) {}
void recomenda_esquecer_conta(void) {}
void salvos_esquecer(void) {}
void stalker_esquecer(void) {}
void ajustes_perfil_padrao(void) {}
int  syncprog_puxar_sessao(int p, uint64_t s) { (void)p;(void)s; return 0; }
int  syncprog_empurrar_sessao(int p, uint64_t s) { (void)p;(void)s; return 0; }
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
void xtream_esquecer(void) {}
void servidores_esquecer_todos(void) {}
int  ajustes_edicao_perfil(void) { return 0; }
void ajustes_edicao_encerrar(void) {}
int  ajustes_migrar_escopo(void) { return 0; }
unsigned long long ajustes_sync_hash(void) { return 0; }
char *nuvem_addon_alternar(const char *c, int a, const char *b, unsigned p, int *s) {
  (void)c; (void)a; (void)b; (void)p; *s = 200; return strdup("[]");
}
int  traktauth_desconectado(void) { return 1; }
void farolsinal_esquecer(void) {}
void fontevolta_esquecer_tudo(void) {}
void iptv_contas_esquecer_todas(void) {}
void stalker_fontes_invalidar(void) {}
void m3u_fontes_invalidar(void) {}
void xtream_fontes_invalidar(void) {}
void guia_fonte_esquecer(void) {}
void diario_dono(const char *c) { (void)c; }
void diario_esquecer(void) {}
void perfil_infantil_esquecer(void) {}
void g5ui_forget(void) {}
void trava_definir_conta(const char *c) { (void)c; }
void trava_esquecer(void) {}
int  rewatch_sujo(void) { return 0; }
void rewatch_sujar(void) {}
int  rewatch_blob_ler(const char *b) { (void)b; return 0; }
char *rewatch_blob_costurar(const char *b) { return b ? strdup(b) : NULL; }
void rewatch_esquecer(void) {}
void segfontes_animeskip_id_conta(const char *id) { (void)id; }
void psparede_esquecer(void) {}
__attribute__((weak)) int addons_perfil_da_lista(void) { return 0; }

// Salvos sem migracao pendente (o modulo tem teste proprio).
struct SalvoItem;
int  salvos_n(void) { return 0; }
const void *salvos_item(int i) { (void)i; return 0; }
int  salvos_migracao_conta(int *p) { (void)p; return 0; }
void salvos_migracao_conta_feita(void) {}
int  salvos_perfil_atual(void) { return 1; }
void cf_esquecer(void) {}
void ov_esquecer(void) {}
void visto_iniciar(void) {}

// ------------------------------------------------------------ roteiro

static int falhas;
static void confere(const char *d, int ok) {
  printf("  %-66s %s\n", d, ok ? "ok" : "FALHOU");
  if (!ok) falhas++;
}

#define ID "tt7654321"

int main(void) {
  int st;
  setvbuf(stdout, NULL, _IOLBF, 0);

  // SEM LOGIN o teste inteiro: sessao_logada() == 0, sessao_usuario() == "".
  logada = 0; usuario = "";

  printf("-- sem login: o historico em memoria acompanha o perfil\n");

  // Perfil 1 ativo. O arranque chama sync_iniciar com a sessao que existe —
  // nenhuma — e e ele que leva o contexto ao mapa do historico.
  sync_iniciar();

  // "Marcar como assistido" no perfil 1: o espelho local imediato
  // (ctxmenu.c/app.c chamam cat_historico_definir_id na hora) e o gesto no
  // jornal, com Trakt vinculado (destinos=VISTO_TRAKT — contapend_gesto grava
  // em conta-pend-anon.txt mesmo sem conta Nuvio).
  cat_historico_definir_id(ID, "movie", 1);
  confere("marca do perfil 1 na memoria", cat_historico_estado_id(ID, "movie") == 1);
  st = contapend_gesto(ID, "movie", NULL, 0, 1, VISTO_TRAKT, NULL, 0) != 0;
  confere("gesto do perfil 1 gravado no jornal", st);

  // TROCA PARA O PERFIL 2. No app.c da base, deslogado, nenhuma reabertura de
  // sessao roda: o unico ajuste de contexto que chega e o sync_iniciar da
  // saida da tela de escolha. E ele que este teste chama.
  escolher(2);
  sync_iniciar();
  confere("perfil 2 nao ve o assistido do perfil 1",
          cat_historico_estado_id(ID, "movie") == -1);

  // VOLTA AO PERFIL 1 pelo caminho que o app.c passa a chamar:
  // perfilAbrirSessao -> sync_abrir_sessao -> sync_iniciar. A reabertura
  // reposiciona o contexto e o jornal re-marca o que e do 1.
  escolher(1);
  sync_abrir_sessao(2);
  sync_iniciar();
  confere("o assistido do perfil 1 volta pelo jornal",
          cat_historico_estado_id(ID, "movie") == 1);

  // O 2, que nunca marcou nada, continua vazio — nada mistura.
  escolher(2);
  sync_abrir_sessao(1);
  sync_iniciar();
  confere("perfil 2 continua sem o assistido do 1",
          cat_historico_estado_id(ID, "movie") == -1);

  printf("%s\n", falhas ? "FALHOU" : "hist_semlogin: tudo ok");
  return falhas ? 1 : 0;
}
