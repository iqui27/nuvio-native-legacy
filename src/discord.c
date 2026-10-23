// Ver discord.h.
#include "jfid.h"
#include "discord.h"
#include "discordws.h"
#include "catalogo.h"
#include "dados.h"
#include "js.h"
#include "jsw.h"
#include "idioma.h"
#include "perfis.h"
#include "player.h"
#include "rede.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#ifndef NV_DISCORD_CLIENT_ID
#define NV_DISCORD_CLIENT_ID ""
#endif

#define DIS_API      "https://discord.com/api/v10"
#define DIS_GATEWAY  "wss://gateway.discord.gg/?v=10&encoding=json"
#define DIS_ESCOPO   "openid%20sdk.social_layer_presence"
#define DIS_ARQ_FMT  "discord-p%d.txt"
// Sem nada tocando por este tempo o gateway fecha. Curto o bastante para a
// atividade nao ficar pendurada depois do filme; longo o bastante para trocar
// de fonte ou de episodio sem derrubar e refazer a sessao.
#define DIS_OCIOSO_MS   60000u
// Entre duas presencas. O gateway aceita bem mais, mas cada uma vira evento
// para todos os amigos da pessoa; 5 s cobre pausar/despausar sem rajada.
#define DIS_INTERVALO_MS 5000u

// ------------------------------------------------------------------ estado
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static DisEstado estado;
static int perfil = -1;
static char token[256], refresh[256];
static long expiraEm;                 // epoca em s; 0 = nao se sabe
static char deviceCode[256], userCode[32], url[256], erro[160];
static long codigoExpiraEm;
static unsigned pollMs = 5000, proxPollMs;
static int fioVivo;                   // um fio de HTTP por vez
static int geracao;                   // invalida o fio de um perfil que ja saiu

static long agoraSeg(void) { return (long)time(NULL); }

static int ligadoSemTrava(void) { return estado == DIS_LIGADO && token[0]; }
static int ligado(void) {
  int r;
  pthread_mutex_lock(&trava); r = ligadoSemTrava(); pthread_mutex_unlock(&trava);
  return r;
}

static void restringirArquivo(const char *nome) {
  char caminho[600];
  if (dados_caminho(caminho, sizeof caminho, nome)) chmod(caminho, 0600);
}

static void gravar(void) {
  char nome[32], buf[700];
  snprintf(nome, sizeof nome, DIS_ARQ_FMT, perfil);
  snprintf(buf, sizeof buf, "%s\t%s\t%ld\n", token, refresh, expiraEm);
  if (dados_gravar(nome, buf)) restringirArquivo(nome);
}

static void carregar(int p) {
  char nome[32], *b, *c1, *c2;
  token[0] = refresh[0] = 0;
  expiraEm = 0;
  estado = DIS_PARADO;
  perfil = p;
  snprintf(nome, sizeof nome, DIS_ARQ_FMT, p);
  restringirArquivo(nome);
  b = dados_ler(nome);
  if (!b) return;
  c1 = strchr(b, '\t');
  c2 = c1 ? strchr(c1 + 1, '\t') : NULL;
  if (c1 && c2) {
    *c1 = 0; *c2 = 0;
    snprintf(token, sizeof token, "%s", b);
    snprintf(refresh, sizeof refresh, "%s", c1 + 1);
    expiraEm = atol(c2 + 1);
    if (token[0]) estado = DIS_LIGADO;
  }
  free(b);
}

