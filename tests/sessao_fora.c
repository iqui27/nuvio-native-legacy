// O SERVIDOR DA CONTA FORA DO AR NAO DESLOGA NINGUEM (#215).
//
// Antes: qualquer falha da renovacao do token — inclusive 504 ou nenhuma
// resposta — apagava sessao.txt ("renovacao recusada: saindo"). Com
// api.nuvio.tv em 504 por horas (02/10/2026), cada TV cujo token vencesse
// nesse intervalo saia da conta sozinha e nao conseguia entrar de novo, porque
// o login fala com o mesmo servidor. E o pedido de codigo mostrava so "could
// not request the code (HTTP 504)".
//
// Este teste roda o sessao.c REAL contra um nuvem.c dublado:
//   1. token vencido + renovacao 504 / sem resposta / 429: sessao FICA (em
//      memoria e em disco) e quem chamou recebe o HTTP do servidor;
//   2. renovacao recusada de verdade (400): sessao sai, como antes;
//   3. RPC 401 + renovacao 503: sessao fica;
//   4. pedido de codigo com 504 / sem resposta / 400 em texto puro: a frase
//      certa na tela, e o fluxo pode ser repetido (Tentar de novo);
//   5. troca do codigo com 503: continua esperando, nao pede QR novo.
#include "sessao.h"
#include "nuvem.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "nvtmp.h"

// ------------------------------------------------------------ disco dublado
static const char *pasta(void) { const char *d = getenv("NV_T_DIR"); return d && *d ? d : nvt_dir(); }
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
  snprintf(c, sizeof c, "%s/%s", pasta(), nome);
  f = fopen(c, "wb"); if (!f) return 0; fputs(conteudo ? conteudo : "", f); fclose(f); return 1;
}
int dados_apagar(const char *nome) {
  char c[600]; snprintf(c, sizeof c, "%s/%s", pasta(), nome); return remove(c) == 0;
}
void dados_uuid(char *dst, unsigned tam) { snprintf(dst, tam, "00000000-0000-4000-8000-000000000000"); }
const char *i18n(const char *s) { return s; }

// ------------------------------------------------------------ servidor dublado
static int stRenovar = 200, stRpc = 200, stPedir = 200, stTroca = 200;
static const char *corpoPedir = NULL;
static int renovacoes, falhasFreio, trocas;
static int aprovar;   // a pessoa ja autorizou no celular
// JWT com payload {"sub":"u1","exp":1} (vencido) e {"sub":"u1","exp":4102444800} (2100).
#define JWT_VENCIDO "x.eyJzdWIiOiJ1MSIsImV4cCI6MX0.y"
#define JWT_BOM     "x.eyJzdWIiOiJ1MSIsImV4cCI6NDEwMjQ0NDgwMH0.y"

int nuvem_pronta(void) { return 1; }
const char *nuvem_base_login(void) { return "https://login.exemplo/tv"; }
int nuvem_erro_ausente(const char *c) { (void)c; return 0; }
void nuvem_falhou(void) { falhasFreio++; }
void nuvem_ok(void) { }
const char *nuvem_ultimo_erro(void) { return ""; }
static char *resposta(int st, const char *corpo, int *status) {
  *status = st;
  if (!st) return NULL;
  return strdup(corpo);
}
char *nuvem_post(const char *caminho, const char *corpo, const char *bearer, int *status) {
  (void)corpo; (void)bearer;
  if (strstr(caminho, "grant_type=refresh_token")) {
    renovacoes++;
    return resposta(stRenovar, stRenovar == 200
      ? "{\"access_token\":\"" JWT_BOM "\",\"refresh_token\":\"r2\"}"
      : stRenovar == 400 ? "{\"error\":\"invalid_grant\"}" : "error code: 504", status);
  }
  if (strstr(caminho, "/auth/v1/signup"))
    return resposta(200, "{\"access_token\":\"" JWT_BOM "\",\"refresh_token\":\"anon\"}", status);
  if (strstr(caminho, "start_tv_login_session"))
    return resposta(stPedir, corpoPedir ? corpoPedir : stPedir != 200 ? "error code: 504"
      : "{\"code\":\"abc\",\"web_url\":\"https://login.exemplo/tv?code=abc\",\"poll_interval_seconds\":1}", status);
  if (strstr(caminho, "poll_tv_login_session"))
    return resposta(200, aprovar ? "{\"status\":\"approved\"}" : "{\"status\":\"pending\"}", status);
  if (strstr(caminho, "tv-logins-exchange")) {
    __atomic_add_fetch(&trocas, 1, __ATOMIC_RELAXED);
    return resposta(stTroca, stTroca == 200
      ? "{\"access_token\":\"" JWT_BOM "\",\"refresh_token\":\"r3\"}" : "error code: 503", status);
  }
  return resposta(stRpc, stRpc == 200 ? "[]" : stRpc == 401 ? "{\"message\":\"JWT expired\"}" : "error code: 504", status);
}
char *nuvem_rpc_com(const char *funcao, const char *corpo, const char *bearer, int *status) {
  char c[300]; snprintf(c, sizeof c, "/rest/v1/rpc/%s", funcao);
  return nuvem_post(c, corpo, bearer, status);
}
char *nuvem_tabela(const char *t, const char *q, const char *b, int *status) {
  (void)t; (void)q; (void)b; return resposta(stRpc, "[]", status);
}

