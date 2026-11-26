// Teste dirigido #farol-52: renovacao concorrente de JWT em sessao.c vs
// prazo de 6 s e farolcompor_encerrar() em farolcompor.c, com sessao.c +
// nuvem.c + rede.c reais contra servidor loopback.
#include "farolcompor.h"
#include "farolgosto.h"
#define FarolMotivo FarolRecMotivo
#include "farolrec.h"
#undef FarolMotivo
#include "nuvem.h"
#include "rede.h"
#include "sessao.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static const char *G_dir = NULL;
static pthread_mutex_t G_discoTx = PTHREAD_MUTEX_INITIALIZER;
static int G_authEntrou = 0;

char *dados_ler(const char *nome) {
  char c[600];
  FILE *f;
  long n;
  char *b;
  pthread_mutex_lock(&G_discoTx);
  snprintf(c, sizeof c, "%s/%s", G_dir, nome);
  f = fopen(c, "rb");
  if (!f) {
    pthread_mutex_unlock(&G_discoTx);
    return NULL;
  }
  fseek(f, 0, SEEK_END);
  n = ftell(f);
  fseek(f, 0, SEEK_SET);
  b = (char *)malloc((size_t)n + 1);
  if (b) {
    n = (long)fread(b, 1, (size_t)n, f);
    b[n] = '\0';
  }
  fclose(f);
  pthread_mutex_unlock(&G_discoTx);
  return b;
}

int dados_gravar(const char *nome, const char *conteudo) {
  char c[600];
  FILE *f;
  pthread_mutex_lock(&G_discoTx);
  snprintf(c, sizeof c, "%s/%s", G_dir, nome);
  f = fopen(c, "wb");
  if (f) {
    fputs(conteudo ? conteudo : "", f);
    fclose(f);
  }
  pthread_mutex_unlock(&G_discoTx);
  return f != NULL;
}

int dados_apagar(const char *nome) {
  char c[600];
  int r;
  pthread_mutex_lock(&G_discoTx);
  snprintf(c, sizeof c, "%s/%s", G_dir, nome);
  r = remove(c) == 0;
  pthread_mutex_unlock(&G_discoTx);
  return r;
}

void dados_uuid(char *dst, unsigned tam) {
  snprintf(dst, tam, "00000000-0000-4000-8000-000000000000");
}
const char *i18n(const char *s) { return s; }
void trava_invalidar_autorizacoes(void) {}

int rede_saude_offline(void) { return 0; }
int perfil_infantil_ativo(void) { return 0; }
int player_aberto(void) { return 0; }
int seguro_perfil_ativo(void) { return 0; }
int farolsinal_recomendar(void) { return 1; }
int perfis_ativo(void) { return 1; }
const char *ajustes_tmdb_idioma(void) { return "pt-BR"; }

// Igual a recomenda.c -> bearerSessao(): copia o token da sessao real.
int recomenda_cabecalhos(const char **cab, char *aut, size_t na, char *via, size_t nv,
                         char *perfil, size_t np) {
  char tok[3000];
  __atomic_add_fetch(&G_authEntrou, 1, __ATOMIC_SEQ_CST);
  if (!sessao_token_copiar(tok, sizeof tok)) {
    memset(tok, 0, sizeof tok);
    return 0;
  }
  snprintf(aut, na, "Authorization: Bearer %s", tok);
  memset(tok, 0, sizeof tok);
  snprintf(via, nv, "X-Nuvio-Auth: nuvio");
  snprintf(perfil, np, "X-Nuvio-Perfil: 1");
  if (cab) {
    cab[0] = aut;
    cab[1] = via;
    cab[2] = perfil;
    cab[3] = NULL;
  }
  return 1;
}

static long long agoraMs(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (long long)t.tv_sec * 1000LL + (long long)(t.tv_nsec / 1000000L);
}

static void srvConfig(const char *base, const char *scenario, double refreshDelayS,
                      int resetHits, int resetRefresh) {
  char url[512], body[256];
  RedePedido req;
  RedeResposta res;
  snprintf(url, sizeof url, "%s/__scenario", base);
  snprintf(body, sizeof body,
           "{\"scenario\":\"%s\",\"delay_s\":0.0,\"refresh_delay_s\":%.2f,"
           "\"reset_hits\":%s,\"reset_refresh\":%s}",
           scenario, refreshDelayS, resetHits ? "true" : "false",
           resetRefresh ? "true" : "false");
  memset(&req, 0, sizeof req);
  memset(&res, 0, sizeof res);
  req.metodo = "POST";
  req.url = url;
  req.corpo = body;
  req.n_corpo = strlen(body);
  req.prazo_ms = 2000;
  req.max_bytes = 4096;
  rede_pedir(&req, &res);
  assert(res.erro == REDE_OK && res.status == 200);
  rede_resposta_limpar(&res);
}

