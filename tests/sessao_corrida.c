// RENOVACAO DO TOKEN COM VARIOS FIOS AO MESMO TEMPO (#203).
//
// sync.c, contapend.c, visto.c e perfilsel.c chamam sessao_rpc de fios
// proprios; recomenda.c e avisos.c leem o token de outros fios. Antes, sem
// trava nenhuma em sessao.c:
//   - dez fios que levavam 401 juntos faziam dez renovacoes com o MESMO
//     refresh token. O servidor (como o GoTrue) gira o refresh token: a
//     primeira passa, as outras levam 400 — e o 400 deslogava a TV;
//   - quem lia o token durante a renovacao podia pegar o JWT pela metade.
//
// Este teste roda o sessao.c REAL contra um servidor dublado que GIRA o
// refresh token (reuso = 400) e demora na renovacao (abre a janela):
//   rpc401   : 16 fios levam 401 juntos -> UMA renovacao, todos 200, logado.
//   vencido  : token vencido no disco, 16 fios -> UMA renovacao, logado.
//   r504     : renovacao em 504 com 16 fios -> UM pedido, sessao fica.
//   leitores : 8 fios fazendo RPC (com 401 forcado em rodadas) e 4 lendo o
//              token: nenhuma copia sai rasgada.
//   copia    : token vencido no disco: sessao_token_copiar renova UMA vez e
//              devolve o novo (o Worker nunca recebe o vencido).
//   copia504 : mesma coisa com a renovacao em 504: copia devolve 0, dst vazio.
//   recusado : Worker 401 com o token atual: UMA renovacao; 401 repetido com o
//              token ja trocado nao renova de novo.
// Com -DANTIGO compila contra o sessao.c velho (sem sessao_token_copiar),
// para a evidencia de antes: SESSAO_C=<arquivo> CFLAGS_X=-DANTIGO.
#include "sessao.h"
#ifdef ANTIGO
const char *sessao_token(void);   // sessao.c de antes de #203
#endif
#include "nuvem.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "nvtmp.h"

// ------------------------------------------------------------ disco dublado
static const char *pasta(void) { const char *d = getenv("NV_T_DIR"); return d && *d ? d : nvt_dir(); }
static pthread_mutex_t discoTrava = PTHREAD_MUTEX_INITIALIZER;
char *dados_ler(const char *nome) {
  char c[600]; FILE *f; long n; char *b;
  snprintf(c, sizeof c, "%s/%s", pasta(), nome);
  f = fopen(c, "rb"); if (!f) return NULL;
  fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
  b = malloc((size_t)n + 1); n = (long)fread(b, 1, (size_t)n, f); b[n] = 0; fclose(f);
  return b;
}
int dados_gravar(const char *nome, const char *conteudo) {
  char c[600]; FILE *f;
  pthread_mutex_lock(&discoTrava);
  snprintf(c, sizeof c, "%s/%s", pasta(), nome);
  f = fopen(c, "wb"); if (f) { fputs(conteudo ? conteudo : "", f); fclose(f); }
  pthread_mutex_unlock(&discoTrava);
  return f != NULL;
}
int dados_apagar(const char *nome) {
  char c[600]; int r;
  pthread_mutex_lock(&discoTrava);
  snprintf(c, sizeof c, "%s/%s", pasta(), nome); r = remove(c) == 0;
  pthread_mutex_unlock(&discoTrava);
  return r;
}
void dados_uuid(char *dst, unsigned tam) { snprintf(dst, tam, "00000000-0000-4000-8000-000000000000"); }
const char *i18n(const char *s) { return s; }

// ------------------------------------------------------------ servidor dublado
// Payload {"sub":"u1","exp":4102444800} (2100) e {"sub":"u1","exp":1} (vencido).
#define PL_BOM     "eyJzdWIiOiJ1MSIsImV4cCI6NDEwMjQ0NDgwMH0"
#define PL_VENCIDO "eyJzdWIiOiJ1MSIsImV4cCI6MX0"
static pthread_mutex_t srv = PTHREAD_MUTEX_INITIALIZER;
static int geracao = 1;          // token valido = t<geracao>; refresh valido = r<geracao>
static int renovacoes, recusas400;
static int stRenovar = 200;
static int forcar401;            // so via __atomic   // leitores: o servidor "revoga" o token atual

int nuvem_pronta(void) { return 1; }
const char *nuvem_base_login(void) { return "https://login.exemplo/tv"; }
int nuvem_erro_ausente(const char *c) { (void)c; return 0; }
void nuvem_falhou(void) { }
void nuvem_ok(void) { }
const char *nuvem_ultimo_erro(void) { return ""; }

static char *resp(int st, const char *corpo, int *status) { *status = st; return st ? strdup(corpo) : NULL; }

