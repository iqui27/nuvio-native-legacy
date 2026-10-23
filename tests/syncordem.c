// A ORDEM DA HOME VOLTA DO CACHE NO FLUXO DE VERDADE, e nao so na unidade.
//
// Issue #125 ("metadata is delayed, messing up my home screen order"). O
// cache local de catordemcache.c ja passava no seu proprio teste
// (tests/catordemcache.sh) e mesmo assim a linha "ordem restaurada do cache
// local" quase nunca aparecia nos logs de campo: o defeito estava em QUANDO
// sync.c chama o cache, nao no cache. Este teste roda o sync.c REAL — fio de
// verdade, sync_iniciar/sync_passo de verdade — com rede, perfis e disco
// dublados, e reproduz as aberturas que o app faz:
//
//   1. boot com perfil salvo, pergunta de perfil aberta, a pessoa responde
//      depois do ciclo interrompido, DURANTE ele (perfis_puxar no ar) ou no
//      quadro entre o fim do fio e o sync_passo que o recolhe;
//   2. o mesmo trocando de perfil (salvo 1, escolhe 2 — o caso que falhava);
//   3. a resposta da conta chega com a MESMA ordem: nada e refeito;
//   4. o ciclo completo do perfil escolhido sempre roda, e nao so no
//      sync_periodico cinco minutos depois.
//
// Cada "sessao" e um processo novo (as estaticas de sync.c nao voltam a zero
// dentro de um processo); o disco dublado e uma pasta de verdade, entao o
// que uma sessao grava a seguinte le. Ver tests/syncordem.sh.
#include "sync.h"
#include "nvtmp.h"
#include "addons.h"
#include "catordem.h"
#include "catordemcache.h"
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
// Mesma semantica de perfis.c: `ativo` nasce do perfil.txt, `escolhido` e de
// sessao, a pergunta vale enquanto nao houver escolha nesta sessao.

static int ativo = 1, escolhido;
int  perfis_ativo(void)          { return ativo; }
int  perfis_ativo_addons(void)   { return ativo; }
int  perfis_n(void)              { return 2; }
int  perfis_precisa_escolher(void) { return !escolhido; }
const char *perfis_dono(void)    { return "dono-a"; }
void perfis_esquecer(void)       { ativo = 1; escolhido = 0; }
// Sem lista de verdade: sync_trocar_perfil cai no perfil 1 como principal.
const ContaPerfil *perfis_item(int i) { (void)i; return NULL; }
// Ajustes por perfil (ajustes.c) e a arte escolhida (arteescolha.c) nao entram
// neste teste; o sync so os chama.
const char *i18n(const char *s) { return s; }
int ajustes_social(void) { return 1; }
int ajustes_hist_conta(void) { return 1; }
void ajustes_perfil_guardar(int perfil) { (void)perfil; }
int  ajustes_perfil_restaurar(int perfil) { (void)perfil; return 0; }
void ajustes_perfil_esquecer(void) { }
void arteesc_esquecer(void) { }
void sorg_esquecer(void) { }
static void carregarAtivo(void) {
  char *b = dados_ler("perfil.txt");
  if (b) { if (atoi(b) > 0) ativo = atoi(b); free(b); }
}
static void escolher(int indice) {
  char l[16];
  ativo = indice;
  escolhido = 1;
  snprintf(l, sizeof l, "%d\n", indice);
  dados_gravar("perfil.txt", l);
  printf("[perfis] perfil ativo: %d\n", indice);
}

// O primeiro ciclo pode ser SEGURADO dentro de perfis_puxar: e o caso em que
// a pessoa responde a pergunta (que abre do cache de perfis, no primeiro
// quadro) antes de a rede devolver a lista.
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  sinal = PTHREAD_COND_INITIALIZER;
static int segurar, puxandoPerfis;
int perfis_puxar(void) {
  pthread_mutex_lock(&trava);
  puxandoPerfis = 1;
  pthread_cond_broadcast(&sinal);
  while (segurar) pthread_cond_wait(&sinal, &trava);
  puxandoPerfis = 0;
  pthread_mutex_unlock(&trava);
  return 2;
}
static void esperarPuxando(void) {
  pthread_mutex_lock(&trava);
  while (!puxandoPerfis) pthread_cond_wait(&sinal, &trava);
  pthread_mutex_unlock(&trava);
}
static void soltar(void) {
  pthread_mutex_lock(&trava);
  segurar = 0;
  pthread_cond_broadcast(&sinal);
  pthread_mutex_unlock(&trava);
}

