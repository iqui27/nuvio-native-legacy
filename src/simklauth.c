#include "simklauth.h"
#include "idioma.h"
#include "nuvem.h"
#include "dados.h"
#include "rede.h"
#include "sync.h"
#include "js.h"
#include "jsw.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define SMK_ARQ  "simkl.txt"
#define SMK_BASE "https://api.simkl.com"
// O Simkl nao devolve `interval`; 5s e o passo que o app web usa.
#define SMK_POLL_MS 5000u

static SmkEstado estado = SMK_PARADO;
static char userCode[48];
static char url[200];
static char erro[200];
static char token[300];
static unsigned proximoPoll, comecouMs, limiteMs = 900000u;

static pthread_t fio;
static int fioVivo, fioPronto, tokenNovo;

// Todo pedido leva client_id, app-name e app-version na QUERY — nao em
// cabecalho. Sem eles o Simkl responde erro sem dizer o que faltou.
static char *pegar(const char *caminho, int *status) {
  char completo[500], cid[200], nome[120];
  const char *cab[2];
  nuvem_url_escapar(nuvem_simkl_cliente(), cid, sizeof cid);
  nuvem_url_escapar(nuvem_simkl_app()[0] ? nuvem_simkl_app() : "nuvio", nome, sizeof nome);
  snprintf(completo, sizeof completo,
           "%s%s?client_id=%s&app-name=%s&app-version=1.0.1", SMK_BASE, caminho, cid, nome);
  cab[0] = "Accept: application/json";
  cab[1] = NULL;
  return rede_baixar_st(completo, 20, cab, status);
}

// ---------------------------------------------------------------- disco

static void gravar(void) {
  char buf[400];
  snprintf(buf, sizeof buf, "%s\n", token);
  dados_gravar(SMK_ARQ, buf);
}

int simklauth_carregar(void) {
  char *b = dados_ler(SMK_ARQ);
  if (!b) return 0;
  { char *fim = b + strlen(b);
    while (fim > b && (fim[-1] == '\n' || fim[-1] == '\r')) *--fim = 0; }
  if (b[0]) { snprintf(token, sizeof token, "%s", b); estado = SMK_LIGADO; }
  free(b);
  return token[0] != 0;
}

void simklauth_esquecer(void) {
  token[0] = userCode[0] = url[0] = erro[0] = 0;
  estado = SMK_PARADO;
  dados_apagar(SMK_ARQ);
}

// ---------------------------------------------------------------- fluxo

static void *fioPedir(void *u) {
  char *r;
  int st = 0;
  (void)u;
  erro[0] = userCode[0] = 0;

  if (!nuvem_simkl_cliente()[0]) {
    snprintf(erro, sizeof erro, "pacote sem a chave do Simkl");
    estado = SMK_ERRO;
    fioPronto = 1;
    return NULL;
  }

  r = pegar("/oauth/pin", &st);
  if (r && st >= 200 && st < 300) {
    const char *fim = r + strlen(r);
    double expira;
    js_texto(r, fim, "user_code", userCode, sizeof userCode);
    // O campo aparece nas duas grafias na documentacao; aceitar as duas evita
    // uma tela vazia por causa de um "i" a menos.
    if (!js_texto(r, fim, "verification_url", url, sizeof url))
      js_texto(r, fim, "verification_uri", url, sizeof url);
    expira = js_num(r, fim, "expires_in", 0);
    if (expira > 30.0 && expira < 3600.0) limiteMs = (unsigned)(expira * 1000.0);
  }
  if (!userCode[0]) {
    snprintf(erro, sizeof erro, i18n("nao consegui pedir o codigo ao Simkl (HTTP %d)"), st);
    estado = SMK_ERRO;
  } else {
    if (!url[0]) snprintf(url, sizeof url, "https://simkl.com/pin");
    estado = SMK_AGUARDANDO;
  }
  free(r);
  fioPronto = 1;
  return NULL;
}

static void *fioPoll(void *u) {
  char caminho[120], *r;
  int st = 0;
  (void)u;
  snprintf(caminho, sizeof caminho, "/oauth/pin/%s", userCode);
  r = pegar(caminho, &st);
  if (r && st >= 200 && st < 300) {
    char res[16], t[300];
    const char *fim = r + strlen(r);
    js_texto(r, fim, "result", res, sizeof res);
    if (!strcmp(res, "KO")) {
      /* ainda nao autorizado */
    } else if (js_texto(r, fim, "access_token", t, sizeof t) && t[0]) {
      snprintf(token, sizeof token, "%s", t);
      tokenNovo = 1;
      estado = SMK_LIGADO;
    } else {
      // Resposta que nao e KO nem traz token: o Simkl invalidou este PIN.
      snprintf(erro, sizeof erro, "o Simkl invalidou este código");
      estado = SMK_ERRO;
    }
  } else if (st) {
    snprintf(erro, sizeof erro, i18n("falha ao consultar o Simkl (HTTP %d)"), st);
    estado = SMK_ERRO;
  }
  free(r);
  fioPronto = 1;
  return NULL;
}

static void soltar(void *(*rotina)(void *)) {
  if (fioVivo) return;
  fioPronto = 0;
  if (pthread_create(&fio, NULL, rotina, NULL) == 0) { pthread_detach(fio); fioVivo = 1; }
  else { snprintf(erro, sizeof erro, "sem fio para falar com o Simkl"); estado = SMK_ERRO; }
}

void simklauth_comecar(void) {
  if (estado == SMK_PEDINDO || estado == SMK_AGUARDANDO) return;
  erro[0] = 0;
  comecouMs = 0;
  estado = SMK_PEDINDO;
  soltar(fioPedir);
}

void simklauth_passo(unsigned agoraMs) {
  if (fioVivo && fioPronto) { fioVivo = 0; fioPronto = 0; }
  if (fioVivo) return;

  if (tokenNovo) {
    Jsw c;
    tokenNovo = 0;
    gravar();
    jsw_iniciar(&c);
    jsw_obj_ini(&c);
    jsw_cs(&c, "access_token", token);
    jsw_obj_fim(&c);
    sync_empurrar_credencial("simkl", jsw_texto_final(&c));
    jsw_livre(&c);
    printf("[simkl] vinculado nesta TV\n");
    fflush(stdout);
  }

  if (estado != SMK_AGUARDANDO) return;
  if (!comecouMs) comecouMs = agoraMs;
  if (agoraMs - comecouMs > limiteMs) {
    snprintf(erro, sizeof erro, "o código expirou");
    estado = SMK_ERRO;
    return;
  }
  if (agoraMs >= proximoPoll) {
    proximoPoll = agoraMs + SMK_POLL_MS;
    soltar(fioPoll);
  }
}

void simklauth_cancelar(void) {
  if (estado == SMK_PEDINDO || estado == SMK_AGUARDANDO || estado == SMK_ERRO)
    estado = token[0] ? SMK_LIGADO : SMK_PARADO;
}

SmkEstado   simklauth_estado(void) { return estado; }
const char *simklauth_codigo(void) { return userCode; }
const char *simklauth_url(void)    { return url; }
const char *simklauth_erro(void)   { return erro; }
