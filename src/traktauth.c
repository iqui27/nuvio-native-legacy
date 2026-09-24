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

// O VINCULO E POR PERFIL: trakt-p<N>.txt e trakt-fluxo-p<N>.txt.
//
// Ate aqui era UM trakt.txt por aparelho, e o perfil 2 de uma conta usava o
// Trakt do perfil 1 — watchlist, "continuar assistindo", historico, tudo.
// Relato do dono na C9 (conta com 2 perfis): "o perfil 2 mostra o mesmo Trakt
// do 1", com "[trakt] vinculo desta TV mantido" no log logo depois da troca. O
// app oficial guarda as credenciais de provedor por perfil
// (sync_pull_provider_credentials leva p_profile_id), e este arquivo passa a
// dizer o mesmo.
//
// MIGRACAO DO trakt.txt ANTIGO: ele e do PERFIL 1, e so dele. Antes dos perfis
// o app sincronizava sempre o perfil 1 (perfis.h), entao quem vinculou o Trakt
// nesta TV vinculou o perfil 1. Ele e RENOMEADO para trakt-p1.txt na primeira
// leitura (ver migrarLegado) e nao e copiado para nenhum outro perfil — copiar
// seria recriar exatamente o vazamento que esta separacao existe para fechar.
// Se o perfil 1 ja tem arquivo proprio, o antigo esta velho e so e apagado. O
// mesmo vale para o pedido pendente (trakt-fluxo.txt).
#define TRA_ARQ_LEGADO   "trakt.txt"
#define TRA_FLUXO_LEGADO "trakt-fluxo.txt"
#define TRA_ARQ_FMT      "trakt-p%d.txt"
#define TRA_FLUXO_FMT    "trakt-fluxo-p%d.txt"
// Quantos perfis o logout varre. Mesmo teto de fontepref/buscasrec (o dobro
// de CONTA_PERFIL_MAX): dados_apagar em arquivo que nao existe custa nada.
#define TRA_PERFIS 16
#define TRA_BASE "https://api.trakt.tv"
// Quando o Trakt nao manda `interval`, 5s e o que a documentacao dele sugere.
#define TRA_POLL_PADRAO 5000u

static TraEstado estado = TRA_PARADO;
static char deviceCode[128];
static char userCode[32];
static char url[160];
static char erro[200];
static char token[300], refresh[300];
// 1 quando o carregar achou o access token vencido no papel mas com refresh
// valido: a renovacao sai no primeiro passo, sem esperar o primeiro 401 e sem
// gastar as chamadas do ciclo de descoberta com uma credencial morta.
static int renovacaoPend;
static unsigned pollMs = TRA_POLL_PADRAO;
static unsigned proximoPoll, comecouMs, limiteMs;
// Prazo em RELOGIO DE PAREDE, nao em ticks: o pedido tem de sobreviver a um
// reinicio do app, e SDL_GetTicks zera junto com o processo.
static long expiraEm;

static pthread_t fio;
static int fioVivo, fioPronto;

// DE QUEM E O ESTADO ACIMA. `perfil` escolhe os arquivos; `geracao` sobe a
// cada troca de perfil. Um fio que saiu antes da troca (poll do codigo,
// renovacao) volta com a resposta do perfil ANTERIOR: publicar nas variaveis
// daqui entregaria o token do 1 ao 2 — o mesmo vazamento, so que por corrida.
// O fio leva a geracao e o perfil de quando saiu, compara sob a trava e, se
// mudou, grava o resultado no arquivo do perfil dele sem tocar no estado vivo.
// Gravar e nao descartar porque a renovacao ROTACIONA o refresh: jogar fora a
// resposta mataria o vinculo do perfil anterior.
static int perfil = 1;
static unsigned geracao;
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
// Copiados no fio principal por soltar(), antes do pthread_create: o fio nao
// le `refresh`/`deviceCode` vivos, que a troca de perfil zera.
static unsigned gerFio;
static int perfilFio;
static char refreshFio[300], deviceCodeFio[128];
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
// O NOME VOLTA POR VALOR, e nao num buffer estatico: os fios de poll e de
// renovacao montam o nome do arquivo do perfil DELES enquanto o laco principal
// monta o do perfil novo. Um buffer compartilhado podia trocar um pelo outro
// no meio — gravar o token do 1 em trakt-p2.txt, que e o defeito inteiro.
typedef struct { char s[40]; } TraNome;
static TraNome arqToken(int p) {
  TraNome n;
  snprintf(n.s, sizeof n.s, TRA_ARQ_FMT, p);
  return n;
}
static TraNome arqFluxo(int p) {
  TraNome n;
  snprintf(n.s, sizeof n.s, TRA_FLUXO_FMT, p);
  return n;
}