int discord_disponivel(void) { return NV_DISCORD_CLIENT_ID[0] != 0; }
DisEstado discord_estado(void) {
  DisEstado e;
  pthread_mutex_lock(&trava); e = estado; pthread_mutex_unlock(&trava);
  return e;
}
#ifdef NV_TPK40
// Tizen 4/5's manual ELF loader cannot initialize compiler TLS. Use the same
// per-thread snapshot lifetime with pthread keys, as the network layer does.
typedef struct { char codigo[32], endereco[256], mensagem[160]; } DisCopia;
static pthread_key_t copiaKey;
static pthread_once_t copiaOnce = PTHREAD_ONCE_INIT;
static int copiaKeyOk;
static void copiaCriar(void) { copiaKeyOk = pthread_key_create(&copiaKey, free) == 0; }
static DisCopia *copiaDoFio(void) {
  pthread_once(&copiaOnce, copiaCriar);
  if (!copiaKeyOk) return NULL;
  DisCopia *p = pthread_getspecific(copiaKey);
  if (!p) {
    p = calloc(1, sizeof *p);
    if (p && pthread_setspecific(copiaKey,p)) { free(p); p = NULL; }
  }
  return p;
}
#endif
const char *discord_codigo(void) {
#ifdef NV_TPK40
  DisCopia *p = copiaDoFio(); if (!p) return "";
  char *copia = p->codigo;
#else
  static _Thread_local char copia[sizeof userCode];
#endif
  pthread_mutex_lock(&trava); memcpy(copia, userCode, sizeof userCode); pthread_mutex_unlock(&trava);
  return copia;
}
const char *discord_url(void) {
#ifdef NV_TPK40
  DisCopia *p = copiaDoFio(); if (!p) return "";
  char *copia = p->endereco;
#else
  static _Thread_local char copia[sizeof url];
#endif
  pthread_mutex_lock(&trava); memcpy(copia, url, sizeof url); pthread_mutex_unlock(&trava);
  return copia;
}
const char *discord_erro(void) {
#ifdef NV_TPK40
  DisCopia *p = copiaDoFio(); if (!p) return "";
  char *copia = p->mensagem;
#else
  static _Thread_local char copia[sizeof erro];
#endif
  pthread_mutex_lock(&trava); memcpy(copia, erro, sizeof erro); pthread_mutex_unlock(&trava);
  return copia;
}

// ---------------------------------------------------------- HTTP (em fio)
static char *postarForm(const char *caminho, const char *corpo, int *st) {
  static const char *cab[] = { "Content-Type: application/x-www-form-urlencoded", NULL };
  char u[160];
  snprintf(u, sizeof u, DIS_API "%s", caminho);
  return rede_postar_seguro_st(u, 20, cab, corpo, st);
}

// Resposta de token: access_token, refresh_token, expires_in. Grava com a trava.
static int lerToken(const char *r, int ger) {
  char tk[256], rf[256];
  double exp;
  if (!r || !js_texto_raiz(r, "access_token", tk, sizeof tk) || !tk[0]) return 0;
  if (strlen(tk) == sizeof tk - 1) return 0; // Reject possible truncation.
  if (!js_texto_raiz(r, "refresh_token", rf, sizeof rf)) rf[0] = 0;
  if (strlen(rf) == sizeof rf - 1) return 0;
  exp = js_num(r, NULL, "expires_in", 0);
  pthread_mutex_lock(&trava);
  if (ger == geracao) {
    snprintf(token, sizeof token, "%s", tk);
    if (rf[0]) snprintf(refresh, sizeof refresh, "%s", rf);
    expiraEm = exp > 0 ? agoraSeg() + (long)exp : 0;
    estado = DIS_LIGADO;
    gravar();
  }
  pthread_mutex_unlock(&trava);
  return 1;
}

typedef struct { int ger; char a[1024], autorizacao[sizeof token]; } Pedido;