// ------------------------------------------------------------ rede dublada

static const char *ORDEM_A =
  "[{\"settings_json\":{\"items\":["
  "{\"addon_id\":\"xperience\",\"type\":\"movie\",\"catalog_id\":\"foryou\",\"order\":0},"
  "{\"addon_id\":\"cinemeta\",\"type\":\"movie\",\"catalog_id\":\"top\",\"order\":1},"
  "{\"addon_id\":\"cinemeta\",\"type\":\"series\",\"catalog_id\":\"top\",\"order\":2,\"enabled\":false}]}}]";
static const char *ORDEM_B =
  "[{\"settings_json\":{\"items\":["
  "{\"addon_id\":\"cinemeta\",\"type\":\"series\",\"catalog_id\":\"top\",\"order\":0},"
  "{\"addon_id\":\"xperience\",\"type\":\"movie\",\"catalog_id\":\"foryou\",\"order\":1}]}}]";

static volatile int rpcCatHome;
static int rpcCredencial;
// Modo "troca": a RPC das colecoes do perfil 1 fica presa ate a pessoa trocar
// para o 2 — o ciclo do 1 termina com o 2 ja ativo.
static int segurarCol, puxandoCol;
// #378: modo "legconta" — a conta devolve um blob de ajustes com o idioma da
// legenda, e o que sync.c faz com ele fica anotado (stubs mais abaixo).
static int modoBlob, blobsAplicados;
static char idiomasConta[512];
static const char *legContaValor = "en";   // valor da legenda no proximo blob
static int idiomasChamadas;
char *sessao_rpc(const char *funcao, const char *corpo, int *st) {
  *st = 200;
  // modoBlob 2: so o perfil 1 tem blob na conta; o 2 nunca salvou ajustes.
  if (modoBlob == 2 && !strcmp(funcao, "sync_pull_profile_settings_blob") &&
      strstr(corpo, "\"p_profile_id\":2"))
    return strdup("[]");
  if (modoBlob && !strcmp(funcao, "sync_pull_profile_settings_blob"))
  { char b[256];
    snprintf(b, sizeof b, "[{\"settings_json\":{\"features\":{\"player_settings\":{"
             "\"subtitle_preferred_language\":{\"type\":\"string\",\"value\":\"%s\"}}}}}]",
             legContaValor);
    return strdup(b); }
  if (!strcmp(funcao, "sync_pull_collections")) {
    int p1 = strstr(corpo, "\"p_profile_id\":1") != NULL;
    pthread_mutex_lock(&trava);
    puxandoCol = 1;
    pthread_cond_broadcast(&sinal);
    while (segurarCol && p1) pthread_cond_wait(&sinal, &trava);
    pthread_mutex_unlock(&trava);
    return strdup(p1 ? "[{\"marca\":\"perfil1\"}]" : "[{\"marca\":\"perfil2\"}]");
  }
  if (!strcmp(funcao, "sync_pull_home_catalog_settings")) {
    rpcCatHome++;
    // A conta guarda uma ordem por perfil: o 2 tem ORDEM_A, o 1 ORDEM_B.
    return strdup(strstr(corpo, "\"p_profile_id\":2") ? ORDEM_A : ORDEM_B);
  }
  if (!strcmp(funcao, "sync_push_provider_credentials")) {
    // Resposta do servidor de campo (tapmal5, 1.4.3) para trakt e simkl.
    rpcCredencial++;
    *st = 400;
    return strdup("{\"code\":\"22023\",\"message\":\"Unsupported provider credential\"}");
  }
  return strdup("[]");
}
char *sessao_tabela(const char *t, const char *q, int *st) {
  (void)t; (void)q; *st = 200;
  return strdup("[]");
}
static const char *usuario = "conta-a";
static int logada = 1;
int  sessao_logada(void)          { return logada; }
const char *sessao_usuario(void)  { return usuario; }
int  nuvem_freio_ativo(void)      { return 0; }
int  nuvem_erro_ausente(const char *c) { (void)c; return 0; }
const char *nuvem_trakt_cliente(void) { return ""; }
void nuvem_url_escapar(const char *v, char *d, unsigned t) { snprintf(d, t, "%s", v); }

