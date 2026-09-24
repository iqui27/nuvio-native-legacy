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

// O VINCULO E POR PERFIL (simkl-p<N>.txt), pelo mesmo motivo e com a mesma
// migracao do Trakt (ver o topo de traktauth.c): o simkl.txt antigo e do
// perfil 1 — antes dos perfis o app so sincronizava o 1 — e e RENOMEADO para
// simkl-p1.txt, nunca copiado para outro perfil.
#define SMK_ARQ_LEGADO "simkl.txt"
#define SMK_ARQ_FMT    "simkl-p%d.txt"
#define SMK_PERFIS     16   // o logout varre p0..16, como fontepref/traktauth
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

// DE QUEM E O ESTADO ACIMA. Mesma costura de traktauth.c: a troca de perfil
// sobe `geracao`, e um fio que saiu antes dela nunca publica nas variaveis
// vivas — um PIN autorizado depois da troca vai para o arquivo do perfil que
// o pediu.
static int perfil = 1;
static unsigned geracao, gerFio;
static int perfilFio;
static char userCodeFio[48];
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;

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

// Por valor, nao em buffer estatico: o fio do poll monta o nome do perfil dele
// enquanto o laco principal monta o do perfil novo (ver arqToken em traktauth.c).
typedef struct { char s[40]; } SmkNome;
static SmkNome arq(int p) {
  SmkNome n;
  snprintf(n.s, sizeof n.s, SMK_ARQ_FMT, p);
  return n;
}

static void gravarEm(int p, const char *tk) {
  char buf[400];
  snprintf(buf, sizeof buf, "%s\n", tk);
  dados_gravar(arq(p).s, buf);
}
static void gravar(void) { gravarEm(perfil, token); }

// simkl.txt antigo -> simkl-p1.txt. Idempotente; ver a nota no topo.
static void migrarLegado(void) {
  char *b = dados_ler(SMK_ARQ_LEGADO), *ja;
  if (!b) return;
  ja = dados_ler(arq(1).s);
  if (ja) { free(ja); dados_apagar(SMK_ARQ_LEGADO); }
  else if (dados_gravar(arq(1).s, b)) {
    dados_apagar(SMK_ARQ_LEGADO);
    printf("[simkl] simkl.txt migrado para simkl-p1.txt (so o perfil 1)\n");
    fflush(stdout);
  }
  free(b);
}

int simklauth_carregar(void) {
  char *b;
  migrarLegado();
  b = dados_ler(arq(perfil).s);
  if (!b) return 0;
  { char *fim = b + strlen(b);
    while (fim > b && (fim[-1] == '\n' || fim[-1] == '\r')) *--fim = 0; }
  if (b[0]) { snprintf(token, sizeof token, "%s", b); estado = SMK_LIGADO; }
  free(b);
  return token[0] != 0;
}

const char *simklauth_token(void) { return token; }

static void zerarEstado(void) {
  token[0] = userCode[0] = url[0] = erro[0] = 0;
  tokenNovo = 0;
  comecouMs = 0;
  estado = SMK_PARADO;
}

int simklauth_carregar_perfil(int p) {
  pthread_mutex_lock(&trava);
  perfil = p > 0 ? p : 1;
  geracao++;
  zerarEstado();
  pthread_mutex_unlock(&trava);
  return simklauth_carregar();
}

int simklauth_trocar_perfil(int p) {
  int antes;
  if (p <= 0) p = 1;
  if (p == perfil) return 0;
  antes = token[0] != 0;
  simklauth_carregar_perfil(p);
  printf("[simkl] perfil %d: %s\n", perfil,
         token[0] ? "vinculo deste perfil carregado" : "sem vinculo neste perfil");
  fflush(stdout);
  return antes || token[0];
}

