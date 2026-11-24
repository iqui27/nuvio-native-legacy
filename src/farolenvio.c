// Envio do registro do Farol — ver farolenvio.h.
// Sem SDL, sem _Thread_local (NV_TPK40), sem log de token/URL/corpo.
#include "farolenvio.h"
#include "dados.h"
#include "farol.h"
#include "rede.h"
#include "resolverregra.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef NV_REC_URL
#define FE_FALLBACK_URL NV_REC_URL
#else
#define FE_FALLBACK_URL ""
#endif

// Ganchos fracos: na build completa vencem nvbackend.c, avisos.c, redesaude.c
// e trakt.c; nos testes isolados os padroes seguros linkam sem a arvore.
__attribute__((weak)) size_t nv_rec_url_copiar(char *dst, size_t tam) {
  if (!dst || !tam) return 0;
  snprintf(dst, tam, "%s", FE_FALLBACK_URL);
  return strlen(dst);
}
__attribute__((weak)) int avisos_registro_cabecalhos(const char **cab, char *aut, size_t nAut,
                                                     char *via, size_t nVia, char *chave, size_t nChave) {
  (void)cab; (void)aut; (void)nAut; (void)via; (void)nVia; (void)chave; (void)nChave;
  return 0;
}
__attribute__((weak)) int rede_saude_offline(void) { return 0; }
__attribute__((weak)) void trakt_http_recusada(void) {}
__attribute__((weak)) void avisos_enviar_registro_atual(void) {}

enum { J_ENVIAR = 1, J_PENDENTE, J_CARREGAR, J_DESCARTAR };
enum { T_OK = 0, T_TRANSPORTE, T_PRAZO, T_CANCELADO };

#define FE_TOK_N 49   /* PARAM_TOK_RX do servidor: 1..48 */

typedef struct {
  int       tipo;
  int       cancelar;
  long long fim;          // prazo total (monotonico, ms)
  long long fimReq;       // prazo do pedido corrente
  unsigned  conhecidoMs;
  unsigned  totalMs;
  int       incluirLog;
  int       resSintoma;
  // Copia profunda da entrada (J_ENVIAR).
  FarolPacoteEntrada   in;
  FarolPerfil          perfil;
  int                  temPerfil;
  char                 codigo[48];
  FarolPacoteTentativa tentou[FAROL_PACOTE_TENTOU_MAX];
  char                 tentouAcao[FAROL_PACOTE_TENTOU_MAX][32];
  char                 midiaTxt[9][24];
  FarolEvento         *eventos;    // heap (FAROL_ANEL_N)
  int                  nEventos;
  int                  eventosDoChamador;
} EnvioJob;

typedef struct {
  char fp[FAROL_PACOTE_FP_HEX_N];
  char codigo[FE_TOK_N];
  char plat[FE_TOK_N];
  char versao[FE_TOK_N];
  int  soMaior;
} Chave;

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  condFim = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t travaEnviados = PTHREAD_MUTEX_INITIALIZER;
static int             ativos;
static EnvioJob       *jobAtual;
static FarolEnvioInfo  info;
static int             logPedido;         // incluir_log a entregar no passo
static long            proximoReenvio;    // epoch; 0 = ja
static int             reenvioAuto;       // 1 = apenas rede/offline (nunca 4xx/5xx)
static int             offlineAntes;
static int             temUrlOverride;
static char            urlOverride[384];
static long long     (*relogioS)(void);
static unsigned        prazoTotalMs = FAROLENVIO_PRAZO_MS;
static unsigned        prazoConhecidoMs = FAROLENVIO_PRAZO_CONHECIDO_MS;

// ---------------------------------------------------------------- relogio --

static long long agoraMs(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (long long)t.tv_sec * 1000LL + (long long)(t.tv_nsec / 1000000L);
}

static long agoraS(void) {
  long long (*f)(void);
  pthread_mutex_lock(&trava);
  f = relogioS;
  pthread_mutex_unlock(&trava);
  return f ? (long)f() : (long)time(NULL);
}

// --------------------------------------------------------------- utilidades --

static void copiar(char *dst, size_t n, const char *s) {
  if (!dst || !n) return;
  snprintf(dst, n, "%s", s ? s : "");
}

// Alfabeto do PARAM_TOK_RX do servidor; fora dele o parametro e omitido.
static int tokOk(const char *s) {
  size_t n = 0;
  if (!s || !*s) return 0;
  for (; s[n]; n++) {
    unsigned char c = (unsigned char)s[n];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
          c == '_' || c == '.' || c == ':' || c == '+' || c == '-'))
      return 0;
  }
  return n <= 48;
}

static int hex16Ok(const char *s) {
  int i;
  if (!s) return 0;
  for (i = 0; i < 16; i++) {
    char c = s[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return 0;
  }
  return s[16] == 0;
}

// Codigo do recibo: 6 caracteres do base32 de Crockford (codigo.js).
static int codigoOk(const char *s) {
  int i;
  if (!s) return 0;
  for (i = 0; i < 6; i++) {
    char c = s[i];
    if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z')) ||
        c == 'I' || c == 'L' || c == 'O' || c == 'U')
      return 0;
  }
  return s[6] == 0;
}