static void gravarFluxo(void) {
  char buf[600];
  snprintf(buf, sizeof buf, "%s\t%s\t%s\t%ld\n", deviceCode, userCode, url, expiraEm);
  dados_gravar(arqFluxo(perfil).s, buf);
}

static void esquecerFluxo(void) {
  deviceCode[0] = userCode[0] = 0;
  expiraEm = 0;
  dados_apagar(arqFluxo(perfil).s);
}

// Um arquivo antigo vira o do perfil 1 (ver a nota no topo). Idempotente: sem
// o arquivo antigo nao faz nada, e roda a cada carregar.
static void migrarUm(const char *legado, const char *novo) {
  char *b = dados_ler(legado), *ja;
  char alvo[40];
  if (!b) return;
  snprintf(alvo, sizeof alvo, "%s", novo);   // `novo` e temporario de arq*()
  ja = dados_ler(alvo);
  if (ja) {
    free(ja);
    dados_apagar(legado);
    printf("[trakt] %s antigo descartado: o perfil 1 ja tem %s\n", legado, alvo);
  } else if (dados_gravar(alvo, b)) {
    dados_apagar(legado);
    printf("[trakt] %s migrado para %s (so o perfil 1)\n", legado, alvo);
  }
  fflush(stdout);
  free(b);
}
static void migrarLegado(void) {
  migrarUm(TRA_ARQ_LEGADO, arqToken(1).s);
  migrarUm(TRA_FLUXO_LEGADO, arqFluxo(1).s);
}

static void gravarLinha(int p, const char *tk, const char *rf, long criado,
                        long expira, int pendente) {
  char buf[800];
  // Mesmo formato do art/trakt.txt de antes ("token<TAB>clientId"), para o
  // arquivo continuar legivel por quem ja conhecia o de la. A diferenca e o
  // LUGAR: aqui e a pasta da instalacao, nao o pacote.
  // Colunas 3-6 (refresh, created_at, expires_in, pendente) sao novas: sem o
  // refresh guardado o token nao pode ser reenviado a conta depois de um
  // reinicio, e o push falhado ficava perdido para sempre.
  snprintf(buf, sizeof buf, "%s\t%s\t%s\t%ld\t%ld\t%d\n", tk, nuvem_trakt_cliente(),
           rf, criado, expira, pendente);
  dados_gravar(arqToken(p).s, buf);
}

static void gravar(void) {
  gravarLinha(perfil, token, refresh, criadoEm, expiraSeg, pushPendente);
}

// Zera o que e do perfil em memoria. NAO apaga arquivo nem mexe em trakt.c.
static void zerarEstado(void) {
  token[0] = refresh[0] = url[0] = erro[0] = 0;
  deviceCode[0] = userCode[0] = 0;
  expiraEm = 0;
  criadoEm = expiraSeg = 0;
  pushPendente = 0;
  renovacaoPend = 0;
  tokenNovo = 0;
  comecouMs = 0;
  pollMs = TRA_POLL_PADRAO;
  estado = TRA_PARADO;
}

int traktauth_carregar(void) {
  char *b;
  char *col[6] = { NULL, NULL, NULL, NULL, NULL, NULL };
  migrarLegado();
  b = dados_ler(arqToken(perfil).s);
  if (!b) goto fluxo;
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
    // Vencido no papel: criadoEm+expiraSeg ja passaram. Definir este token faria
    // cada chamada do primeiro ciclo de descoberta sair para a rede e voltar
    // 401 — segundos por chamada, no pool que o resto da home tambem usa. Com
    // refresh na mao a renovacao resolve antes; o passo aplica o token novo e
    // manda remontar as fileiras.
    if (refresh[0] && criadoEm > 0 && expiraSeg > 0 &&
        (long)time(NULL) >= criadoEm + expiraSeg - 300) {
      renovacaoPend = 1;
      printf("[trakt] token vencido no arquivo — renovando antes do 401\n");
    } else {
      trakt_definir(token, nuvem_trakt_cliente());
    }
    estado = TRA_LIGADO;
  }
  free(b);
  if (token[0]) return 1;

