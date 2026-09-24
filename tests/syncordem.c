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
#include "addons.h"
#include "catordem.h"
#include "catordemcache.h"
#include "traktauth.h"
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
// Mesma semantica de perfis.c: `ativo` nasce do perfil.txt, `escolhido` e de
// sessao, a pergunta vale enquanto nao houver escolha nesta sessao.

static int ativo = 1, escolhido;
int  perfis_ativo(void)          { return ativo; }
int  perfis_ativo_addons(void)   { return ativo; }
int  perfis_n(void)              { return 2; }
int  perfis_precisa_escolher(void) { return !escolhido; }
const char *perfis_dono(void)    { return "dono-a"; }
void perfis_esquecer(void)       { ativo = 1; escolhido = 0; }
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
char *sessao_rpc(const char *funcao, const char *corpo, int *st) {
  *st = 200;
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
char *sessao_tabela(const char *t, const char *q, int *st) { (void)t; (void)q; *st = 200; return strdup("[]"); }
int  sessao_logada(void)          { return 1; }
const char *sessao_usuario(void)  { return "conta-a"; }
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
void desc_repetir(void) { repeticoes++; }
void desc_repetir_addons(void) { repeticoes++; }
void desc_refazer_continuar(void) {}
void desc_esquecer(void) {}
void desc_tmdb_definir(const char *c) { (void)c; }

// ------------------------------------------------------------ o resto, mudo

int  addons_definir_lista(const AddonRemoto *l, int n) { (void)l; (void)n; return 0; }
void addons_esquecer(void) {}
int  addons_exportar(AddonRemoto *s, int m) { (void)s; (void)m; return 0; }
void agenda_esquecer(void) {}
int  ajustes_aplicar_blob(const char *j) { (void)j; return 0; }
void ajustes_definir_ocultar_nao_lancados(int l) { (void)l; }
int  ajustes_mesclar_blob(const char *b, char **s) { (void)b; *s = NULL; return 0; }
void buscasrec_esquecer(void) {}
void cachearte_limpar_referencias(void) {}
int  cat_apagar_cache(void) { return 0; }
static int colDoOutro, colDoCerto;
int  col_definir_json(const char *j) {
  if (j && strstr(j, "perfil1")) colDoOutro++;
  if (j && strstr(j, "perfil2")) colDoCerto++;
  return 0;
}
int  contalib_aplicar_catalogo(void) { return 0; }
int  contalib_aplicar_vistos(void) { return 0; }
void contalib_esquecer(void) {}
int  contalib_ler_biblioteca(const char *j) { (void)j; return 0; }
int  contalib_ler_vistos(const char *j) { (void)j; return 0; }
void contalib_reconciliar(void) {}
void debrid_definir_chave(const char *s, const char *c) { (void)s; (void)c; }
void debrid_esquecer(void) {}
void extras_definir_chave(const char *c) { (void)c; }
void fontepref_esquecer(void) {}
void homeestado_esquecer(void) {}
void mapa_esquecer(void) {}
void prog_esquecer_tudo(void) {}
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
    escolher(2);
    sync_iniciar();                    // app.c, ramo da escolha: fio vivo
    pthread_mutex_lock(&trava);
    segurarCol = 0;
    pthread_cond_broadcast(&sinal);
    pthread_mutex_unlock(&trava);
    ateTerminar();
    ateTerminar();
    confere("colecoes do perfil 1 nao aplicadas no 2", colDoOutro == 0);
    confere("ciclo do perfil 2 rodou e aplicou as dele", colDoCerto > 0);
    confere("ordem final e a do perfil 2", !strcmp(catordem_chave(0), "xperience_movie_foryou"));
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
