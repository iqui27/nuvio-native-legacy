// Watch Together: a cola com o app — ver juntos.h.
#include "juntos.h"
#include "atividade.h"
#include "discordws.h"
#include "idioma.h"
#include "ilha.h"
#include "js.h"
#include "juntosfonte.h"
#include "juntosui.h"
#include "perfilinfantil.h"
#include "player.h"
#include "recomenda.h"
#include "rede.h"
#include "trakt.h"
#include "video.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef NV_REC_URL
#define NV_REC_URL ""
#endif

#if defined(NV_ANDROID)
#define PLAT "android"
#elif defined(NV_TPK)
#define PLAT "tpk"
#elif defined(__EMSCRIPTEN__)
#define PLAT "wgt"
#elif defined(__APPLE__)
#define PLAT "mac"
#elif defined(NV_LINUX_DESKTOP)
#define PLAT "linux"
#else
#define PLAT "lg"
#endif

#define PRAZO_S          12
#define CONVITES_MS      20000u   // fora do player (plano 7.2)
#define CONVITES_PLR_MS  60000u   // dentro do player
#define CONVITES_ESPERA  15000u   // o app assenta antes da primeira consulta
#define FONTE_ESPERA_MS  8000u    // sem a mesma fonte, a mais parecida depois disto
#define RECONECTA_MAX_MS 30000u
#define RECONECTA_TETO   60000u   // depois de 60 s fora, a sala ja seguiu: sai

// ---- o fio de rede ---------------------------------------------------------------------
typedef enum { OP_CRIAR = 1, OP_ENTRAR, OP_CONVITES, OP_RECUSAR, OP_FIM, OP_SILENCIAR, OP_CONVIDAR } OpTipo;
typedef struct { OpTipo tipo; unsigned gen; char rota[48]; char *corpo; int get; } Op;
typedef struct { OpTipo tipo; unsigned gen; int status; char *txt; } Resp;
#define FILA 8
static SDL_mutex *mtx;
static SDL_cond *cond;
static Op fila[FILA]; static int nFila;
static Resp caixa[FILA]; static int nCaixa;
static int fioVivo;
static unsigned gen;   // sobe na troca de perfil e ao sair: resposta velha nao vale

static int fioLaco(void *u) {
  (void)u;
  for (;;) {
    Op op;
    Resp r;
    const char *cab[4];
    char aut[3200], via[40], pf[40], url[400];
    SDL_LockMutex(mtx);
    while (!nFila) SDL_CondWait(cond, mtx);
    op = fila[0];
    memmove(&fila[0], &fila[1], sizeof fila[0] * (size_t)(--nFila));
    SDL_UnlockMutex(mtx);
    memset(&r, 0, sizeof r);
    r.tipo = op.tipo; r.gen = op.gen;
    printf("[juntos] http: op=%d requesting auth\n", op.tipo);
    if (recomenda_cabecalhos(cab, aut, sizeof aut, via, sizeof via, pf, sizeof pf)) {
      printf("[juntos] http: op=%d auth ready, request started\n", op.tipo);
      snprintf(url, sizeof url, "%s%s", NV_REC_URL, op.rota);
      r.txt = op.get ? rede_baixar_st(url, PRAZO_S, cab, &r.status)
                     : rede_postar_st(url, PRAZO_S, cab, op.corpo ? op.corpo : "{}", &r.status);
      if (r.status == 401 && !strcmp(via, "X-Nuvio-Auth: trakt"))
        trakt_http_recusada();
    } else {
      r.status = -1; // Local auth failure, not an HTTP response.
      printf("[juntos] http: op=%d auth unavailable (request not sent)\n", op.tipo);
    }
    printf("[juntos] http: op=%d status=%d transport=%s\n", op.tipo, r.status,
           r.status < 0 ? "auth unavailable" : r.status ? "ok" : "failed");
    free(op.corpo);
    SDL_LockMutex(mtx);
    if (nCaixa < FILA) caixa[nCaixa++] = r;
    else free(r.txt);
    SDL_UnlockMutex(mtx);
  }
  return 0;
}

static void pedir(OpTipo tipo, const char *rota, const char *corpo, int get) {
  Op op;
  if (!NV_REC_URL[0]) { printf("[juntos] http: service not configured\n"); return; }
  if (!mtx) { mtx = SDL_CreateMutex(); cond = SDL_CreateCond(); }
  if (!mtx || !cond) { printf("[juntos] http: queue initialization failed\n"); return; }
  memset(&op, 0, sizeof op);
  op.tipo = tipo; op.gen = gen; op.get = get;
  snprintf(op.rota, sizeof op.rota, "%s", rota);
  op.corpo = corpo ? strdup(corpo) : NULL;
  // Perfil infantil: o servidor aplica as regras quando recebe `infantil:1`
  // (corpo no POST, ?infantil=1 no GET). Marca por perfil: perfilinfantil.h.
  if (perfil_infantil_ativo()) {
    if (get) {
      size_t n = strlen(op.rota);
      snprintf(op.rota + n, sizeof op.rota - n, "%s", strchr(op.rota, '?') ? "&infantil=1" : "?infantil=1");
    } else {
      const char *c = corpo && corpo[0] == '{' ? corpo : "{}";
      size_t n = strlen(c) + 24;
      char *novo = (char *)malloc(n);
      if (novo) {
        snprintf(novo, n, "{\"infantil\":1%s%s", c[1] == '}' ? "" : ",", c + 1);
        free(op.corpo);
        op.corpo = novo;
      }
    }
  }
  SDL_LockMutex(mtx);
  if (nFila == FILA) { SDL_UnlockMutex(mtx); free(op.corpo); printf("[juntos] http: queue full\n"); return; }
  fila[nFila++] = op;
  if (!fioVivo) {
    SDL_Thread *t = SDL_CreateThread(fioLaco, "nv-juntos", NULL);
    if (t) { SDL_DetachThread(t); fioVivo = 1; }
    else printf("[juntos] http: worker creation failed\n");
  }
  SDL_CondSignal(cond);
  SDL_UnlockMutex(mtx);
  printf("[juntos] room: request queued op=%d\n", tipo);
}

