#include "simklauth.h"
#include "perfisessao.h"
#include "idioma.h"
#include "nuvem.h"
#include "dados.h"
#include "rede.h"
#include "sync.h"
#include "sessao.h"
#include "js.h"
#include "jsw.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "credfio.h"

// O VINCULO E POR PERFIL (simkl-p<N>.txt), pelo mesmo motivo e com a mesma
// migracao do Trakt (ver o topo de traktauth.c): o simkl.txt antigo e do
// perfil 1 — antes dos perfis o app so sincronizava o 1 — e e RENOMEADO para
// simkl-p1.txt, nunca copiado para outro perfil.
#define SMK_ARQ_LEGADO "simkl.txt"
#define SMK_ARQ_FMT    "simkl-p%d.txt"
#define SMK_PERFIS     16   // o logout varre p0..16, como fontepref/traktauth
#define SMK_BASE "https://api.simkl.com"
// O pedido de PIN devolve `interval` (5 s na pratica); sem ele, 5 s, o passo
// do app web. Nunca consultar mais rapido que isso: o Simkl responde "Slow down".
#define SMK_POLL_MS 5000u
#define SMK_LIMITE_PADRAO_MS 900000u
// Falhas de rede/HTTP seguidas toleradas antes de desistir. Uma queda de
// Wi-Fi ou um 5xx/429 passageiro NAO e o codigo recusado.
#define SMK_FALHAS_MAX 8

static SmkEstado estado = SMK_PARADO;
static char userCode[48];
static char url[200];
static char erro[200];
static char token[300];
static unsigned long long credGeracao = 1;
static unsigned proximoPoll, comecouMs, limiteMs = SMK_LIMITE_PADRAO_MS, passoMs = SMK_POLL_MS;
static unsigned ultimoAgora;
static int falhasSeguidas;

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

// O perfil pode mudar enquanto o PIN responde; a conta Nuvio nao pode herdar
// a autorizacao de quem saiu. Captura local, sem renovacao/rede na UI.
extern int sessao_identidade_local_copiar(char *, size_t, char *, size_t) NV_PERFIL_OPCIONAL;
extern const char *sessao_usuario(void) NV_PERFIL_OPCIONAL;
static char usuarioFio[80], usuarioEstado[80];
static void usuarioCopiar(char dst[80]) {
  char jwt[3000];
  dst[0] = 0;
  if (sessao_identidade_local_copiar) {
    sessao_identidade_local_copiar(dst, 80, jwt, sizeof jwt);
    memset(jwt, 0, sizeof jwt);
  } else if (sessao_usuario) snprintf(dst, 80, "%s", sessao_usuario());
}
static int mesmaConta(const char *usuario) {
  char atual[80];
  usuarioCopiar(atual);
  return !strcmp(usuario, atual);
}

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
  usuarioCopiar(usuarioEstado);
  migrarLegado();
  b = dados_ler(arq(perfil).s);
  if (!b) return 0;
  { char *fim = b + strlen(b);
    while (fim > b && (fim[-1] == '\n' || fim[-1] == '\r')) *--fim = 0; }
  pthread_mutex_lock(&trava);
  if (b[0] && strcmp(b, "!")) {
    if (strcmp(token, b)) credGeracao++;
    snprintf(token, sizeof token, "%s", b); estado = SMK_LIGADO;
  }
  int ligado = token[0] != 0;
  pthread_mutex_unlock(&trava);
  free(b);
  return ligado;
}

const char *simklauth_token(void) { return token; }

unsigned long long simklauth_credencial_copiar(char *dst, size_t tam) {
  unsigned long long g;
  pthread_mutex_lock(&trava);
  if (dst && tam) snprintf(dst, tam, "%s", token);
  g = credGeracao;
  pthread_mutex_unlock(&trava);
  return g;
}

