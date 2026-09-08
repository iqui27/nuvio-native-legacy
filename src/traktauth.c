#include "traktauth.h"
#include "idioma.h"
#include "nuvem.h"
#include "dados.h"
#include "rede.h"
#include "trakt.h"
#include "sync.h"
#include "sessao.h"
#include "descoberta.h"
#include "js.h"
#include "jsw.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

#define TRA_ARQ  "trakt.txt"
#define TRA_FLUXO "trakt-fluxo.txt"
#define TRA_BASE "https://api.trakt.tv"
// Quando o Trakt nao manda `interval`, 5s e o que a documentacao dele sugere.
#define TRA_POLL_PADRAO 5000u

static TraEstado estado = TRA_PARADO;
static char deviceCode[128];
static char userCode[32];
static char url[160];
static char erro[200];
static char token[300], refresh[300];
static unsigned pollMs = TRA_POLL_PADRAO;
static unsigned proximoPoll, comecouMs, limiteMs;
// Prazo em RELOGIO DE PAREDE, nao em ticks: o pedido tem de sobreviver a um
// reinicio do app, e SDL_GetTicks zera junto com o processo.
static long expiraEm;

static pthread_t fio;
static int fioVivo, fioPronto;
// 1 quando o fio acabou de conseguir o token e o laco principal ainda nao o
// aplicou. Aplicar dentro do fio mexeria em trakt.c enquanto a UI le dele.
static int tokenNovo;
static long criadoEm, expiraSeg;   // do token: o web exige os dois na conta
static int  pushPendente;          // a conta ainda nao tem este token (push falhou ou nunca saiu)

static char *postar(const char *caminho, const char *corpo, int *status) {
  char completo[300];
  const char *cab[2];
  snprintf(completo, sizeof completo, "%s%s", TRA_BASE, caminho);
  // O Trakt exige o cabecalho de versao da API; sem ele responde 412.
  cab[0] = "trakt-api-version: 2";
  cab[1] = NULL;
  return rede_postar_st(completo, 20, cab, corpo, status);
}

// ---------------------------------------------------------------- disco

// O PEDIDO PENDENTE vai para o disco. Sem isto o codigo do dispositivo vivia so
// na memoria: bastava o app reiniciar — ou o proprio deploy — para a
// autorizacao feita no celular nao ter mais ninguem perguntando por ela. Foi
// exatamente o que aconteceu: o dono autorizou e o app "nao atualizou", porque
// a instancia que tinha pedido o codigo ja nao existia. O app web guarda o
// mesmo estado (TraktAuthStore.saveDeviceFlow).
static void gravarFluxo(void) {
  char buf[600];
  snprintf(buf, sizeof buf, "%s\t%s\t%s\t%ld\n", deviceCode, userCode, url, expiraEm);
  dados_gravar(TRA_FLUXO, buf);
}

static void esquecerFluxo(void) {
  deviceCode[0] = userCode[0] = 0;
  expiraEm = 0;
  dados_apagar(TRA_FLUXO);
}

static void gravar(void) {
  char buf[400];
  // Mesmo formato do art/trakt.txt de antes ("token<TAB>clientId"), para o
  // arquivo continuar legivel por quem ja conhecia o de la. A diferenca e o
  // LUGAR: aqui e a pasta da instalacao, nao o pacote.
  // Colunas 3-6 (refresh, created_at, expires_in, pendente) sao novas: sem o
  // refresh guardado o token nao pode ser reenviado a conta depois de um
  // reinicio, e o push falhado ficava perdido para sempre.
  snprintf(buf, sizeof buf, "%s\t%s\t%s\t%ld\t%ld\t%d\n", token, nuvem_trakt_cliente(),
           refresh, criadoEm, expiraSeg, pushPendente);
  dados_gravar(TRA_ARQ, buf);
}