// ---- estado do fio principal ----------------------------------------------------------
static JuntosFase fase;
static JsMotor motor;
static int temMotor, salaFalhou;
static Uint32 repetirSalaEm, agoraPasso;
static DiscordWs *ws;
static char salaId[40], codigo[12], wsBase[256], webBase[256], bilhete[1600], tituloSala[160];
static Uint32 caiuEm, proxConexao, atrasoConexao;
static int pedindoBilhete;
// anfitriao preparado
static struct {
  char imdb[24], ep[24], titulo[160], poster[512];
  int t, e, soAnf, n, enviados;   // enviados: already in the POST /v1/sala body
  char conv[JUNTOS_CONVIDADOS_MAX][96];
} prep;
// abrir o titulo (convidado / proximo episodio)
static int abrirPedido, escolhendo;
static char abrirImdb[24], abrirTitulo[160], abrirPoster[512];
static int abrirT, abrirE;
static JsImpressao impSala;
static JsVeredito veredito;
static int fonteEscolhida = -1, salaDurConferida, fonteVista = -1;
// convites
static Uint32 iniciouEm, proxConvites;
static int buscandoConvites;
static char vistos[16][40]; static int nVistos;
// celular
static char urlCelular[200];
static Uint32 entrouEm;   // primeiro "conectado" desta sala (para o feed)
static int juntoMandado, maxMembros;

int juntos_disponivel(void) { return NV_REC_URL[0] && recomenda_ativo() && recomenda_meu_id()[0]; }
int juntos_ativo(void) { return fase != JF_OCIOSO; }
JuntosFase juntos_fase(void) { return fase; }
const JsMotor *juntos_motor(void) { return temMotor ? &motor : NULL; }
const char *juntos_titulo(void) { return tituloSala; }
const char *juntos_codigo(void) { return codigo; }
int juntos_precisa_auth(void) { return salaFalhou == 2; }
const char *juntos_url_celular(void) { return urlCelular; }
int juntos_sou_anfitriao(void) { return temMotor && jsm_sou_anfitriao(&motor); }
int juntos_pode_controlar(void) { return !temMotor || jsm_pode_controlar(&motor); }

static double agoraLocal(void) { return (double)SDL_GetTicks64(); }

static void separarEpJ(const char *ep, char *imdb, size_t n, int *t, int *e) {
  const char *p = strchr(ep, ':');
  *t = *e = 0;
  if (!p) { snprintf(imdb, n, "%s", ep); return; }
  snprintf(imdb, n, "%.*s", (int)(p - ep), ep);
  sscanf(p + 1, "%d:%d", t, e);
}

// ---- o player de verdade, para o motor -------------------------------------------------
static double pPos(void *u, double *t) {
  Uint64 stamp;
  double p = video_pos_amostra(&stamp);
  (void)u;
  *t = (double)stamp;
  return p;
}
static int pTocando(void *u) { (void)u; return video_tocando() && !player_pausado(); }
static int pPronto(void *u) { (void)u; return player_juntos_preparado() && video_duracao() > 0; }
static unsigned pBuffer(void *u) { (void)u; return video_bufferando_ms(); }
static double pDur(void *u) { (void)u; return video_duracao(); }
static void pPausar(void *u, int p) { (void)u; player_juntos_pausar(p); }
static void pBuscar(void *u, double s) { (void)u; player_juntos_buscar(s); }
static void pVel(void *u, int c) { (void)u; video_taxa_sync(c * 10); }

static int enviarWs(void *u, const char *j) { (void)u; return ws ? dws_enviar(ws, j) : -1; }

static char nivelDaTv(void) {
#ifdef NV_ANDROID
  // Room rate corrections would restart ExoPlayer when switching PCM policy.
  return 'B';
#else
  // Level A requires speed support; VelMedidor verifies the actual clock.
  return video_velocidade_suportada() && !video_velocidade_bloqueada() ? 'A' : 'B';
#endif
}

static void iniciarMotor(void) {
  JsPlayer jp = { pPos, pTocando, pPronto, pBuffer, pDur, pPausar, pBuscar, pVel, NULL };
  jsm_iniciar(&motor, recomenda_meu_id(), PLAT, "", nivelDaTv(), &jp, enviarWs, NULL);
  temMotor = 1;
  if (fonteEscolhida >= 0) jsm_fonte(&motor, jsf_nome(veredito));
}

static void fecharWs(void) {
  if (ws) {
    printf("[juntos] websocket: close code=%d\n", dws_codigo_fechamento(ws));
    dws_fechar(ws); ws = NULL;
  }
  if (temMotor) jsm_caiu(&motor);
}

