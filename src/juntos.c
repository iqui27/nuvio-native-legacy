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
#include "video.h"
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
    char aut[400], via[40], pf[40], url[400];
    SDL_LockMutex(mtx);
    while (!nFila) SDL_CondWait(cond, mtx);
    op = fila[0];
    memmove(&fila[0], &fila[1], sizeof fila[0] * (size_t)(--nFila));
    SDL_UnlockMutex(mtx);
    memset(&r, 0, sizeof r);
    r.tipo = op.tipo; r.gen = op.gen;
    if (recomenda_cabecalhos(cab, aut, sizeof aut, via, sizeof via, pf, sizeof pf)) {
      snprintf(url, sizeof url, "%s%s", NV_REC_URL, op.rota);
      r.txt = op.get ? rede_baixar_st(url, PRAZO_S, cab, &r.status)
                     : rede_postar_st(url, PRAZO_S, cab, op.corpo ? op.corpo : "{}", &r.status);
    }
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
  if (!NV_REC_URL[0]) return;
  if (!mtx) { mtx = SDL_CreateMutex(); cond = SDL_CreateCond(); }
  if (!mtx || !cond) return;
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
  if (nFila == FILA) { SDL_UnlockMutex(mtx); free(op.corpo); return; }
  fila[nFila++] = op;
  if (!fioVivo) {
    SDL_Thread *t = SDL_CreateThread(fioLaco, "nv-juntos", NULL);
    if (t) { SDL_DetachThread(t); fioVivo = 1; }
  }
  SDL_CondSignal(cond);
  SDL_UnlockMutex(mtx);
}

// ---- estado do fio principal ----------------------------------------------------------
static JuntosFase fase;
static JsMotor motor;
static int temMotor;
static DiscordWs *ws;
static char salaId[40], codigo[12], wsBase[256], webBase[256], bilhete[1600], tituloSala[160];
static Uint32 caiuEm, proxConexao, atrasoConexao;
static int pedindoBilhete;
// anfitriao preparado
static struct {
  char imdb[24], titulo[160], poster[512];
  int t, e, soAnf, n;
  char conv[JUNTOS_CONVIDADOS_MAX][96];
} prep;
// abrir o titulo (convidado / proximo episodio)
static int abrirPedido, escolhendo;
static char abrirImdb[24], abrirTitulo[160], abrirPoster[512];
static int abrirT, abrirE;
static JsImpressao impSala;
static JsVeredito veredito;
static int fonteEscolhida = -1, salaDurConferida;
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
const char *juntos_url_celular(void) { return urlCelular; }
int juntos_sou_anfitriao(void) { return temMotor && jsm_sou_anfitriao(&motor); }
int juntos_pode_controlar(void) { return !temMotor || jsm_pode_controlar(&motor); }

static double agoraLocal(void) { return (double)SDL_GetTicks(); }

static void separarEpJ(const char *ep, char *imdb, size_t n, int *t, int *e) {
  const char *p = strchr(ep, ':');
  *t = *e = 0;
  if (!p) { snprintf(imdb, n, "%s", ep); return; }
  snprintf(imdb, n, "%.*s", (int)(p - ep), ep);
  sscanf(p + 1, "%d:%d", t, e);
}

// ---- o player de verdade, para o motor -------------------------------------------------
// A amostra de posicao: o backend nao diz quando leu (o LG le a cada ~200 ms),
// entao "quando" e o primeiro quadro em que o valor mudou.
static double posUlt = -1, posEm;
static double pPos(void *u, double *t) {
  double p = video_pos();
  (void)u;
  if (p != posUlt) { posUlt = p; posEm = agoraLocal(); }
  *t = posEm;
  return p;
}
static int pTocando(void *u) { (void)u; return video_tocando() && !player_pausado(); }
static int pPronto(void *u) { (void)u; return player_aberto() && video_pronto() && video_duracao() > 0; }
static unsigned pBuffer(void *u) { (void)u; return video_bufferando_ms(); }
static double pDur(void *u) { (void)u; return video_duracao(); }
static void pPausar(void *u, int p) { (void)u; player_juntos_pausar(p); }
static void pBuscar(void *u, double s) { (void)u; player_juntos_buscar(s); }
static void pVel(void *u, int c) { (void)u; if (video_velocidade_suportada()) video_velocidade(c); }

static int enviarWs(void *u, const char *j) { (void)u; return ws ? dws_enviar(ws, j) : -1; }

static char nivelDaTv(void) {
  // Nivel A so onde a plataforma diz que muda a velocidade E nao esta travada
  // (passthrough no Android). A prova de verdade vem do motor (VelMedidor).
  return video_velocidade_suportada() && !video_velocidade_bloqueada() ? 'A' : 'B';
}