int traktauth_carregar(void) {
  char *b = dados_ler(TRA_ARQ);
  char *col[6] = { NULL, NULL, NULL, NULL, NULL, NULL };
  if (!b) return 0;
  { char *fim = b + strlen(b);
    while (fim > b && (fim[-1] == '\n' || fim[-1] == '\r')) *--fim = 0; }
  { int i; col[0] = b;
    for (i = 1; i < 6 && col[i - 1]; i++) { col[i] = strchr(col[i - 1], '\t'); if (col[i]) *col[i]++ = 0; } }
  if (col[2]) snprintf(refresh, sizeof refresh, "%s", col[2]);
  criadoEm  = col[3] ? atol(col[3]) : 0;
  expiraSeg = col[4] ? atol(col[4]) : 0;
  // Arquivo antigo (2 colunas) nao diz se a conta recebeu: assume que nao e
  // tenta uma vez — o servidor responde, e a resposta fica no log.
  pushPendente = col[5] ? atoi(col[5]) : 1;
  if (b[0]) {
    snprintf(token, sizeof token, "%s", b);
    trakt_definir(token, nuvem_trakt_cliente());
    estado = TRA_LIGADO;
  }
  free(b);
  if (token[0]) return 1;

  // Sem token, mas pode haver um pedido em andamento de antes do reinicio.
  { char *f = dados_ler(TRA_FLUXO);
    if (f) {
      char *c[4] = { f, NULL, NULL, NULL };
      int i;
      for (i = 1; i < 4 && c[i - 1]; i++) {
        c[i] = strchr(c[i - 1], '\t');
        if (c[i]) *c[i]++ = 0;
      }
      { char *fim = f + strlen(f);
        while (fim > f && (fim[-1] == '\n' || fim[-1] == '\r')) *--fim = 0; }
      if (c[3]) {
        long agora = (long)time(NULL);
        long ate = atol(c[3]);
        if (ate > agora + 5) {
          snprintf(deviceCode, sizeof deviceCode, "%s", c[0]);
          snprintf(userCode, sizeof userCode, "%s", c[1]);
          snprintf(url, sizeof url, "%s", c[2]);
          expiraEm = ate;
          estado = TRA_AGUARDANDO;
          printf("[trakt] retomando o pedido pendente (%lds restantes)\n", ate - agora);
        } else {
          dados_apagar(TRA_FLUXO);
        }
      }
      free(f);
    } }
  return 0;
}

void traktauth_esquecer(void) {
  token[0] = refresh[0] = url[0] = erro[0] = 0;
  estado = TRA_PARADO;
  dados_apagar(TRA_ARQ);
  esquecerFluxo();
}

// ---------------------------------------------------------------- fluxo

static void *fioPedir(void *u) {
  Jsw w;
  char *r;
  int st = 0;
  (void)u;
  erro[0] = userCode[0] = deviceCode[0] = 0;

  if (!nuvem_trakt_cliente()[0] || !nuvem_trakt_segredo()[0]) {
    // Caso de COMPILACAO, nao do usuario: o pacote saiu sem as chaves do
    // aplicativo. Dizer isso evita a pessoa tentar de novo para sempre.
    snprintf(erro, sizeof erro, "pacote sem as chaves do Trakt");
    estado = TRA_ERRO;
    fioPronto = 1;
    return NULL;
  }

  jsw_iniciar(&w);
  jsw_obj_ini(&w);
  jsw_cs(&w, "client_id", nuvem_trakt_cliente());
  jsw_obj_fim(&w);
  r = postar("/oauth/device/code", jsw_texto_final(&w), &st);
  jsw_livre(&w);

  if (r && st >= 200 && st < 300) {
    const char *fim = r + strlen(r);
    double intervalo, expira;
    js_texto(r, fim, "device_code", deviceCode, sizeof deviceCode);
    js_texto(r, fim, "user_code", userCode, sizeof userCode);
    js_texto(r, fim, "verification_url", url, sizeof url);
    intervalo = js_num(r, fim, "interval", 0);
    expira = js_num(r, fim, "expires_in", 0);
    if (intervalo >= 1.0 && intervalo <= 60.0) pollMs = (unsigned)(intervalo * 1000.0);
    limiteMs = (expira > 30.0 && expira < 3600.0) ? (unsigned)(expira * 1000.0) : 600000u;
  }
  if (!deviceCode[0] || !userCode[0]) {
    if (st == 429) snprintf(erro, sizeof erro, "o Trakt pediu para esperar; tente daqui a pouco");
    else snprintf(erro, sizeof erro, i18n("nao consegui pedir o codigo ao Trakt (HTTP %d)"), st);
    estado = TRA_ERRO;
  } else {
    if (!url[0]) snprintf(url, sizeof url, "https://trakt.tv/activate");
    expiraEm = (long)time(NULL) + (long)(limiteMs / 1000u);
    gravarFluxo();
    estado = TRA_AGUARDANDO;
  }
  free(r);
  fioPronto = 1;
  return NULL;
}