// "Assistiram juntos" no feed (decisao 8): so sessao de verdade (10 min com
// alguem mais), uma vez por sala. atividade.c respeita o alcance da pessoa.
static void mandarJunto(void) {
  char imdb[24]; int t = 0, e = 0, seg;
  if (!temMotor || juntoMandado || !entrouEm || !motor.temSala || maxMembros < 2) return;
  seg = (int)((SDL_GetTicks() - entrouEm) / 1000u);
  if (seg < 600) return;
  juntoMandado = 1;
  separarEpJ(motor.sala.ep, imdb, sizeof imdb, &t, &e);
  atividade_junto(imdb, t > 0 ? "series" : "movie", motor.sala.titulo, motor.sala.poster, t, e, 0);
  printf("[juntos] assistiram juntos: evento para o feed\n");
}

static void zerar(void) {
  mandarJunto();
  entrouEm = 0; juntoMandado = 0; maxMembros = 0;
  fecharWs();
  gen++;
  fase = JF_OCIOSO;
  temMotor = salaFalhou = 0;
  repetirSalaEm = 0;
  salaId[0] = codigo[0] = bilhete[0] = tituloSala[0] = urlCelular[0] = 0;
  pedindoBilhete = 0;
  escolhendo = 0; abrirPedido = 0; fonteEscolhida = -1; salaDurConferida = 0; fonteVista = -1;
  memset(&prep, 0, sizeof prep);
  video_taxa_sync(1000);
  juntosui_sala_fechou();
}

// ---- rotas ---------------------------------------------------------------------------------
static void conectar(void) {
  char url[2048];
  printf("[juntos] websocket: endpoint=%d ticket=%d\n", !!wsBase[0], !!bilhete[0]);
  if (!wsBase[0] || !bilhete[0]) return;
  snprintf(url, sizeof url, "%s?b=%s", wsBase, bilhete);
  bilhete[0] = 0;   // uso unico
  ws = dws_abrir(url);
  printf("[juntos] websocket: handshake %s\n", ws ? "pending" : "allocation failed");
  if (fase != JF_AGUARDANDO) fase = fase == JF_DENTRO || fase == JF_RECONECTANDO ? JF_RECONECTANDO : JF_CONECTANDO;
}

static void pedirBilhete(void) {
  char corpo[96];
  if (pedindoBilhete || !salaId[0]) return;
  pedindoBilhete = 1;
  snprintf(corpo, sizeof corpo, "{\"id\":\"%s\"}", salaId);
  pedir(OP_ENTRAR, "/v1/sala/entrar", corpo, 0);
}

static void lerEnderecos(const char *t) {
  js_texto(t, NULL, "ws", wsBase, sizeof wsBase);
  js_texto(t, NULL, "web", webBase, sizeof webBase);
}

static void criarSala(void) {
  // Each input byte can expand to six JSON bytes (\\u00xx), plus delimiters.
  char imp[2048], tit[sizeof prep.titulo * 6 + 8], pos[sizeof prep.poster * 6 + 8];
  char conv[sizeof prep.conv * 6 + JUNTOS_CONVIDADOS_MAX * 3 + 8];
  char corpo[sizeof imp + sizeof tit + sizeof pos + sizeof conv + 256];
  int i, k = 0;
  const Stream *s = stream_item(stream_atual());
  JsImpressao ii;
  // Saved links and saved parts can play without an item in the current list.
  // Duration is still useful; unknown release metadata must not block a room.
  Stream desconhecida = {0};
  if (!s) { desconhecida.fileIdx = -1; s = &desconhecida; }
  jsf_impressao(s, video_duracao(), &ii);
  impSala = ii;
  if (jsp_impressao(imp, sizeof imp, &ii) < 0) {
    juntosui_erro("json invalido");
    return;
  }
  conv[0] = 0;
  for (i = 0; i < prep.n; i++) {
    char e[sizeof prep.conv[0] * 6 + 8];
    k += snprintf(conv + k, sizeof conv - (size_t)k, "%s\"%s\"", i ? "," : "", jsp_esc(e, sizeof e, prep.conv[i]));
    if (k < 0 || (size_t)k >= sizeof conv) {
      juntosui_erro("json invalido");
      return;
    }
  }
  jsp_esc(tit, sizeof tit, prep.titulo);
  jsp_esc(pos, sizeof pos, prep.poster);
  double p = video_pos();
  int len = snprintf(corpo, sizeof corpo,
           "{\"ep\":\"%s\",\"titulo\":\"%s\",\"poster\":\"%s\",\"imp\":%s,\"controle\":\"%s\",\"pos\":%.1f,\"convidados\":[%s]}",
           prep.ep, tit, pos, imp, prep.soAnf ? "anfitriao" : "todos",
           isfinite(p) ? fmax(0, fmin(86400, p)) : 0, conv);
  if (len < 0 || (size_t)len >= sizeof corpo) {
    juntosui_erro("json invalido");
    return;
  }
  snprintf(tituloSala, sizeof tituloSala, "%s", prep.titulo);
  fase = JF_CRIANDO;
  prep.enviados = prep.n;
  pedir(OP_CRIAR, "/v1/sala", corpo, 0);
  printf("[juntos] criando sala (%d convidado(s), fonte %s)\n", prep.n, ii.rotulo);
}

