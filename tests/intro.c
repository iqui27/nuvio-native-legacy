// LEITURA DOS MARCADORES DO TheIntroDB.
//
// As cargas abaixo sao RESPOSTAS REAIS da API, capturadas com curl antes de o
// leitor existir — nao invencao minha. Sao elas que fixam as tres coisas que
// mudaram junto com a fonte e que dariam erro silencioso se trocadas:
//
//   1. as chaves valem ARRAY e nao objeto (um tipo pode ter varios trechos);
//   2. os tempos vem em MILISSEGUNDOS (segundos daria um numero mil vezes
//      errado sem parecer errado);
//   3. `null` tem significado: start = 0, end = "ate o fim da midia".
//
//   bash tests/intro.sh
#include "../src/intro.h"
#include "../src/capmkv.h"
#include "../src/credfonte.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <utime.h>
#include <time.h>
#include <sys/resource.h>

// DUBLE DE REDE. intro.c pede pela rede_pedir; o teste troca o buscador
// (intro_definir_buscador) por um que serve as respostas GRAVADAS de
// tests/fixtures. Os dois simbolos abaixo so existem para linkar.
#include "../src/rede.h"
#include "nvtmp.h"
static char dadosTeste[NVT_MAX];
const char *dados_dir(void){return dadosTeste;}
char *dados_caminho(char *dst,unsigned cap,const char *nome) {
  if(!dadosTeste[0] || !nome || !nome[0])return NULL;
  snprintf(dst,cap,"%s/%s",dadosTeste,nome);return dst;
}
void dados_fs_travar(void){}
void dados_fs_liberar(void){}
void dados_marcar_sujo(int leve){(void)leve;}
static int fakeBuscar(const char *,char **,int *);
static const char *fribbFonte;
int rede_pedir(const RedePedido *p, RedeResposta *r) {
  char *j=NULL;memset(r,0,sizeof *r);
  if(!p->receber_corpo)return 0;
  if(fribbFonte){
    FILE *f=fopen(fribbFonte,"rb");assert(f);char bloco[16384];size_t n;int ok=1;
    while((n=fread(bloco,1,sizeof bloco,f))&&ok)ok=p->receber_corpo(bloco,n,p->corpo_usuario);
    assert(!ferror(f));fclose(f);r->status=200;return ok;
  }
  fakeBuscar(p->url,&j,&r->status);
  int ok=1;
  // Cortes de 7 bytes atravessam aspas, campos, escapes e objetos.
  for(size_t i=0;j&&j[i]&&ok;){size_t n=strlen(j+i);if(n>7)n=7;
    ok=p->receber_corpo(j+i,n,p->corpo_usuario);i+=n;}
  free(j);return ok;
}
void rede_resposta_limpar(RedeResposta *r) { (void)r; }
int mkvass_cabecalho(const char *url, unsigned char **buf, long *n) {
  (void)url; (void)buf; (void)n; return 0;
}
char *rede_baixar_trecho_st(const char *url, int s, long long ini, long long fim,
                           long *tam, int *status, int *erro, char *final, unsigned tamFinal) {
  (void)url; (void)s; (void)ini; (void)fim; (void)status; (void)erro; (void)final; (void)tamFinal;
  *tam = 0; return NULL;
}
char *rede_baixar_trecho(const char *url, int s, long ini, long fim, long *tam) {
  (void)url; (void)s; (void)ini; (void)fim; *tam = 0; return NULL;
}

// Le uma resposta gravada de tests/fixtures/theintrodb (chamadas reais a
// api.theintrodb.org/v3/media em 07/10/2026, sem chave). Estatico: cabe.
static const char *fixture(const char *nome) {
  static char buf[4][2048]; static int k;
  char caminho[256]; FILE *f; size_t n;
  char *b = buf[k++ & 3];
  snprintf(caminho, sizeof caminho, "tests/fixtures/theintrodb/%s", nome);
  f = fopen(caminho, "rb");
  if (!f) { fprintf(stderr, "fixture ausente: %s\n", caminho); exit(1); }
  n = fread(b, 1, sizeof buf[0] - 1, f); fclose(f); b[n] = 0;
  return b;
}
static double credFinal(const IntroTrecho *v, int n) {
  double s = 0.0; int i;
  for (i = 0; i < n; i++) if (v[i].tipo == INTRO_CREDITOS && v[i].inicio > s) s = v[i].inicio;
  return s;
}

// O BUSCADOR FALSO: cada URL vira uma fixture ou um status. Guarda a ultima URL.
static pthread_mutex_t fakeTrava = PTHREAD_MUTEX_INITIALIZER;
static char ultimaUrl[400], ultimaAni[400], ultimaBackup[400];
static int fakeStatus500;
static int mapPedidos, aniPedidos, dbPedidos, mapFora, backupPedidos;
static int backupErro,backupBloquear,backupEntrou,backupLiberar;
static pthread_cond_t backupCond=PTHREAD_COND_INITIALIZER;
static int fakeBuscar(const char *url, char **corpo, int *status) {
  const char *arq = NULL;
  pthread_mutex_lock(&fakeTrava);
  if(strstr(url,"api.theintrodb.org"))snprintf(ultimaUrl, sizeof ultimaUrl, "%s", url);
  pthread_mutex_unlock(&fakeTrava);
  *corpo = NULL;
  pthread_mutex_lock(&fakeTrava);
  if (strstr(url, "Fribb/anime-lists")) mapPedidos++;
  if (strstr(url, "api.aniskip.com")) { aniPedidos++; snprintf(ultimaAni, sizeof ultimaAni, "%s", url); }
  if (strstr(url, "api.theintrodb.org")) dbPedidos++;
  if (strstr(url, "api.introdb.app")) {backupPedidos++;snprintf(ultimaBackup,sizeof ultimaBackup,"%s",url);}
  pthread_mutex_unlock(&fakeTrava);
  // Casos de integracao do backup, isolados das fixtures antigas.
  if(strstr(url,"tt990000")||strstr(url,"tmdb_id=9900001")){
    int backup=strstr(url,"api.introdb.app")!=NULL;
    if(backup){
      pthread_mutex_lock(&fakeTrava);
      if(backupBloquear){backupEntrou=1;pthread_cond_broadcast(&backupCond);
        while(!backupLiberar)pthread_cond_wait(&backupCond,&fakeTrava);}
      int erro=backupErro;pthread_mutex_unlock(&fakeTrava);
      if(erro==500){*status=500;return 500;}
      char id[24]="";const char *p=strstr(url,"imdb_id=");assert(p);
      sscanf(p+8,"%23[^&]",id);
      char b[1024];snprintf(b,sizeof b,
        "{\"imdb_id\":\"%s\",\"media_type\":\"tv\",\"is_movie\":false,\"season\":1,\"episode\":2,"
        "\"intro\":{\"start_sec\":20,\"end_sec\":80},\"recap\":{\"start_ms\":0,\"end_ms\":8000},"
        "\"outro\":{\"start_sec\":2900,\"end_sec\":2960}}",erro==200?"tt0000000":id);
      *corpo=strdup(b);*status=200;return 200;
    }
    if(strstr(url,"tt9900002")){*status=404;return 404;}
    if(strstr(url,"tt9900003")){
      *corpo=strdup("{\"season\":1,\"episode\":2,\"intro\":[{\"start_ms\":10000,\"end_ms\":70000}],"
        "\"recap\":[{\"start_ms\":0,\"end_ms\":8000}],\"credits\":[{\"start_ms\":2900000,\"end_ms\":2960000}]}");
    }else{
      *corpo=strdup(strstr(url,"tmdb_id=9900001")?
        "{\"season\":4,\"episode\":92,\"intro\":[{\"start_ms\":10000,\"end_ms\":70000}]}":
        "{\"season\":1,\"episode\":2,\"intro\":[{\"start_ms\":10000,\"end_ms\":70000}]}");
    }
    *status=200;return 200;
  }
  if(strstr(url,"api.introdb.app")){*status=404;return 404;}
  if (strstr(url, "Fribb/anime-lists")) {
    *status = mapFora ? 503 : 200;
    if (!mapFora) *corpo = strdup(
      "[{\"imdb_id\":[\"\"],\"mal_id\":539,\"themoviedb_id\":{\"tv\":43004},\"season\":{\"tvdb\":1,\"tmdb\":1}},"
      "{\"ignored\":\"brace } and escaped \\\" quote\",\"imdb_id\":[\"tt5607616\"],\"mal_id\":31240,\"themoviedb_id\":{\"tv\":65942},\"season\":{\"tvdb\":1,\"tmdb\":1}},"
      "{\"imdb_id\":[\"tt5607616\"],\"mal_id\":39587,\"themoviedb_id\":{\"tv\":65942},\"season\":{\"tvdb\":2,\"tmdb\":1},\"episode_offset\":{\"tmdb\":26}},"
      "{\"imdb_id\":[\"tt5607616\",\"tt5607617\"],\"mal_id\":42203,\"themoviedb_id\":{\"tv\":65942},\"season\":{\"tvdb\":2,\"tmdb\":1},\"episode_offset\":{\"tvdb\":13,\"tmdb\":38}}]");
    return *status;
  }
  if (strstr(url, "skip-times/39587/1?") || strstr(url, "skip-times/42203/1?")) {
    *corpo = strdup("{\"results\":[{\"skipType\":\"op\",\"interval\":{\"startTime\":10,\"endTime\":100},\"episodeLength\":1440},{\"skipType\":\"ed\",\"interval\":{\"startTime\":1350,\"endTime\":1440},\"episodeLength\":1440}]}");
    *status = 200; return 200;
  }
  if (strstr(url, "imdb_id=tt4300000")) arq = "theintrodb/bb_s01e01.json";
  if (fakeStatus500) { *status = 500; return 500; }
  if (strstr(url, "tmdb_id=37854&season=4&episode=92")) arq = "theintrodb/onepiece_tmdb_s04e92.json";
  else if (strstr(url, "imdb_id=tt0388629&season=4&episode=1&") || strstr(url, "imdb_id=tt0388629&season=4&episode=1\0"))
    arq = "theintrodb/onepiece_imdb_s04e01_remap.json";
  else if (strstr(url, "imdb_id=tt0903747&season=1&episode=1")) arq = "theintrodb/bb_s01e01.json";
  else if (strstr(url, "imdb_id=tt1375666")) arq = "theintrodb/inception.json";
  else if (strstr(url, "kitsu.io/api/edge/anime/12/mappings")) arq = "kitsu12_mappings.json";
  else if (strstr(url, "aniskip.com/v2/skip-times/21/1?")) arq = "aniskip/onepiece_mal21_ep1_len1500.json";
  if (!arq) { *status = 404; return 404; }
  { char c[256]; FILE *f; long n;
    snprintf(c, sizeof c, "tests/fixtures/%s", arq);
    f = fopen(c, "rb"); assert(f);
    fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
    *corpo = malloc((size_t)n + 1); assert(*corpo);
    n = (long)fread(*corpo, 1, (size_t)n, f); (*corpo)[n] = 0; fclose(f); }
  *status = 200; return 200;
}
// Espera o fio do pedido terminar (ate 3 s): devolve quantos trechos ha.
static int esperar(int minimo) {
  IntroTrecho t[8]; int i, n = 0;
  for (i = 0; i < 300; i++) { n = intro_trechos(t, 8); if (n >= minimo && minimo > 0) break; usleep(10000); }
  return n;
}