static void *fioPedir(void *u) {
  Pedido *p = (Pedido *)u;
  char corpo[200], dc[256], uc[32], vu[256];
  int st = 0;
  char *r;
  snprintf(corpo, sizeof corpo, "client_id=%s&scope=%s", NV_DISCORD_CLIENT_ID, DIS_ESCOPO);
  r = postarForm("/oauth2/device/authorize", corpo, &st);
  pthread_mutex_lock(&trava);
  if (p->ger == geracao && estado == DIS_PEDINDO) {
    if (r && st == 200 && js_texto_raiz(r, "device_code", dc, sizeof dc) &&
        js_texto_raiz(r, "user_code", uc, sizeof uc)) {
      double iv = js_num(r, NULL, "interval", 5), ex = js_num(r, NULL, "expires_in", 300);
      // verification_uri_complete ja leva o codigo: e o que vai no QR.
      if (!js_texto_raiz(r, "verification_uri_complete", vu, sizeof vu) || !vu[0])
        snprintf(vu, sizeof vu, "https://discord.com/activate");
      snprintf(deviceCode, sizeof deviceCode, "%s", dc);
      snprintf(userCode, sizeof userCode, "%s", uc);
      snprintf(url, sizeof url, "%s", vu);
      pollMs = (unsigned)(iv > 0 ? iv : 5) * 1000u;
      proxPollMs = 0;
      codigoExpiraEm = agoraSeg() + (long)ex;
      estado = DIS_AGUARDANDO;
    } else {
      char e[100] = "";
      if (r) js_texto_raiz(r, "error", e, sizeof e);
      printf("[discord] authorization: HTTP %d\n", st);
      snprintf(erro, sizeof erro, st ? "o Discord recusou o pedido (%d)" : "sem conexão com o Discord", st);
      estado = DIS_ERRO;
    }
  }
  fioVivo = 0;
  pthread_mutex_unlock(&trava);
  free(r);
  free(p);
  return NULL;
}

// OAuth values are opaque: '+' and '&' must survive form encoding.
static void formValor(const char *in, char *out, size_t tam) {
  static const char hex[] = "0123456789ABCDEF";
  size_t n = 0;
  while (*in && n + 3 < tam) {
    unsigned char c = (unsigned char)*in++;
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') out[n++] = c;
    else { out[n++] = '%'; out[n++] = hex[c >> 4]; out[n++] = hex[c & 15]; }
  }
  out[n] = 0;
}

static void *fioPoll(void *u) {
  Pedido *p = (Pedido *)u;
  char corpo[1100], codificado[sizeof deviceCode * 3], e[64] = "";
  int st = 0;
  char *r;
  formValor(p->a, codificado, sizeof codificado);
  snprintf(corpo, sizeof corpo,
           "grant_type=urn%%3Aietf%%3Aparams%%3Aoauth%%3Agrant-type%%3Adevice_code"
           "&device_code=%s&client_id=%s", codificado, NV_DISCORD_CLIENT_ID);
  r = postarForm("/oauth2/token", corpo, &st);
  if (r && st == 200 && lerToken(r, p->ger)) {
    pthread_mutex_lock(&trava);
    if (p->ger == geracao) { deviceCode[0] = userCode[0] = 0; printf("[discord] linked\n"); }
  } else {
    if (r) js_texto_raiz(r, "error", e, sizeof e);
    pthread_mutex_lock(&trava);
    if (p->ger == geracao && estado == DIS_AGUARDANDO) {
      if (!strcmp(e, "slow_down")) pollMs += 5000;
      else if (!strcmp(e, "expired_token") || !strcmp(e, "access_denied")) {
        snprintf(erro, sizeof erro, !strcmp(e, "access_denied")
                 ? "autorização negada no Discord" : "o código expirou — OK pede outro");
        estado = DIS_ERRO;
      } else if (strcmp(e, "authorization_pending") != 0 && st) {
        printf("[discord] poll: HTTP %d\n", st);
      }
    }
  }
  fioVivo = 0;
  pthread_mutex_unlock(&trava);
  free(r);
  free(p);
  return NULL;
}