// ------------------------------------------------------------ home dublada
// desc_remontar_fileiras e o que reordena a home. Guardar a PRIMEIRA chave da
// ordem no momento da remontagem prova em que ordem as fileiras sairam.

static int remontagens;
static char primeiraNaRemontagem[200];
void desc_remontar_fileiras(void) {
  remontagens++;
  snprintf(primeiraNaRemontagem, sizeof primeiraNaRemontagem, "%s", catordem_chave(0));
}
static int repeticoes;
void desc_repetir_silencioso(void) {}
#include "plugins.h"
/* sync.c pushes/reads plugin repos (F09); this test has none. */
void plugins_retrato(PlugRetrato *r) { memset(r, 0, sizeof *r); }
int  plugins_confirmar(unsigned rev, unsigned geracao) { (void)rev; (void)geracao; return 0; }
int  plugins_definir_da_conta(const PlugRepo *l, int n, unsigned g) { (void)l; (void)n; (void)g; return 0; }
int  plugins_ler_conta(const char *json, PlugRepo *saida, int max) { (void)json; (void)saida; (void)max; return -1; }
void plugins_esquecer(void) {}
void plugins_perfil_mudou(void) {}
void desc_repetir(void) { repeticoes++; }
void desc_repetir_addons(void) { repeticoes++; }
void desc_refazer_continuar(void) {}
void desc_loc_apagar(void) {}
void desc_esquecer(void) {}
void desc_tmdb_definir(const char *c) { (void)c; }

// ------------------------------------------------------------ o resto, mudo

static int historicoPerfil;
static char historicoDono[80];
void cat_historico_contexto(const char *u, int p) {
  snprintf(historicoDono, sizeof historicoDono, "%s", u); historicoPerfil = p;
}

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
int  ajustes_aplicar_blob(const char *j) { (void)j; blobsAplicados++; return 0; }
void ajustes_idiomas_da_conta(const char *b) {
  idiomasChamadas++;
  snprintf(idiomasConta, sizeof idiomasConta, "%s", b ? b : "");
}
void ajustes_definir_ocultar_nao_lancados(int l) { (void)l; }
int  ajustes_mesclar_blob(const char *b, char **s) { (void)b; *s = NULL; return 0; }
void ajustes_tmdb_idioma_relatar(const char *b) { (void)b; }
void buscasrec_esquecer(void) {}
void cachearte_limpar_referencias(void) {}
int  cat_apagar_cache(void) { return 0; }
static int colDoOutro, colDoCerto;
int  col_definir_json(const char *j) {
  if (j && strstr(j, "perfil1")) colDoOutro++;
  if (j && strstr(j, "perfil2")) colDoCerto++;
  return 0;
}
unsigned col_revisao(void) { return 0; }
int col_resposta_valida(const char *j) { return j && *j; }
void colfileiras_sincronizar(void) {}
int colfileiras_receber(const char *j) { return col_definir_json(j); }
void colfileiras_contexto(void) {}
int  contalib_aplicar_catalogo(void) { return 0; }
int  contalib_aplicar_vistos(void) { return 0; }
void contalib_esquecer(void) {}
int  contalib_ler_biblioteca(const char *j) { (void)j; return 0; }
int  contalib_ler_vistos(const char *j) { (void)j; return 0; }
unsigned contalib_vistos_revisao(void) { return 0; }   // #199
int  simkl_ativo(void) { return 0; }
void contalib_reconciliar(void) {}
void debrid_definir_chave(const char *s, const char *c) { (void)s; (void)c; }
void debrid_esquecer(void) {}
void extras_definir_chave(const char *c) { (void)c; }
void fontepref_esquecer(void) {}
void legmem_esquecer(void) {}
void audiomem_esquecer(void) {}
void homeestado_esquecer(void) {}
void mapa_esquecer(void) {}
void prog_esquecer_tudo(void) {}
void perfilcont_esquecer(void) {}
void recomenda_esquecer(void) {}
void salvos_esquecer(void) {}
void stalker_esquecer(void) {}
int sessao_token_copiar(char *j, size_t nj) { snprintf(j, nj, "tok"); return 1; }
uint64_t perfis_sessao_geracao(void) { return 0; }
int perfis_sessao_atual(uint64_t g) { return !g; }
int perfis_sessao_travar(uint64_t g) { return !g; }
void perfis_sessao_destravar(void) {}
void perfis_invalidar_sessao(void) {}
int perfis_puxar_sessao(uint64_t s, const char *u, const char *t) { (void)s;(void)u;(void)t; return perfis_puxar(); }
int sessao_identidade_local_copiar(char *u, size_t nu, char *j, size_t nj) {
  snprintf(u, nu, "%s", sessao_usuario()); return sessao_token_copiar(j, nj);
}
void sessao_token_recusado(const char *t) { (void)t; }
char *nuvem_rpc_com(const char *f, const char *c, const char *b, int *s) { return sessao_rpc(f, c, s); }
char *nuvem_tabela(const char *f, const char *c, const char *b, int *s) { return sessao_tabela(f, c, s); }
void debrid_conta_iniciar(void *p) { (void)p; }
void debrid_conta_definir(const char *j) { (void)j; }
void debrid_aplicar_chaves_conta(void) {}
void desc_abrir_sessao(void) {}
void desc_sessao_passo(void) {}
int desc_sessao_preparada(void) { return 1; }
void extras_abrir_sessao(void) {}
void ajustes_perfil_padrao(void) {}
void recomenda_esquecer_conta(void) {}
int syncprog_puxar_sessao(int p, uint64_t s) { (void)p;(void)s; return 0; }
int syncprog_empurrar_sessao(int p, uint64_t s) { (void)p;(void)s; return 0; }
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
static void confere(const char *d, int ok) {
  printf("  %-66s %s\n", d, ok ? "ok" : "FALHOU");
  if (!ok) falhas++;
}