static void iniciarMotor(void) {
  JsPlayer jp = { pPos, pTocando, pPronto, pBuffer, pDur, pPausar, pBuscar, pVel, NULL };
  jsm_iniciar(&motor, recomenda_meu_id(), PLAT, "", nivelDaTv(), &jp, enviarWs, NULL);
  temMotor = 1;
  if (fonteEscolhida >= 0) jsm_fonte(&motor, jsf_nome(veredito));
}

static void fecharWs(void) {
  if (ws) { dws_fechar(ws); ws = NULL; }
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
  temMotor = 0;
  salaId[0] = codigo[0] = bilhete[0] = tituloSala[0] = urlCelular[0] = 0;
  pedindoBilhete = 0;
  escolhendo = 0; abrirPedido = 0; fonteEscolhida = -1; salaDurConferida = 0;
  memset(&prep, 0, sizeof prep);
  if (video_velocidade_suportada()) video_velocidade(100);
  juntosui_sala_fechou();
}

// ---- rotas ---------------------------------------------------------------------------------
static void conectar(void) {
  char url[2048];
  if (!wsBase[0] || !bilhete[0]) return;
  snprintf(url, sizeof url, "%s?b=%s", wsBase, bilhete);
  bilhete[0] = 0;   // uso unico
  ws = dws_abrir(url);
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
  char corpo[4096], imp[1024], ep[48], tit[340], pos[400], conv[1400];
  int i, k = 0;
  const Stream *s = stream_item(stream_atual());
  JsImpressao ii;
  if (!s) return;
  jsf_impressao(s, video_duracao(), &ii);
  impSala = ii;
  jsp_impressao(imp, sizeof imp, &ii);
  if (prep.t > 0 && prep.e > 0) snprintf(ep, sizeof ep, "%s:%d:%d", prep.imdb, prep.t, prep.e);
  else snprintf(ep, sizeof ep, "%s", prep.imdb);
  conv[0] = 0;
  for (i = 0; i < prep.n; i++) {
    char e[200];
    k += snprintf(conv + k, sizeof conv - (size_t)k, "%s\"%s\"", i ? "," : "", jsp_esc(e, sizeof e, prep.conv[i]));
    if ((size_t)k >= sizeof conv) break;
  }
  jsp_esc(tit, sizeof tit, prep.titulo);
  jsp_esc(pos, sizeof pos, prep.poster);
  snprintf(corpo, sizeof corpo,
           "{\"ep\":\"%s\",\"titulo\":\"%s\",\"poster\":\"%s\",\"imp\":%s,\"controle\":\"%s\",\"pos\":%.1f,\"convidados\":[%s]}",
           ep, tit, pos, imp, prep.soAnf ? "anfitriao" : "todos", video_pos(), conv);
  snprintf(tituloSala, sizeof tituloSala, "%s", prep.titulo);
  fase = JF_CRIANDO;
  pedir(OP_CRIAR, "/v1/sala", corpo, 0);
  printf("[juntos] criando sala (%d convidado(s), fonte %s)\n", prep.n, ii.rotulo);
}

void juntos_preparar(const char *imdb, int t, int e, const char *titulo, const char *poster,
                     const char *const *conv, int n, int soAnf) {
  int i;
  if (!juntos_disponivel() || juntos_ativo()) return;
  memset(&prep, 0, sizeof prep);
  snprintf(prep.imdb, sizeof prep.imdb, "%s", imdb ? imdb : "");
  snprintf(prep.titulo, sizeof prep.titulo, "%s", titulo ? titulo : "");
  snprintf(prep.poster, sizeof prep.poster, "%s", poster ? poster : "");
  prep.t = t; prep.e = e; prep.soAnf = soAnf;
  for (i = 0; i < n && i < JUNTOS_CONVIDADOS_MAX; i++) snprintf(prep.conv[prep.n++], sizeof prep.conv[0], "%s", conv[i]);
  fase = JF_PREPARADO;
  printf("[juntos] sala preparada: nasce quando a fonte tocar\n");
}