static int srvIntField(const char *base, const char *path, const char *key) {
  char url[512], pat[64];
  RedePedido req;
  RedeResposta res;
  int v = -1;
  snprintf(url, sizeof url, "%s%s", base, path);
  snprintf(pat, sizeof pat, "\"%s\":", key);
  memset(&req, 0, sizeof req);
  memset(&res, 0, sizeof res);
  req.metodo = "GET";
  req.url = url;
  req.prazo_ms = 2000;
  req.max_bytes = 4096;
  rede_pedir(&req, &res);
  assert(res.erro == REDE_OK && res.status == 200 && res.corpo);
  {
    const char *p = strstr(res.corpo, pat);
    assert(p != NULL);
    v = atoi(p + strlen(pat));
  }
  rede_resposta_limpar(&res);
  return v;
}

typedef struct {
  int   status;
  char *resp;
} RpcRes;

static void *fioOutroRefresh(void *u) {
  RpcRes *out = (RpcRes *)u;
  out->resp = sessao_rpc("sync_pull", "{}", &out->status);
  return NULL;
}

static const char *lookupPessoa(int32_t id, void *u) {
  (void)id; (void)u;
  return "Pessoa Publica";
}

static void montarGostoECand(FarolGosto *g, FarolComporCand *cand, int n) {
  int i;
  farolgosto_iniciar(g, 1);
  g->diaBase = 100;
  g->titulos = 12;
  g->genero[0].id = 53;
  g->genero[0].peso = 512;
  g->genero[0].n = 2;
  g->genero[0].dia = 100;
  for (i = 0; i < n; i++) {
    memset(&cand[i], 0, sizeof cand[i]);
    snprintf(cand[i].imdb, sizeof cand[i].imdb, "tt30000%02d", i + 1);
    cand[i].tmdb = 500 + i;
    snprintf(cand[i].titulo, sizeof cand[i].titulo, "Filme Teste %d", i + 1);
    cand[i].ano = 2023;
    cand[i].minutos = 95;
    cand[i].generos[0] = 53;
    cand[i].nGeneros = 1;
    cand[i].nota10 = 75;
    cand[i].motivos[0].tipo = FMO_GENERO;
    cand[i].motivos[0].a = 53;
    cand[i].nMotivos = 1;
  }
}

static long long relogioFixo(void) { return 1795000000LL; }