void juntos_preparar(const char *imdb, int t, int e, const char *titulo, const char *poster,
                     const char *const *conv, int n, int soAnf) {
  int i;
  if (!juntos_disponivel() || juntos_ativo()) return;
  // Validate before copying: never turn a truncated or foreign ID into an IMDb ID.
  const char *id = imdb ? imdb : "";
  size_t digits = !strncmp(id, "tt", 2) ? strspn(id + 2, "0123456789") : 0;
  const char *suffix = digits ? id + 2 + digits : id;
  int st = 0, se = 0;
  if (*suffix == ':') {
    size_t nt = strspn(suffix + 1, "0123456789");
    const char *next = suffix + 1 + nt;
    size_t ne = *next == ':' ? strspn(next + 1, "0123456789") : 0;
    if (!nt || nt > 4 || !ne || ne > 5 || next[1 + ne]) digits = 0;
    else sscanf(suffix + 1, "%d:%d", &st, &se);
  } else if (*suffix) digits = 0;
  if (!digits || digits > 10 || t < 0 || t > 9999 || e < 0 || e > 99999 || (t > 0 && !e)) {
    juntosui_erro("ep invalido");
    return;
  }
  if (!t && !e && *suffix) { t = st; e = se; }
  memset(&prep, 0, sizeof prep);
  snprintf(prep.imdb, sizeof prep.imdb, "%.*s", (int)(digits + 2), id);
  if (e > 0 || *suffix) snprintf(prep.ep, sizeof prep.ep, "%s:%d:%d", prep.imdb, t, e);
  else snprintf(prep.ep, sizeof prep.ep, "%s", prep.imdb);
  snprintf(prep.titulo, sizeof prep.titulo, "%s", titulo ? titulo : "");
  snprintf(prep.poster, sizeof prep.poster, "%s", poster ? poster : "");
  prep.t = t; prep.e = e; prep.soAnf = soAnf;
  for (i = 0; i < n && i < JUNTOS_CONVIDADOS_MAX; i++) snprintf(prep.conv[prep.n++], sizeof prep.conv[0], "%s", conv[i]);
  fase = JF_PREPARADO;
  snprintf(abrirImdb, sizeof abrirImdb, "%s", prep.imdb);
  snprintf(abrirTitulo, sizeof abrirTitulo, "%s", prep.titulo);
  snprintf(abrirPoster, sizeof abrirPoster, "%s", prep.poster);
  abrirT = t; abrirE = e; abrirPedido = 1;
  snprintf(tituloSala, sizeof tituloSala, "%s", prep.titulo);
  iniciarMotor();
  snprintf(motor.sala.ep, sizeof motor.sala.ep, "%s", prep.ep);
  snprintf(motor.sala.titulo, sizeof motor.sala.titulo, "%s", prep.titulo);
  snprintf(motor.sala.poster, sizeof motor.sala.poster, "%s", prep.poster);
  motor.sala.n = 1;
  snprintf(motor.sala.m[0].id, sizeof motor.sala.m[0].id, "%s", motor.eu);
  snprintf(motor.sala.m[0].nome, sizeof motor.sala.m[0].nome, "%s", recomenda_vinculo_nome());
  snprintf(motor.sala.m[0].st, sizeof motor.sala.m[0].st, "carregando");
  motor.sala.m[0].anf = motor.sala.m[0].on = 1;
  printf("[juntos] lobby open: preparing paused source\n");
}

int juntos_convidar(const char *const *conv, int n) {
  char corpo[1600], e[200];
  int i, k;
  // No server room yet (preparing, or POST /v1/sala in flight): queue; criarSala
  // or the OP_CRIAR reply sends them.
  if (!salaId[0] && (fase == JF_PREPARADO || fase == JF_CRIANDO)) {
    if (prep.n + n > JUNTOS_CONVIDADOS_MAX) return 0;
    for (i = 0; i < n; i++) snprintf(prep.conv[prep.n++], sizeof prep.conv[0], "%s", conv[i]);
    printf("[juntos] lobby: %d invite(s) queued until the room opens\n", n);
    return 1;
  }
  if (!salaId[0] || !n) return 0;
  printf("[juntos] lobby: inviting %d friend(s)\n", n);
  k = snprintf(corpo, sizeof corpo, "{\"id\":\"%s\",\"convidados\":[", salaId);
  for (i = 0; i < n && i < JUNTOS_CONVIDADOS_MAX && (size_t)k < sizeof corpo - 200; i++)
    k += snprintf(corpo + k, sizeof corpo - (size_t)k, "%s\"%s\"", i ? "," : "", jsp_esc(e, sizeof e, conv[i]));
  snprintf(corpo + k, sizeof corpo - (size_t)k, "]}");
  pedir(OP_CONVIDAR, "/v1/sala/convidar", corpo, 0);
  return 1;
}

void juntos_entrar_id(const char *id) {
  char corpo[96];
  if (!juntos_disponivel() || !id || strlen(id) != 32) return;
  if (juntos_ativo()) juntos_sair();
  snprintf(salaId, sizeof salaId, "%s", id);
  snprintf(corpo, sizeof corpo, "{\"id\":\"%s\"}", id);
  fase = JF_ENTRANDO;
  pedindoBilhete = 1;
  pedir(OP_ENTRAR, "/v1/sala/entrar", corpo, 0);
}

void juntos_entrar_codigo(const char *cod) {
  char corpo[64], e[32];
  if (!juntos_disponivel() || !cod) return;
  if (juntos_ativo()) juntos_sair();
  snprintf(corpo, sizeof corpo, "{\"codigo\":\"%s\"}", jsp_esc(e, sizeof e, cod));
  fase = JF_ENTRANDO;
  pedindoBilhete = 1;
  pedir(OP_ENTRAR, "/v1/sala/entrar", corpo, 0);
}

void juntos_sair(void) {
  char corpo[96];
  if (!juntos_ativo()) return;
  if (temMotor && ws && dws_estado(ws) == DWS_ABERTO) jsm_sair(&motor);
  // O anfitriao que sai leva a sala junto no D1 (melhor esforco; a expiracao cobre).
  if (salaId[0] && temMotor && jsm_sou_anfitriao(&motor) && motor.sala.n <= 1) {
    snprintf(corpo, sizeof corpo, "{\"id\":\"%s\"}", salaId);
    pedir(OP_FIM, "/v1/sala/fim", corpo, 0);
  }
  printf("[juntos] saiu da sala\n");
  zerar();
}