static void *fioRenovar(void *u) {
  Pedido *p = (Pedido *)u;
  char corpo[1100], codificado[sizeof refresh * 3], e[64] = "";
  int st = 0;
  char *r;
  formValor(p->a, codificado, sizeof codificado);
  snprintf(corpo, sizeof corpo, "grant_type=refresh_token&refresh_token=%s&client_id=%s",
           codificado, NV_DISCORD_CLIENT_ID);
  r = postarForm("/oauth2/token", corpo, &st);
  if (!(r && st == 200 && lerToken(r, p->ger))) {
    if (r) js_texto_raiz(r, "error", e, sizeof e);
    printf("[discord] refresh: HTTP %d\n", st);
    pthread_mutex_lock(&trava);
    // So 400 invalid_grant e vinculo morto. Sem rede, tenta de novo depois.
    if (p->ger == geracao && st == 400 && !strcmp(e, "invalid_grant")) { token[0] = 0; estado = DIS_INVALIDO; gravar(); }
  } else {
    pthread_mutex_lock(&trava);
    if (p->ger == geracao) printf("[discord] token refreshed\n");
  }
  fioVivo = 0;
  pthread_mutex_unlock(&trava);
  free(r);
  free(p);
  return NULL;
}

// Arte: o Discord so mostra imagem de fora pelo proxy dele ("mp:external/...").
static char arteUrl[NV_ARTE_URL_MAX], arteMp[600];
static int arteVivo;
static unsigned proxArte;

static void *fioArte(void *u) {
  Pedido *p = (Pedido *)u;
  char ender[200], cab[300], caminho[512];
  const char *cabs[3];
  char *corpo, *r;
  int st = 0;
  Jsw w;
  jsw_iniciar(&w);
  jsw_obj_ini(&w);
  jsw_chave(&w, "urls");
  jsw_arr_ini(&w);
  jsw_str(&w, p->a);
  jsw_arr_fim(&w);
  jsw_obj_fim(&w);
  corpo = strdup(jsw_texto_final(&w));
  jsw_livre(&w);
  pthread_mutex_lock(&trava);
  if (p->ger != geracao || !ligadoSemTrava()) {
    arteVivo = 0; pthread_mutex_unlock(&trava);
    free(corpo); free(p); return NULL;
  }
  pthread_mutex_unlock(&trava);
  // Capture the original profile token when scheduling; never use a new one.
  snprintf(cab, sizeof cab, "Authorization: Bearer %s", p->autorizacao);
  cabs[0] = cab; cabs[1] = NULL;
  snprintf(ender, sizeof ender, "https://discord.com/api/v9/applications/%s/external-assets",
           NV_DISCORD_CLIENT_ID);
  r = corpo ? rede_postar_seguro_st(ender, 15, cabs, corpo, &st) : NULL;
  caminho[0] = 0;
  if (r && st == 200) {
    const char *el = js_raiz_array(r);
    if (el) js_texto(el, js_fim(el), "external_asset_path", caminho, sizeof caminho);
  } else {
    printf("[discord] artwork: HTTP %d\n", st);
  }
  pthread_mutex_lock(&trava);
  if (p->ger == geracao && !strcmp(arteUrl, p->a)) {
    // O proximo montar() pega e mudou() ve a arte nova: reenvia sozinho.
    if (caminho[0]) snprintf(arteMp, sizeof arteMp, "mp:%s", caminho);
  }
  arteVivo = 0;
  pthread_mutex_unlock(&trava);
  free(corpo);
  free(r);
  free(p);
  return NULL;
}

static void soltar(void *(*rotina)(void *), const char *arg, int *vivo) {
  pthread_t f;
  Pedido *p = (Pedido *)calloc(1, sizeof *p);
  if (!p) return;
  p->ger = geracao;
  snprintf(p->autorizacao, sizeof p->autorizacao, "%s", token);
  snprintf(p->a, sizeof p->a, "%s", arg ? arg : "");
  *vivo = 1;
  if (pthread_create(&f, NULL, rotina, p) == 0) pthread_detach(f);
  else { *vivo = 0; free(p); }
}

// ------------------------------------------------------------ vinculo (UI)
void discord_comecar(void) {
  if (!discord_disponivel()) return;
  pthread_mutex_lock(&trava);
  if (estado == DIS_PEDINDO || estado == DIS_AGUARDANDO || fioVivo) { pthread_mutex_unlock(&trava); return; }
  erro[0] = 0;
  userCode[0] = deviceCode[0] = url[0] = 0;
  estado = DIS_PEDINDO;
  soltar(fioPedir, NULL, &fioVivo);
  pthread_mutex_unlock(&trava);
}