static void *fioPoll(void *u) {
  Jsw w;
  char *r;
  int st = 0;
  (void)u;

  jsw_iniciar(&w);
  jsw_obj_ini(&w);
  jsw_cs(&w, "code", deviceCode);
  jsw_cs(&w, "client_id", nuvem_trakt_cliente());
  jsw_cs(&w, "client_secret", nuvem_trakt_segredo());
  jsw_obj_fim(&w);
  r = postar("/oauth/device/token", jsw_texto_final(&w), &st);
  jsw_livre(&w);

  if (r && st >= 200 && st < 300) {
    char t[300];
    if (js_texto(r, r + strlen(r), "access_token", t, sizeof t)) {
      snprintf(token, sizeof token, "%s", t);
      // O refresh vai junto para a conta: sem ele, o vinculo morre no dia em
      // que o access token vencer e o app web nao teria como renovar.
      if (!js_texto(r, r + strlen(r), "refresh_token", refresh, sizeof refresh))
        refresh[0] = 0;
      // O web guarda created_at e expires_in na conta e e por eles que decide
      // renovar. Sem os dois o servidor recusa a linha (HTTP 400) e o vinculo
      // fica so nesta TV.
      criadoEm  = (long)js_num(r, r + strlen(r), "created_at", (double)time(NULL));
      expiraSeg = (long)js_num(r, r + strlen(r), "expires_in", 86400);
      tokenNovo = 1;
      esquecerFluxo();
      estado = TRA_LIGADO;
    } else {
      snprintf(erro, sizeof erro, "o Trakt respondeu sem token");
      estado = TRA_ERRO;
    }
  } else if (st == 400) {
    /* ainda nao autorizado: seguir perguntando */
  } else if (st == 429) {
    // O Trakt mandou ir mais devagar. Subir o intervalo, com teto.
    pollMs += 5000u;
    if (pollMs > 60000u) pollMs = 60000u;
  } else if (st == 409) {
    snprintf(erro, sizeof erro, "este codigo ja foi usado");
    esquecerFluxo();
    estado = TRA_ERRO;
  } else if (st == 410) {
    snprintf(erro, sizeof erro, "o codigo expirou");
    esquecerFluxo();
    estado = TRA_ERRO;
  } else if (st == 418) {
    snprintf(erro, sizeof erro, "autorizacao negada no Trakt");
    esquecerFluxo();
    estado = TRA_ERRO;
  } else if (st) {
    snprintf(erro, sizeof erro, i18n("falha ao trocar o codigo (HTTP %d)"), st);
    estado = TRA_ERRO;
  }
  free(r);
  fioPronto = 1;
  return NULL;
}