int juntos_precarregando(void) {
  return juntos_ativo() && (!temMotor || !motor.temSala ||
    motor.sala.rel.estado == JSE_LOBBY ||
    (motor.playArmado && motor.sala.rel.estado == JSE_TOCANDO &&
     agoraLocal() < jsr_local(&motor.rel, motor.sala.rel.t0)));
}

int juntos_comecar(void) {
  if (!temMotor || !jsm_sou_anfitriao(&motor) ||
      motor.sala.rel.estado != JSE_LOBBY) return -1;
  if (!pPronto(NULL)) {
    printf("[juntos] start: waiting for paused source readiness\n");
    return -1;
  }
  if (salaFalhou || !motor.temSala || !motor.conectado) {
    printf("[juntos] start: room unavailable, starting alone\n");
    if (!salaFalhou) juntosui_erro("rede");
    juntos_sair(); // invalidates in-flight room replies before releasing playback
    player_juntos_pausar(0);
    return 1;
  }
  printf("[juntos] start: source ready, sending cinema countdown\n");
  return jsm_cmd(&motor, "comecar", -1, NULL, agoraLocal());
}
int juntos_encerrar(void) {
  char corpo[96];
  if (!temMotor || !jsm_sou_anfitriao(&motor)) return 0;
  jsm_cmd(&motor, "encerrar", -1, NULL, agoraLocal());
  snprintf(corpo, sizeof corpo, "{\"id\":\"%s\"}", salaId);
  pedir(OP_FIM, "/v1/sala/fim", corpo, 0);
  return 1;
}
int juntos_controle_anfitriao(int so) { return temMotor ? jsm_controle(&motor, so) : -1; }

void juntos_pedir_celular(void) {
  urlCelular[0] = 0;
  // Perfil infantil: chat livre desligado (o responsavel liga em outro lugar).
  if (temMotor) jsm_pedir_celular(&motor, perfil_infantil_ativo() ? 0 : 1);
}

void juntos_perfil_trocado(void) {
  // A pessoa e o perfil: trocar de perfil e sair da sala (plano 9).
  if (juntos_ativo()) juntos_sair();
  gen++;
  nVistos = 0;
  proxConvites = 0;
}

// ---- convites ---------------------------------------------------------------------------
static int visto(const char *id) {
  int i;
  for (i = 0; i < nVistos; i++) if (!strcmp(vistos[i], id)) return 1;
  if (nVistos == 16) { memmove(vistos[0], vistos[1], sizeof vistos[0] * 15); nVistos--; }
  snprintf(vistos[nVistos++], sizeof vistos[0], "%s", id);
  return 0;
}

static void lerConvites(const char *t) {
  const char *a = js_array(t, NULL, "convites");
  while (a && *a == '{') {
    const char *f = js_fim(a), *de;
    JuntosConvite c;
    memset(&c, 0, sizeof c);
    js_texto_raiz_em(a, f, "id", c.id, sizeof c.id);
    js_texto_raiz_em(a, f, "ep", c.ep, sizeof c.ep);
    js_texto_raiz_em(a, f, "titulo", c.titulo, sizeof c.titulo);
    js_texto_raiz_em(a, f, "poster", c.poster, sizeof c.poster);
    jsp_ler_impressao(a, f, &c.imp);
    de = strstr(a, "\"de\":{");
    if (de && de < f) {
      const char *df = js_fim(de + 5);
      js_texto(de, df, "id", c.deId, sizeof c.deId);
      js_texto(de, df, "nome", c.deNome, sizeof c.deNome);
      js_texto(de, df, "av", c.deAv, sizeof c.deAv);
    }
    if (c.id[0] && !visto(c.id)) juntosui_convite(&c);
    a = js_prox(f);
  }
}

void juntos_recusar(const char *id) {
  char corpo[96];
  snprintf(corpo, sizeof corpo, "{\"id\":\"%s\"}", id);
  pedir(OP_RECUSAR, "/v1/sala/recusar", corpo, 0);
}

void juntos_silenciar(const char *de) {
  char corpo[256], e[200];
  snprintf(corpo, sizeof corpo, "{\"de\":\"%s\"}", jsp_esc(e, sizeof e, de));
  pedir(OP_SILENCIAR, "/v1/sala/silenciar", corpo, 0);
}

// ---- respostas do fio ----------------------------------------------------------------------

static void pedirAbrir(const char *ep, const char *titulo, const char *poster) {
  separarEpJ(ep, abrirImdb, sizeof abrirImdb, &abrirT, &abrirE);
  snprintf(abrirTitulo, sizeof abrirTitulo, "%s", titulo ? titulo : "");
  snprintf(abrirPoster, sizeof abrirPoster, "%s", poster ? poster : "");
  abrirPedido = 1;
  escolhendo = 1;
  fonteEscolhida = fonteVista = -1;
  salaDurConferida = 0;
}