void discord_cancelar(void) {
  pthread_mutex_lock(&trava);
  geracao++;
  deviceCode[0] = userCode[0] = url[0] = 0;
  estado = token[0] ? DIS_LIGADO : DIS_PARADO;
  pthread_mutex_unlock(&trava);
}

// ------------------------------------------------------------ gateway
typedef enum { G_FORA, G_CONECTANDO, G_HELLO, G_IDENTIFICANDO, G_PRONTO } GEstado;
static DiscordWs *ws;
static GEstado g;
static unsigned hbIntervalo, proxHb, ultimoEnvio, proxTentativa, proxRenovar, recuoMs = 5000;
static int hbSemAck;
static long long seq = -1;
static unsigned ociosoDesde;

// O que esta na tela e o que ja foi mandado.
typedef struct {
  char detalhes[128], estadoTxt[128], arte[600], arteTxt[128];
  long long ini, fim;   // ms; 0 = sem relogio
  int vazio;
} Presenca;
static Presenca enviada, querida;
static int temEnviada;

static void gatewayFechar(const char *porque) {
  if (ws) { printf("[discord] gateway closed: %s\n", porque); dws_fechar(ws); }
  ws = NULL;
  g = G_FORA;
  temEnviada = 0;
  seq = -1;
  hbIntervalo = 0; hbSemAck = 0;
}

void discord_esquecer(void) {
  char nome[32];
  gatewayFechar("unlinked");
  pthread_mutex_lock(&trava);
  geracao++;
  token[0] = refresh[0] = 0;
  deviceCode[0] = userCode[0] = url[0] = erro[0] = 0;
  arteUrl[0] = arteMp[0] = 0;
  proxArte = 0;
  expiraEm = 0;
  estado = DIS_PARADO;
  snprintf(nome, sizeof nome, DIS_ARQ_FMT, perfil);
  dados_apagar(nome);
  pthread_mutex_unlock(&trava);
}

void discord_encerrar(void) {
  gatewayFechar("app shutting down");
  pthread_mutex_lock(&trava);
  geracao++;
  deviceCode[0] = userCode[0] = url[0] = 0;
  estado = token[0] ? DIS_LIGADO : DIS_PARADO;
  pthread_mutex_unlock(&trava);
}

static void mandar(const char *json) {
  if (ws && dws_enviar(ws, json) != 0) gatewayFechar("send failed");
}

static void mandarHeartbeat(void) {
  char b[64];
  if (seq >= 0) snprintf(b, sizeof b, "{\"op\":1,\"d\":%lld}", seq);
  else snprintf(b, sizeof b, "{\"op\":1,\"d\":null}");
  mandar(b);
}

static void mandarIdentify(void) {
  Jsw w;
  char bearer[300];
  pthread_mutex_lock(&trava);
  snprintf(bearer, sizeof bearer, "Bearer %s", token);
  pthread_mutex_unlock(&trava);
  jsw_iniciar(&w);
  jsw_obj_ini(&w);
  jsw_ci(&w, "op", 2);
  jsw_chave(&w, "d");
  jsw_obj_ini(&w);
  jsw_cs(&w, "token", bearer);
  jsw_ci(&w, "intents", 0);
  jsw_chave(&w, "properties");
  jsw_obj_ini(&w);
  jsw_cs(&w, "os", "linux");
  jsw_cs(&w, "browser", "Nuvio");
  jsw_cs(&w, "device", "Nuvio TV");
  jsw_obj_fim(&w);
  jsw_obj_fim(&w);
  jsw_obj_fim(&w);
  mandar(jsw_texto_final(&w));
  jsw_livre(&w);
}