void simklauth_esquecer(void) {
  int p;
  pthread_mutex_lock(&trava);
  geracao++;
  zerarEstado();
  // LOGOUT: todos os perfis e o arquivo antigo.
  for (p = 0; p <= SMK_PERFIS; p++) dados_apagar(arq(p).s);
  dados_apagar(SMK_ARQ_LEGADO);
  perfil = 1;
  pthread_mutex_unlock(&trava);
}

// ---------------------------------------------------------------- fluxo

static void *fioPedir(void *u) {
  char *r;
  int st = 0;
  char uc[48] = "", vu[200] = "";
  unsigned novoLimite = 0;
  (void)u;

  if (!nuvem_simkl_cliente()[0]) {
    pthread_mutex_lock(&trava);
    if (gerFio == geracao) {
      snprintf(erro, sizeof erro, "pacote sem a chave do Simkl");
      estado = SMK_ERRO;
    }
    pthread_mutex_unlock(&trava);
    fioPronto = 1;
    return NULL;
  }

  r = pegar("/oauth/pin", &st);
  if (r && st >= 200 && st < 300) {
    const char *fim = r + strlen(r);
    double expira;
    js_texto(r, fim, "user_code", uc, sizeof uc);
    // O campo aparece nas duas grafias na documentacao; aceitar as duas evita
    // uma tela vazia por causa de um "i" a menos.
    if (!js_texto(r, fim, "verification_url", vu, sizeof vu))
      js_texto(r, fim, "verification_uri", vu, sizeof vu);
    expira = js_num(r, fim, "expires_in", 0);
    if (expira > 30.0 && expira < 3600.0) novoLimite = (unsigned)(expira * 1000.0);
  }
  pthread_mutex_lock(&trava);
  // PIN pedido por um perfil que ja saiu da tela: ninguem vai digita-lo.
  if (gerFio != geracao) {
    pthread_mutex_unlock(&trava);
    free(r);
    fioPronto = 1;
    return NULL;
  }
  erro[0] = 0;
  snprintf(userCode, sizeof userCode, "%s", uc);
  snprintf(url, sizeof url, "%s", vu);
  if (novoLimite) limiteMs = novoLimite;
  if (!userCode[0]) {
    snprintf(erro, sizeof erro, i18n("nao consegui pedir o codigo ao Simkl (HTTP %d)"), st);
    estado = SMK_ERRO;
  } else {
    if (!url[0]) snprintf(url, sizeof url, "https://simkl.com/pin");
    estado = SMK_AGUARDANDO;
  }
  pthread_mutex_unlock(&trava);
  free(r);
  fioPronto = 1;
  return NULL;
}

static void *fioPoll(void *u) {
  char caminho[120], *r;
  int st = 0;
  (void)u;
  snprintf(caminho, sizeof caminho, "/oauth/pin/%s", userCodeFio);
  r = pegar(caminho, &st);
  pthread_mutex_lock(&trava);
  if (gerFio != geracao) {
    // Autorizado depois da troca de perfil: e do perfil que pediu o PIN.
    char t[300];
    if (r && st >= 200 && st < 300 &&
        js_texto(r, r + strlen(r), "access_token", t, sizeof t) && t[0]) {
      gravarEm(perfilFio, t);
      printf("[simkl] autorizacao do perfil %d chegou depois da troca; guardada no arquivo dele\n",
             perfilFio);
    }
    pthread_mutex_unlock(&trava);
    free(r);
    fioPronto = 1;
    return NULL;
  }
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
  pthread_mutex_unlock(&trava);
  free(r);
  fioPronto = 1;
  return NULL;
}

static void soltar(void *(*rotina)(void *)) {
  if (fioVivo) return;
  fioPronto = 0;
  gerFio = geracao;
  perfilFio = perfil;
  snprintf(userCodeFio, sizeof userCodeFio, "%s", userCode);
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
  // Pedido feito com um fio do perfil anterior ainda no ar: sai agora.
  if (estado == SMK_PEDINDO) { soltar(fioPedir); return; }

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
