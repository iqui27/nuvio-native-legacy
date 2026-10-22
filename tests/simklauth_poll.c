// POLL DO PIN DO SIMKL (#266: "o codigo expira rapido demais / o Simkl recusou
// o codigo" antes de dar tempo de digitar o PIN). Respostas de AMOSTRA, sem
// token nem client_id de verdade. Pendente, 429, 5xx, corpo vazio e HTML NAO
// encerram; so negado/expirado ou o prazo do expires_in.
#include "simklauth.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *dados_ler(const char *n) { (void)n; return NULL; }
int dados_gravar(const char *n, const char *c) { (void)n; (void)c; return 1; }
int dados_apagar(const char *n) { (void)n; return 0; }
const char *nuvem_simkl_cliente(void) { return "cliente-teste"; }
const char *nuvem_simkl_app(void)     { return "nuvio"; }
void nuvem_url_escapar(const char *v, char *d, unsigned t) { snprintf(d, t, "%s", v); }
int  sync_empurrar_credencial(const char *p, const char *j) { (void)p; (void)j; return 0; }
const char *i18n(const char *s) { return s; }

// roteiro do poll: cada chamada consome uma entrada
static struct { int st; const char *corpo; } roteiro[16];
static int nRot, iRot, nPolls, nPedidos;
static unsigned instantes[4096];
extern unsigned teste_agora;
unsigned teste_agora;
char *rede_baixar_st(const char *url, int seg, const char *const *cab, int *st) {
  (void)seg; (void)cab;
  if (strstr(url, "/oauth/pin/")) {
    instantes[nPolls++] = teste_agora;
    if (iRot < nRot) { *st = roteiro[iRot].st; return roteiro[iRot++].corpo ? strdup(roteiro[iRot - 1].corpo) : NULL; }
    *st = 200; return strdup("{\"result\":\"KO\",\"message\":\"Authorization pending\"}");
  }
  nPedidos++;
  *st = 200;
  return strdup("{\"result\":\"OK\",\"user_code\":\"AB12\",\"verification_url\":\"https://simkl.com/pin\","
                "\"expires_in\":600,\"interval\":5,\"device_code\":\"x\"}");
}

#include <unistd.h>
// avanca o relogio em 1 s por volta ate `ate_ms`, deixando o fio terminar
static void anda(unsigned ate_ms) {
  while (teste_agora < ate_ms) { teste_agora += 1000; simklauth_passo(teste_agora); usleep(1500); }
}

int main(void) {
  char t[64];
  // --- classificacao
  assert(simklauth_classificar(200, "{\"result\":\"KO\",\"message\":\"Authorization pending\"}", t, sizeof t) == SMK_POLL_ESPERA);
  assert(simklauth_classificar(200, "{\"result\": \"KO\"}", t, sizeof t) == SMK_POLL_ESPERA);
  assert(simklauth_classificar(200, "{\"result\":\"OK\",\"access_token\":\"TOKEN-FALSO\"}", t, sizeof t) == SMK_POLL_OK && !strcmp(t, "TOKEN-FALSO"));
  assert(simklauth_classificar(200, "{\"result\":\"KO\",\"message\":\"Slow down\"}", t, sizeof t) == SMK_POLL_LENTO);
  assert(simklauth_classificar(429, "", t, sizeof t) == SMK_POLL_LENTO);
  assert(simklauth_classificar(200, "{\"result\":\"KO\",\"message\":\"Code expired\"}", t, sizeof t) == SMK_POLL_NEGADO);
  assert(simklauth_classificar(200, "{\"result\":\"KO\",\"message\":\"Access denied\"}", t, sizeof t) == SMK_POLL_NEGADO);
  assert(simklauth_classificar(200, "", t, sizeof t) == SMK_POLL_FALHA);          // antes: "invalidou"
  assert(simklauth_classificar(200, "<html>Just a moment</html>", t, sizeof t) == SMK_POLL_FALHA);
  assert(simklauth_classificar(503, "{}", t, sizeof t) == SMK_POLL_FALHA);        // antes: erro fatal
  assert(simklauth_classificar(0, NULL, t, sizeof t) == SMK_POLL_FALHA);
  printf("ok  classificacao das respostas do poll\n");

  // --- fluxo: lixo transitorio no meio nao derruba; ritmo = interval; so um PIN
  teste_agora = 100000;
  roteiro[nRot].st = 200; roteiro[nRot++].corpo = "";
  roteiro[nRot].st = 503; roteiro[nRot++].corpo = "<html>";
  roteiro[nRot].st = 0;   roteiro[nRot++].corpo = NULL;
  simklauth_carregar_perfil(1);
  simklauth_comecar();
  anda(teste_agora + 3000);              // pede o PIN
  anda(teste_agora + 120000);            // 2 minutos de poll
  assert(simklauth_estado() == SMK_AGUARDANDO);
  assert(nPedidos == 1);                 // nenhum codigo novo pedido
  { int i, minimo = 1 << 30;
    for (i = 1; i < nPolls; i++) if ((int)(instantes[i] - instantes[i - 1]) < minimo) minimo = (int)(instantes[i] - instantes[i - 1]);
    assert(nPolls > 10 && minimo >= 5000); }
  { int r = simklauth_restante_s(); assert(r > 400 && r < 600); }   // 600 s - ~123 s
  printf("ok  respostas ruins nao encerram; poll >= interval; um PIN so; contagem regressiva\n");

  // --- prazo: so expira apos expires_in (600 s)
  anda(teste_agora + 400000);
  assert(simklauth_estado() == SMK_AGUARDANDO);
  anda(teste_agora + 120000);
  assert(simklauth_estado() == SMK_ERRO && !strcmp(simklauth_erro(), "o código expirou"));
  printf("ok  so expira depois de expires_in\n");

  // --- negado de verdade encerra
  nRot = iRot = 0; roteiro[nRot].st = 200; roteiro[nRot++].corpo = "{\"result\":\"KO\",\"message\":\"Code expired\"}";
  simklauth_cancelar(); simklauth_comecar();
  anda(teste_agora + 30000);
  assert(simklauth_estado() == SMK_ERRO);
  printf("simklauth_poll: tudo ok\n");
  return 0;
}