static void putUtf8(char *dst, size_t cap, size_t *u, unsigned cp) {
  char b[4];
  size_t n, k;
  if (cp < 0x80) { b[0] = (char)cp; n = 1; }
  else if (cp < 0x800) { b[0] = (char)(0xC0 | (cp >> 6)); b[1] = (char)(0x80 | (cp & 0x3F)); n = 2; }
  else if (cp < 0x10000) {
    b[0] = (char)(0xE0 | (cp >> 12)); b[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
    b[2] = (char)(0x80 | (cp & 0x3F)); n = 3;
  } else {
    b[0] = (char)(0xF0 | (cp >> 18)); b[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    b[2] = (char)(0x80 | ((cp >> 6) & 0x3F)); b[3] = (char)(0x80 | (cp & 0x3F)); n = 4;
  }
  if (*u + n >= cap) return;
  for (k = 0; k < n; k++) dst[(*u)++] = b[k];
}

static int hexVal(const char *p, unsigned *out) {
  unsigned v = 0;
  int i;
  for (i = 0; i < 4; i++) {
    char c = p[i];
    v <<= 4;
    if (c >= '0' && c <= '9') v |= (unsigned)(c - '0');
    else if (c >= 'a' && c <= 'f') v |= (unsigned)(c - 'a' + 10);
    else if (c >= 'A' && c <= 'F') v |= (unsigned)(c - 'A' + 10);
    else return 0;
  }
  *out = v;
  return 1;
}

// Le a string JSON que comeca em `p` (apos a aspa) ate `fim`. Devolve o ponteiro
// depois da aspa de fecho, ou NULL se a string nao fecha.
static const char *lerStr(const char *p, const char *fim, char *dst, size_t cap) {
  size_t u = 0;
  if (cap) dst[0] = 0;
  while (p < fim && *p != '"') {
    if (*p == '\\' && p + 1 < fim) {
      char e = p[1];
      p += 2;
      if (e == 'u' && p + 4 <= fim) {
        unsigned cp, lo;
        if (!hexVal(p, &cp)) return NULL;
        p += 4;
        if (cp >= 0xD800 && cp < 0xDC00 && p + 6 <= fim && p[0] == '\\' && p[1] == 'u' &&
            hexVal(p + 2, &lo) && lo >= 0xDC00 && lo < 0xE000) {
          cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
          p += 6;
        } else if (cp >= 0xD800 && cp < 0xE000) cp = 0xFFFD;
        putUtf8(dst, cap, &u, cp);
        continue;
      }
      {
        char c = e == 'n' ? '\n' : e == 't' ? '\t' : e == 'r' ? '\r' : e == 'b' ? '\b' :
                 e == 'f' ? '\f' : e;
        if (u + 1 < cap) dst[u++] = c;
      }
      continue;
    }
    if (u + 1 < cap) dst[u++] = *p;
    p++;
  }
  if (cap) dst[u < cap ? u : cap - 1] = 0;
  return p < fim ? p + 1 : NULL;
}

static const char *pularEsp(const char *p, const char *fim) {
  while (p < fim && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
  return p;
}

// Fim do valor composto que comeca em `p` ('{' ou '['), respeitando strings.
static const char *fimComposto(const char *p, const char *fim) {
  int prof = 0;
  for (; p < fim; p++) {
    if (*p == '"') {
      for (p++; p < fim && *p != '"'; p++) if (*p == '\\') p++;
      if (p >= fim) return NULL;
    } else if (*p == '{' || *p == '[') prof++;
    else if (*p == '}' || *p == ']') { if (--prof == 0) return p + 1; }
  }
  return NULL;
}

// Procura "chave": no NIVEL de topo do objeto [ini, fim). Devolve o ponteiro
// para o valor (apos ':' e espacos) ou NULL.
static const char *acharChave(const char *ini, const char *fim, const char *chave) {
  const char *p = pularEsp(ini, fim);
  size_t nk = strlen(chave);
  if (p >= fim || *p != '{') return NULL;
  p++;
  while (p < fim) {
    char nome[48];
    p = pularEsp(p, fim);
    if (p >= fim || *p == '}') return NULL;
    if (*p == ',') { p++; continue; }
    if (*p != '"') return NULL;
    p = lerStr(p + 1, fim, nome, sizeof nome);
    if (!p) return NULL;
    p = pularEsp(p, fim);
    if (p >= fim || *p != ':') return NULL;
    p = pularEsp(p + 1, fim);
    if (strlen(nome) == nk && !memcmp(nome, chave, nk)) return p;
    // pula o valor
    if (*p == '"') {
      for (p++; p < fim && *p != '"'; p++) if (*p == '\\') p++;
      if (p >= fim) return NULL;
      p++;
    } else if (*p == '{' || *p == '[') {
      p = fimComposto(p, fim);
      if (!p) return NULL;
    } else {
      while (p < fim && *p != ',' && *p != '}') p++;
    }
  }
  return NULL;
}

static int strChave(const char *ini, const char *fim, const char *chave, char *dst, size_t cap) {
  const char *v = acharChave(ini, fim, chave);
  if (cap) dst[0] = 0;
  if (!v || v >= fim || *v != '"') return 0;
  return lerStr(v + 1, fim, dst, cap) != NULL;
}

static int intChave(const char *ini, const char *fim, const char *chave, long *out) {
  const char *v = acharChave(ini, fim, chave);
  char *e;
  long n;
  if (!v || v >= fim) return 0;
  n = strtol(v, &e, 10);
  if (e == v) return 0;
  *out = n;
  return 1;
}

typedef struct {
  char estado[16];
  char versao[48];
  char notaPt[256];
  char notaEn[256];
} Conhecido;

// "conhecido": {...} | null. Devolve 1 com o objeto, 0 sem (null/ausente/lixo).
static int lerConhecido(const char *corpo, size_t n, Conhecido *c) {
  const char *fim = corpo + n, *v, *vf;
  memset(c, 0, sizeof *c);
  if (!corpo || !n) return 0;
  v = acharChave(corpo, fim, "conhecido");
  if (!v || v >= fim || *v != '{') return 0;
  vf = fimComposto(v, fim);
  if (!vf) return 0;
  if (!strChave(v, vf, "estado", c->estado, sizeof c->estado)) return 0;
  if (strcmp(c->estado, "corrigido") && strcmp(c->estado, "conhecido") &&
      strcmp(c->estado, "contorno")) {
    memset(c, 0, sizeof *c);
    return 0;
  }
  strChave(v, vf, "versao", c->versao, sizeof c->versao);
  strChave(v, vf, "nota_pt", c->notaPt, sizeof c->notaPt);
  strChave(v, vf, "nota_en", c->notaEn, sizeof c->notaEn);
  return 1;
}

// ------------------------------------------------------------- job / estado --

static void limparJob(EnvioJob *j) {
  if (!j) return;
  free(j->eventos);
  memset(j, 0, sizeof *j);
  free(j);
}

static int jobCancelado(EnvioJob *j) {
  int s;
  pthread_mutex_lock(&trava);
  s = j->cancelar;
  pthread_mutex_unlock(&trava);
  return s;
}

static int envioParar(void *u) {
  EnvioJob *j = (EnvioJob *)u;
  return jobCancelado(j) || agoraMs() >= j->fimReq;
}

// Publica o resultado (sob trava). `cod` NULL preserva o ultimo codigo.
static void publicar(EnvioJob *j, FarolEnvioEstado est, FarolEnvioMotivo mot, int http,
                     const char *cod, const Conhecido *con) {
  long agora = agoraS();
  pthread_mutex_lock(&trava);
  info.estado = est;
  info.motivo = mot;
  info.http = http;
  info.doPendente = j->tipo == J_PENDENTE;
  info.codigoNovo = 0;
  if (cod && codigoOk(cod)) {
    copiar(info.codigo, sizeof info.codigo, cod);
    if (!resolverregra_codigo_exibir(info.codigo, info.codigoExibir, sizeof info.codigoExibir))
      info.codigoExibir[0] = 0;
    info.codigoQuando = agora;
    info.codigoNovo = 1;
  }
  if (con) {
    copiar(info.conhecidoEstado, sizeof info.conhecidoEstado, con->estado);
    copiar(info.conhecidoVersao, sizeof info.conhecidoVersao, con->versao);
    copiar(info.notaPt, sizeof info.notaPt, con->notaPt);
    copiar(info.notaEn, sizeof info.notaEn, con->notaEn);
  } else {
    info.conhecidoEstado[0] = info.conhecidoVersao[0] = 0;
    info.notaPt[0] = info.notaEn[0] = 0;
  }
  if (j->tipo == J_ENVIAR || j->tipo == J_PENDENTE) info.incluirLog = j->incluirLog;
  if (est == FAROLENVIO_OK && j->incluirLog) logPedido = 1;
  // Espera do proximo reenvio automatico (so offline / falha de transporte).
  if (est == FAROLENVIO_PENDENTE) {
    reenvioAuto = 1;
    proximoReenvio = agora + FAROLENVIO_REENVIO_S;
    if (mot == FAROLENVIO_M_OFFLINE) offlineAntes = 1;
  } else {
    reenvioAuto = 0;
  }
  info.geracao++;
  pthread_mutex_unlock(&trava);
}

static void marcarPendente(int tem, long criado) {
  pthread_mutex_lock(&trava);
  info.temPendente = tem;
  info.pendenteCriado = tem ? criado : 0;
  info.pendenteVence = tem ? criado + FAROLENVIO_PENDENTE_VALIDADE_S : 0;
  if (!tem) reenvioAuto = 0;
  pthread_mutex_unlock(&trava);
}

// ------------------------------------------------------------------- rede --

static int obterBaseUrl(char *dst, size_t cap) {
  size_t n;
  dst[0] = 0;
  pthread_mutex_lock(&trava);
  if (temUrlOverride) {
    copiar(dst, cap, urlOverride);
    pthread_mutex_unlock(&trava);
  } else {
    pthread_mutex_unlock(&trava);
    nv_rec_url_copiar(dst, cap);
  }
  n = strlen(dst);
  while (n > 0 && dst[n - 1] == '/') dst[--n] = 0;
  return n > 0;
}

// Um pedido HTTP com o prazo `ms` (cortado pelo prazo total). Devolve o status
// (0 = sem resposta) e o tipo de falha de transporte em *t.
static int pedir(EnvioJob *j, const char *metodo, const char *url, const char **cab,
                 const char *corpo, size_t nCorpo, unsigned ms, char **resp, size_t *nResp, int *t) {
  long long agora = agoraMs();
  int http = 0;
  *resp = NULL; *nResp = 0; *t = T_OK;
  if (jobCancelado(j)) { *t = T_CANCELADO; return 0; }
  if (agora >= j->fim) { *t = T_PRAZO; return 0; }
  if (agora + (long long)ms > j->fim) ms = (unsigned)(j->fim - agora);
  if (ms < 1) ms = 1;
  pthread_mutex_lock(&trava);
  j->fimReq = agora + (long long)ms;
  pthread_mutex_unlock(&trava);
#ifdef __EMSCRIPTEN__
  {
    RedePararEstado prev = rede_parar_trocar(envioParar, j);
    int seg = (int)((ms + 999u) / 1000u);
    char *r;
    (void)nCorpo;
    if (seg < 1) seg = 1;
    r = !strcmp(metodo, "POST") ? rede_postar_st(url, seg, cab, corpo, &http)
                                : rede_baixar_st(url, seg, cab, &http);
    rede_parar_trocar(prev.parar, prev.usuario);
    if (jobCancelado(j)) { *t = T_CANCELADO; free(r); return 0; }
    if (http == 0) { *t = agoraMs() >= j->fimReq ? T_PRAZO : T_TRANSPORTE; free(r); return 0; }
    *resp = r;
    *nResp = r ? strlen(r) : 0;
  }
#else
  {
    RedePedido req;
    RedeResposta res;
    memset(&req, 0, sizeof req);
    memset(&res, 0, sizeof res);
    req.metodo = metodo;
    req.url = url;
    req.cabecalhos = cab;
    req.corpo = corpo;
    req.n_corpo = nCorpo;
    req.prazo_ms = ms;
    req.max_bytes = 65536;
    req.max_cabecalhos = 8192;
    req.seguir = 0;
    req.parar = envioParar;
    req.parar_usuario = j;
    rede_pedir(&req, &res);
    http = res.status;
    if (jobCancelado(j)) *t = T_CANCELADO;
    else if (res.erro == REDE_PRAZO || (res.erro == REDE_CANCELADO && agoraMs() >= j->fimReq))
      *t = T_PRAZO;
    else if (res.erro != REDE_OK || http == 0) *t = T_TRANSPORTE;
    if (*t == T_OK && res.corpo && res.n_corpo > 0) {
      *resp = res.corpo;
      *nResp = res.n_corpo;
      res.corpo = NULL;
    }
    rede_resposta_limpar(&res);
    if (*t != T_OK) { free(*resp); *resp = NULL; *nResp = 0; http = 0; }
  }
#endif
  return http;
}

// --------------------------------------------------------- arquivos (fio) --

static void enviadosAdicionar(const char *codigo, int sintoma) {
  ResolverEnviado v[RES_ENVIADOS_MAX];
  char buf[RES_ENVIADOS_MAX * 40];
  char *s;
  int n;
  long agora = agoraS();
  pthread_mutex_lock(&travaEnviados);
  s = dados_ler(FAROLENVIO_ENVIADOS_ARQ);
  n = resolverregra_enviados_ler(s, agora, v, RES_ENVIADOS_MAX);
  free(s);
  n = resolverregra_enviados_add(v, n, RES_ENVIADOS_MAX, codigo, agora, sintoma);
  if (resolverregra_enviados_serializar(v, n, buf, sizeof buf)) dados_gravar(FAROLENVIO_ENVIADOS_ARQ, buf);
  pthread_mutex_unlock(&travaEnviados);
}

static void ultimoCodigoCarregar(void) {
  ResolverEnviado v[RES_ENVIADOS_MAX];
  char *s;
  int n;
  long agora = agoraS();
  pthread_mutex_lock(&travaEnviados);
  s = dados_ler(FAROLENVIO_ENVIADOS_ARQ);
  pthread_mutex_unlock(&travaEnviados);
  if (!s) return;
  n = resolverregra_enviados_ler(s, agora, v, RES_ENVIADOS_MAX);
  free(s);
  if (n > 0 && codigoOk(v[0].codigo)) {
    pthread_mutex_lock(&trava);
    if (!info.codigo[0]) {
      copiar(info.codigo, sizeof info.codigo, v[0].codigo);
      if (!resolverregra_codigo_exibir(info.codigo, info.codigoExibir, sizeof info.codigoExibir))
        info.codigoExibir[0] = 0;
      info.codigoQuando = v[0].quando;
    }
    pthread_mutex_unlock(&trava);
  }
}

// Envelope do pendente: o pacote vai por ULTIMO, para ser recortado sem parse.
static int pendenteGravar(const Chave *k, int sintoma, long criado, int autoReenvio,
                          int incluirLog, const char *pacote, size_t n) {
  size_t cap = FAROLENVIO_PENDENTE_MAX + 1, u;
  char *buf;
  int ok;
  if (!pacote || n == 0 || pacote[0] != '{') return 0;
  buf = (char *)malloc(cap);
  if (!buf) return 0;
  u = (size_t)snprintf(buf, cap,
                       "{\"v\":1,\"criado\":%ld,\"sintoma\":%d,\"auto\":%d,\"incluir_log\":%d,"
                       "\"fp\":\"%s\",\"codigo\":\"%s\","
                       "\"plataforma\":\"%s\",\"versao\":\"%s\",\"so_maior\":%d,\"pacote\":",
                       criado, sintoma, autoReenvio ? 1 : 0, incluirLog ? 1 : 0,
                       k->fp, k->codigo, k->plat, k->versao, k->soMaior);
  if (u + n + 2 > FAROLENVIO_PENDENTE_MAX) { free(buf); return 0; }
  memcpy(buf + u, pacote, n);
  u += n;
  buf[u++] = '}';
  buf[u] = 0;
  ok = dados_gravar(FAROLENVIO_PENDENTE_ARQ, buf);
  memset(buf, 0, u);
  free(buf);
  return ok;
}

// Le o pendente: 1 = valido (pacote em *pacote, heap); 0 = ausente; -1 = lixo
// ou vencido (ja apagado). `soMeta` nao copia o pacote.
static int pendenteLer(Chave *k, int *sintoma, long *criado, int *autoReenvio, int *incluirLog,
                       char **pacote, size_t *nPacote, int soMeta) {
  char *s = dados_ler(FAROLENVIO_PENDENTE_ARQ);
  const char *fim, *p;
  size_t n;
  long v = 0, cr = 0, sint = RES_S_GERAL, so = -1, autoR = 1, incLog = 0;
  long agora = agoraS();
  memset(k, 0, sizeof *k);
  if (autoReenvio) *autoReenvio = 1;
  if (incluirLog) *incluirLog = -1;
  if (pacote) *pacote = NULL;
  if (nPacote) *nPacote = 0;
  if (!s) return 0;
  n = strlen(s);
  fim = s + n;
  if (n == 0 || n > FAROLENVIO_PENDENTE_MAX || !intChave(s, fim, "v", &v) || v != 1 ||
      !intChave(s, fim, "criado", &cr) || cr <= 0 ||
      !strChave(s, fim, "fp", k->fp, sizeof k->fp) || !hex16Ok(k->fp))
    goto lixo;
  // Vencido (24 h) ou do futuro (relogio voltou): apaga.
  if (agora - cr >= FAROLENVIO_PENDENTE_VALIDADE_S || cr - agora > 3600) goto lixo;
  intChave(s, fim, "sintoma", &sint);
  intChave(s, fim, "so_maior", &so);
  intChave(s, fim, "auto", &autoR);
  if (autoReenvio) *autoReenvio = autoR ? 1 : 0;
  if (incluirLog && intChave(s, fim, "incluir_log", &incLog)) *incluirLog = incLog ? 1 : 0;
  strChave(s, fim, "codigo", k->codigo, sizeof k->codigo);
  strChave(s, fim, "plataforma", k->plat, sizeof k->plat);
  strChave(s, fim, "versao", k->versao, sizeof k->versao);
  if (!tokOk(k->codigo)) k->codigo[0] = 0;
  if (!tokOk(k->plat)) k->plat[0] = 0;
  if (!tokOk(k->versao)) k->versao[0] = 0;
  k->soMaior = (int)so;
  p = acharChave(s, fim, "pacote");
  if (!p || *p != '{' || fim[-1] != '}') goto lixo;
  {
    const char *pf = fimComposto(p, fim);
    if (!pf || pf != fim - 1) goto lixo;
    if (!soMeta && pacote) {
      size_t np = (size_t)(pf - p);
      *pacote = (char *)malloc(np + 1);
      if (!*pacote) { free(s); return 0; }
      memcpy(*pacote, p, np);
      (*pacote)[np] = 0;
      *nPacote = np;
    }
  }
  *sintoma = (int)sint;
  *criado = cr;
  free(s);
  return 1;
lixo:
  free(s);
  dados_apagar(FAROLENVIO_PENDENTE_ARQ);
  return -1;
}

// ----------------------------------------------------------- fio de envio --

static void montarConsulta(char *url, size_t cap, const char *base, const Chave *k) {
  int u = snprintf(url, cap, "%s/v1/farol/conhecido?fp=%s", base, k->fp);
  if (u < 0 || (size_t)u >= cap) { url[0] = 0; return; }
  if (k->codigo[0]) u += snprintf(url + u, cap - (size_t)u, "&c=%s", k->codigo);
  if ((size_t)u < cap && k->plat[0]) u += snprintf(url + u, cap - (size_t)u, "&p=%s", k->plat);
  if ((size_t)u < cap && k->versao[0]) u += snprintf(url + u, cap - (size_t)u, "&v=%s", k->versao);
  if ((size_t)u < cap && k->soMaior >= 0 && k->soMaior <= 99)
    u += snprintf(url + u, cap - (size_t)u, "&so_maior=%d", k->soMaior);
  if ((size_t)u >= cap) url[0] = 0;
}

// Consulta + POST. `pacote` pode vir do montador ou do pendente. Devolve 1
// quando o pendente (se houver) deve ser APAGADO, 0 para manter (resultado ja
// publicado), 2 para GRAVAR pendente automatico (falha de rede/prazo), 3 para
// GRAVAR pendente manual (recusa HTTP do servidor; nunca reenvia em passo).
static int transmitir(EnvioJob *j, const Chave *k, const char *pacote, size_t n, int sintoma,
                      FarolEnvioMotivo *motOut, int *httpOut) {
  char base[384], url[640];
  char aut[3200], via[64], chave[160];
  const char *cab[5] = {0};
  char *resp = NULL;
  size_t nResp = 0;
  int http, t, usouTrakt;
  Conhecido con;

  *motOut = FAROLENVIO_M_NENHUM;
  *httpOut = 0;
  if (jobCancelado(j)) goto cancelado;
  if (!obterBaseUrl(base, sizeof base)) {
    if (jobCancelado(j)) goto cancelado;
    publicar(j, FAROLENVIO_FALHOU, FAROLENVIO_M_INDISPONIVEL, 0, NULL, NULL);
    return 0;
  }
  aut[0] = via[0] = chave[0] = 0;
  if (!avisos_registro_cabecalhos(cab, aut, sizeof aut, via, sizeof via, chave, sizeof chave) ||
      !cab[0] || !cab[1]) {
    memset(aut, 0, sizeof aut);
    if (jobCancelado(j)) goto cancelado;
    publicar(j, FAROLENVIO_FALHOU, FAROLENVIO_M_CONTA, 0, NULL, NULL);
    return 0;
  }
  usouTrakt = strstr(via, "trakt") != NULL;
  if (jobCancelado(j)) { memset(aut, 0, sizeof aut); goto cancelado; }

  // 1. Ja ha correcao conhecida? (melhor esforco: so `corrigido` para o envio)
  montarConsulta(url, sizeof url, base, k);
  if (url[0]) {
    const char *cabGet[4];
    cabGet[0] = cab[0]; cabGet[1] = cab[1]; cabGet[2] = NULL; cabGet[3] = NULL;
    if (jobCancelado(j)) { memset(aut, 0, sizeof aut); goto cancelado; }
    http = pedir(j, "GET", url, cabGet, NULL, 0, j->conhecidoMs, &resp, &nResp, &t);
    if (t == T_CANCELADO || jobCancelado(j)) { free(resp); memset(aut, 0, sizeof aut); goto cancelado; }
    if (t == T_TRANSPORTE) {
      free(resp); memset(aut, 0, sizeof aut);
      *motOut = FAROLENVIO_M_CONEXAO;
      return 2;
    }
    if (http == 401) {
      free(resp); memset(aut, 0, sizeof aut);
      if (usouTrakt) trakt_http_recusada();
      *motOut = FAROLENVIO_M_CONTA;
      *httpOut = 401;
      return 3;
    }
    if (http == 200 && resp && lerConhecido(resp, nResp, &con) && !strcmp(con.estado, "corrigido")) {
      free(resp); memset(aut, 0, sizeof aut);
      publicar(j, FAROLENVIO_JA_CORRIGIDO, FAROLENVIO_M_CORRIGIDO, 200, NULL, &con);
      return 1;
    }
    free(resp); resp = NULL; nResp = 0;
  }

  // 2. POST do registro.
  if (jobCancelado(j)) { memset(aut, 0, sizeof aut); goto cancelado; }
  snprintf(url, sizeof url, "%s/v1/farol/registro", base);
#ifdef __EMSCRIPTEN__
  cab[2] = NULL;   // o XHR do WGT poe o Content-Type do POST
#endif
  http = pedir(j, "POST", url, cab, pacote, n, j->totalMs, &resp, &nResp, &t);
  memset(aut, 0, sizeof aut);
  if (t == T_CANCELADO || jobCancelado(j)) { free(resp); goto cancelado; }
  farol_registrar_envio(http);
  if (t == T_TRANSPORTE || t == T_PRAZO) {
    free(resp);
    *motOut = t == T_PRAZO ? FAROLENVIO_M_PRAZO : FAROLENVIO_M_CONEXAO;
    return 2;
  }
  if (http >= 200 && http < 300) {
    char cod[16];
    int temCon;
    cod[0] = 0;
    if (resp) strChave(resp, resp + nResp, "codigo", cod, sizeof cod);
    temCon = resp && lerConhecido(resp, nResp, &con);
    free(resp);
    if (!codigoOk(cod)) {
      *motOut = FAROLENVIO_M_SERVIDOR;
      *httpOut = http;
      return 3;
    }
    enviadosAdicionar(cod, sintoma);
    publicar(j, FAROLENVIO_OK, FAROLENVIO_M_OK, http, cod, temCon ? &con : NULL);
    return 1;
  }
  free(resp);
  if (http == 401) {
    if (usouTrakt) trakt_http_recusada();
    *motOut = FAROLENVIO_M_CONTA;
    *httpOut = http;
    return 3;
  }
  if (http == 429) { *motOut = FAROLENVIO_M_LIMITE; *httpOut = http; return 3; }
  if (http == 422) { publicar(j, FAROLENVIO_FALHOU, FAROLENVIO_M_RECUSADO, http, NULL, NULL); return 1; }
  *motOut = FAROLENVIO_M_SERVIDOR;
  *httpOut = http;
  return 3;
cancelado:
  publicar(j, FAROLENVIO_CANCELADO, FAROLENVIO_M_CANCELADO, 0, NULL, NULL);
  return 0;
}

static void chaveDe(Chave *k, const FarolPacoteRes *res, const FarolPerfil *p, const char *codigo) {
  memset(k, 0, sizeof *k);
  copiar(k->fp, sizeof k->fp, res->fp);
  if (tokOk(codigo)) copiar(k->codigo, sizeof k->codigo, codigo);
  if (tokOk(p->aparelho.plataforma)) copiar(k->plat, sizeof k->plat, p->aparelho.plataforma);
  if (tokOk(p->cliente.versao)) copiar(k->versao, sizeof k->versao, p->cliente.versao);
  k->soMaior = p->aparelho.so_maior;
}

static void executarEnviar(EnvioJob *j) {
  FarolPacoteRes res;
  Chave k;
  char *json;
  int r, offline, httpOut = 0;
  FarolEnvioMotivo mot = FAROLENVIO_M_NENHUM;
  size_t cap = FAROL_JSON_MAX + 1;

  if (jobCancelado(j)) {
    publicar(j, FAROLENVIO_CANCELADO, FAROLENVIO_M_CANCELADO, 0, NULL, NULL);
    return;
  }
  if (!j->temPerfil) { farolperfil_obter(&j->perfil); j->temPerfil = 1; }
  j->in.perfil = &j->perfil;
  if (!j->eventosDoChamador) j->nEventos = farol_eventos(j->eventos, FAROL_ANEL_N);
  j->in.eventos = j->eventos;
  j->in.n_eventos = j->nEventos;

  json = (char *)malloc(cap);
  memset(&res, 0, sizeof res);
  if (!json || !farolpacote_montar(&j->in, json, cap, &res) || !res.bytes) {
    free(json);
    if (jobCancelado(j)) publicar(j, FAROLENVIO_CANCELADO, FAROLENVIO_M_CANCELADO, 0, NULL, NULL);
    else publicar(j, FAROLENVIO_FALHOU, FAROLENVIO_M_PACOTE, 0, NULL, NULL);
    return;
  }
  pthread_mutex_lock(&trava);
  info.bytes = res.bytes;
  info.linhaEscritos = res.linha_escritos;
  info.linhaCortados = res.linha_cortados;
  copiar(info.fp, sizeof info.fp, res.fp);
  pthread_mutex_unlock(&trava);
  chaveDe(&k, &res, &j->perfil, j->in.codigo);

  if (jobCancelado(j)) {
    publicar(j, FAROLENVIO_CANCELADO, FAROLENVIO_M_CANCELADO, 0, NULL, NULL);
  } else {
    offline = rede_saude_offline();
    if (jobCancelado(j)) {
      publicar(j, FAROLENVIO_CANCELADO, FAROLENVIO_M_CANCELADO, 0, NULL, NULL);
    } else if (offline) {
      long agora = agoraS();
      if (pendenteGravar(&k, j->resSintoma, agora, 1, j->incluirLog, json, res.bytes)) {
        marcarPendente(1, agora);
        publicar(j, FAROLENVIO_PENDENTE, FAROLENVIO_M_OFFLINE, 0, NULL, NULL);
      } else {
        publicar(j, FAROLENVIO_FALHOU, FAROLENVIO_M_OFFLINE, 0, NULL, NULL);
      }
    } else {
      r = transmitir(j, &k, json, res.bytes, j->resSintoma, &mot, &httpOut);
      if (r == 2 || r == 3) {
        if (jobCancelado(j)) {
          publicar(j, FAROLENVIO_CANCELADO, FAROLENVIO_M_CANCELADO, 0, NULL, NULL);
        } else {
          long agora = agoraS();
          int autoR = (r == 2);
          int gravou = pendenteGravar(&k, j->resSintoma, agora, autoR, j->incluirLog, json, res.bytes);
          if (gravou) marcarPendente(1, agora);
          if (r == 2 && gravou) publicar(j, FAROLENVIO_PENDENTE, mot, 0, NULL, NULL);
          else publicar(j, FAROLENVIO_FALHOU, mot, httpOut, NULL, NULL);
        }
      } else if (r == 1) {
        // Envio novo resolveu (ou 422): um pendente antigo do MESMO fp fica redundante.
        Chave kp;
        int s;
        long cr;
        if (pendenteLer(&kp, &s, &cr, NULL, NULL, NULL, NULL, 1) == 1 && !strcmp(kp.fp, k.fp)) {
          dados_apagar(FAROLENVIO_PENDENTE_ARQ);
          marcarPendente(0, 0);
        }
      }
    }
  }
  memset(json, 0, res.bytes);
  free(json);
}

static void executarPendente(EnvioJob *j, int soCarregar) {
  Chave k;
  int sintoma = RES_S_GERAL, r, autoR = 1, incLog = -1, offline, httpOut = 0;
  long criado = 0;
  char *pacote = NULL;
  size_t n = 0;
  FarolEnvioMotivo mot = FAROLENVIO_M_NENHUM;
  if (soCarregar) ultimoCodigoCarregar();
  r = pendenteLer(&k, &sintoma, &criado, &autoR, &incLog, soCarregar ? NULL : &pacote, &n, soCarregar);
  if (r != 1) {
    int tinha;
    pthread_mutex_lock(&trava);
    tinha = info.temPendente;
    pthread_mutex_unlock(&trava);
    marcarPendente(0, 0);
    if (!soCarregar) {
      if (jobCancelado(j)) publicar(j, FAROLENVIO_CANCELADO, FAROLENVIO_M_CANCELADO, 0, NULL, NULL);
      else if (r < 0 && tinha) publicar(j, FAROLENVIO_FALHOU, FAROLENVIO_M_EXPIROU, 0, NULL, NULL);
      else publicar(j, FAROLENVIO_FALHOU, FAROLENVIO_M_PACOTE, 0, NULL, NULL);
    }
    return;
  }
  marcarPendente(1, criado);
  if (soCarregar) {
    pthread_mutex_lock(&trava);
    reenvioAuto = autoR ? 1 : 0;
    if (incLog >= 0) info.incluirLog = incLog;
    pthread_mutex_unlock(&trava);
    return;
  }
  if (incLog >= 0) j->incluirLog = incLog;
  pthread_mutex_lock(&trava);
  info.bytes = n;
  copiar(info.fp, sizeof info.fp, k.fp);
  pthread_mutex_unlock(&trava);
  if (jobCancelado(j)) {
    publicar(j, FAROLENVIO_CANCELADO, FAROLENVIO_M_CANCELADO, 0, NULL, NULL);
  } else {
    offline = rede_saude_offline();
    if (jobCancelado(j)) {
      publicar(j, FAROLENVIO_CANCELADO, FAROLENVIO_M_CANCELADO, 0, NULL, NULL);
    } else if (offline) {
      if (!autoR) pendenteGravar(&k, sintoma, criado, 1, j->incluirLog, pacote, n);
      publicar(j, FAROLENVIO_PENDENTE, FAROLENVIO_M_OFFLINE, 0, NULL, NULL);
    } else {
      r = transmitir(j, &k, pacote, n, sintoma, &mot, &httpOut);
      if (r == 2 || r == 3) {
        if (jobCancelado(j)) {
          publicar(j, FAROLENVIO_CANCELADO, FAROLENVIO_M_CANCELADO, 0, NULL, NULL);
        } else if (r == 2) {
          if (!autoR) pendenteGravar(&k, sintoma, criado, 1, j->incluirLog, pacote, n);
          publicar(j, FAROLENVIO_PENDENTE, mot, 0, NULL, NULL);
        } else {
          if (autoR) pendenteGravar(&k, sintoma, criado, 0, j->incluirLog, pacote, n);
          publicar(j, FAROLENVIO_FALHOU, mot, httpOut, NULL, NULL);
        }
      } else if (r == 1) {
        dados_apagar(FAROLENVIO_PENDENTE_ARQ);
        marcarPendente(0, 0);
      }
    }
  }
  if (pacote) { memset(pacote, 0, n); free(pacote); }
}

static void *fioEnvio(void *u) {
  EnvioJob *j = (EnvioJob *)u;
  switch (j->tipo) {
    case J_ENVIAR:    executarEnviar(j); break;
    case J_PENDENTE:  executarPendente(j, 0); break;
    case J_CARREGAR:  executarPendente(j, 1); break;
    case J_DESCARTAR:
      dados_apagar(FAROLENVIO_PENDENTE_ARQ);
      marcarPendente(0, 0);
      break;
    default: break;
  }
  pthread_mutex_lock(&trava);
  if (jobAtual == j) jobAtual = NULL;
  ativos--;
  if (ativos == 0) pthread_cond_broadcast(&condFim);
  pthread_mutex_unlock(&trava);
  limparJob(j);
  return NULL;
}

// Lanca `j` (dono passa ao fio). Devolve 1 se lancou; libera `j` em falha.
static int lancar(EnvioJob *j) {
  pthread_attr_t attr;
  pthread_t fio;
  int erro;
  if (pthread_attr_init(&attr) != 0) { limparJob(j); return 0; }
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  pthread_attr_setstacksize(&attr, FAROLENVIO_PILHA);
  pthread_mutex_lock(&trava);
  if (jobAtual) {
    pthread_mutex_unlock(&trava);
    pthread_attr_destroy(&attr);
    limparJob(j);
    return 0;
  }
  j->fim = agoraMs() + (long long)prazoTotalMs;
  j->fimReq = j->fim;
  j->conhecidoMs = prazoConhecidoMs;
  j->totalMs = prazoTotalMs;
  jobAtual = j;
  ativos++;
  if (j->tipo == J_ENVIAR || j->tipo == J_PENDENTE) {
    info.estado = FAROLENVIO_ENVIANDO;
    info.motivo = FAROLENVIO_M_NENHUM;
    info.http = 0;
    info.codigoNovo = 0;
    info.doPendente = j->tipo == J_PENDENTE;
    info.geracao++;
  }
  erro = pthread_create(&fio, &attr, fioEnvio, j);
  if (erro) {
    jobAtual = NULL;
    ativos--;
    if (ativos == 0) pthread_cond_broadcast(&condFim);
    if (j->tipo == J_ENVIAR || j->tipo == J_PENDENTE) {
      info.estado = FAROLENVIO_FALHOU;
      info.motivo = FAROLENVIO_M_CONEXAO;
      info.geracao++;
    }
  }
  pthread_mutex_unlock(&trava);
  pthread_attr_destroy(&attr);
  if (erro) { limparJob(j); return 0; }
  return 1;
}

static EnvioJob *novoJob(int tipo) {
  EnvioJob *j = (EnvioJob *)calloc(1, sizeof *j);
  if (j) j->tipo = tipo;
  return j;
}

// ------------------------------------------------------------------ API --

void farolenvio_opcoes_padrao(FarolEnvioOpcoes *o) {
  if (!o) return;
  o->incluir_log = 0;
  o->res_sintoma = RES_S_GERAL;
}

void farolenvio_iniciar(void) {
  EnvioJob *j = novoJob(J_CARREGAR);
  if (j) lancar(j);
}

static const char *copiarMidia(EnvioJob *j, int i, const char *s) {
  if (!s) return NULL;
  copiar(j->midiaTxt[i], sizeof j->midiaTxt[i], s);
  return j->midiaTxt[i];
}

int farol_enviar(const FarolPacoteEntrada *in, const FarolEnvioOpcoes *op) {
  FarolEnvioOpcoes o;
  EnvioJob *j;
  int i;
  if (!in) return 0;
  if (op) o = *op; else farolenvio_opcoes_padrao(&o);
  if (farolenvio_em_voo()) return 0;
  j = novoJob(J_ENVIAR);
  if (!j) return 0;
  j->eventos = (FarolEvento *)calloc(FAROL_ANEL_N, sizeof *j->eventos);
  if (!j->eventos) { limparJob(j); return 0; }
  j->incluirLog = o.incluir_log ? 1 : 0;
  j->resSintoma = o.res_sintoma;

  // Copia profunda: o fio vive mais que a chamada.
  j->in = *in;
  copiar(j->codigo, sizeof j->codigo, in->codigo);
  j->in.codigo = in->codigo ? j->codigo : NULL;
  j->in.n_tentou = in->tentou ? in->n_tentou : 0;
  if (j->in.n_tentou < 0) j->in.n_tentou = 0;
  if (j->in.n_tentou > FAROL_PACOTE_TENTOU_MAX) j->in.n_tentou = FAROL_PACOTE_TENTOU_MAX;
  for (i = 0; i < j->in.n_tentou; i++) {
    j->tentou[i] = in->tentou[i];
    copiar(j->tentouAcao[i], sizeof j->tentouAcao[i], in->tentou[i].acao);
    j->tentou[i].acao = in->tentou[i].acao ? j->tentouAcao[i] : NULL;
  }
  j->in.tentou = j->in.n_tentou ? j->tentou : NULL;
  if (in->perfil) { j->perfil = *in->perfil; j->temPerfil = 1; }
  j->in.perfil = NULL;
  j->in.midia.container = copiarMidia(j, 0, in->midia.container);
  j->in.midia.video     = copiarMidia(j, 1, in->midia.video);
  j->in.midia.hdr       = copiarMidia(j, 2, in->midia.hdr);
  j->in.midia.res       = copiarMidia(j, 3, in->midia.res);
  j->in.midia.audio     = copiarMidia(j, 4, in->midia.audio);
  j->in.midia.legenda   = copiarMidia(j, 5, in->midia.legenda);
  j->in.midia.entrega   = copiarMidia(j, 6, in->midia.entrega);
  if (in->eventos && in->n_eventos > 0) {
    int ne = in->n_eventos > FAROL_ANEL_N ? FAROL_ANEL_N : in->n_eventos;
    // Os MAIS RECENTES: o fim do vetor cronologico do chamador.
    memcpy(j->eventos, in->eventos + (in->n_eventos - ne), (size_t)ne * sizeof *j->eventos);
    j->nEventos = ne;
    j->eventosDoChamador = 1;
  }
  j->in.eventos = NULL;
  j->in.n_eventos = 0;
  return lancar(j);
}

int farolenvio_retentar(void) {
  EnvioJob *j;
  int tem, incLog;
  pthread_mutex_lock(&trava);
  tem = info.temPendente;
  incLog = info.incluirLog;
  pthread_mutex_unlock(&trava);
  if (!tem || farolenvio_em_voo()) return 0;
  j = novoJob(J_PENDENTE);
  if (!j) return 0;
  j->incluirLog = incLog;
  return lancar(j);
}

int farolenvio_descartar_pendente(void) {
  EnvioJob *j = novoJob(J_DESCARTAR);
  return j ? lancar(j) : 0;
}

void farolenvio_passo(void) {
  int pedirLog, tem, emVoo, offline, vencido, devido, incLog;
  long agora = agoraS();
  offline = rede_saude_offline();
  pthread_mutex_lock(&trava);
  pedirLog = logPedido;
  logPedido = 0;
  tem = info.temPendente;
  emVoo = jobAtual != NULL;
  vencido = tem && agora >= info.pendenteVence;
  if (offlineAntes && !offline) proximoReenvio = 0;   // a rede voltou: tenta ja
  offlineAntes = offline;
  devido = reenvioAuto && !offline && agora >= proximoReenvio;
  incLog = info.incluirLog;
  pthread_mutex_unlock(&trava);
  if (pedirLog) avisos_enviar_registro_atual();       // opt-in, no fio da UI
  if (!tem || emVoo) return;
  // Vencido: o fio apaga (sem I/O aqui), com ou sem rede.
  if (vencido || devido) {
    EnvioJob *j = novoJob(J_PENDENTE);
    if (j) {
      j->incluirLog = incLog;
      pthread_mutex_lock(&trava);
      proximoReenvio = agora + FAROLENVIO_REENVIO_S;
      pthread_mutex_unlock(&trava);
      lancar(j);
    }
  }
}

void farolenvio_cancelar(void) {
  pthread_mutex_lock(&trava);
  if (jobAtual) jobAtual->cancelar = 1;
  pthread_mutex_unlock(&trava);
}

void farolenvio_encerrar(void) {
  pthread_mutex_lock(&trava);
  if (jobAtual) jobAtual->cancelar = 1;
  while (ativos > 0) pthread_cond_wait(&condFim, &trava);
  pthread_mutex_unlock(&trava);
}

int farolenvio_em_voo(void) {
  int v;
  pthread_mutex_lock(&trava);
  v = jobAtual != NULL;
  pthread_mutex_unlock(&trava);
  return v;
}

FarolEnvioEstado farolenvio_info(FarolEnvioInfo *o) {
  FarolEnvioEstado e;
  pthread_mutex_lock(&trava);
  e = info.estado;
  if (o) *o = info;
  pthread_mutex_unlock(&trava);
  return e;
}

void farolenvio_definir_url(const char *baseUrl) {
  pthread_mutex_lock(&trava);
  temUrlOverride = baseUrl != NULL;
  copiar(urlOverride, sizeof urlOverride, baseUrl);
  pthread_mutex_unlock(&trava);
}

void farolenvio_teste_relogio(long long (*s)(void)) {
  pthread_mutex_lock(&trava);
  relogioS = s;
  pthread_mutex_unlock(&trava);
}

void farolenvio_teste_prazos(unsigned totalMs, unsigned conhecidoMs) {
  pthread_mutex_lock(&trava);
  prazoTotalMs = totalMs ? totalMs : FAROLENVIO_PRAZO_MS;
  prazoConhecidoMs = conhecidoMs ? conhecidoMs : FAROLENVIO_PRAZO_CONHECIDO_MS;
  pthread_mutex_unlock(&trava);
}

void farolenvio_teste_esquecer(void) {
  farolenvio_encerrar();
  pthread_mutex_lock(&trava);
  memset(&info, 0, sizeof info);
  logPedido = 0;
  proximoReenvio = 0;
  offlineAntes = 0;
  reenvioAuto = 0;
  pthread_mutex_unlock(&trava);
}