fluxo:
  // Sem token, mas pode haver um pedido em andamento de antes do reinicio.
  { char *f = dados_ler(arqFluxo(perfil).s);
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
          dados_apagar(arqFluxo(perfil).s);
        }
      }
      free(f);
    } }
  return 0;
}

int traktauth_carregar_perfil(int p) {
  pthread_mutex_lock(&trava);
  perfil = p > 0 ? p : 1;
  geracao++;
  zerarEstado();
  pthread_mutex_unlock(&trava);
  return traktauth_carregar();
}

int traktauth_trocar_perfil(int p) {
  int antes;
  if (p <= 0) p = 1;
  if (p == perfil) return 0;
  antes = trakt_ativo() || token[0];
  // A credencial EM USO sai inteira, venha ela deste arquivo ou da conta
  // (sync.c, temTraktRem): trakt_esquecer zera o token de trakt.c e as tabelas
  // da ultima leitura. O que o perfil novo tiver chega por traktauth_carregar
  // (vinculo local) ou pelo proximo ciclo de sync (credencial da conta dele).
  trakt_esquecer();
  traktauth_carregar_perfil(p);
  printf("[trakt] perfil %d: %s\n", perfil,
         token[0] ? "vinculo deste perfil carregado" : "sem vinculo neste perfil");
  fflush(stdout);
  return antes || trakt_ativo() || token[0];
}

int traktauth_perfil(void) { return perfil; }

void traktauth_esquecer(void) {
  int p;
  pthread_mutex_lock(&trava);
  geracao++;
  zerarEstado();
  // LOGOUT: o vinculo de TODOS os perfis sai, e o arquivo antigo tambem. O
  // proximo a entrar comeca no perfil 1 da conta dele (perfis_esquecer), e um
  // trakt-p1.txt que sobrasse seria o Trakt de quem saiu.
  for (p = 0; p <= TRA_PERFIS; p++) {
    dados_apagar(arqToken(p).s);
    dados_apagar(arqFluxo(p).s);
  }
  dados_apagar(TRA_ARQ_LEGADO);
  dados_apagar(TRA_FLUXO_LEGADO);
  perfil = 1;
  pthread_mutex_unlock(&trava);
}

// 1 quando o fio ainda fala pelo perfil em vigor. Chamar com a trava.
static int fioAtual(void) { return gerFio == geracao; }

// ---------------------------------------------------------------- fluxo