char *nuvem_post(const char *caminho, const char *corpo, const char *bearer, int *status) {
  char buf[400], esperado[200];
  if (strstr(caminho, "grant_type=refresh_token")) {
    char r[40]; int ok, g;
    pthread_mutex_lock(&srv);
    renovacoes++;
    snprintf(r, sizeof r, "\"r%d\"", geracao);
    ok = stRenovar == 200 && strstr(corpo, r) != NULL;
    if (ok) { geracao++; __atomic_store_n(&forcar401, 0, __ATOMIC_SEQ_CST); }
    else if (stRenovar == 200) recusas400++;
    g = geracao;
    pthread_mutex_unlock(&srv);
    usleep(30000);   // a renovacao demora: e a janela da corrida
    if (stRenovar != 200) return resp(stRenovar, "error code: 504", status);
    if (!ok) return resp(400, "{\"error\":\"invalid_grant\",\"error_description\":\"Refresh Token Already Used\"}", status);
    snprintf(buf, sizeof buf, "{\"access_token\":\"t%04d." PL_BOM ".s%04d\",\"refresh_token\":\"r%d\"}", g, g, g);
    return resp(200, buf, status);
  }
  pthread_mutex_lock(&srv);
  snprintf(esperado, sizeof esperado, "t%04d." PL_BOM ".s%04d", geracao, geracao);
  { int bom = bearer && !strcmp(bearer, esperado) && !__atomic_load_n(&forcar401, __ATOMIC_SEQ_CST);
    pthread_mutex_unlock(&srv);
    usleep(2000);
    return resp(bom ? 200 : 401, bom ? "[]" : "{\"message\":\"JWT expired\"}", status); }
}
char *nuvem_rpc_com(const char *funcao, const char *corpo, const char *bearer, int *status) {
  char c[300]; snprintf(c, sizeof c, "/rest/v1/rpc/%s", funcao);
  return nuvem_post(c, corpo, bearer, status);
}
char *nuvem_tabela(const char *t, const char *q, const char *b, int *status) {
  (void)t; (void)q; return nuvem_post("/rest/v1/tabela", "", b, status);
}

// ------------------------------------------------------------ roteiro
static int falhas;
static void confere(const char *d, int ok) {
  printf("  %-62s %s\n", d, ok ? "ok" : "FALHOU");
  if (!ok) falhas++;
}

#define FIOS 16
static int stDe[FIOS];
// Largada junta (macOS nao tem pthread_barrier).
static pthread_mutex_t lgT = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  lgC = PTHREAD_COND_INITIALIZER;
static int lgN;
static void largar(void) {
  pthread_mutex_lock(&lgT);
  if (++lgN == FIOS) pthread_cond_broadcast(&lgC);
  else while (lgN < FIOS) pthread_cond_wait(&lgC, &lgT);
  pthread_mutex_unlock(&lgT);
}
static void *fioRpc(void *u) {
  int i = (int)(long)u, st = -1;
  char *r;
  largar();
  r = sessao_rpc("sync_pull_collections", "{}", &st);
  stDe[i] = st;
  free(r);
  return NULL;
}
static void rodarFios(void) {
  pthread_t t[FIOS]; int i;
  lgN = 0;
  for (i = 0; i < FIOS; i++) pthread_create(&t[i], NULL, fioRpc, (void *)(long)i);
  for (i = 0; i < FIOS; i++) pthread_join(t[i], NULL);
}
static int todos(int st) { int i; for (i = 0; i < FIOS; i++) if (stDe[i] != st) return 0; return 1; }

static int parar;                // so via __atomic
static int rasgados, lidos;
static void *fioLeitor(void *u) {
  char t[3000];
  (void)u;
  while (!__atomic_load_n(&parar, __ATOMIC_SEQ_CST)) {
#ifdef ANTIGO
    snprintf(t, sizeof t, "%s", sessao_token());
#else
    sessao_token_copiar(t, sizeof t);
#endif
    if (t[0]) {
      int a = -1, b = -2;
      char meio[200];
      if (sscanf(t, "t%d.%199[^.].s%d", &a, meio, &b) != 3 || a != b || strcmp(meio, PL_BOM))
        __atomic_add_fetch(&rasgados, 1, __ATOMIC_RELAXED);
      __atomic_add_fetch(&lidos, 1, __ATOMIC_RELAXED);
    }
  }
  return NULL;
}
static void *fioRpcLaco(void *u) {
  int i; (void)u;
  for (i = 0; i < 40; i++) { int st; free(sessao_rpc("x", "{}", &st)); }
  return NULL;
}
static void *fioRevoga(void *u) {
  (void)u;
  while (!__atomic_load_n(&parar, __ATOMIC_SEQ_CST)) { usleep(15000); __atomic_store_n(&forcar401, 1, __ATOMIC_SEQ_CST); }
  return NULL;
}

static void gravar(const char *payload) {
  char b[400];
  snprintf(b, sizeof b, "t0001.%s.s0001\nr1\n0\n", payload);
  dados_gravar("sessao.txt", b);
}