static void mandarPresenca(const Presenca *p) {
  Jsw w;
  jsw_iniciar(&w);
  jsw_obj_ini(&w);
  jsw_ci(&w, "op", 3);
  jsw_chave(&w, "d");
  jsw_obj_ini(&w);
  jsw_ci(&w, "since", 0);
  jsw_chave(&w, "activities");
  jsw_arr_ini(&w);
  if (!p->vazio) {
    jsw_obj_ini(&w);
    jsw_cs(&w, "name", "Nuvio");
    jsw_ci(&w, "type", 3);   // 3 = Watching ("Assistindo Nuvio")
    jsw_cs(&w, "application_id", NV_DISCORD_CLIENT_ID);
    if (p->detalhes[0]) jsw_cs(&w, "details", p->detalhes);
    if (p->estadoTxt[0]) jsw_cs(&w, "state", p->estadoTxt);
    if (p->ini) {
      jsw_chave(&w, "timestamps");
      jsw_obj_ini(&w);
      jsw_ci(&w, "start", p->ini);
      if (p->fim) jsw_ci(&w, "end", p->fim);
      jsw_obj_fim(&w);
    }
    if (p->arte[0]) {
      jsw_chave(&w, "assets");
      jsw_obj_ini(&w);
      jsw_cs(&w, "large_image", p->arte);
      if (p->arteTxt[0]) jsw_cs(&w, "large_text", p->arteTxt);
      jsw_obj_fim(&w);
    }
    jsw_obj_fim(&w);
  }
  jsw_arr_fim(&w);
  jsw_cs(&w, "status", "online");
  jsw_cb(&w, "afk", 0);
  jsw_obj_fim(&w);
  jsw_obj_fim(&w);
  mandar(jsw_texto_final(&w));
  jsw_livre(&w);
}

// Mensagem do gateway. op: 0 evento, 1 pede heartbeat, 7 reconectar,
// 9 invalid session, 10 hello, 11 ack.
static void tratar(const char *m, unsigned agora) {
  int op = (int)js_num(m, NULL, "op", -1);
  double s = js_num(m, NULL, "s", -1);
  if (s >= 0) seq = (long long)s;
  switch (op) {
    case 10: {
      const char *d = strstr(m, "\"d\"");
      hbIntervalo = (unsigned)js_num(d ? d : m, NULL, "heartbeat_interval", 41250);
      // Primeiro heartbeat com jitter, como o Discord pede.
      proxHb = agora + (unsigned)(hbIntervalo * ((rand() % 100) / 100.0));
      hbSemAck = 0;
      mandarIdentify();
      g = G_IDENTIFICANDO;
      break;
    }
    case 11: hbSemAck = 0; break;
    case 1: mandarHeartbeat(); break;
    case 7: gatewayFechar("Discord requested reconnect"); proxTentativa = agora + 1000; break;
    case 9: gatewayFechar("invalid session"); proxTentativa = agora + 5000; break;
    case 0: {
      char t[32] = "";
      js_texto_raiz(m, "t", t, sizeof t);
      if (!strcmp(t, "READY")) {
        printf("[discord] gateway ready\n");
        g = G_PRONTO;
        recuoMs = 5000;
        temEnviada = 0;
      }
      break;
    }
    default: break;
  }
}