// Manda o token para a CONTA, para os outros aparelhos da pessoa herdarem o
// vinculo. A forma do credential_json e a do app web (credentialJsonFromState):
// access_token, refresh_token, token_type, created_at, expires_in.
static void empurrarParaConta(void) {
  Jsw c;
  if (!token[0] || !sessao_logada()) return;
  jsw_iniciar(&c);
  jsw_obj_ini(&c);
  jsw_cs(&c, "access_token", token);
  if (refresh[0]) jsw_cs(&c, "refresh_token", refresh);
  jsw_cs(&c, "token_type", "bearer");
  jsw_ci(&c, "created_at", (int)(criadoEm ? criadoEm : (long)time(NULL)));
  jsw_ci(&c, "expires_in", (int)(expiraSeg > 0 ? expiraSeg : 86400));
  jsw_obj_fim(&c);
  // Recusa 4xx tambem encerra a pendencia: este servidor nao aceita "trakt"
  // (400 22023, igual ao que o web ve) e insistir a cada ciclo seria ruido.
  // O vinculo continua valendo nesta TV, guardado em disco.
  if (sync_empurrar_credencial("trakt", jsw_texto_final(&c)) != 0) { pushPendente = 0; gravar(); }
  jsw_livre(&c);
}

static void soltar(void *(*rotina)(void *)) {
  if (fioVivo) return;
  fioPronto = 0;
  if (pthread_create(&fio, NULL, rotina, NULL) == 0) { pthread_detach(fio); fioVivo = 1; }
  else { snprintf(erro, sizeof erro, "sem fio para falar com o Trakt"); estado = TRA_ERRO; }
}

void traktauth_comecar(void) {
  if (estado == TRA_PEDINDO || estado == TRA_AGUARDANDO) return;
  erro[0] = 0;
  comecouMs = 0;
  pollMs = TRA_POLL_PADRAO;
  estado = TRA_PEDINDO;
  soltar(fioPedir);
}

void traktauth_passo(unsigned agoraMs) {
  if (fioVivo && fioPronto) { fioVivo = 0; fioPronto = 0; }
  if (fioVivo) return;
  // Token que a conta ainda nao tem (push falhou, ou veio de um arquivo antigo):
  // uma tentativa por ciclo de sync concluido, nunca em laco.
  { static unsigned ultimaTentativa;
    if (pushPendente && token[0] && estado == TRA_LIGADO && sync_estado() == SYNC_PRONTO
        && agoraMs - ultimaTentativa > 60000u) { ultimaTentativa = agoraMs; empurrarParaConta(); } }

  // Aplicar o token no LACO PRINCIPAL, nunca no fio: trakt.c e lido pela UI.
  if (tokenNovo) {
    tokenNovo = 0;
    trakt_definir(token, nuvem_trakt_cliente());
    pushPendente = 1;
    empurrarParaConta();
    gravar();
    // O catalogo foi montado SEM Trakt: continuar assistindo, "entre amigos" e
    // as listas dele nao existem nas fileiras que estao na tela. Sem esta
    // remontagem, vincular so tinha efeito visivel no proximo arranque.
    desc_repetir();
    printf("[trakt] vinculado nesta TV\n");
    fflush(stdout);
  }

  if (estado != TRA_AGUARDANDO) return;
  if (!comecouMs) comecouMs = agoraMs;
  if (expiraEm && (long)time(NULL) >= expiraEm) {
    snprintf(erro, sizeof erro, "o codigo expirou");
    esquecerFluxo();
    estado = TRA_ERRO;
    return;
  }
  if (agoraMs >= proximoPoll) {
    proximoPoll = agoraMs + pollMs;
    soltar(fioPoll);
  }
}

void traktauth_cancelar(void) {
  if (estado == TRA_PEDINDO || estado == TRA_AGUARDANDO || estado == TRA_ERRO)
    estado = token[0] ? TRA_LIGADO : TRA_PARADO;
}

TraEstado   traktauth_estado(void) { return estado; }
const char *traktauth_codigo(void) { return userCode; }
const char *traktauth_url(void)    { return url; }
const char *traktauth_erro(void)   { return erro; }