static void *fioPedir(void *u) {
  Jsw w;
  char *r;
  int st = 0;
  char dc[128] = "", uc[32] = "", vu[160] = "";
  unsigned novoPoll = 0, novoLimite = 600000u;
  (void)u;

  if (!nuvem_trakt_cliente()[0] || !nuvem_trakt_segredo()[0]) {
    // Caso de COMPILACAO, nao do usuario: o pacote saiu sem as chaves do
    // aplicativo. Dizer isso evita a pessoa tentar de novo para sempre.
    pthread_mutex_lock(&trava);
    if (fioAtual()) {
      snprintf(erro, sizeof erro, "pacote sem as chaves do Trakt");
      estado = TRA_ERRO;
    }
    pthread_mutex_unlock(&trava);
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
    js_texto(r, fim, "device_code", dc, sizeof dc);
    js_texto(r, fim, "user_code", uc, sizeof uc);
    js_texto(r, fim, "verification_url", vu, sizeof vu);
    intervalo = js_num(r, fim, "interval", 0);
    expira = js_num(r, fim, "expires_in", 0);
    if (intervalo >= 1.0 && intervalo <= 60.0) novoPoll = (unsigned)(intervalo * 1000.0);
    if (expira > 30.0 && expira < 3600.0) novoLimite = (unsigned)(expira * 1000.0);
  }
  pthread_mutex_lock(&trava);
  // Pedido de um perfil que ja nao esta na tela: o codigo nao e mostrado a
  // ninguem, entao nao ha o que guardar.
  if (!fioAtual()) {
    pthread_mutex_unlock(&trava);
    free(r);
    fioPronto = 1;
    return NULL;
  }
  erro[0] = 0;
  snprintf(deviceCode, sizeof deviceCode, "%s", dc);
  snprintf(userCode, sizeof userCode, "%s", uc);
  snprintf(url, sizeof url, "%s", vu);
  if (novoPoll) pollMs = novoPoll;
  limiteMs = novoLimite;
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
  pthread_mutex_unlock(&trava);
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
  jsw_cs(&w, "code", deviceCodeFio);
  jsw_cs(&w, "client_id", nuvem_trakt_cliente());
  jsw_cs(&w, "client_secret", nuvem_trakt_segredo());
  jsw_obj_fim(&w);
  r = postar("/oauth/device/token", jsw_texto_final(&w), &st);
  jsw_livre(&w);

  pthread_mutex_lock(&trava);
  if (!fioAtual()) {
    // O perfil mudou com o poll no ar. Autorizado: o vinculo e do perfil que
    // pediu o codigo, e vai para o arquivo DELE — o perfil em vigor nao ve
    // nada. Qualquer outra resposta e so descartada.
    char t[300], rf[300] = "";
    if (r && st >= 200 && st < 300 &&
        js_texto(r, r + strlen(r), "access_token", t, sizeof t)) {
      js_texto(r, r + strlen(r), "refresh_token", rf, sizeof rf);
      gravarLinha(perfilFio, t, rf,
                  (long)js_num(r, r + strlen(r), "created_at", (double)time(NULL)),
                  (long)js_num(r, r + strlen(r), "expires_in", 86400), 1);
      dados_apagar(arqFluxo(perfilFio).s);
      printf("[trakt] autorizacao do perfil %d chegou depois da troca; guardada no arquivo dele\n",
             perfilFio);
    }
    pthread_mutex_unlock(&trava);
    free(r);
    fioPronto = 1;
    return NULL;
  }
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
  pthread_mutex_unlock(&trava);
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

// RENOVAR PELO REFRESH TOKEN. O refresh sempre foi GRAVADO (coluna 3 do
// trakt.txt) mas nunca usado — um access token vencido ficava para sempre,
// e a tela dizia "conectado" com tudo voltando 401. grant_type padrao OAuth;
// o Trakt responde com um refresh NOVO (rotacao), que vai junto para a conta.
static void *fioRenovar(void *u) {
  Jsw w;
  char *r;
  int st = 0;
  (void)u;
  jsw_iniciar(&w);
  jsw_obj_ini(&w);
  jsw_cs(&w, "refresh_token", refreshFio);
  jsw_cs(&w, "client_id", nuvem_trakt_cliente());
  jsw_cs(&w, "client_secret", nuvem_trakt_segredo());
  jsw_cs(&w, "redirect_uri", "urn:ietf:wg:oauth:2.0:oob");
  jsw_cs(&w, "grant_type", "refresh_token");
  jsw_obj_fim(&w);
  r = postar("/oauth/token", jsw_texto_final(&w), &st);
  jsw_livre(&w);

  pthread_mutex_lock(&trava);
  if (!fioAtual()) {
    // Renovacao do perfil ANTERIOR. O refresh velho morreu nesta resposta, entao
    // descartar mataria o vinculo daquele perfil: o par novo vai para o arquivo
    // dele, com pendencia de envio a conta (sai quando ele voltar a ser o ativo).
    char t[300], rf[300] = "";
    if (r && st >= 200 && st < 300 &&
        js_texto(r, r + strlen(r), "access_token", t, sizeof t)) {
      js_texto(r, r + strlen(r), "refresh_token", rf, sizeof rf);
      gravarLinha(perfilFio, t, rf,
                  (long)js_num(r, r + strlen(r), "created_at", (double)time(NULL)),
                  (long)js_num(r, r + strlen(r), "expires_in", 86400), 1);
      printf("[trakt] renovacao do perfil %d chegou depois da troca; guardada no arquivo dele\n",
             perfilFio);
    }
    pthread_mutex_unlock(&trava);
    free(r);
    fioPronto = 1;
    return NULL;
  }
  if (r && st >= 200 && st < 300) {
    char t[300];
    const char *fim = r + strlen(r);
    if (js_texto(r, fim, "access_token", t, sizeof t)) {
      snprintf(token, sizeof token, "%s", t);
      // O refresh ROTACIONA: o velho morre nesta resposta.
      if (!js_texto(r, fim, "refresh_token", refresh, sizeof refresh))
        refresh[0] = 0;
      criadoEm  = (long)js_num(r, fim, "created_at", (double)time(NULL));
      expiraSeg = (long)js_num(r, fim, "expires_in", 86400);
      tokenNovo = 1;
      estado = TRA_LIGADO;
      printf("[trakt] credencial renovada pelo refresh token\n");
    } else {
      snprintf(erro, sizeof erro, "a renovacao veio sem token");
      estado = TRA_INVALIDO;
    }
  } else if (st == 400 || st == 401 || st == 403 || st == 404) {
    // invalid_grant: o refresh tambem morreu — so re-pareando.
    snprintf(erro, sizeof erro, "%s",
             i18n("a sessão do Trakt expirou — conecte de novo"));
    estado = TRA_INVALIDO;
  }
  // Falha de TRANSPORTE (st==0): estado nao muda; traktauth_passo tenta de
  // novo, porque uma TV sem rede por um minuto nao e sessao morta.
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
  snprintf(refreshFio, sizeof refreshFio, "%s", refresh);
  snprintf(deviceCodeFio, sizeof deviceCodeFio, "%s", deviceCode);
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
  // PEDINDO sem fio no ar: o pedido foi feito enquanto um fio do perfil
  // anterior ainda respondia, e soltar() recusou. Sai agora.
  if (estado == TRA_PEDINDO) { soltar(fioPedir); return; }

  // 401 NA SESSAO: a rede marcou a credencial como recusada. Com refresh
  // guardado, renova — uma tentativa por minuto, porque falha de transporte
  // nao derruba o estado. Sem refresh (credencial do pacote ou vinda da conta
  // sem ele), nao ha caminho: o estado cai para INVALIDO e a tela finalmente
  // diz a verdade ("expirou", nao "conectado").
  //
  // A guarda nao e `estado == TRA_LIGADO`: um token carregado do pacote deixa
  // o estado em PARADO com trakt_ativo() ligado, e era justamente esse o caso
  // que mostrava "conectado" com tudo falhando.
  if (((trakt_recusada() && trakt_ativo()) || renovacaoPend) &&
      estado != TRA_PEDINDO && estado != TRA_AGUARDANDO &&
      estado != TRA_INVALIDO) {
    // ultimaRenovacao==0 e "nunca tentei", nao "tentei no instante zero" —
    // sem este caso um 401 no primeiro minuto de app so renovava depois dos
    // 60 s de uptime, que era o "demora para o Trakt aparecer".
    static unsigned ultimaRenovacao;
    if (refresh[0]) {
      if (!ultimaRenovacao || agoraMs - ultimaRenovacao > 60000u) {
        ultimaRenovacao = agoraMs;
        soltar(fioRenovar);
      }
    } else {
      snprintf(erro, sizeof erro, "%s",
               i18n("a sessão do Trakt expirou — conecte de novo"));
      estado = TRA_INVALIDO;
    }
  }

  // Token que a conta ainda nao tem (push falhou, ou veio de um arquivo antigo):
  // uma tentativa por ciclo de sync concluido, nunca em laco.
  { static unsigned ultimaTentativa;
    if (pushPendente && token[0] && estado == TRA_LIGADO && sync_estado() == SYNC_PRONTO
        && agoraMs - ultimaTentativa > 60000u) { ultimaTentativa = agoraMs; empurrarParaConta(); } }

  // Aplicar o token no LACO PRINCIPAL, nunca no fio: trakt.c e lido pela UI.
  if (tokenNovo) {
    tokenNovo = 0;
    renovacaoPend = 0;
    trakt_definir(token, nuvem_trakt_cliente());
    pushPendente = 1;
    empurrarParaConta();
    gravar();
    // O catalogo foi montado SEM Trakt: continuar assistindo, "entre amigos" e
    // as listas dele nao existem nas fileiras que estao na tela. Sem esta
    // remontagem, vincular so tinha efeito visivel no proximo arranque.
    desc_repetir();
    printf("[trakt] vinculado nesta TV (perfil %d)\n", perfil);
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