static const IntroTrecho *achar(const IntroTrecho *v, int n, int tipo) {
  int i;
  for (i = 0; i < n; i++) if (v[i].tipo == tipo) return &v[i];
  return NULL;
}

// Compara em milissegundos para a assercao nao depender de ponto flutuante.
static int mesmoSeg(double a, double esperado) {
  double d = a - esperado;
  return d < 0.001 && d > -0.001;
}

static void testarBackup(void){
  IntroTrecho v[8];char url[256];int n,antes;
  const char *j="{\"imdb_id\":\"tt0460681\",\"media_type\":\"tv\",\"is_movie\":false,\"season\":1,\"episode\":1,"
    "\"intro\":{\"start_sec\":150,\"end_sec\":210},\"recap\":null,\"outro\":{\"start_ms\":2737000,\"end_ms\":2785000}}";
  assert(intro_montar_url_backup(url,sizeof url,"tt0460681",1,1));
  assert(!strcmp(url,"https://api.introdb.app/segments?imdb_id=tt0460681&season=1&episode=1"));
  assert(intro_montar_url_backup(url,sizeof url,"tt0460681",0,1)&&strstr(url,"season=0&episode=1"));
  assert(intro_montar_url_backup(url,sizeof url,"tt1375666",0,0)&&strstr(url,"is_movie=true"));
  assert(!intro_montar_url_backup(url,sizeof url,"tt123&season=9",1,1)&&!url[0]);
  assert(!intro_montar_url_backup(url,8,"tt0460681",1,1)&&!url[0]);
  assert(!intro_montar_url_backup(url,sizeof url,"tmdb:123",1,1));
  n=intro_extrair_backup(j,"tt0460681",1,1,v,8);
  assert(n==2&&v[0].inicio==150&&v[0].fim==210&&v[1].inicio==2737&&v[1].fim==2785);
  assert(intro_extrair_backup(j,"tt0460681",1,2,v,8)==-1);
  assert(intro_extrair_backup(j,"tt0000000",1,1,v,8)==-1);
  assert(intro_extrair_backup(j,"tt0460681",0,0,v,8)==-1);
  assert(intro_extrair_backup("{\"imdb_id\":\"tt0460681\"","tt0460681",1,1,v,8)==-1);
  const char *movie="{\"imdb_id\":\"tt1375666\",\"media_type\":\"movie\",\"is_movie\":true,"
    "\"outro\":{\"start_sec\":8000,\"end_sec\":8500},\"post_credits\":{\"start_sec\":8300,\"end_sec\":8400}}";
  assert(intro_extrair_backup(movie,"tt1375666",0,0,v,8)==1&&v[0].fim==8300);
  const char *invalid="{\"imdb_id\":\"tt0460681\",\"media_type\":\"tv\",\"is_movie\":false,\"season\":1,\"episode\":1,"
    "\"intro\":{\"start_sec\":-1,\"end_sec\":20},\"recap\":{\"start_sec\":20,\"end_sec\":20},\"outro\":{\"start_sec\":2700,\"end_sec\":null}}";
  assert(intro_extrair_backup(invalid,"tt0460681",1,1,v,8)==0);
  puts("ok  IntroDB /segments: identidade, segundos/ms, especiais, intervalos e cena pos-creditos");

  intro_definir_buscador(fakeBuscar);
  intro_pedir_ids_mapeados("tt9900001",9900001,4,92,1,2,0,0);
  assert(esperar(3)==3);n=intro_trechos(v,8);
  assert(achar(v,n,INTRO_ABERTURA)->inicio==10); // prioridade do principal
  assert(achar(v,n,INTRO_RESUMO)->fim==8&&achar(v,n,INTRO_CREDITOS)->fim==2960);
  pthread_mutex_lock(&fakeTrava);
  assert(strstr(ultimaUrl,"tmdb_id=9900001&season=4&episode=92"));
  assert(strstr(ultimaBackup,"imdb_id=tt9900001&season=1&episode=2"));antes=backupPedidos;
  pthread_mutex_unlock(&fakeTrava);
  intro_definir_duracao(3000,0);usleep(200000);
  pthread_mutex_lock(&fakeTrava);assert(backupPedidos==antes);pthread_mutex_unlock(&fakeTrava);
  assert(intro_creditos_limitados(v,8)==1&&v[0].fim==2960);
  double fim;int tipo;
  assert(intro_auto_ativo(2910,&fim,&tipo)&&tipo==INTRO_CREDITOS&&fim==2960);
  assert(intro_auto_pular(tipo,fim,3000,0,0,1));
  IntroTrecho cap={30,90,INTRO_ABERTURA,0};intro_definir_capitulos(&cap,1);
  n=intro_trechos(v,8);assert(achar(v,n,INTRO_ABERTURA)->inicio==30);

  intro_pedir_ids("tt9900002",0,1,2,0,0);assert(esperar(3)==3);
  n=intro_trechos(v,8);assert(achar(v,n,INTRO_ABERTURA)->inicio==20);
  intro_pedir_ids("tt9900003",0,1,2,0,0);assert(esperar(3)==3);usleep(100000);
  pthread_mutex_lock(&fakeTrava);assert(backupPedidos==antes+1);backupErro=500;pthread_mutex_unlock(&fakeTrava);
  intro_pedir_ids("tt9900004",0,1,2,0,0);assert(esperar(1)==1);usleep(100000);
  pthread_mutex_lock(&fakeTrava);backupErro=0;pthread_mutex_unlock(&fakeTrava);
  intro_pedir_ids("tt9900004",0,1,2,0,0);assert(esperar(3)==3);
  pthread_mutex_lock(&fakeTrava);backupErro=200;pthread_mutex_unlock(&fakeTrava);
  intro_pedir_ids("tt9900005",0,1,2,0,0);assert(esperar(1)==1);usleep(100000);
  pthread_mutex_lock(&fakeTrava);backupErro=0;pthread_mutex_unlock(&fakeTrava);
  intro_pedir_ids("tt9900005",0,1,2,0,0);assert(esperar(3)==3);
  puts("ok  backup preenche tipos ausentes; principal/capitulos preservados; cache e falhas recuperaveis");

  pthread_mutex_lock(&fakeTrava);backupBloquear=1;backupLiberar=backupEntrou=0;pthread_mutex_unlock(&fakeTrava);
  intro_pedir_ids("tt9900006",0,1,2,0,0);
  assert(esperar(1)==1); // principal ja disponivel durante GET do backup
  pthread_mutex_lock(&fakeTrava);
  while(!backupEntrou)pthread_cond_wait(&backupCond,&fakeTrava);
  pthread_mutex_unlock(&fakeTrava);
  intro_desligar();
  pthread_mutex_lock(&fakeTrava);backupLiberar=1;pthread_cond_broadcast(&backupCond);pthread_mutex_unlock(&fakeTrava);
  usleep(200000);assert(intro_trechos(v,8)==0);
  pthread_mutex_lock(&fakeTrava);backupBloquear=0;pthread_mutex_unlock(&fakeTrava);
  puts("ok  backup lento nao atrasa marcadores existentes nem publica depois de sair do video");
}