static void resposta(Resp *r) {
  int ok = r->txt && r->status >= 200 && r->status < 300;
  switch (r->tipo) {
    case OP_CRIAR:
      if (!ok) {
        char erro[64] = "";
        if (r->txt) js_texto(r->txt, NULL, "erro", erro, sizeof erro);
        // Only server-defined reasons: never echo arbitrary response text or credentials.
        const char *motivo = NULL;
        const char *seguros[] = {"ep invalido", "json invalido", "limite", "infantil",
          "juntos indisponivel", "tente de novo"};
        for (size_t i = 0; i < sizeof seguros / sizeof *seguros; i++)
          if (!strcmp(erro, seguros[i])) motivo = seguros[i];
        printf("[juntos] criar sala falhou (%d%s%s)\n", r->status, motivo ? ": " : "", motivo ? motivo : "");
        salaFalhou = r->status == -1 || r->status == 401 || r->status == 403 ? 2 : 1;
        juntosui_erro(motivo ? motivo : salaFalhou == 2 ? "auth" : r->status == 429 ? "limite" : "rede");
        repetirSalaEm = agoraPasso + 10000u; // Retain solo Start while auth/network recovers.
        fase = JF_PREPARADO;
        break;
      }
      js_texto(r->txt, NULL, "id", salaId, sizeof salaId);
      salaFalhou = 0;
      js_texto(r->txt, NULL, "codigo", codigo, sizeof codigo);
      js_texto(r->txt, NULL, "bilhete", bilhete, sizeof bilhete);
      lerEnderecos(r->txt);
      if (prep.n > prep.enviados) {   // invited while the room was being created
        const char *mais[JUNTOS_CONVIDADOS_MAX];
        for (int i = prep.enviados; i < prep.n; i++) mais[i - prep.enviados] = prep.conv[i];
        juntos_convidar(mais, prep.n - prep.enviados);
        prep.enviados = prep.n;
      }
      // Keep the prepared host until the first WebSocket room snapshot.
      jsm_fonte(&motor, "mesma");
      jsm_estado_app(&motor, "carregando", 0);
      conectar();
      juntosui_sala_abriu();
      printf("[juntos] room: created, invite code ready\n");
      break;
    case OP_ENTRAR: {
      pedindoBilhete = 0;
      if (!ok) {
        printf("[juntos] entrar falhou (%d)\n", r->status);
        if (fase == JF_ENTRANDO) {
          juntosui_erro(r->status == -1 || r->status == 401 ? "auth" : r->status == 404 ? "sem_sala" : r->status == 403 ? "recusado" : r->status == 429 ? "limite" : "rede");
          zerar();
        }
        break;
      }
      js_texto(r->txt, NULL, "bilhete", bilhete, sizeof bilhete);
      lerEnderecos(r->txt);
      if (fase == JF_ENTRANDO) {
        const char *res = strstr(r->txt, "\"resumo\":{");
        char ep[64] = "", tit[160] = "", pos[512] = "";
        js_texto(r->txt, NULL, "id", salaId, sizeof salaId);
        if (res) {
          const char *rf = js_fim(res + 9);
          js_texto_raiz_em(res + 9, rf, "ep", ep, sizeof ep);
          js_texto_raiz_em(res + 9, rf, "titulo", tit, sizeof tit);
          js_texto_raiz_em(res + 9, rf, "poster", pos, sizeof pos);
          jsp_ler_impressao(res + 9, rf, &impSala);
        }
        snprintf(tituloSala, sizeof tituloSala, "%s", tit);
        iniciarMotor();
        jsm_estado_app(&motor, "procurando", 0);
        if ((int)js_num(r->txt, NULL, "pend", 0)) fase = JF_AGUARDANDO;
        else if (ep[0]) pedirAbrir(ep, tit, pos);
        juntosui_sala_abriu();
      }
      conectar();
      break;
    }
    case OP_CONVIDAR:
      printf("[juntos] invite: request completed status=%d\n", r->status);
      break;
    case OP_CONVITES:
      buscandoConvites = 0;
      if (ok && !juntos_ativo()) lerConvites(r->txt);
      break;
    default: break;
  }
}

// ---- eventos do motor -----------------------------------------------------------------
static void eventos(void) {
  JsEvento e;
  while (jsm_evento(&motor, &e)) {
    switch (e.tipo) {
      case JSV_EPISODIO: {
        snprintf(tituloSala, sizeof tituloSala, "%s", motor.sala.titulo);
        impSala = motor.sala.imp;
        juntosui_sala_abriu();
        // A different movie can also have episode 0/0: compare room identity,
        // already checked by the motor, rather than just season/episode.
        pedirAbrir(e.txt, tituloSala, motor.sala.poster);
        escolhendo = 0;
        break;
      }
      case JSV_ACEITO:
        // O anfitriao aceitou: o socket pendente foi fechado de proposito;
        // um bilhete novo entra direto.
        fase = JF_ENTRANDO;
        fecharWs();
        pedirBilhete();
        break;
      case JSV_CELULAR:
        if (webBase[0] && salaId[0]) snprintf(urlCelular, sizeof urlCelular, "%s%s%s", webBase, salaId, e.k);
        break;
      case JSV_FIM:
        juntosui_evento(&e);
        zerar();
        return;
      case JSV_ERRO:
        if (!strcmp(e.k, "fim") || !strcmp(e.k, "cheia") || !strcmp(e.k, "sala_fechada")) {
          juntosui_erro(e.k);
          zerar();
          return;
        }
        break;
      default: break;
    }
    juntosui_evento(&e);
  }
}