void juntos_convidar(const char *const *conv, int n) {
  char corpo[1600], e[200];
  int i, k;
  if (!salaId[0] || !n) return;
  k = snprintf(corpo, sizeof corpo, "{\"id\":\"%s\",\"convidados\":[", salaId);
  for (i = 0; i < n && i < JUNTOS_CONVIDADOS_MAX && (size_t)k < sizeof corpo - 200; i++)
    k += snprintf(corpo + k, sizeof corpo - (size_t)k, "%s\"%s\"", i ? "," : "", jsp_esc(e, sizeof e, conv[i]));
  snprintf(corpo + k, sizeof corpo - (size_t)k, "]}");
  pedir(OP_CONVIDAR, "/v1/sala/convidar", corpo, 0);
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

int juntos_comecar(void) {
  if (!temMotor) return -1;
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
  fonteEscolhida = -1;
  salaDurConferida = 0;
}

static void resposta(Resp *r) {
  int ok = r->txt && r->status >= 200 && r->status < 300;
  switch (r->tipo) {
    case OP_CRIAR:
      if (!ok) {
        printf("[juntos] criar sala falhou (%d)\n", r->status);
        juntosui_erro(r->status == 429 ? "limite" : "rede");
        zerar();
        break;
      }
      js_texto(r->txt, NULL, "id", salaId, sizeof salaId);
      js_texto(r->txt, NULL, "codigo", codigo, sizeof codigo);
      js_texto(r->txt, NULL, "bilhete", bilhete, sizeof bilhete);
      lerEnderecos(r->txt);
      iniciarMotor();
      jsm_fonte(&motor, "mesma");
      jsm_estado_app(&motor, "carregando", 0);
      conectar();
      juntosui_sala_abriu();
      printf("[juntos] sala aberta, codigo %s\n", codigo);
      break;
    case OP_ENTRAR: {
      pedindoBilhete = 0;
      if (!ok) {
        printf("[juntos] entrar falhou (%d)\n", r->status);
        if (fase == JF_ENTRANDO) {
          juntosui_erro(r->status == 404 ? "sem_sala" : r->status == 403 ? "recusado" : r->status == 429 ? "limite" : "rede");
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
        // A sala trocou de episodio: todos abrem o mesmo, cada um com a fonte
        // lembrada para a serie (fontepref/bingeGroup), nao a impressao velha.
        char imdb[24]; int t, ep;
        separarEpJ(e.txt, imdb, sizeof imdb, &t, &ep);
        { int pt = 0, pe = 0; player_episodio_atual(&pt, &pe);
          if (!(player_aberto() && pt == t && pe == ep)) { pedirAbrir(e.txt, tituloSala, ""); escolhendo = 0; } }
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
    printf("[juntos] conectado a sala\n");
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
  // Anfitriao: a sala nasce com a fonte tocando (a duracao entra na impressao).
  if (fase == JF_PREPARADO && player_aberto() && video_pronto() && video_duracao() > 0 && stream_atual() >= 0)
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
  // Duracao conferida depois do load: a fonte do convidado pode ser outra versao.
  if (!salaDurConferida && fonteEscolhida >= 0 && video_pronto() && video_duracao() > 0) {
    int pal = 0;
    JsVeredito v = jsf_duracao(veredito, impSala.durMs, video_duracao(), &pal);
    salaDurConferida = 1;
    if (v != veredito) { veredito = v; jsm_fonte(&motor, jsf_nome(v)); juntosui_fonte_diferente(v, pal); }
  }
  if (escolhendo && !player_aberto()) jsm_estado_app(&motor, "procurando", 0);
  jsm_passo(&motor, agoraLocal());
  if (motor.sala.n > maxMembros) maxMembros = motor.sala.n;
  eventos();
}

// ---- app.c -----------------------------------------------------------------------------------
int juntos_pediu_abrir(char *imdb, size_t ni, int *t, int *e, char *titulo, size_t nt, char *poster, size_t np) {
  if (!abrirPedido) return 0;
  abrirPedido = 0;
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
  if (!temMotor || !motor.temSala) return 0;
  if (motor.sala.rel.estado == JSE_LOBBY) r = jsm_cmd(&motor, "comecar", -1, NULL, agoraLocal());
  else r = jsm_cmd(&motor, tocandoAgora ? "pausa" : "play", -1, NULL, agoraLocal());
  if (r == 0) travado();
  return 1;
}

int juntos_ao_buscar(double seg) {
  int r;
  if (!temMotor || !motor.temSala) return 0;
  r = jsm_cmd(&motor, "seek", seg, NULL, agoraLocal());
  if (r == 0) travado();
  return 1;
}

int juntos_ao_proximo(const char *alvo) {
  if (!temMotor || !motor.temSala) return 0;
  if (jsm_cmd(&motor, "proximo", -1, alvo, agoraLocal()) == 0) { travado(); return 1; }
  return 0;   // o app abre o proximo como sempre; os outros vem pela sala
}

int juntos_ao_voltar(void) {
  if (!juntos_ativo()) return 0;
  juntosui_perguntar_sair();
  return 1;
}

void juntos_acao(const char *chave, int botao) { juntosui_acao(chave, botao); }