// ------------------------------------------------------------ roteiro
static int falhas;
static void confere(const char *d, int ok) {
  printf("  %-66s %s\n", d, ok ? "ok" : "FALHOU");
  if (!ok) falhas++;
}
static int temArquivo(void) { char *b = dados_ler("sessao.txt"); int ok = b && b[0]; free(b); return ok; }
static void gravarSessao(const char *jwt) {
  char b[400]; snprintf(b, sizeof b, "%s\nrefresh-1\n0\n", jwt); dados_gravar("sessao.txt", b);
}
static unsigned agora = 1000;
static void esperarFluxo(SesEstado ate) {
  int i;
  for (i = 0; i < 6000; i++) {
    sessao_passo(agora += 20);
    if (sessao_estado() == ate || sessao_estado() == SES_ERRO || sessao_estado() == SES_LOGADO) {
      int j; for (j = 0; j < 5; j++) { sessao_passo(agora += 20); usleep(1000); }
      if (sessao_estado() == ate || sessao_estado() == SES_ERRO || sessao_estado() == SES_LOGADO) return;
    }
    usleep(1000);
  }
}

int main(int argc, char **argv) {
  const char *caso = argc > 1 ? argv[1] : "";
  int st = -5;
  char *r;
  setvbuf(stdout, NULL, _IOLBF, 0);
  printf("-- sessao: %s\n", caso);
  if (!strcmp(caso, "renova504") || !strcmp(caso, "renova0") || !strcmp(caso, "renova429")) {
    stRenovar = !strcmp(caso, "renova504") ? 504 : !strcmp(caso, "renova429") ? 429 : 0;
    gravarSessao(JWT_VENCIDO);
    sessao_iniciar();
    r = sessao_rpc("sync_pull_collections", "{}", &st);
    confere("token vencido, renovacao falha no servidor: RPC sem corpo", r == NULL);
    confere("quem chamou recebe o HTTP do servidor (nao 401)", st == stRenovar);
    confere("sessao continua logada em memoria", sessao_logada());
    confere("sessao.txt continua no disco", temArquivo());
    confere("a falha aciona o freio", falhasFreio > 0);
    free(r);
    // O servidor volta: a mesma sessao renova e a RPC passa.
    stRenovar = 200;
    r = sessao_rpc("sync_pull_collections", "{}", &st);
    confere("servidor de volta: renova e a RPC responde", r && st == 200 && sessao_logada());
    free(r);
  } else if (!strcmp(caso, "renova400")) {
    stRenovar = 400;
    gravarSessao(JWT_VENCIDO);
    sessao_iniciar();
    r = sessao_rpc("sync_pull_collections", "{}", &st);
    confere("renovacao RECUSADA (400): sessao sai, como antes", !sessao_logada() && !temArquivo());
    free(r);
  } else if (!strcmp(caso, "rpc401")) {
    stRpc = 401; stRenovar = 503;
    gravarSessao(JWT_BOM);
    sessao_iniciar();
    r = sessao_rpc("sync_pull_collections", "{}", &st);
    confere("RPC 401 + renovacao 503: status 503, sessao fica",
            !r && st == 503 && sessao_logada() && temArquivo());
    free(r);
  } else if (!strcmp(caso, "pedir504") || !strcmp(caso, "pedir0") || !strcmp(caso, "pedir400")) {
    stPedir = !strcmp(caso, "pedir504") ? 504 : !strcmp(caso, "pedir400") ? 400 : 0;
    if (stPedir == 400) corpoPedir = "error code: 400";
    sessao_login_comecar();
    esperarFluxo(SES_AGUARDANDO);
    printf("  tela: %s\n", sessao_erro());
    confere("pedido de codigo falhou: estado de erro (botao Tentar de novo)", sessao_estado() == SES_ERRO);
    if (stPedir == 504)
      confere("frase: servidor nao respondeu (HTTP 504), tentar em minutos",
              !strcmp(sessao_erro(), "O servidor da conta Nuvio não respondeu (HTTP 504). Tente de novo em alguns minutos."));
    else if (stPedir == 0)
      confere("frase: servidor nao respondeu, classe de transporte sem HTTP",
              strstr(sessao_erro(), "[transport; curl 0]") != NULL);
    else
      confere("frase: servidor recusou o pedido do codigo (HTTP 400)",
              !strcmp(sessao_erro(), "O servidor da conta Nuvio recusou o pedido do código (HTTP 400). Tente de novo em alguns minutos."));
    // Tentar de novo com o servidor de volta: o codigo aparece.
    stPedir = 200; corpoPedir = NULL;
    sessao_login_comecar();
    esperarFluxo(SES_AGUARDANDO);
    confere("Tentar de novo com o servidor de volta: codigo na tela",
            sessao_estado() == SES_AGUARDANDO && !strcmp(sessao_codigo(), "abc"));
  } else if (!strcmp(caso, "troca503")) {
    stTroca = 503;
    aprovar = 1;
    sessao_login_comecar();
    esperarFluxo(SES_AGUARDANDO);
    { int i; for (i = 0; i < 5000; i++) {
        sessao_passo(agora += 1); usleep(1000);
        if (__atomic_load_n(&trocas, __ATOMIC_RELAXED) >= 3 && sessao_estado() == SES_AGUARDANDO) break;
      } }
    confere("autorizado mas troca 503: continua esperando, sem erro na tela",
            sessao_estado() == SES_AGUARDANDO && !sessao_logada());
    stTroca = 200;
    { int i; for (i = 0; i < 5000 && !sessao_logada(); i++) { sessao_passo(agora += 1); usleep(1000); } }
    confere("servidor de volta: a mesma autorizacao entra, sem QR novo", sessao_logada());
  } else {
    printf("caso desconhecido\n");
    return 2;
  }
  printf("%s\n", falhas ? "FALHOU" : "PASSOU");
  return falhas ? 1 : 0;
}