static void websocket(Uint32 agora) {
  char *m;
  if (!ws) {
    if ((fase == JF_RECONECTANDO || fase == JF_CONECTANDO) && !pedindoBilhete && agora >= proxConexao) {
      if (caiuEm && agora - caiuEm > RECONECTA_TETO) {
        printf("[juntos] fora da sala ha mais de 60 s: saindo\n");
        juntosui_erro("caiu");
        zerar();
        return;
      }
      if (bilhete[0]) conectar(); else pedirBilhete();
    }
    return;
  }
  if (dws_estado(ws) == DWS_ABERTO && !motor.conectado) {
    jsm_conectou(&motor, agoraLocal());
    if (fase != JF_AGUARDANDO) fase = JF_DENTRO;
    if (!entrouEm) entrouEm = agora ? agora : 1;
    caiuEm = 0;
    atrasoConexao = 0;
    printf("[juntos] websocket: open\n");
  }
  while ((m = dws_receber(ws))) {
    jsm_receber(&motor, m, agoraLocal());
    free(m);
  }
  if (dws_estado(ws) == DWS_CAIU) {
    int cod = dws_codigo_fechamento(ws);
    fecharWs();
    if (cod == 4100) return;   // aceito: JSV_ACEITO ja pediu bilhete novo
    if (cod == 4403 || cod == 4409 || cod == 4410 || cod == 4404) {
      juntosui_erro(cod == 4409 ? "cheia" : cod == 4410 ? "fim" : cod == 4404 ? "sem_sala" : "recusado");
      zerar();
      return;
    }
    // Reconexao exponencial (1, 2, 4... 30 s). O video segue pelo relogio
    // conhecido enquanto isso (plano 6.7).
    if (!caiuEm) caiuEm = agora ? agora : 1;
    atrasoConexao = atrasoConexao ? (atrasoConexao * 2 > RECONECTA_MAX_MS ? RECONECTA_MAX_MS : atrasoConexao * 2) : 1000;
    proxConexao = agora + atrasoConexao;
    fase = JF_RECONECTANDO;
    printf("[juntos] caiu (codigo %d); de novo em %u ms\n", cod, atrasoConexao);
  }
}

void juntos_passo(Uint32 agora) {
  if (!NV_REC_URL[0]) return;
  agoraPasso = agora;
  if (!iniciouEm) iniciouEm = agora ? agora : 1;
  if (mtx) {
    Resp lote[FILA];
    int n, i;
    SDL_LockMutex(mtx);
    n = nCaixa;
    memcpy(lote, caixa, sizeof lote[0] * (size_t)n);
    nCaixa = 0;
    SDL_UnlockMutex(mtx);
    for (i = 0; i < n; i++) {
      if (lote[i].gen == gen) resposta(&lote[i]);
      else if (lote[i].tipo == OP_CONVITES) buscandoConvites = 0;
      free(lote[i].txt);
    }
  }
  if (fase == JF_PREPARADO) {
    static Uint32 ultimoPreparo;
    if (agora - ultimoPreparo >= 5000u) {
      ultimoPreparo = agora;
      printf("[juntos] source: waiting open=%d player=%d source=%d ready=%d duration=%d error=%d\n",
             abrirPedido, player_aberto(), player_tem_video(), player_juntos_preparado(),
             video_duracao() > 0, video_falhou());
    }
  }
  // Retry through the worker: Nuvio refresh can block; Trakt refresh is
  // applied by traktauth_passo on the main loop, as for the social worker.
  // Host creates the room once the paused preparation has a duration.
  if (fase == JF_PREPARADO && (!salaFalhou || (int)(agora - repetirSalaEm) >= 0) &&
      !abrirPedido && pPronto(NULL))
    criarSala();
  // Convites: so fora de sala, depois de o app assentar.
  if (!juntos_ativo() && juntos_disponivel() && !buscandoConvites && agora - iniciouEm > CONVITES_ESPERA &&
      (int)(agora - proxConvites) >= 0) {
    buscandoConvites = 1;
    proxConvites = agora + (player_aberto() ? CONVITES_PLR_MS : CONVITES_MS);
    pedir(OP_CONVITES, "/v1/sala/convites", NULL, 1);
  }
  if (!temMotor) return;
  websocket(agora);
  if (!temMotor) return;
  // The room owns time; personal speed stays at 1x. Sync uses its own rate.
  if (video_velocidade_atual() != 100) video_velocidade(100);
  // A retry or manual source change needs a fresh duration verdict.
  if (video_pronto() && stream_atual() != fonteVista) {
    fonteVista = stream_atual();
    fonteEscolhida = fonteVista;
    salaDurConferida = 0;
  }
  // Duracao conferida depois do load: a fonte do convidado pode ser outra versao.
  if (!salaDurConferida && fonteEscolhida >= 0 && video_pronto() && video_duracao() > 0) {
    int pal = 0;
    JsVeredito v = jsf_duracao(veredito, impSala.durMs, video_duracao(), &pal);
    salaDurConferida = 1;
    if (v != veredito) { veredito = v; jsm_fonte(&motor, jsf_nome(v)); juntosui_fonte_diferente(v, pal); }
  }
  if (escolhendo && !player_aberto()) jsm_estado_app(&motor, "procurando", 0);
  else if (player_aberto()) {
    VideoStats stats;
    const Stream *s = stream_item(stream_atual());
    motor.altura = s ? s->altura : 0;
    video_estatisticas(&stats);
    int pct = stats.bufferPct;
    if (pct < 0 && stats.bufferSeg >= 0 && stats.bufferAlvoSeg > 0)
      pct = (int)fmin(100, 100 * stats.bufferSeg / stats.bufferAlvoSeg);
    if (salaFalhou) pct = 0;
    jsm_estado_app(&motor, salaFalhou || video_falhou() ? "erro" : "carregando", pct > 0 ? pct : 0);
    if (!motor.temSala && motor.sala.n) {
      motor.sala.m[0].pct = pct > 0 ? pct : 0;
      snprintf(motor.sala.m[0].st, sizeof motor.sala.m[0].st, "%s", salaFalhou ? "erro" : "carregando");
    }
  }
  // The backend retains the previous playback error until a new source loads.
  // Island updates run before app.c consumes abrirPedido: that error is not ours.
  if (fase == JF_PREPARADO && !abrirPedido && player_tem_video() && video_falhou()) {
    printf("[juntos] source: paused preparation failed\n");
    juntosui_erro("rede");
    zerar();
    return;
  }
  jsm_passo(&motor, agoraLocal());
  if (motor.sala.n > maxMembros) maxMembros = motor.sala.n;
  eventos();
}