// ---------------------------------------------------- o que esta tocando
static long long agoraMsEpoca(void) {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

// Only known public artwork CDNs are eligible for third-party presence.
// A configured add-on poster can contain a personal API key in its path/query.
static int posterPublico(const char *u) {
  return u && !strchr(u, '?') &&
      (!strncmp(u, "https://image.tmdb.org/", 23) ||
       !strncmp(u, "https://images.metahub.space/", 29));
}

// 1 se ha algo para mostrar; preenche `p`.
static int montar(Presenca *p) {
  const CatItem *ci;
  const char *ep, *poster;
  char publica[128];
  float pos, dur;
  memset(p, 0, sizeof *p);
  if (!player_com_video()) return 0;
  ci = cat_item(player_indice());
  if (!ci || !ci->titulo[0]) return 0;
  // Personal-server titles are private media: never published to Discord.
  if (jfid_e(ci->imdb)) return 0;
  snprintf(p->detalhes, sizeof p->detalhes, "%s", ci->titulo);
  ep = player_linha_episodio();
  if (player_eh_canal()) snprintf(p->estadoTxt, sizeof p->estadoTxt, "TV ao vivo");
  else if (ep && *ep) snprintf(p->estadoTxt, sizeof p->estadoTxt, "%s", ep);
  else if (ci->meta[0]) snprintf(p->estadoTxt, sizeof p->estadoTxt, "%.4s", ci->meta);
  if (player_pausado()) {
    // Pausado: sem relogio (o Discord contaria sozinho) e o estado diz.
    char t[128];
    snprintf(t, sizeof t, "%s%s%s", p->estadoTxt, p->estadoTxt[0] ? " · " : "", i18n("Pausado"));
    snprintf(p->estadoTxt, sizeof p->estadoTxt, "%s", t);
  } else if (!player_eh_canal()) {
    long long agora = agoraMsEpoca();
    pos = player_posicao_seg();
    dur = player_duracao_seg();
    p->ini = agora - (long long)(pos * 1000.0f);
    if (dur > 60.0f && dur > pos) p->fim = p->ini + (long long)(dur * 1000.0f);
  }
  // Never forward rejected add-on URLs; derive artwork from a numeric IMDb ID.
  poster = ci->poster;
  if (!posterPublico(poster)) {
    size_t n = 2;
    publica[0] = 0;
    if (!strncmp(ci->imdb, "tt", 2)) {
      while (ci->imdb[n] >= '0' && ci->imdb[n] <= '9') n++;
      if (n > 2 && (!ci->imdb[n] || ci->imdb[n] == ':'))
        snprintf(publica, sizeof publica,
                 "https://images.metahub.space/poster/medium/%.*s/img", (int)n, ci->imdb);
    }
    poster = publica;
  }
  pthread_mutex_lock(&trava);
  if (poster[0]) {
    if (strcmp(arteUrl, poster) != 0) {
      snprintf(arteUrl, sizeof arteUrl, "%s", poster);
      arteMp[0] = 0;
      proxArte = 0;
      if (!arteVivo) soltar(fioArte, arteUrl, &arteVivo);
    }
    snprintf(p->arte, sizeof p->arte, "%s", arteMp);
  } else {
    arteUrl[0] = arteMp[0] = 0;
    proxArte = 0;
  }
  pthread_mutex_unlock(&trava);
  snprintf(p->arteTxt, sizeof p->arteTxt, "%s", ci->titulo);
  return 1;
}

// Mudou o bastante para mandar de novo? O relogio anda sozinho no Discord;
// so um salto (>10 s: busca, pulo de abertura) conta.
static int mudou(const Presenca *a, const Presenca *b) {
  long long d = a->ini - b->ini;
  if (a->vazio != b->vazio || strcmp(a->detalhes, b->detalhes) ||
      strcmp(a->estadoTxt, b->estadoTxt) || strcmp(a->arte, b->arte)) return 1;
  if ((a->ini == 0) != (b->ini == 0)) return 1;
  return d > 10000 || d < -10000;
}

// ------------------------------------------------------------------ passo
void discord_recarregar(void) {
  gatewayFechar("settings restored");
  pthread_mutex_lock(&trava); geracao++; perfil = -1; pthread_mutex_unlock(&trava);
}
void discord_passo(unsigned agora) {
  int tem;
  if (!discord_disponivel()) return;
  if (perfis_ativo() != perfil) {
    gatewayFechar("profile changed");
    pthread_mutex_lock(&trava);
    geracao++;
    proxTentativa = proxRenovar = proxArte = 0;
    hbIntervalo = hbSemAck = 0;
    deviceCode[0] = userCode[0] = url[0] = erro[0] = 0;
    arteUrl[0] = arteMp[0] = 0;
    carregar(perfis_ativo());
    pthread_mutex_unlock(&trava);
  }

  // Vinculo: poll do codigo e prazo dele.
  pthread_mutex_lock(&trava);
  if (estado == DIS_AGUARDANDO && !fioVivo) {
    if (codigoExpiraEm && agoraSeg() > codigoExpiraEm) {
      snprintf(erro, sizeof erro, "o código expirou — OK pede outro");
      estado = DIS_ERRO;
    } else if (!proxPollMs || (int)(agora - proxPollMs) >= 0) {
      proxPollMs = agora + pollMs;
      soltar(fioPoll, deviceCode, &fioVivo);
    }
  }
  // Retry artwork skipped while an older profile/title job was still active.
  if (ligadoSemTrava() && player_com_video() && arteUrl[0] && !arteMp[0] && !arteVivo &&
      (!proxArte || (int)(agora - proxArte) >= 0)) {
    proxArte = agora + 60000;
    soltar(fioArte, arteUrl, &arteVivo);
  }
  // Token perto de vencer (1 h antes): renova em segundo plano.
  if (ligadoSemTrava() && !fioVivo && refresh[0] && expiraEm && agoraSeg() > expiraEm - 3600 &&
      (!proxRenovar || (int)(agora - proxRenovar) >= 0)) {
    proxRenovar = agora + 60000;   // sem rede: tenta de novo em 1 min
    soltar(fioRenovar, refresh, &fioVivo);
  }
  pthread_mutex_unlock(&trava);

  tem = ligado() && montar(&querida);
  if (tem) ociosoDesde = 0;
  else if (!ociosoDesde) ociosoDesde = agora ? agora : 1;

  // Nada tocando por um minuto: fecha (o Discord apaga a atividade).
  if (!tem && ws && ociosoDesde && agora - ociosoDesde > DIS_OCIOSO_MS) {
    gatewayFechar("nothing playing");
    return;
  }

  // Conectar quando ha o que mostrar.
  if (!ws) {
    if (!tem || (proxTentativa && (int)(agora - proxTentativa) < 0)) return;
    printf("[discord] connecting to gateway\n");
    ws = dws_abrir(DIS_GATEWAY);
    g = G_CONECTANDO;
    return;
  }

  { int e = dws_estado(ws);
    if (e == DWS_CAIU) {
      int cod = dws_codigo_fechamento(ws);
      gatewayFechar("connection lost");
      // 4004 = token recusado. Renova; se nao der, o fio marca INVALIDO.
      if (cod == 4004) {
        pthread_mutex_lock(&trava);
        if (refresh[0] && !fioVivo) soltar(fioRenovar, refresh, &fioVivo);
        else if (!refresh[0]) { estado = DIS_INVALIDO; token[0] = 0; }
        pthread_mutex_unlock(&trava);
      }
      proxTentativa = agora + recuoMs;
      if (recuoMs < 300000) recuoMs *= 2;
      return;
    }
    if (e == DWS_CONECTANDO) return;
    if (g == G_CONECTANDO) g = G_HELLO; }

  { char *m;
    int n = 0;
    while (ws && n++ < 8 && (m = dws_receber(ws))) { tratar(m, agora); free(m); } }
  if (!ws) return;

  if (hbIntervalo && (int)(agora - proxHb) >= 0) {
    if (hbSemAck) { gatewayFechar("heartbeat unacknowledged"); proxTentativa = agora + 2000; return; }
    hbSemAck = 1;
    mandarHeartbeat();
    proxHb = agora + hbIntervalo;
  }

  if (g != G_PRONTO) return;
  if (!tem) querida.vazio = 1;
  if ((!temEnviada || mudou(&querida, &enviada)) &&
      (!temEnviada || agora - ultimoEnvio >= DIS_INTERVALO_MS)) {
    mandarPresenca(&querida);
    enviada = querida;
    temEnviada = 1;
    ultimoEnvio = agora;
  }
}