int main(int argc, char **argv) {
  const char *base, *dir;
  FarolGosto g;
  FarolComporCand cand[8];
  FarolComporInfo inf;
  pthread_t tOutro;
  RpcRes rpcRes;
  long long tInicio, tEnc0, dtEnc, tJob2;
  char tok[3000];
  int antesAuth;

  if (argc < 3) {
    fprintf(stderr, "uso: %s <base_url> <pasta_dados>\n", argv[0]);
    return 2;
  }
  base = argv[1];
  dir = argv[2];
  G_dir = dir;
  setvbuf(stdout, NULL, _IOLBF, 0);

  // Token vencido (exp=1) e refresh token r1: dispara refresh no outro fio.
  assert(dados_gravar("sessao.txt",
                      "t0001.eyJzdWIiOiJ1MSIsImV4cCI6MX0.s0001\nr1\n0\n"));
  sessao_iniciar();
  assert(sessao_logada() == 1);

  farolcompor_teste_zera();
  farolcompor_definir_url(base);
  farolcompor_definir_auth(NULL, NULL, NULL);
  farolcompor_relogio(relogioFixo);
  farolcompor_pasta(dir);
  farolcompor_definir_consentimento(1);
  montarGostoECand(&g, cand, 8);

  srvConfig(base, "ok_home", 9.0, 1, 1);

  memset(&rpcRes, 0, sizeof rpcRes);
  assert(pthread_create(&tOutro, NULL, fioOutroRefresh, &rpcRes) == 0);

  // Espera o outro fio entrar no POST /auth/v1/token (renovando == 1).
  tInicio = agoraMs();
  while (srvIntField(base, "/__refresh_state", "started") < 1) {
    assert(agoraMs() - tInicio < 3000 && "outro fio nao iniciou refresh");
    usleep(10000);
  }
  usleep(50000);

  // Caso 1: compositor entra durante refresh de 9 s e chama farolcompor_encerrar().
  antesAuth = __atomic_load_n(&G_authEntrou, __ATOMIC_SEQ_CST);
  assert(farolcompor_pedir_home(&g, cand, 8, NULL, 0, lookupPessoa, NULL, 101) == 1);
  while (__atomic_load_n(&G_authEntrou, __ATOMIC_SEQ_CST) == antesAuth) {
    assert(agoraMs() - tInicio < 3000 && "compositor nao entrou em recomenda_cabecalhos");
    usleep(5000);
  }
  usleep(50000);
  assert(farolcompor_em_voo() == 1);

  tEnc0 = agoraMs();
  farolcompor_encerrar();
  dtEnc = agoraMs() - tEnc0;
  if (dtEnc >= 500) {
    fprintf(stderr, "FALHOU: farolcompor_encerrar bloqueou por %lld ms esperando refresh\n", dtEnc);
    return 1;
  }
  assert(farolcompor_em_voo() == 0);
  assert(srvIntField(base, "/__hits", "hits") == 0);
  puts("ok  sessao refresh concorrente: farolcompor_encerrar() rapido sem esperar 9 s");

  // Caso 2: compositor entra durante o mesmo refresh de 9 s e respeita prazo de 6 s.
  antesAuth = __atomic_load_n(&G_authEntrou, __ATOMIC_SEQ_CST);
  tJob2 = agoraMs();
  assert(farolcompor_pedir_home(&g, cand, 8, NULL, 0, lookupPessoa, NULL, 102) == 1);
  while (__atomic_load_n(&G_authEntrou, __ATOMIC_SEQ_CST) == antesAuth) {
    assert(agoraMs() - tJob2 < 3000 && "compositor nao entrou em recomenda_cabecalhos (caso 2)");
    usleep(5000);
  }
  while (farolcompor_em_voo() && agoraMs() - tJob2 < 6500) {
    usleep(20000);
  }
  if (farolcompor_em_voo() != 0) {
    fprintf(stderr, "FALHOU: farolcompor_em_voo() ainda == 1 apos %lld ms (preso no refresh)\n",
            agoraMs() - tJob2);
    return 1;
  }
  assert(farolcompor_estado(&inf) == FC_REGRAS && inf.motivo == FC_MOT_PRAZO);
  assert(inf.preservaRegra == 1);
  assert(srvIntField(base, "/__hits", "hits") == 0);
  puts("ok  sessao refresh concorrente: prazo 6 s -> farolcompor_em_voo()==0 e 0 POSTs");

  // O outro fio continua intacto e termina o refresh de 9 s com o token novo.
  pthread_join(tOutro, NULL);
  assert(rpcRes.status == 200 && rpcRes.resp != NULL && !strcmp(rpcRes.resp, "[]"));
  free(rpcRes.resp);
  assert(sessao_logada() == 1);
  assert(srvIntField(base, "/__refresh_state", "hits") == 1);
  memset(tok, 0, sizeof tok);
  assert(sessao_token_copiar(tok, sizeof tok) == 1);
  assert(strncmp(tok, "t0002.", 6) == 0);
  memset(tok, 0, sizeof tok);
  assert(srvIntField(base, "/__refresh_state", "hits") == 1);
  puts("ok  sessao refresh concorrente: outro fio recebeu token renovado normalmente");

  // P2: o timeout antes do POST nao desperdicou a cota diaria da Home.
  assert(farolcompor_pedir_home(&g, cand, 8, NULL, 0, lookupPessoa, NULL, 103) == 1);
  {
    long long tWait = agoraMs();
    while (farolcompor_em_voo() && agoraMs() - tWait < 3000) usleep(10000);
    assert(farolcompor_em_voo() == 0);
  }
  assert(farolcompor_estado(&inf) == FC_PRONTO || farolcompor_estado(&inf) == FC_CARREGANDO);
  assert(farolcompor_passo() == 1);
  assert(srvIntField(base, "/__hits", "hits") == 1);
  puts("ok  sessao refresh concorrente: cota diaria Home preservada apos aborto pre-POST");

  farolcompor_encerrar();
  farolcompor_teste_zera();
  return 0;
}