// ---- app.c -----------------------------------------------------------------------------------
int juntos_pediu_abrir(char *imdb, size_t ni, int *t, int *e, char *titulo, size_t nt, char *poster, size_t np) {
  if (!abrirPedido) return 0;
  abrirPedido = 0;
  printf("[juntos] source: open request handed to player\n");
  snprintf(imdb, ni, "%s", abrirImdb);
  *t = abrirT; *e = abrirE;
  snprintf(titulo, nt, "%s", abrirTitulo);
  snprintf(poster, np, "%s", abrirPoster);
  return 1;
}

int juntos_escolhendo_fonte(void) { return escolhendo && temMotor; }

int juntos_escolher_fonte(const Stream *lista, int n, Uint32 agora, Uint32 desde) {
  int i = -1;
  JsVeredito v;
  if (!escolhendo || !temMotor) return -1;
  v = jsf_casar(&impSala, lista, n, NULL, &i);
  // A mesma fonte decide na hora; sem ela, espera os outros addons um pouco.
  if (v != JSF_MESMA && agora - desde < FONTE_ESPERA_MS) return -1;
  escolhendo = 0;
  veredito = v;
  fonteEscolhida = i;
  if (i < 0) { jsm_estado_app(&motor, "semfonte", 0); juntosui_fonte_diferente(JSF_NENHUMA, 0); return -1; }
  jsm_estado_app(&motor, "carregando", 0);
  jsm_fonte(&motor, jsf_nome(v));
  printf("[juntos] fonte da sala: %s (indice %d)\n", jsf_nome(v), i);
  return i;
}

void juntos_preferir_reserva(void) {
  int best = -1, n = stream_n();
  const Stream *base = stream_item(0);
  unsigned char *excluded;
  if (!temMotor || !motor.temSala || n <= 0 || !base || stream_item(n - 1) != base + n - 1) return;
  excluded = malloc((size_t)n);
  if (!excluded) return;
  for (int i = 0; i < n; i++) excluded[i] = (unsigned char)stream_automatico_excluida(i);
  jsf_casar(&impSala, base, n, excluded, &best);
  free(excluded);
  stream_preferir(best);
  salaDurConferida = 0;
}

// ---- player.c -------------------------------------------------------------------------------
static void travado(void) {
  const JsMembro *a = NULL;
  int i;
  char t[200];
  for (i = 0; i < motor.sala.n; i++) if (motor.sala.m[i].anf) a = &motor.sala.m[i];
  snprintf(t, sizeof t, i18n("Só %s controla nesta sessão"), a ? a->nome : "");
  player_toast_ex(t, 3500, "aj_users", 1);
}

int juntos_ao_alternar(int tocandoAgora) {
  int r;
  if (juntos_precarregando() && (!temMotor || !motor.temSala)) return 1;
  if (!temMotor || !motor.temSala) return 0;
  if (motor.sala.rel.estado == JSE_LOBBY) { juntosui_sala_abriu(); return 1; }
  else r = jsm_cmd(&motor, tocandoAgora ? "pausa" : "play", -1, NULL, agoraLocal());
  if (r == 0) travado();
  return 1;
}

int juntos_ao_buscar(double seg) {
  int r;
  if (!temMotor || !motor.temSala) return 0;
  if (juntos_precarregando()) return 1;
  r = jsm_cmd(&motor, "seek", seg, NULL, agoraLocal());
  if (r == 0) travado();
  return 1;
}

int juntos_ao_proximo(const char *alvo) {
  if (!temMotor || !motor.temSala) return 0;
  if (jsm_cmd(&motor, "proximo", -1, alvo, agoraLocal()) == 0) travado();
  return 1;   // Wait for the authoritative room update, including after send failure.
}

void juntos_escolher_titulo(const char *imdb, int t, int e, const char *titulo, const char *poster) {
  char alvo[64], j[2048], tit[400], art[1100];
  if (!temMotor || !motor.temSala || !jsm_pode_controlar(&motor) || !imdb ||
      strncmp(imdb, "tt", 2) || strspn(imdb + 2, "0123456789") != strlen(imdb + 2)) return;
  if (t > 0 && e > 0) snprintf(alvo, sizeof alvo, "%s:%d:%d", imdb, t, e);
  else snprintf(alvo, sizeof alvo, "%s", imdb);
  if (!strcmp(alvo, motor.sala.ep)) return;
  snprintf(j, sizeof j, "{\"t\":\"cmd\",\"op\":\"proximo\",\"seqBase\":%d,\"ep\":\"%s\",\"titulo\":\"%s\",\"poster\":\"%s\"}",
           motor.sala.seq, alvo, jsp_esc(tit, sizeof tit, titulo), jsp_esc(art, sizeof art, poster));
  enviarWs(NULL, j);
}

int juntos_ao_voltar(void) {
  if (!juntos_ativo()) return 0;
  juntosui_perguntar_sair();
  return 1;
}

void juntos_acao(const char *chave, int botao) { juntosui_acao(chave, botao); }