static void zerarEstado(void) {
  credGeracao++;
  token[0] = userCode[0] = url[0] = erro[0] = 0;
  tokenNovo = 0;
  comecouMs = 0;
  falhasSeguidas = 0;
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

void simklauth_desconectar(void) {
  pthread_mutex_lock(&trava);
  geracao++;
  zerarEstado();
  dados_gravar(arq(perfil).s, "!");
  pthread_mutex_unlock(&trava);
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

// O Simkl respondeu 401 a um pedido que saiu com a credencial de geracao `cred`
// (simklauth_credencial_copiar). So desfaz o vinculo se ele ainda e ESSE: com a
// troca de perfil ou um novo vinculo no meio do voo, o 401 e de um token que ja
// nao esta aqui e o do perfil atual fica. Devolve 1 quando desfez. (O esquecer
// acima e o logout, varre todos.)
int simklauth_invalidar_se(unsigned long long cred) {
  int atual;
  pthread_mutex_lock(&trava);
  atual = cred == credGeracao;
  if (atual) {
    geracao++;
    zerarEstado();
    dados_apagar(arq(perfil).s);
  }
  pthread_mutex_unlock(&trava);
  return atual;
}

// ---------------------------------------------------------------- fluxo

static void *fioPedir(void *u) {
  char *r;
  int st = 0;
  char uc[48] = "", vu[200] = "";
  unsigned novoLimite = 0, novoPasso = 0;
  (void)u;

  if (!nuvem_simkl_cliente()[0]) {
    pthread_mutex_lock(&trava);
    if (gerFio == geracao && mesmaConta(usuarioFio)) {
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
    expira = js_num(r, fim, "interval", 0);   // reaproveita a variavel
    if (expira >= 1.0 && expira <= 60.0) novoPasso = (unsigned)(expira * 1000.0);
  }
  pthread_mutex_lock(&trava);
  // PIN pedido por um perfil que ja saiu da tela: ninguem vai digita-lo.
  if (gerFio != geracao || !mesmaConta(usuarioFio)) {
    pthread_mutex_unlock(&trava);
    free(r);
    fioPronto = 1;
    return NULL;
  }
  erro[0] = 0;
  snprintf(userCode, sizeof userCode, "%s", uc);
  snprintf(url, sizeof url, "%s", vu);
  // Cada codigo novo traz o relogio dele; sem o campo, o padrao — nunca o
  // limite do codigo anterior.
  limiteMs = novoLimite ? novoLimite : SMK_LIMITE_PADRAO_MS;
  passoMs = novoPasso ? novoPasso : SMK_POLL_MS;
  falhasSeguidas = 0;
  comecouMs = 0;
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

// O QUE UMA RESPOSTA DO POLL SIGNIFICA. Antes, qualquer 2xx que nao fosse
// exatamente {"result":"KO"} e sem token virava "o Simkl invalidou este codigo"
// na hora, e qualquer HTTP fora de 2xx tambem — corpo vazio, pagina do
// Cloudflare, 429 do proprio limite do Simkl, um 5xx: o codigo era dado como
// recusado segundos depois de aparecer. So e fim de verdade: expirar o prazo
// (simklauth_passo) ou o Simkl dizer que negou/expirou/nao existe.
SmkPoll simklauth_classificar(int st, const char *corpo, char *tk, unsigned tam) {
  char res[16] = "", msg[120] = "";
  const char *fim;
  if (tk && tam) tk[0] = 0;
  if (st == 429) return SMK_POLL_LENTO;
  if (!corpo || st < 200 || st >= 300) return SMK_POLL_FALHA;
  fim = corpo + strlen(corpo);
  if (tk && tam && js_texto(corpo, fim, "access_token", tk, tam) && tk[0]) return SMK_POLL_OK;
  js_texto(corpo, fim, "result", res, sizeof res);
  js_texto(corpo, fim, "message", msg, sizeof msg);
  if (!strcmp(res, "KO") || !strcmp(res, "ko")) {
    char m[120];
    unsigned i;
    for (i = 0; i < sizeof m - 1 && msg[i]; i++)
      m[i] = (char)((msg[i] >= 'A' && msg[i] <= 'Z') ? msg[i] + 32 : msg[i]);
    m[i] = 0;
    if (strstr(m, "slow")) return SMK_POLL_LENTO;
    // "Authorization pending" e o caso comum: continuar. So palavras de fim
    // de verdade encerram.
    if (strstr(m, "expire") || strstr(m, "denied") || strstr(m, "reject") ||
        strstr(m, "invalid") || strstr(m, "not found") || strstr(m, "revoked"))
      return SMK_POLL_NEGADO;
    return SMK_POLL_ESPERA;
  }
  // Nem KO nem token (corpo vazio, HTML, JSON estranho): resposta ruim, nao veredito.
  return SMK_POLL_FALHA;
}

static void *fioPoll(void *u) {
  char caminho[120], *r;
  int st = 0;
  (void)u;
  snprintf(caminho, sizeof caminho, "/oauth/pin/%s", userCodeFio);
  r = pegar(caminho, &st);
  pthread_mutex_lock(&trava);
  if (!mesmaConta(usuarioFio)) {
    pthread_mutex_unlock(&trava);
    free(r);
    fioPronto = 1;
    return NULL;
  }
  char *salvo = dados_ler(arq(perfilFio).s);
  int saiu = salvo && !strcmp(salvo, "!");
  free(salvo);
  if (saiu && gerFio != geracao) {
    pthread_mutex_unlock(&trava); free(r); fioPronto = 1; return NULL;
  }
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
  { char t[300];
    switch (simklauth_classificar(st, r, t, sizeof t)) {
      case SMK_POLL_OK:
        if (strcmp(token, t)) credGeracao++;
        snprintf(token, sizeof token, "%s", t);
        tokenNovo = 1;
        estado = SMK_LIGADO;
        falhasSeguidas = 0;
        break;
      case SMK_POLL_ESPERA:
        falhasSeguidas = 0;
        break;
      case SMK_POLL_LENTO:
        falhasSeguidas = 0;
        passoMs += 5000u;   // o Simkl pediu calma: afrouxa o passo
        if (passoMs > 30000u) passoMs = 30000u;
        break;
      case SMK_POLL_NEGADO:
        snprintf(erro, sizeof erro, "o Simkl invalidou este código");
        estado = SMK_ERRO;
        break;
      default:   // SMK_POLL_FALHA: rede, 5xx, corpo ilegivel
        printf("[simkl] poll sem resposta util (HTTP %d, %d seguidas); sigo esperando\n",
               st, falhasSeguidas + 1);
        fflush(stdout);
        // Rede morta, timeout, 5xx e corpo cortado (Wi-Fi lento da Shield) so
        // acabam pelo prazo do codigo. Desistir antes so cabe a um 4xx que se
        // repete (chave recusada), nao a falha de transporte.
        falhasSeguidas++;
        if (st >= 400 && st < 500 && st != 408 && falhasSeguidas >= SMK_FALHAS_MAX) {
          snprintf(erro, sizeof erro, i18n("falha ao consultar o Simkl (HTTP %d)"), st);
          estado = SMK_ERRO;
        }
        break;
    }
  }
  pthread_mutex_unlock(&trava);
  free(r);
  fioPronto = 1;
  return NULL;
}

static void soltar(void *(*rotina)(void *)) {
  if (fioVivo) return;
  usuarioCopiar(usuarioFio);
  if (strcmp(usuarioFio, usuarioEstado)) return;
  fioPronto = 0;
  gerFio = geracao;
  perfilFio = perfil;
  snprintf(userCodeFio, sizeof userCodeFio, "%s", userCode);
  if (pthread_create(&fio, NULL, rotina, NULL) == 0) { pthread_detach(fio); fioVivo = 1; }
  else { snprintf(erro, sizeof erro, "sem fio para falar com o Simkl"); estado = SMK_ERRO; }
}

void simklauth_comecar(void) {
  if (estado == SMK_PEDINDO || estado == SMK_AGUARDANDO) return;
  usuarioCopiar(usuarioEstado);
  erro[0] = 0;
  comecouMs = 0;
  estado = SMK_PEDINDO;
  soltar(fioPedir);
}

void simklauth_passo(unsigned agoraMs) {
  // tokenNovo pode ter ficado pendente enquanto outro push ocupava o fio.
  if (!mesmaConta(usuarioEstado)) {
    if (fioVivo && fioPronto) { fioVivo = 0; fioPronto = 0; }
    return;
  }
  { int res; credfio_resultado("simkl", &res); }   // so libera o lugar (#203)
  if (fioVivo && fioPronto) { fioVivo = 0; fioPronto = 0; }
  if (fioVivo) return;
  // Pedido feito com um fio do perfil anterior ainda no ar: sai agora.
  if (estado == SMK_PEDINDO) { soltar(fioPedir); return; }

  if (tokenNovo) {
    Jsw c;
    int saiu;
    gravar();
    jsw_iniciar(&c);
    jsw_obj_ini(&c);
    jsw_cs(&c, "access_token", token);
    jsw_obj_fim(&c);
    // Fio proprio (#203). Se ja ha um no ar, tokenNovo fica e tenta no proximo quadro.
    saiu = credfio_iniciar("simkl", jsw_texto_final(&c));
    jsw_livre(&c);
    if (saiu) {
      tokenNovo = 0;
      printf("[simkl] vinculado nesta TV\n");
      fflush(stdout);
    }
  }

  if (estado != SMK_AGUARDANDO) return;
  ultimoAgora = agoraMs;
  if (!comecouMs) { comecouMs = agoraMs ? agoraMs : 1; proximoPoll = 0; }
  if (agoraMs - comecouMs > limiteMs) {
    snprintf(erro, sizeof erro, "o código expirou");
    estado = SMK_ERRO;
    return;
  }
  if (agoraMs >= proximoPoll) {
    proximoPoll = agoraMs + passoMs;
    soltar(fioPoll);
  }
}

void simklauth_cancelar(void) {
  if (estado == SMK_PEDINDO || estado == SMK_AGUARDANDO || estado == SMK_ERRO)
    estado = token[0] ? SMK_LIGADO : SMK_PARADO;
}

SmkEstado   simklauth_estado(void) { return estado; }
// Segundos que faltam para o codigo expirar (o mesmo relogio do passo), ou -1
// quando nao ha codigo esperando.
int simklauth_restante_s(void) {
  unsigned gasto;
  if (estado != SMK_AGUARDANDO || !comecouMs) return -1;
  gasto = ultimoAgora - comecouMs;
  return gasto >= limiteMs ? 0 : (int)((limiteMs - gasto + 999u) / 1000u);
}
const char *simklauth_codigo(void) { return userCode; }
const char *simklauth_url(void)    { return url; }
const char *simklauth_erro(void)   { return erro; }