int main(int argc, char **argv) {
  const char *caso = argc > 1 ? argv[1] : "";
  setvbuf(stdout, NULL, _IOLBF, 0);
  printf("-- sessao_corrida: %s\n", caso);
  if (!strcmp(caso, "rpc401") || !strcmp(caso, "vencido")) {
    // rpc401: t0001 nao venceu pelo relogio, mas o servidor o recusa (401)
    // ate a renovacao. vencido: o `exp` do t0001 ja passou. Nos dois, r1 e o
    // refresh valido e a primeira renovacao o gira para r2.
    gravar(!strcmp(caso, "rpc401") ? PL_BOM : PL_VENCIDO);
    if (!strcmp(caso, "rpc401")) __atomic_store_n(&forcar401, 1, __ATOMIC_SEQ_CST);
    sessao_iniciar();
    rodarFios();
    printf("  renovacoes=%d recusas400=%d\n", renovacoes, recusas400);
    confere("16 fios: UMA renovacao so", renovacoes == 1);
    confere("nenhum refresh token reusado (400)", recusas400 == 0);
    confere("todos os 16 pedidos voltaram 200", todos(200));
    confere("continua logado", sessao_logada());
  } else if (!strcmp(caso, "r504")) {
    gravar(PL_VENCIDO);
    stRenovar = 504;
    sessao_iniciar();
    rodarFios();
    printf("  renovacoes=%d\n", renovacoes);
    confere("renovacao em 504 com 16 fios: UM pedido ao servidor", renovacoes == 1);
    confere("todos recebem 504 (falha do servidor, nao 401)", todos(504));
    confere("sessao fica", sessao_logada());
  } else if (!strcmp(caso, "leitores")) {
    pthread_t l[4], r[8], v;
    int i;
    gravar(PL_BOM);
    sessao_iniciar();
    for (i = 0; i < 4; i++) pthread_create(&l[i], NULL, fioLeitor, NULL);
    pthread_create(&v, NULL, fioRevoga, NULL);
    for (i = 0; i < 8; i++) pthread_create(&r[i], NULL, fioRpcLaco, NULL);
    for (i = 0; i < 8; i++) pthread_join(r[i], NULL);
    __atomic_store_n(&parar, 1, __ATOMIC_SEQ_CST);
    for (i = 0; i < 4; i++) pthread_join(l[i], NULL);
    pthread_join(v, NULL);
    printf("  renovacoes=%d recusas400=%d lidos=%d rasgados=%d\n", renovacoes, recusas400, lidos, rasgados);
    confere("nenhuma copia do token rasgada", rasgados == 0);
    confere("nenhum refresh token reusado (400)", recusas400 == 0);
    confere("continua logado", sessao_logada());
  } else if (!strcmp(caso, "copia") || !strcmp(caso, "copia504")) {
    // Quem leva o token ao Worker pela sessao_token_copiar: um token VENCIDO
    // nao sai. A copia renova uma vez e devolve o novo; se a renovacao falha,
    // sai token nenhum (nada vai ao Worker).
    char t[3000];
    int ok;
    t[0] = 0;
    gravar(PL_VENCIDO);
    if (!strcmp(caso, "copia504")) stRenovar = 504;
    sessao_iniciar();
    ok = sessao_token_copiar(t, sizeof t);
    printf("  renovacoes=%d token=%.12s\n", renovacoes, t);
    if (!strcmp(caso, "copia")) {
      confere("token vencido: a copia renova UMA vez", renovacoes == 1);
      confere("devolve o token novo, nao o vencido",
              ok && !strstr(t, PL_VENCIDO) && !strncmp(t, "t0002.", 6));
      confere("token novo: segunda copia nao renova de novo",
              sessao_token_copiar(t, sizeof t) && renovacoes == 1);
      confere("continua logado", sessao_logada());
    } else {
      confere("renovacao em 504: copia devolve 0 e dst vazio", !ok && t[0] == 0);
      confere("renovacao em 504: UM pedido ao servidor", renovacoes == 1);
      confere("sessao fica", sessao_logada());
    }
  } else if (!strcmp(caso, "copia-sem-refresh")) {
    char t[3000] = "sentinela";
    int ok;
    dados_gravar("sessao.txt", "t0001." PL_VENCIDO ".s0001\n\n0\n");
    sessao_iniciar();
    ok = sessao_token_copiar(t, sizeof t);
    confere("vencido sem refresh: devolve 0 e dst vazio", !ok && !t[0]);
    confere("sem refresh: nenhum pedido de renovacao", renovacoes == 0);
  } else if (!strcmp(caso, "recusado")) {
    // O Worker recusa com 401 o token que mandamos: UMA renovacao. Um 401 que
    // chega com o token ja trocado (outro fio renovou) nao renova de novo.
    char t[3000];
    t[0] = 0;
    gravar(PL_BOM);
    sessao_iniciar();
    sessao_token_copiar(t, sizeof t);
    sessao_token_recusado(t);
    confere("401 do Worker: UMA renovacao", renovacoes == 1);
    sessao_token_recusado(t);
    confere("401 com o token ja trocado: nao renova de novo", renovacoes == 1);
    confere("continua logado", sessao_logada());
    confere("o token seguinte e o novo", sessao_token_copiar(t, sizeof t) && !strncmp(t, "t0002.", 6));
  } else {
    printf("caso desconhecido\n");
    return 2;
  }
  printf("%s\n", falhas ? "FALHOU" : "PASSOU");
  return falhas ? 1 : 0;
}