int main(int argc,char **argv) {
  IntroTrecho v[8];
  int n;
  nvt_set(dadosTeste, sizeof dadosTeste, "/nuvio-intro-XXXXXX"); assert(mkdtemp(dadosTeste));
  if(argc==2){ // Medicao reproduzivel com captura real, sem buffer da lista.
    fribbFonte=argv[1];intro_definir_buscador(fakeBuscar);
    intro_registrar_tmdb("tt5607616",65942,"{\"genres\":[{\"id\":16}],\"original_language\":\"ja\"}");
    intro_pedir_ids("tt5607616",65942,1,39,0,0);assert(esperar(2)==2);
    char caminho[256];struct stat st;
    snprintf(caminho,sizeof caminho,"%s/anime-fribb.idx",dadosTeste);
    assert(!stat(caminho,&st));printf("Fribb index: %lld bytes\n",(long long)st.st_size);
    struct rusage uso;assert(!getrusage(RUSAGE_SELF,&uso));
    printf("Peak RSS: %ld %s (parser + fixture streamed from disk)\n",uso.ru_maxrss,
#ifdef __APPLE__
           "bytes"
#else
           "KiB"
#endif
    );
    intro_desligar();assert(!unlink(caminho));assert(!rmdir(dadosTeste));return 0;
  }

  // Lixo no buffer MKV nao pode atribuir os creditos ao AniSkip.
  { MkvCap caps[] = {{0, "Prologue"}, {60, "Opening"}, {150, "Part A"},
                     {1380, "Ending"}, {1450, "Next Episode Preview"}};
    for (int lixo = 0; lixo < 2; lixo++) {
      CredEntrada e = {.dur = 1500}; int fonte;
      intro_desligar(); intro_definir_duracao(e.dur, 0);
      memset(v, lixo ? 0xa5 : 0, sizeof v);
      n = capmkv_trechos(caps, 5, v, 8);
      assert(n == 3);
      intro_definir_capitulos(v, n);
      double ic = intro_creditos_seg();
      if (intro_creditos_aniskip()) e.aniskip = ic; else e.introdb = ic;
      assert(cred_escolher(&e, &fonte) == 1380 && fonte == CRED_FONTE_INTRODB);
      for (int i = 0; i < n; i++) assert(v[i].ani == 0);
    }
    intro_desligar();
    puts("ok  lixo no buffer MKV nao muda a decisao dos creditos");
  }

  // v4 OpenAPI: GET /media, movie-media example; no tmdb_id in the body.
  n = intro_extrair("{\"tvdb_id\":169,\"type\":\"movie\","
                    "\"intro\":[{\"start_ms\":null,\"end_ms\":23000}],"
                    "\"recap\":[{\"start_ms\":25000,\"end_ms\":134000}],"
                    "\"credits\":[{\"start_ms\":5801777,\"end_ms\":6371111},"
                                   "{\"start_ms\":6408000,\"end_ms\":null}],"
                    "\"preview\":[{\"start_ms\":1680000,\"end_ms\":1740000}]}", v, 8);
  assert(n == 5);
  assert(v[0].tipo == INTRO_ABERTURA && v[0].inicio == 0 && v[0].fim == 23);
  assert(v[1].tipo == INTRO_RESUMO && v[1].inicio == 25 && v[1].fim == 134);
  assert(v[2].tipo == INTRO_CREDITOS && mesmoSeg(v[2].inicio, 5801.777) && mesmoSeg(v[2].fim, 6371.111));
  assert(v[3].tipo == INTRO_CREDITOS && v[3].inicio == 6408 && v[3].fim == 0);
  assert(v[4].tipo == INTRO_PREVIA && v[4].inicio == 1680 && v[4].fim == 1740);
  // v4 OpenAPI: episode-media example; coordinates stay in the caller's order.
  { const char *j = "{\"tvdb_id\":71663,\"type\":\"tv\",\"season\":1,\"episode\":1,"
                    "\"intro\":[{\"start_ms\":30000,\"end_ms\":90000},"
                                "{\"start_ms\":120000,\"end_ms\":150000}]}";
    assert(intro_resposta_confere(j, 1, 1));
    assert(!intro_resposta_confere(j, 1, 2));
    assert(intro_extrair(j, v, 8) == 2);
    assert(v[0].tipo == INTRO_ABERTURA && v[0].inicio == 30 && v[0].fim == 90);
    assert(v[1].tipo == INTRO_ABERTURA && v[1].inicio == 120 && v[1].fim == 150); }
  puts("ok  v4 OpenAPI samples: TVDB identity, all segments, caller coordinates");

  // FILME: /v3/media?imdb_id=tt0111161 (Shawshank). So creditos, sem fim.
  memset(v, 0xa5, sizeof v);
  n = intro_extrair("{\"tmdb_id\":278,\"type\":\"movie\","
                    "\"credits\":[{\"start_ms\":8300000,\"end_ms\":null}]}", v, 8);
  assert(n == 1 && v[0].ani == 0);
  { const IntroTrecho *c = achar(v, n, INTRO_CREDITOS);
    assert(c);
    assert(mesmoSeg(c->inicio, 8300.0));   // 8.300.000 ms, e nao 8.300.000 s
    // fim ZERO e o contrato de "ate o fim da midia" (end_ms nulo). Sem isto,
    // um fim 0 seria lido como trecho vazio e o marcador sumiria.
    assert(c->fim == 0.0); }
  puts("ok  filme: creditos em milissegundos, fim nulo vira 'ate o fim'");

  // FILME com abertura e SEM creditos: /v3/media?imdb_id=tt0816692
  // (Interstellar). start_ms nulo = comeca no zero.
  n = intro_extrair("{\"tmdb_id\":157336,\"type\":\"movie\","
                    "\"intro\":[{\"start_ms\":null,\"end_ms\":53000}]}", v, 8);
  assert(n == 1);
  { const IntroTrecho *a = achar(v, n, INTRO_ABERTURA);
    assert(a && mesmoSeg(a->inicio, 0.0) && mesmoSeg(a->fim, 53.0));
    assert(!achar(v, n, INTRO_CREDITOS)); }
  puts("ok  filme: start nulo vira zero, e ausencia de creditos nao inventa um");

  // SERIE: /v3/media?imdb_id=tt14688458&season=1&episode=1 (Silo T1E1).
  n = intro_extrair("{\"tmdb_id\":125988,\"type\":\"tv\",\"season\":1,\"episode\":1,"
                    "\"intro\":[{\"start_ms\":272500,\"end_ms\":366000}],"
                    "\"credits\":[{\"start_ms\":3503000,\"end_ms\":null}]}", v, 8);
  assert(n == 2);
  { const IntroTrecho *a = achar(v, n, INTRO_ABERTURA);
    const IntroTrecho *c = achar(v, n, INTRO_CREDITOS);
    assert(a && mesmoSeg(a->inicio, 272.5) && mesmoSeg(a->fim, 366.0));
    assert(c && mesmoSeg(c->inicio, 3503.0)); }
  puts("ok  serie: abertura e creditos na mesma resposta");

  // MAIS DE UM TRECHO DO MESMO TIPO — e a razao de a chave ser array. Um leitor
  // que so pegasse o primeiro elemento passaria nos casos acima e falharia aqui.
  n = intro_extrair("{\"type\":\"tv\",\"intro\":["
                    "{\"start_ms\":1000,\"end_ms\":2000},"
                    "{\"start_ms\":5000,\"end_ms\":6000}]}", v, 8);
  assert(n == 2);
  assert(mesmoSeg(v[0].inicio, 1.0) && mesmoSeg(v[1].inicio, 5.0));
  puts("ok  duas aberturas no mesmo episodio entram as duas");

  // RESUMO tem tipo proprio: o player pula abertura e resumo, mas o posplay so
  // se importa com creditos. Trocar os tipos faria o painel subir na abertura.
  n = intro_extrair("{\"type\":\"tv\",\"recap\":[{\"start_ms\":0,\"end_ms\":30000}]}", v, 8);
  assert(n == 1 && v[0].tipo == INTRO_RESUMO);
  puts("ok  recap vira INTRO_RESUMO e nao abertura");

  // TRECHO INVERTIDO OU VAZIO NAO ENTRA. Vem de dado errado na base, e um
  // trecho com fim <= inicio deixaria intro_ativo verdadeiro para sempre.
  n = intro_extrair("{\"type\":\"tv\",\"intro\":["
                    "{\"start_ms\":9000,\"end_ms\":9000},"
                    "{\"start_ms\":9000,\"end_ms\":1000}]}", v, 8);
  assert(n == 0);
  puts("ok  trecho vazio ou invertido e descartado");

  // O QUE A API RESPONDE QUANDO NAO CONHECE O TITULO, e o resto do lixo.
  assert(intro_extrair("{\"error\":\"media not found\"}", v, 8) == 0);
  assert(intro_extrair("{}", v, 8) == 0);
  assert(intro_extrair("nao e json", v, 8) == 0);
  assert(intro_extrair("", v, 8) == 0);
  assert(intro_extrair(NULL, v, 8) == 0);
  assert(intro_extrair("{\"credits\":[{\"start_ms\":1000,\"end_ms\":2000}]}", v, 0) == 0);
  puts("ok  404, vazio, lixo e max=0 devolvem zero sem estourar");

  // JANELA RECUSADA / ACEITA (#202): creditos "ate o fim" com inicio cedo
  // demais deixavam o botao 25-30 min na tela.
  {
    const char *m;
    assert(intro_janela_ok(INTRO_CREDITOS, 8300, 0, 8520, 1, &m) == 1);       // Shawshank
    assert(intro_janela_ok(INTRO_CREDITOS, 5400, 0, 8400, 1, &m) == 0);       // 50 min de janela
    assert(intro_janela_ok(INTRO_CREDITOS, 3000, 0, 8400, 1, &m) == 0);       // antes de 50%
    // 2.0.3: 18 min antes do fim ja nao e a parte final do filme (janela 12%, max 15 min)
    assert(intro_janela_ok(INTRO_CREDITOS, 7300, 7900, 8400, 1, &m) == 0);
    assert(intro_janela_ok(INTRO_CREDITOS, 7600, 8200, 8400, 1, &m) == 1);    // 13 min antes do fim
    assert(intro_janela_ok(INTRO_CREDITOS, 3503, 0, 3600, 0, &m) == 1);       // serie
    assert(intro_janela_ok(INTRO_ABERTURA, 60, 400, 3600, 0, &m) == 0);       // > 3 min
    assert(intro_janela_ok(INTRO_ABERTURA, 272, 366, 3600, 0, &m) == 1);
    assert(intro_janela_ok(INTRO_CREDITOS, 100, 0, 0, 1, &m) == 0);           // sem duracao: nao aceita
    // Silo T2E6, TCL 08/10 11:46:40: creditos 2519-2582 aceitos com dur=0s (a
    // duracao do video ainda nao tinha chegado). Sem duracao nao ha como
    // saber se o marcador e deste corte; aceitar e confiar so no fim do proprio
    // marcador. Com a duracao real a janela decide (e re-decide: valido()).
    assert(intro_janela_ok(INTRO_CREDITOS, 2519.08, 2582.106, 0, 0, &m) == 0);
    assert(intro_janela_ok(INTRO_PREVIA, 2519.08, 2582.106, 0, 0, &m) == 0);
    assert(intro_janela_ok(INTRO_CREDITOS, 2519.08, 2582.106, 2582.144, 0, &m) == 1);  // corte de 2582 s
    assert(intro_janela_ok(INTRO_CREDITOS, 2519.08, 2582.106, 3077.0, 0, &m) == 0);    // corte de 3077 s
    assert(intro_janela_ok(INTRO_ABERTURA, 215, 292, 0, 0, &m) == 1);                  // abertura segue sem duracao
    puts("ok  guarda de janela");
  }
#ifdef NV_SHOT_HOOKS
  {
    // AUTO-HIDE: aparece sozinho, some em 10 s, volta so com os controles, e
    // nao reaparece sozinho. Creditos de filme: 7700 s ate o fim (dur 8400).
    IntroTrecho t[1] = {{7700, 0, INTRO_CREDITOS}};
    double fim; int tipo;
    intro_shot_definir(t, 1);
    intro_definir_duracao(8400, 1);
    assert(!intro_botao(7000, 100.0, 0, 0, &fim, &tipo));      // fora da janela
    assert(intro_botao(7701, 101.0, 0, 0, &fim, &tipo) && tipo == INTRO_CREDITOS);
    assert(intro_botao(7705, 105.0, 0, 0, &fim, &tipo));       // ainda 4 s
    assert(!intro_botao(7712, 112.0, 0, 0, &fim, &tipo));      // 11 s: sumiu
    assert(!intro_botao_visivel(NULL, NULL));                  // tecla nao pula as cegas
    assert(!intro_botao(8000, 700.0, 0, 0, &fim, &tipo));      // 25 min depois: nada
    assert(intro_botao(8000, 701.0, 1, 0, &fim, &tipo));       // controles: volta
    assert(intro_botao_visivel(&fim, &tipo));
    assert(!intro_botao(8001, 702.0, 0, 0, &fim, &tipo));      // controles somem: some
    // focado nao expira
    assert(intro_botao(8002, 900.0, 0, 1, &fim, &tipo));
    assert(intro_botao(8003, 905.0, 0, 1, &fim, &tipo));
    // creditos recusados (inicio cedo): botao nunca aparece, nem com controles
    {
      IntroTrecho r[1] = {{3000, 0, INTRO_CREDITOS}};
      intro_shot_definir(r, 1);
      assert(!intro_botao(3500, 1000.0, 1, 0, &fim, &tipo));
      assert(!intro_ativo(3500, &fim, &tipo));
    }
    puts("ok  botao some em 10 s, volta com controles, janela recusada nao aparece");
  }
#endif


  // --- 2.0.3: RESPOSTAS REAIS GRAVADAS + SANIDADE CONTRA A DURACAO ------------
  //
  // Duracoes: arquivo tipico de cada episodio (Breaking Bad T1E1 58:01, T1E2
  // 48:06; Silo T1E1 59 min pelo TVmaze; AoT 24:00; One Piece T1E1 25:00, que e
  // o end_ms da previa). O que se prova: unidade (ms -> s), o marcador cai nos
  // creditos DESTE episodio, e o marcador de OUTRO episodio/corte e recusado.
  { struct { const char *arq; int t, e; double dur, credEsperado; } fx[] = {
      { "bb_s01e01.json",       1, 1, 3481.0, 3431.0 },
      { "bb_s01e02.json",       1, 2, 2886.0, 2839.0 },
      { "silo_s01e01.json",     1, 1, 3540.0, 3503.0 },
      { "aot_s02e01.json",      2, 1, 1440.0, 1330.079 },
      { "onepiece_s01e01.json", 1, 1, 1500.0, 1389.0 },
    };
    const char *m;
    for (size_t k = 0; k < sizeof fx / sizeof *fx; k++) {
      const char *j = fixture(fx[k].arq);
      double c, resta;
      int i;
      assert(intro_resposta_confere(j, fx[k].t, fx[k].e));
      n = intro_extrair(j, v, 8);
      assert(n >= 2);
      c = credFinal(v, n);
      assert(mesmoSeg(c, fx[k].credEsperado));
      resta = fx[k].dur - c;
      // creditos nos ultimos 40-120 s: segundos, nao ms nem minutos
      assert(resta > 30.0 && resta < 130.0);
      for (i = 0; i < n; i++)
        if (!intro_janela_ok(v[i].tipo, v[i].inicio, v[i].fim, fx[k].dur, 0, &m)) {
          fprintf(stderr, "FALHOU %s trecho %d tipo %d: %s\n", fx[k].arq, i, v[i].tipo, m);
          assert(0);
        }
      printf("ok  %-22s creditos %.0fs de %.0fs (sobram %.0fs, janela %.0fs)\n",
             fx[k].arq, c, fx[k].dur, resta, intro_creditos_janela(fx[k].dur));
    }
    // MARCADOR DE OUTRO EPISODIO: o de T1E2 (2839 s) tocando T1E1 (3481 s)
    // sobra 642 s — meio do terceiro ato, recusado; o de T1E1 (3431 s) num
    // arquivo de T1E2 (2886 s) passa do fim.
    assert(!intro_janela_ok(INTRO_CREDITOS, 2839.0, 0, 3481.0, 0, &m));
    assert(!intro_janela_ok(INTRO_CREDITOS, 3431.0, 0, 2886.0, 0, &m));
    // OUTRO CORTE: One Piece T1E1 com fim explicito em 1459 s num arquivo de
    // 1420 s (sem a previa) — o marcador e de um arquivo mais longo.
    assert(!intro_janela_ok(INTRO_CREDITOS, 1389.0, 1459.0, 1420.0, 0, &m));
    // ABERTURA no meio do episodio nao e abertura.
    assert(!intro_janela_ok(INTRO_ABERTURA, 1800.0, 1890.0, 3481.0, 0, &m));
    assert( intro_janela_ok(INTRO_ABERTURA, 228.664, 246.143, 3481.0, 0, &m));
    puts("ok  marcador de outro episodio, de outro corte e abertura no meio sao recusados");
  }
  // O TheIntroDB REMAPEIA a numeracao: One Piece imdb T4E1 e T4E2 voltam os
  // dois como T1E48. A resposta que nao ecoa o par pedido e descartada.
  assert(!intro_resposta_confere(fixture("onepiece_imdb_s04e01_remap.json"), 4, 1));
  assert(!intro_resposta_confere(fixture("onepiece_imdb_s04e02_remap.json"), 4, 2));
  assert( intro_resposta_confere(fixture("shawshank.json"), 0, 0));          // filme
  assert( intro_resposta_confere("{\"intro\":[]}", 3, 4));                  // sem eco
  assert(intro_extrair(fixture("onepiece_cinemeta_s04e92_404.json"), v, 8) == 0);
  puts("ok  resposta de outro episodio (remapeada) nao vale");
  // A JANELA DOS CREDITOS DE SERIE e a ESTIMATIVA FIXA (sem porcentagem).
  assert(mesmoSeg(intro_creditos_janela(1320.0), 198.0));   // 22 min
  assert(mesmoSeg(intro_creditos_janela(1500.0), 225.0));   // 25 min (R8: 154 s cabe)
  assert(mesmoSeg(intro_creditos_janela(2700.0), 300.0));   // 45 min
  assert(mesmoSeg(intro_creditos_janela(10800.0), 300.0));  // 3 h: teto
  assert(mesmoSeg(intro_creditos_janela(480.0), 120.0));    // 8 min: piso
  assert(mesmoSeg(intro_creditos_janela(200.0), 100.0));    // nunca mais que metade
  assert(mesmoSeg(intro_fim_estimado(3600.0), INTRO_FIM_SERIE_S) && INTRO_FIM_SERIE_S == 50.0);
  assert(mesmoSeg(intro_fim_estimado(1320.0), 50.0));
  // SILO NA TCL DO DONO (08/10), respostas reais por tmdb_id. T3E4, T3E5 e
  // T2E7 so tem abertura: nenhum credito pode sair delas. T2E6 tem creditos
  // 2519-2582 s, mas o arquivo dele tem 3077 s e os creditos de verdade comecam
  // ~50 s antes do fim: o marcador e de outro corte e a janela o recusa (sobram
  // 558 s) — o cartao fica com a estimativa fixa de 50 s (tests/posplay.c).
  { static const struct { const char *arq; int t, e; } so[] = {
      { "silo_tmdb_s03e04.json", 3, 4 }, { "silo_tmdb_s03e05.json", 3, 5 },
      { "silo_tmdb_s02e07.json", 2, 7 } };
    const char *m;
    for (size_t k = 0; k < sizeof so / sizeof *so; k++) {
      const char *j = fixture(so[k].arq);
      int i;
      assert(intro_resposta_confere(j, so[k].t, so[k].e));
      n = intro_extrair(j, v, 8);
      assert(n == 1 && v[0].tipo == INTRO_ABERTURA);
      for (i = 0; i < n; i++) assert(v[i].tipo != INTRO_CREDITOS);
    }
    n = intro_extrair(fixture("silo_tmdb_s02e06.json"), v, 8);
    assert(n == 2 && v[1].tipo == INTRO_CREDITOS);
    assert(mesmoSeg(v[1].inicio, 2519.08) && mesmoSeg(v[1].fim, 2582.106));
    assert(!intro_janela_ok(INTRO_CREDITOS, v[1].inicio, v[1].fim, 3077.0, 0, &m));
    assert(intro_janela_ok(INTRO_ABERTURA, v[0].inicio, v[0].fim, 3077.0, 0, &m));
    puts("ok  Silo T3E4/T3E5/T2E7 sem creditos; T2E6 (2519 s de 3077 s) recusado"); }
  assert(mesmoSeg(intro_fim_estimado(480.0), INTRO_FIM_CURTO_S) && INTRO_FIM_CURTO_S == 15.0);
  assert(intro_fim_estimado(100.0) == 0.0);                 // clipe: nada
  puts("ok  janela dos creditos (15%, 2-5 min) e estimativa fixa (40 s / 15 s)");
#ifdef NV_SHOT_HOOKS
  {
    // intro_creditos_seg so devolve marcador ACEITO: o de "creditos" a 10 min
    // do fim de um episodio de 58 min nao chega ao cartao.
    IntroTrecho t[2] = {{2881.0, 0, INTRO_CREDITOS}, {228.0, 246.0, INTRO_ABERTURA}};
    intro_shot_definir(t, 2);
    intro_definir_duracao(3481.0, 0);
    assert(intro_creditos_seg() == 0.0);
    t[0].inicio = 3431.0;
    intro_shot_definir(t, 2);
    assert(mesmoSeg(intro_creditos_seg(), 3431.0));
    puts("ok  creditos recusados nao chegam ao cartao");
  }
#endif

  // --- 2.0.3: PEDIDOS (como o plugin oficial), FILME, PREVIA, ANISKIP --------
  { char url[400];
    intro_montar_url(url, sizeof url, "tt0388629", 37854, 4, 92, 1385.4);
    assert(!strcmp(url, "https://api.theintrodb.org/v4/media?tmdb_id=37854&season=4&episode=92&duration_ms=1385400"));
    intro_montar_url(url, sizeof url, "tt1375666", 0, 0, 0, 0.0);
    assert(!strcmp(url, "https://api.theintrodb.org/v4/media?imdb_id=tt1375666"));
    intro_montar_url(url, sizeof url, "tt12409194", 0, 0, 1, 1200.0);
    assert(!strcmp(url, "https://api.theintrodb.org/v4/media?imdb_id=tt12409194&season=0&episode=1&duration_ms=1200000"));
    assert(intro_resposta_confere("{\"season\":0,\"episode\":1}", 0, 1));
    assert(!intro_resposta_confere("{\"season\":1,\"episode\":1}", 0, 1));
    intro_montar_url_aniskip(url, sizeof url, 21, 1, 1500.0);
    assert(strstr(url, "skip-times/21/1?types[]=op&types[]=ed&types[]=recap&episodeLength=1500"));
    puts("ok  URLs: tmdb_id + par do TMDB + duration_ms; imdb sozinho no filme; AniSkip");
  }
  intro_definir_buscador(fakeBuscar);
  { IntroTrecho t[8]; int nt;
    // One Piece pelo TMDB (o par confirmado pelo TMDB, T4E92): marcadores proprios.
    intro_pedir_ids("tt0388629", 37854, 4, 92, 0, 0);
    nt = esperar(2); nt = intro_trechos(t, 8);
    assert(nt == 2 && mesmoSeg(credFinal(t, nt), 1315.0));
    // O mesmo pelo imdb com a numeracao que a API remapeia: nada.
    intro_pedir_ids("tt0388629", 0, 4, 1, 0, 0);
    usleep(200000);
    assert(intro_trechos(t, 8) == 0);
    puts("ok  One Piece por tmdb_id tem marcadores; pelo imdb remapeado, nenhum");
    // 5xx NAO vira "nao conhece": o pedido seguinte acha os dados.
    fakeStatus500 = 1;
    intro_pedir_ids("tt0903747", 0, 1, 1, 0, 0);
    usleep(200000);
    assert(intro_trechos(t, 8) == 0);
    fakeStatus500 = 0;
    intro_pedir_ids("tt0903747", 0, 1, 1, 0, 0);
    assert(esperar(2) == 2);
    puts("ok  resposta 500 nao e guardada como vazia");
    // A DURACAO CHEGA: pede de novo com duration_ms (uma vez por mudanca).
    intro_definir_duracao(3481.0, 0);
    usleep(200000);
    pthread_mutex_lock(&fakeTrava);
    assert(strstr(ultimaUrl, "duration_ms=3481000"));
    pthread_mutex_unlock(&fakeTrava);
    assert(esperar(2) == 2);
    puts("ok  duracao real vai como duration_ms num segundo pedido");
    // FILME (Inception): abertura 0-38 s vira botao no comeco do filme.
    intro_pedir_ids("tt1375666", 0, 0, 0, 0, 0);
    esperar(1);
    intro_definir_duracao(8880.0, 1);
    usleep(200000);
    { double fim; int tipo;
      assert(intro_botao(10.0, 5000.0, 0, 0, &fim, &tipo) && tipo == INTRO_ABERTURA && mesmoSeg(fim, 38.0)); }
    puts("ok  filme: abertura do TheIntroDB vira o botao de pular");
    // ANIME: kitsu:12 -> MAL 21 (mappings do Kitsu) -> AniSkip ep 1.
    intro_pedir_ids("kitsu:12:1", 0, 1, 1, 0, 0);
    nt = esperar(2); nt = intro_trechos(t, 8);
    assert(nt == 2 && mesmoSeg(credFinal(t, nt), 1387.996));
    puts("ok  anime kitsu:12 -> MAL 21 -> AniSkip: abertura e ED");
    // Especial sem um mapeamento MAL proprio nao herda os marcadores de E1 regular.
    { int aniAntes;
      pthread_mutex_lock(&fakeTrava); aniAntes = aniPedidos; pthread_mutex_unlock(&fakeTrava);
      intro_pedir_ids("kitsu:12:1", 0, 0, 1, 0, 0);
      assert(esperar(0) == 0);
      pthread_mutex_lock(&fakeTrava); assert(aniPedidos == aniAntes); pthread_mutex_unlock(&fakeTrava);
    }

    // #430: evidencia vem do TMDB ja baixado, antes ou depois do player.
    const char *anime = "{\"original_language\":\"ja\",\"genres\":[{\"id\":16}]}";
    intro_registrar_tmdb("tt5607616", 65942, anime);
    intro_pedir_ids("tt5607616", 0, 2, 1, 0, 0);
    assert(esperar(2) == 2);
    pthread_mutex_lock(&fakeTrava);
    assert(strstr(ultimaAni, "skip-times/39587/1?"));
    assert(mapPedidos == 1);
    pthread_mutex_unlock(&fakeTrava);
    nt = intro_trechos(t, 8);
    assert(achar(t, nt, INTRO_ABERTURA)->inicio == 10.0);
    assert(achar(t, nt, INTRO_CREDITOS)->inicio == 1350.0);
    intro_pedir_ids("tt5607616", 65942, 1, 39, 0, 0);
    assert(esperar(2) == 2);
    pthread_mutex_lock(&fakeTrava);
    assert(strstr(ultimaAni, "skip-times/42203/1?"));
    assert(mapPedidos == 1); // outro episodio/parte usa o mesmo cache do titulo
    pthread_mutex_unlock(&fakeTrava);
    char indice[256];struct stat st;
    snprintf(indice,sizeof indice,"%s/anime-fribb.idx",dadosTeste);
    assert(!stat(indice,&st)&&st.st_size>0);
    ino_t inode=st.st_ino;time_t mtime=st.st_mtime;
    // Outro titulo usa o arquivo, sem o antigo cache RAM por titulo.
    intro_registrar_tmdb("tt5607617", 0, anime);
    intro_pedir_ids("tt5607617", 0, 2, 14, 0, 0);
    assert(esperar(2)==2);
    assert(!stat(indice,&st)&&st.st_ino==inode&&st.st_mtime==mtime);
    pthread_mutex_lock(&fakeTrava);assert(mapPedidos==1);pthread_mutex_unlock(&fakeTrava);
    intro_pedir_ids("tt5607616",65942,1,39,0,0);assert(esperar(2)==2);
    IntroTrecho cap = {30, 120, INTRO_ABERTURA};
    intro_definir_capitulos(&cap, 1);
    nt = intro_trechos(t, 8);
    assert(achar(t, nt, INTRO_ABERTURA)->inicio == 30);
    assert(achar(t, nt, INTRO_CREDITOS)->inicio == 1350);
    intro_registrar_tmdb("tt5607616", 65942, "{\"original_language\":\"ja\",\"genres\":[{\"id\":18}]}");
    intro_pedir_ids("tt5607616", 0, 2, 1, 0, 0);
    usleep(200000);
    assert(intro_trechos(t, 8) == 0);
    intro_registrar_tmdb("tt5607616", 65942, "{\"origin_country\":[\"JP\"],\"genres\":[{\"id\":16}]}");
    assert(esperar(2) == 2); // evidencia tardia refaz pelo AniSkip
    pthread_mutex_lock(&fakeTrava);
    int antesMap = mapPedidos, antesAni = aniPedidos;
    pthread_mutex_unlock(&fakeTrava);
    intro_registrar_tmdb("tt0903747", 1396, "{\"original_language\":\"en\",\"genres\":[{\"id\":16}]}");
    intro_pedir_ids("tt0903747", 0, 1, 1, 0, 0);
    assert(esperar(2) == 2);
    pthread_mutex_lock(&fakeTrava);
    assert(mapPedidos == antesMap && aniPedidos == antesAni);
    pthread_mutex_unlock(&fakeTrava);
    // Renovacao semanal falha: ainda resolve pelo indice velho; nao repete.
    struct utimbuf velho={time(NULL)-8*24*60*60,time(NULL)-8*24*60*60};
    assert(!utime(indice,&velho));
    pthread_mutex_lock(&fakeTrava);mapFora=1;pthread_mutex_unlock(&fakeTrava);
    intro_pedir_ids("tt5607617",0,2,14,0,0);assert(esperar(2)==2);
    pthread_mutex_lock(&fakeTrava);assert(mapPedidos==2);pthread_mutex_unlock(&fakeTrava);
    intro_pedir_ids("tt5607616",65942,1,39,0,0);assert(esperar(2)==2);
    pthread_mutex_lock(&fakeTrava);assert(mapPedidos==2);mapFora=0;pthread_mutex_unlock(&fakeTrava);
    assert(!utime(indice,&velho));
    intro_pedir_ids("tt5607617",0,2,14,0,0);assert(esperar(2)==2);
    pthread_mutex_lock(&fakeTrava);assert(mapPedidos==3);pthread_mutex_unlock(&fakeTrava);
    // Corrupcao e remocao refazem, mesmo antes da semana.
    FILE *idx=fopen(indice,"r+b");assert(idx);assert(!fseek(idx,16,SEEK_SET));fputc(255,idx);fclose(idx);
    intro_pedir_ids("tt5607616",65942,1,39,0,0);assert(esperar(2)==2);
    pthread_mutex_lock(&fakeTrava);assert(mapPedidos==4);pthread_mutex_unlock(&fakeTrava);
    assert(!unlink(indice));
    intro_pedir_ids("tt5607617",0,2,14,0,0);assert(esperar(2)==2);
    pthread_mutex_lock(&fakeTrava);assert(mapPedidos==5);mapFora=1;int antesDb=dbPedidos;pthread_mutex_unlock(&fakeTrava);
    assert(!unlink(indice));
    intro_registrar_tmdb("tt4300000",430,anime);
    intro_pedir_ids("tt4300000",0,1,1,0,0);assert(esperar(2)==2);
    pthread_mutex_lock(&fakeTrava);
    assert(mapPedidos==6&&dbPedidos==antesDb+1);mapFora=0;
    pthread_mutex_unlock(&fakeTrava);
    // Sem indice e sem rede: outro titulo nao dispara nova descarga no prazo
    // do retry CURTO (10 min apos a 1a falha; antes eram 7 dias).
    intro_pedir_ids("tt5607616",65942,1,39,0,0);usleep(200000);
    pthread_mutex_lock(&fakeTrava);assert(mapPedidos==6);pthread_mutex_unlock(&fakeTrava);
    char retry[256];snprintf(retry,sizeof retry,"%s/anime-fribb.retry",dadosTeste);
    // A falha ficou contada no arquivo ("1"), nao mais vazia.
    { FILE *rf=fopen(retry,"rb");char rb[8]={0};size_t rn;
      assert(rf);rn=fread(rb,1,sizeof rb-1,rf);fclose(rf);rb[rn]=0;
      assert(!strcmp(rb,"1")); }
    pthread_mutex_lock(&fakeTrava);mapFora=1;pthread_mutex_unlock(&fakeTrava);
    // 11 minutos depois o prazo curto passou: tenta de novo.
    struct utimbuf ha11={time(NULL)-660,time(NULL)-660};
    assert(!utime(retry,&ha11));
    intro_pedir_ids("tt5607616",65942,1,39,0,0);usleep(300000);
    pthread_mutex_lock(&fakeTrava);assert(mapPedidos==7);pthread_mutex_unlock(&fakeTrava);
    // 2a falha seguida: o prazo sobe para 1 h — 11 minutos nao bastam.
    { FILE *rf=fopen(retry,"rb");char rb[8]={0};size_t rn;
      assert(rf);rn=fread(rb,1,sizeof rb-1,rf);fclose(rf);rb[rn]=0;
      assert(!strcmp(rb,"2")); }
    assert(!utime(retry,&ha11));
    intro_pedir_ids("tt5607617",0,2,14,0,0);usleep(200000);
    pthread_mutex_lock(&fakeTrava);assert(mapPedidos==7);mapFora=0;pthread_mutex_unlock(&fakeTrava);
    assert(!unlink(retry));
    puts("ok  #430: indice em fluxo criado uma vez/reutilizado entre titulos; semana; stale/503; corrompido/ausente refaz; sem indice -> TheIntroDB");
    intro_definir_buscador(NULL);
    intro_desligar();
  }
  { const char *m;
    // Janela de FILME: 12%, 5-15 min. Creditos de 14 min num filme de 3 h valem.
    assert(mesmoSeg(intro_creditos_janela_filme(7200.0), 864.0));
    assert(mesmoSeg(intro_creditos_janela_filme(10800.0), 900.0));
    assert(mesmoSeg(intro_creditos_janela_filme(1800.0), 300.0));
    assert( intro_janela_ok(INTRO_CREDITOS, 10000.0, 0, 10800.0, 1, &m));
    assert(!intro_janela_ok(INTRO_CREDITOS, 6000.0, 0, 7200.0, 1, &m));    // 20 min antes do fim
    assert(!intro_janela_ok(INTRO_ABERTURA, 2000.0, 2050.0, 8880.0, 1, &m)); // abertura aos 33 min
    assert( intro_janela_ok(INTRO_ABERTURA, 0.0, 53.0, 10140.0, 1, &m));   // Interstellar
    assert(mesmoSeg(intro_fim_estimado_filme(7200.0), 180.0));
    assert(mesmoSeg(intro_fim_estimado_filme(3000.0), 90.0));
    assert(intro_fim_estimado_filme(500.0) == 0.0);
    n = intro_extrair(fixture("interstellar.json"), v, 8);
    assert(n == 1 && v[0].tipo == INTRO_ABERTURA && mesmoSeg(v[0].fim, 53.0));
    puts("ok  filme: janela de creditos 12% (5-15 min), abertura so no comeco, 3 min/90 s fixos");
  }
  // PREVIA: emendada nos creditos, o "Pular creditos" pula as duas; sem
  // creditos, ela marca o fim para o cartao.
#ifdef NV_SHOT_HOOKS
  { double fim; int tipo;
    n = intro_extrair(fixture("onepiece_s01e01.json"), v, 8);
    intro_shot_definir(v, n);
    intro_definir_duracao(1500.0, 0);
    assert(intro_botao(1400.0, 9000.0, 0, 0, &fim, &tipo) && tipo == INTRO_CREDITOS && mesmoSeg(fim, 1500.0));
    assert(!intro_botao(1470.0, 9020.0, 0, 0, &fim, &tipo) || tipo != INTRO_PREVIA);
    n = intro_extrair(fixture("aot_s01e01.json"), v, 8);
    intro_shot_definir(v, n);
    intro_definir_duracao(1440.0, 0);
    assert(mesmoSeg(intro_creditos_seg(), 1432.0));
    // trecho com inicio e fim nulos e descartado (como o plugin oficial)
    assert(intro_extrair("{\"intro\":[{\"start_ms\":null,\"end_ms\":null}]}", v, 8) == 0);
    puts("ok  previa: emendada pula junto com os creditos; sozinha marca o fim");
  }
#endif
#ifdef NV_SHOT_HOOKS
  // CAPITULOS DO ARQUIVO (203-capitulos): substituem, por tipo, o da rede, e
  // sobrevivem a quem chegou antes.
  { double fim; int tipo; IntroTrecho cap[2] = {{1380.0, 0.0, INTRO_CREDITOS, 0},
                                              {60.0, 150.0, INTRO_ABERTURA, 0}}, got[8]; int k;
    n = intro_extrair(fixture("onepiece_s01e01.json"), v, 8);
    intro_shot_definir(v, n);
    intro_definir_duracao(1500.0, 0);
    intro_definir_capitulos(cap, 2);
    k = intro_trechos(got, 8);
    assert(achar(got, k, INTRO_CREDITOS) && mesmoSeg(achar(got, k, INTRO_CREDITOS)->inicio, 1380.0));
    assert(achar(got, k, INTRO_ABERTURA) && mesmoSeg(achar(got, k, INTRO_ABERTURA)->inicio, 60.0));
    assert(intro_botao(1400.0, 9100.0, 0, 0, &fim, &tipo) && tipo == INTRO_CREDITOS);
    intro_definir_capitulos(NULL, 0);
    puts("ok  capitulos do arquivo substituem o marcador da rede por tipo");
  }
#endif
  // ANISKIP: leitura e a regra dos 10% (outro corte).
  memset(v, 0xa5, sizeof v);
  n = intro_extrair_aniskip(fixture("../aniskip/onepiece_mal21_ep1_len0.json"), 1500.0, v, 8);
  assert(n == 3);
  for (int i = 0; i < n; i++) assert(v[i].ani == 1);
  { const IntroTrecho *a = achar(v, n, INTRO_ABERTURA), *c = achar(v, n, INTRO_CREDITOS);
    assert(a && mesmoSeg(a->inicio, 28.783) && mesmoSeg(a->fim, 118.783));
    assert(c && mesmoSeg(c->inicio, 1387.996) && c->fim == 0.0); }   // ED ate o fim
  // TheIntroDB e AniSkip concordam no ED do One Piece T1E1: 1389 s x 1388 s.
  assert(credFinal(v, n) > 1385.0 && credFinal(v, n) < 1391.0);
  // arquivo de 1300 s: todo lancamento conhecido (1444-1500 s) difere > 10%
  assert(intro_extrair_aniskip(fixture("../aniskip/onepiece_mal21_ep1_len0.json"), 1300.0, v, 8) == 0);
  // 1440 s: o resumo (lancamento de 1444 s) e a abertura/ED (1500 s) cabem nos 10%
  assert(intro_extrair_aniskip(fixture("../aniskip/onepiece_mal21_ep1_len0.json"), 1440.0, v, 8) == 3);
  n = intro_extrair_aniskip(fixture("../aniskip/aot_mal16498_ep1_len1440.json"), 1440.0, v, 8);
  assert(n == 2 && mesmoSeg(credFinal(v, n), 1342.795));
  assert(intro_kitsu_mal(fixture("../kitsu12_mappings.json")) == 21);
  assert(intro_extrair_aniskip("{\"found\":false,\"results\":[],\"statusCode\":404}", 0, v, 8) == 0);
  puts("ok  AniSkip: op/ed/recap, ED ate o fim, 10% de duracao, mapeamento kitsu -> MAL");
  // COBERTURA POR TIPO: AniSkip so com OP nao bloqueia o ED do TheIntroDB.
  { IntroTrecho a[8] = {{10.0, 100.0, INTRO_ABERTURA, 1}}, w[8] = {{20.0, 90.0, INTRO_ABERTURA, 0}, {1300.0, 0.0, INTRO_CREDITOS, 0}};
    int na = 1;
    assert(!intro_tipos_completos(a, na));
    na = intro_completar(a, na, w, 2, 8);
    assert(na == 2 && a[0].ani == 1 && a[0].inicio == 10.0);   // OP do AniSkip fica
    assert(a[1].tipo == INTRO_CREDITOS && a[1].ani == 0);      // ED vem do TheIntroDB
    puts("ok  fontes completam por tipo, com origem por trecho"); }
  assert(!rmdir(dadosTeste));
  puts("intro: tudo ok");
  // #329/#331: pular sozinho. Assinatura: (tipo, fim, dur, abertura, resumo, creditos).
  assert(intro_auto_pular(INTRO_ABERTURA, 90.0, 3000.0, 1, 0, 0));
  assert(!intro_auto_pular(INTRO_ABERTURA, 90.0, 3000.0, 0, 1, 1));
  assert(intro_auto_pular(INTRO_RESUMO, 30.0, 3000.0, 0, 1, 0));
  assert(!intro_auto_pular(INTRO_RESUMO, 30.0, 3000.0, 1, 0, 1));
  assert(!intro_auto_pular(INTRO_ABERTURA, 0.0, 3000.0, 1, 1, 1));      // fim 0 = ate o fim da midia
  puts("ok  pulo automatico: abertura/resumo, ligado e com fim");
  // creditos: ajuste proprio + fim explicito dentro da midia
  assert(intro_auto_pular(INTRO_CREDITOS, 2900.0, 3000.0, 0, 0, 1));    // inicio e fim reais -> pula
  assert(!intro_auto_pular(INTRO_CREDITOS, 2900.0, 3000.0, 1, 1, 0));   // ajuste desligado
  assert(!intro_auto_pular(INTRO_CREDITOS, 0.0, 3000.0, 0, 0, 1));      // fim null (ate o fim): nunca
  assert(!intro_auto_pular(INTRO_CREDITOS, 2998.0, 3000.0, 0, 0, 1));   // acaba junto com a midia: proximo episodio
  assert(!intro_auto_pular(INTRO_CREDITOS, 3500.0, 3000.0, 0, 0, 1));   // alem do fim: nunca um seek para fora
  assert(!intro_auto_pular(INTRO_CREDITOS, 2900.0, 0.0, 0, 0, 1));      // duracao desconhecida
  puts("ok  creditos: so com fim explicito e a 5 s do fim; sem ajuste/sem fim/estimativa nao");
  // Marcadores so com inicio (TheIntroDB end_ms:null) e a estimativa nao viram trecho pulavel
  {
    IntroTrecho t[1]={{2900.0,0.0,INTRO_CREDITOS}};
    double fim=-1;int tp=0;
    intro_shot_definir(t,1);intro_definir_duracao(3000.0,0);
    assert(intro_ativo(2950.0,&fim,&tp)&&tp==INTRO_CREDITOS&&fim==0.0);
    assert(!intro_auto_pular(tp,fim,3000.0,1,1,1));
    puts("ok  creditos sem end_ms nao pulam");
  }
  // Uma vez por trecho, e voltar (para dentro ou para antes) nao pula de novo
  {
    IntroTrecho t[3]={{10.0,70.0,INTRO_RESUMO},{70.0,160.0,INTRO_ABERTURA},{2900.0,2960.0,INTRO_CREDITOS}};
    intro_shot_definir(t,3);
    assert(!intro_auto_ja(INTRO_CREDITOS,2960.0));
    intro_auto_marcar(INTRO_CREDITOS,2960.0);
    intro_auto_marcar(INTRO_ABERTURA,160.0);
    intro_auto_marcar(INTRO_RESUMO,70.0);
    // um tipo nao apaga o outro: depois de pular a abertura, o resumo e os creditos continuam marcados
    assert(intro_auto_ja(INTRO_CREDITOS,2960.0));
    assert(intro_auto_ja(INTRO_RESUMO,70.0));
    assert(intro_auto_ja(INTRO_ABERTURA,160.0));
    assert(!intro_auto_ja(INTRO_CREDITOS,2970.0));    // outro marcador (fim diferente) e outro trecho
    // nova reproducao (novo pedido de marcadores) zera
    intro_shot_definir(t,3);
    assert(!intro_auto_ja(INTRO_CREDITOS,2960.0));
    assert(!intro_auto_ja(INTRO_RESUMO,70.0));
    puts("ok  rebobinar nao pula de novo; nova reproducao zera");
  }

  testarBackup();
  return 0;
}