// O laco principal durante a pergunta de perfil: app.c chama sync_passo a
// cada quadro tambem em TELA_ESCOLHA_PERFIL.
static void quadros(int n) {
  static unsigned t = 1000;
  while (n-- > 0) { sync_passo(t += 16); usleep(2000); }
}
static void ateTerminar(void) {
  int i;
  for (i = 0; i < 2000 && sync_estado() == SYNC_RODANDO; i++) quadros(1);
  quadros(3);
}

static void ciclo(void) { sync_iniciar(); ateTerminar(); }

// argv[1] = perfil que a pessoa escolhe; argv[2] = "segura" para responder
// com o primeiro ciclo ainda no ar, "tarde" para responder depois de ele
// acabar e antes de sync_passo o recolher; argv[3] = o que se espera ("frio" quando
// nao ha cache: primeira vez deste perfil nesta TV).
int main(int argc, char **argv) {
  int alvo = argc > 1 ? atoi(argv[1]) : 2;
  int segura = argc > 2 && !strcmp(argv[2], "segura");
  // "tarde": o fio interrompido JA acabou, mas sync_passo ainda nao o viu —
  // a resposta cai no mesmo quadro, entre o fim do fio e o passo seguinte.
  int tarde = argc > 2 && !strcmp(argv[2], "tarde");
  int frio = argc > 3 && !strcmp(argv[3], "frio");
  const char *esperada = alvo == 2 ? "xperience_movie_foryou" : "cinemeta_series_top";
  int remAntes, rpcAntes;

  setvbuf(stdout, NULL, _IOLBF, 0);
  if (argc > 1 && !strcmp(argv[1], "legconta-troca")) {
    // Revisao P1: troca do 1 para o 2 com o ciclo do 1 no ar. O blob do 1
    // ("en") chega DEPOIS da troca (sync_reaplicar_ajustes ja o tinha
    // soltado), o ciclo do 1 e descartado, e o 2 nao tem blob na conta. Os
    // idiomas do 1 nao podem virar "Da conta" do 2.
    printf("-- sessao: troca 1 -> 2 com o blob do 1 chegando depois\n");
    modoBlob = 2;
    escolher(1);
    segurarCol = 1;
    sync_iniciar();
    pthread_mutex_lock(&trava);
    while (!puxandoCol) pthread_cond_wait(&sinal, &trava);
    pthread_mutex_unlock(&trava);
    escolher(2);
    sync_trocar_perfil(1);
    sync_iniciar();
    pthread_mutex_lock(&trava);
    segurarCol = 0;
    pthread_cond_broadcast(&sinal);
    pthread_mutex_unlock(&trava);
    ateTerminar();
    ateTerminar();
    ciclo();                           // mais um ciclo do 2, ainda sem blob
    confere("idiomas do perfil 1 nao vazam para o 2", !strstr(idiomasConta, "\"en\""));
    printf("%s\n", falhas ? "FALHOU" : "PASSOU");
    return falhas ? 1 : 0;
  }
  if (argc > 1 && !strcmp(argv[1], "legconta")) {
    // #378: uma mudanca anterior em Ajustes deixou a protecao gravada. O blob
    // nao e aplicado (certo), mas os idiomas da conta tem de chegar a
    // linguas.c: e deles que "Da conta" vive, e eles nao sobrescrevem escolha
    // local nenhuma.
    printf("-- sessao: ajustes locais protegidos, conta com legenda en\n");
    dados_gravar("ajustes-locais.txt", "1\n");
    modoBlob = 1;
    escolher(1);
    ciclo();
    confere("blob protegido nao aplicado", blobsAplicados == 0);
    confere("idiomas da conta entregues mesmo protegido",
            strstr(idiomasConta, "subtitle_preferred_language") && strstr(idiomasConta, "\"en\""));
    // Revisao P2: troca de perfil solta o blob (sync_reaplicar_ajustes) e o
    // do perfil novo, do MESMO tamanho, tende a cair no mesmo endereco. A
    // deteccao de "blob novo" pelo ponteiro pulava os idiomas dele.
    { sync_reaplicar_ajustes();
      confere("troca de perfil limpa os idiomas da conta", idiomasChamadas >= 2 && !idiomasConta[0]);
      sync_proteger_ajustes_locais();
      legContaValor = "pt";
      ciclo();
      confere("blob do perfil novo entrega os idiomas dele",
              strstr(idiomasConta, "\"pt\"") != NULL); }
    printf("%s\n", falhas ? "FALHOU" : "PASSOU");
    return falhas ? 1 : 0;
  }
  if (argc > 1 && !strcmp(argv[1], "credencial")) {
    // O servidor recusa a credencial: a primeira tentativa sai, as seguintes
    // (renovacao do token, proximo ciclo) nao voltam a perguntar.
    int a, b, c;
    printf("-- sessao: servidor recusa credencial trakt/simkl\n");
    a = sync_empurrar_credencial("trakt", "{\"access_token\":\"x\"}");
    b = sync_empurrar_credencial("trakt", "{\"access_token\":\"y\"}");
    c = sync_empurrar_credencial("simkl", "{\"access_token\":\"z\"}");
    if (a != -1 || b != -1 || c != -1 || rpcCredencial != 2) {
      printf("  FALHOU: retornos %d %d %d, %d RPCs (esperado -1 -1 -1, 2)\n", a, b, c, rpcCredencial);
      return 1;
    }
    printf("  credencial recusada perguntada uma vez por provedor\n");
    return 0;
  }
  if (argc > 1 && !strcmp(argv[1], "credencial-reabrir")) {
    // Outro arranque, mesmo disco: a recusa gravada vale, nenhum RPC sai.
    int a = sync_empurrar_credencial("trakt", "{\"access_token\":\"x\"}");
    int c = sync_empurrar_credencial("simkl", "{\"access_token\":\"z\"}");
    printf("-- sessao: reabre depois da recusa\n");
    if (a != -1 || c != -1 || rpcCredencial != 0) {
      printf("  FALHOU: %d %d, %d RPCs (esperado -1 -1, 0)\n", a, c, rpcCredencial);
      return 1;
    }
    printf("  recusa lembrada do disco, sem pergunta\n");
    return 0;
  }
  if (argc > 1 && !strcmp(argv[1], "troca")) {
    // Perfil 1 ja escolhido; a pessoa volta ao 2 com o ciclo do 1 no ar. Nada
    // do ciclo do 1 pode ser aplicado no 2 (C9 do dono, 24/09: colecoes,
    // biblioteca, Trakt e 103 linhas de progresso do 1 dentro do 2).
    printf("-- sessao: no perfil 1, troca para o 2 com o ciclo do 1 no ar\n");
    escolher(1);
    segurarCol = 1;
    sync_iniciar();
    pthread_mutex_lock(&trava);
    while (!puxandoCol) pthread_cond_wait(&sinal, &trava);
    pthread_mutex_unlock(&trava);
    // Uma mudanca de ajuste feita NESTA TV no perfil 1, ainda sem subir.
    sync_proteger_ajustes_locais();
    escolher(2);
    sync_trocar_perfil(1);             // app.c, ramo da escolha
    { char *p1 = dados_ler("ajustes-locais-p1.txt"), *g = dados_ler("ajustes-locais.txt");
      confere("pendencia do perfil 1 guardada com ele, fora do 2", p1 && p1[0] == '1' && !g);
      free(p1); free(g); }
    confere("home do 2 ainda nao pronta com o ciclo do 1 no ar", !sync_perfil_pronto());
    sync_iniciar();                    // app.c, ramo da escolha: fio vivo
    confere("historico muda antes da resposta do perfil anterior",
            historicoPerfil == 2 && !strcmp(historicoDono, "conta-a"));
    pthread_mutex_lock(&trava);
    segurarCol = 0;
    pthread_cond_broadcast(&sinal);
    pthread_mutex_unlock(&trava);
    ateTerminar();
    ateTerminar();
    confere("colecoes do perfil 1 nao aplicadas no 2", colDoOutro == 0);
    confere("ciclo do perfil 2 rodou e aplicou as dele", colDoCerto > 0);
    confere("ordem final e a do perfil 2", !strcmp(catordem_chave(0), "xperience_movie_foryou"));
    confere("ciclo do 2 aplicado: home do 2 pronta", sync_perfil_pronto());
    // Volta ao 1: a pendencia dele volta a valer (e sai do arquivo do perfil).
    escolher(1);
    sync_trocar_perfil(2);
    { char *p1 = dados_ler("ajustes-locais-p1.txt"), *g = dados_ler("ajustes-locais.txt");
      confere("de volta ao 1: a mudanca local dele continua protegida", g && g[0] == '1' && !p1);
      free(p1); free(g); }
    confere("perfil trocado de novo: nao pronta ate o ciclo do 1", !sync_perfil_pronto());
    printf("%s\n", falhas ? "FALHOU" : "PASSOU");
    return falhas ? 1 : 0;
  }
  carregarAtivo();                     // main.c: perfis_carregar_ativo()
  printf("-- sessao: salvo=%d, escolhe=%d%s%s\n", ativo, alvo,
         segura ? ", responde com o 1o ciclo no ar" :
         tarde ? ", responde antes de sync_passo ver o fio acabar" : "",
         frio ? ", sem cache" : "");
  segurar = segura;
  sync_iniciar();                      // app_iniciar, com sessao gravada
  if (segura) esperarPuxando();
  else if (tarde) { int i; for (i = 0; i < 2000 && sync_estado() == SYNC_RODANDO; i++) usleep(1000); }
  else { ateTerminar(); quadros(2); }  // "[sync] ciclo interrompido"

  escolher(alvo);                      // perfilsel_concluido()
  sync_iniciar();                      // app.c, ramo da escolha

  if (!frio) {
    // ANTES de qualquer resposta de rede do perfil escolhido.
    confere("ordem do perfil escolhido ja na memoria antes da rede",
            rpcCatHome == 0 && !strcmp(catordem_chave(0), esperada));
    confere("home remontada nessa ordem antes da rede",
            remontagens > 0 && !strcmp(primeiraNaRemontagem, esperada));
  }
  remAntes = remontagens;
  rpcAntes = rpcCatHome;
  if (segura) soltar();
  ateTerminar();
  // O ciclo interrompido nao pode engolir o ciclo do perfil escolhido: se o
  // fio ainda estava vivo quando a pessoa respondeu, o ciclo completo tem de
  // partir quando ele acabar, e nao cinco minutos depois (sync_periodico).
  ateTerminar();
  confere("ciclo completo do perfil escolhido rodou", rpcCatHome > rpcAntes);
  confere("ordem final e a da conta", !strcmp(catordem_chave(0), esperada));
  if (frio)
    confere("sem cache: a resposta da conta remonta a home", remontagens > remAntes);
  else
    confere("mesma ordem na resposta da conta: nenhuma remontagem a mais",
            remontagens == remAntes);
  printf("%s\n", falhas ? "FALHOU" : "PASSOU");
  return falhas ? 1 : 0;
}

// sync.c 2.0 chama no logout; faltava aqui e o teste nao ligava (igual a contaoffline.c)
void psparede_esquecer(void) {}

__attribute__((weak)) int addons_perfil_da_lista(void) { return 0; }
