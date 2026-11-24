// FAROLESCOLHA_REDE — cliente assincrono POST /v1/farol/escolha (issue 54).
// Ver src/farolescolha_rede.h. Sem SDL, sem estado por fio, sem log de dados.
#include "farolescolha_rede.h"
#include "rede.h"
#include <ctype.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef FAROL_ESCOLHA_WORKER
#define FER_FALLBACK_URL FAROL_ESCOLHA_WORKER
#elif defined(NV_REC_URL)
#define FER_FALLBACK_URL NV_REC_URL
#else
#define FER_FALLBACK_URL ""
#endif

#define FER_CONSENT_SIM_VAL 2
#define FER_DIA_EPOCA       1767225600LL

// Ganchos fracos dos modulos do app: na build completa as definicoes fortes
// vencem; em testes isolados os padroes seguros permitem linkar sem SDL.
__attribute__((weak)) size_t nv_rec_url_copiar(char *dst, size_t tam) {
  if (!dst || !tam) return 0;
  snprintf(dst, tam, "%s", FER_FALLBACK_URL);
  return strlen(dst);
}
__attribute__((weak)) int recomenda_cabecalhos(const char **cab, char *aut,
                                               size_t na, char *via, size_t nv,
                                               char *perfil, size_t np) {
  (void)cab; (void)aut; (void)na; (void)via; (void)nv; (void)perfil; (void)np;
  return 0;
}
__attribute__((weak)) int perfil_infantil_ativo(void) { return 0; }
__attribute__((weak)) const char *ajustes_tmdb_idioma(void) { return "pt-BR"; }
__attribute__((weak)) int rede_saude_offline(void) { return 0; }
__attribute__((weak)) void trakt_http_recusada(void) {}
__attribute__((weak)) int player_aberto(void) { return 0; }
__attribute__((weak)) int seguro_perfil_ativo(void) { return 0; }
__attribute__((weak)) int farolsinal_recomendar(void) { return 1; }
__attribute__((weak)) int farolsinal_modelo(FarolGosto *dst) {
  (void)dst;
  return 0;
}
__attribute__((weak)) int farolconsent_estado(void) { return 0; }
__attribute__((weak)) int farolconsent_llm_ativo(void) { return 0; }
__attribute__((weak)) void farolconsent_sincronizar_ativo(void) {}
__attribute__((weak)) int farolconsent_campo_autorizado(const char *caminho) {
  (void)caminho;
  return 1;
}
__attribute__((weak)) int perfis_ativo(void) { return 1; }
__attribute__((weak)) uint64_t perfis_sessao_geracao(void) { return 0; }
__attribute__((weak)) const char *sessao_usuario(void) { return ""; }

// ------------------------------------------------------------ estruturas --

typedef struct {
  uint64_t gen;
  long long fim;
  unsigned prazoMs;
  int      cancelar;
  int      usouTrakt;
  int      bloqueou;       // acessado com atomicos (pode vir do fio da rede)
  int      perfil;
  uint64_t sessao;
  uint64_t contaHash;
  int      perguntas[FESC_N];
  int      nPerguntas;
  char     url[512];
  char     aut[3200];
  char     via[64];
  char     perfilCab[64];
  char    *corpo;
  size_t   nCorpo;
} EscolhaJob;

static pthread_mutex_t trava   = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  condFim = PTHREAD_COND_INITIALIZER;

static uint64_t         geracaoAtual;
static int              ativos;
static EscolhaJob      *jobAtual;
static FarolEscRedeInfo estadoAtual;
static FarolEscRedeResp respAtual;

static int  temUrlOverride;
static char urlOverride[384];
static int  temAuthOverride;
static char authOverrideAut[512];
static char authOverrideVia[64];
static char authOverridePerfil[64];

// ------------------------------------------------------ utilitarios puros --

static void zerarMemoria(void *p, size_t n) {
  volatile unsigned char *v = (volatile unsigned char *)p;
  if (!v) return;
  while (n--) *v++ = 0;
}

static long long agoraMs(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (long long)t.tv_sec * 1000LL + (long long)(t.tv_nsec / 1000000L);
}

static uint16_t diaModeloAgora(long long quando) {
  long long d = (quando - FER_DIA_EPOCA) / 86400LL;
  if (d < 0) d = 0;
  if (d > 65535) d = 65535;
  return (uint16_t)d;
}

static int ehFimDeSemana(long long quando) {
  time_t t = (time_t)quando;
  struct tm tmv;
#if defined(_WIN32)
  if (localtime_s(&tmv, &t) != 0) return 0;
#else
  if (!localtime_r(&t, &tmv)) return 0;
#endif
  return (tmv.tm_wday == 0 || tmv.tm_wday == 6) ? 1 : 0;
}

static uint64_t hashStr64(const char *s) {
  uint64_t h = 14695981039346656037ULL;
  if (!s) s = "";
  for (; *s; s++) {
    h ^= (unsigned char)*s;
    h *= 1099511628211ULL;
  }
  return h;
}

static uint64_t contaAtualHash(void) {
  const char *u = sessao_usuario();
  if (!u || !u[0]) return 0;
  return hashStr64(u);
}

static int perfilAtualValido(void) {
  int p = perfis_ativo();
  return (p >= 1 && p <= 8) ? p : 1;
}

static unsigned char minAscii(unsigned char c) {
  return (c >= 'A' && c <= 'Z') ? (unsigned char)(c - 'A' + 'a') : c;
}

static int contemSemCaixa(const char *s, const char *sub) {
  size_t i, n = strlen(sub);
  if (!s || !n) return 0;
  for (; *s; s++) {
    for (i = 0; i < n; i++) {
      if (!s[i] || minAscii((unsigned char)s[i]) != minAscii((unsigned char)sub[i]))
        break;
    }
    if (i == n) return 1;
  }
  return 0;
}

static long lerUtf8(const char **pp, const char *fim) {
  const unsigned char *s = (const unsigned char *)*pp;
  unsigned char c;
  long cp;
  int extra, i;
  if (*pp >= fim) return -1;
  c = s[0];
  if (c < 0x80) { *pp += 1; return c; }
  if (c >= 0xC2 && c <= 0xDF)      { cp = c & 0x1F; extra = 1; }
  else if (c >= 0xE0 && c <= 0xEF) { cp = c & 0x0F; extra = 2; }
  else if (c >= 0xF0 && c <= 0xF4) { cp = c & 0x07; extra = 3; }
  else return -1;
  if (fim - *pp < extra + 1) return -1;
  for (i = 1; i <= extra; i++) {
    if ((s[i] & 0xC0) != 0x80) return -1;
    cp = (cp << 6) | (s[i] & 0x3F);
  }
  if ((extra == 2 && cp < 0x800) || (extra == 3 && cp < 0x10000)) return -1;
  if (cp >= 0xD800 && cp <= 0xDFFF) return -1;
  if (cp > 0x10FFFF) return -1;
  *pp += extra + 1;
  return cp;
}

static int temControleFrase(const char *s) {
  const unsigned char *p = (const unsigned char *)s;
  if (!p) return 1;
  for (; *p; p++) {
    if (*p == 0x7f) return 1;
    if (*p < 0x20 && *p != '\t' && *p != '\n' && *p != '\r') return 1;
  }
  return 0;
}

static int temUrlFrase(const char *s) {
  if (!s) return 0;
  return contemSemCaixa(s, "://") || contemSemCaixa(s, "www.");
}

int farolescolha_rede_normalizar_frase(const char *src, char *dst, size_t cap) {
  const char *p, *fim;
  size_t u = 0;
  int cps = 0;
  int emEspaco = 1;

  if (dst && cap) dst[0] = '\0';
  if (!src || !dst || cap < 2) return 0;
  if (temControleFrase(src) || temUrlFrase(src)) return 0;

  p = src;
  fim = src + strlen(src);
  while (p < fim) {
    const char *ini = p;
    long cp = lerUtf8(&p, fim);
    size_t nb;
    if (cp < 0) {
      zerarMemoria(dst, cap);
      return 0;
    }
    if (cp == ' ' || cp == '\t' || cp == '\n' || cp == '\r') {
      if (!emEspaco) emEspaco = 1;
      continue;
    }
    if (u > 0 && emEspaco) {
      if (cps + 1 > FAROL_ESCOLHA_FRASE_MAX || u + 1 >= cap) break;
      dst[u++] = ' ';
      cps++;
    }
    nb = (size_t)(p - ini);
    if (cps + 1 > FAROL_ESCOLHA_FRASE_MAX || u + nb >= cap) break;
    memcpy(dst + u, ini, nb);
    u += nb;
    cps++;
    emEspaco = 0;
  }
  while (u > 0 && dst[u - 1] == ' ') u--;
  dst[u] = '\0';
  if (u == 0 || temUrlFrase(dst)) {
    zerarMemoria(dst, cap);
    return 0;
  }
  return 1;
}

static int idiomaValido(const char *s) {
  size_t n, i;
  if (!s || !s[0]) return 0;
  n = strlen(s);
  if (n < 2 || n > 7) return 0;
  if (!(s[0] >= 'a' && s[0] <= 'z') || !(s[1] >= 'a' && s[1] <= 'z')) return 0;
  if (n == 2) return 1;
  if (s[2] != '-' || n < 5) return 0;
  for (i = 3; i < n; i++) {
    unsigned char c = (unsigned char)s[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))) return 0;
  }
  return 1;
}

static const char *generoPublicoNome(int32_t id) {
  static const struct { int32_t id; const char *nome; } G[] = {
    { 28, "Acao" }, { 12, "Aventura" }, { 16, "Animacao" }, { 35, "Comedia" },
    { 80, "Crime" }, { 99, "Documentario" }, { 18, "Drama" }, { 10751, "Familia" },
    { 14, "Fantasia" }, { 36, "Historia" }, { 27, "Terror" }, { 10402, "Musica" },
    { 9648, "Misterio" }, { 10749, "Romance" }, { 878, "Ficcao cientifica" },
    { 53, "Suspense" }, { 10752, "Guerra" }, { 37, "Faroeste" },
    { 10759, "Acao e Aventura" }, { 10762, "Infantil" }, { 10763, "Noticias" },
    { 10764, "Reality" }, { 10765, "Ficcao e Fantasia" }, { 10766, "Novela" },
    { 10767, "Talk show" }, { 10768, "Guerra e Politica" },
  };
  size_t i;
  if (id <= 0) return NULL;
  for (i = 0; i < sizeof G / sizeof *G; i++)
    if (G[i].id == id) return G[i].nome;
  return NULL;
}

// ------------------------------------------------------ escritor JSON ------

typedef struct {
  char  *buf;
  size_t n;
  size_t cap;
  int    erro;
} Esc;

static void escLit(Esc *e, const char *s) {
  size_t k;
  if (e->erro) return;
  k = strlen(s);
  if (e->n + k >= e->cap) { e->erro = 1; return; }
  memcpy(e->buf + e->n, s, k);
  e->n += k;
  e->buf[e->n] = '\0';
}

static void escChar(Esc *e, char ch) {
  if (e->erro) return;
  if (e->n + 1 >= e->cap) { e->erro = 1; return; }
  e->buf[e->n++] = ch;
  e->buf[e->n] = '\0';
}

static void escStr(Esc *e, const char *s) {
  const unsigned char *p = (const unsigned char *)(s ? s : "");
  escChar(e, '"');
  for (; *p && !e->erro; p++) {
    unsigned char ch = *p;
    if (ch == '"' || ch == '\\') {
      escChar(e, '\\');
      escChar(e, (char)ch);
    } else if (ch == '\b') { escLit(e, "\\b"); }
    else if (ch == '\f')   { escLit(e, "\\f"); }
    else if (ch == '\n')   { escLit(e, "\\n"); }
    else if (ch == '\r')   { escLit(e, "\\r"); }
    else if (ch == '\t')   { escLit(e, "\\t"); }
    else if (ch < 0x20 || ch == 0x7f) {
      char u[7];
      snprintf(u, sizeof u, "\\u%04x", (unsigned)ch);
      escLit(e, u);
    } else {
      escChar(e, (char)ch);
    }
  }
  escChar(e, '"');
}

static void escInt(Esc *e, long long v) {
  char b[32];
  snprintf(b, sizeof b, "%lld", v);
  escLit(e, b);
}

static void escPesoQ8(Esc *e, int16_t peso) {
  char b[24];
  long x = ((long)peso * 100 + (peso >= 0 ? 128 : -128)) / 256;
  long absx = x < 0 ? -x : x;
  snprintf(b, sizeof b, "%s%ld.%02ld", x < 0 ? "-" : "", absx / 100, absx % 100);
  escLit(e, b);
}

// ------------------------------------------------ resumo seguro do gosto ---

typedef struct {
  int32_t id;
  int16_t peso;
} AfinTmp;

static int cmpAfinDesc(const void *a, const void *b) {
  const AfinTmp *pa = (const AfinTmp *)a;
  const AfinTmp *pb = (const AfinTmp *)b;
  if (pb->peso != pa->peso) return pb->peso > pa->peso ? 1 : -1;
  return pa->id < pb->id ? -1 : (pa->id > pb->id ? 1 : 0);
}

static void escGostoDim(Esc *e, const char *chave, const FarolGosto *g,
                        int dim, const FarolAfin *src, int cap,
                        uint16_t dia, int maxItens, int ehGenero, int *primeiro) {
  AfinTmp tmp[FG_TEMA_N];
  int nt = 0, i, k = 0;
  char camBase[48], camId[56], camNome[56], camPeso[56];
  int okNome, okPeso;

  if (!g || !src || maxItens <= 0 || e->erro) return;
  snprintf(camBase, sizeof camBase, "gosto.%s", chave);
  snprintf(camId, sizeof camId, "gosto.%s[].id", chave);
  if (!farolconsent_campo_autorizado(camBase) ||
      !farolconsent_campo_autorizado(camId))
    return;
  snprintf(camNome, sizeof camNome, "gosto.%s[].nome", chave);
  snprintf(camPeso, sizeof camPeso, "gosto.%s[].peso", chave);
  okNome = farolconsent_campo_autorizado(camNome);
  okPeso = farolconsent_campo_autorizado(camPeso);

  for (i = 0; i < cap && nt < FG_TEMA_N; i++) {
    int32_t w;
    if (src[i].id <= 0) continue; // SOMENTE IDs publicos TMDB (> 0)
    w = farolgosto_peso(g, dim, src[i].id, dia);
    if (w <= 0) continue;
    tmp[nt].id = src[i].id;
    tmp[nt].peso = (int16_t)(w > 32767 ? 32767 : w);
    nt++;
  }
  if (nt == 0) return;
  if (nt > 1) qsort(tmp, (size_t)nt, sizeof tmp[0], cmpAfinDesc);

  for (i = 0; i < nt && k < maxItens; i++) {
    int j, dup = 0;
    const char *nm = ehGenero ? generoPublicoNome(tmp[i].id) : NULL;
    for (j = 0; j < i; j++) if (tmp[j].id == tmp[i].id) { dup = 1; break; }
    if (dup) continue;
    if (k == 0) {
      if (!*primeiro) escChar(e, ',');
      *primeiro = 0;
      escChar(e, '"');
      escLit(e, chave);
      escLit(e, "\":[");
    } else {
      escChar(e, ',');
    }
    escLit(e, "{\"id\":");
    escInt(e, tmp[i].id);
    if (nm && nm[0] && okNome) {
      escLit(e, ",\"nome\":");
      escStr(e, nm);
    }
    if (tmp[i].peso != 0 && okPeso) {
      escLit(e, ",\"peso\":");
      escPesoQ8(e, tmp[i].peso);
    }
    escChar(e, '}');
    k++;
  }
  if (k > 0) escChar(e, ']');
}

size_t farolescolha_rede_montar_corpo(const char *frase, const char *idioma,
                                      const int *perguntas, int nPerguntas,
                                      const FarolGosto *gosto,
                                      char *dst, size_t cap) {
  char fraseNorm[FAROL_ESCOLHA_FRASE_BYTES];
  const char *idiStr;
  int usadas[FESC_N];
  int pergs[FESC_N];
  int nP = 0, i;
  size_t lim;
  Esc e;

  if (dst && cap) dst[0] = '\0';
  if (!frase || !perguntas || nPerguntas <= 0 || !dst || cap < 64) return 0;

  if (!farolescolha_rede_normalizar_frase(frase, fraseNorm, sizeof fraseNorm))
    return 0;

  memset(usadas, 0, sizeof usadas);
  for (i = 0; i < nPerguntas && i < FESC_N; i++) {
    int p = perguntas[i];
    if (p < 0 || p >= FESC_N) { zerarMemoria(fraseNorm, sizeof fraseNorm); return 0; }
    if (usadas[p]) continue;
    usadas[p] = 1;
    pergs[nP++] = p;
  }
  if (nP <= 0 || nP > FESC_N) {
    zerarMemoria(fraseNorm, sizeof fraseNorm);
    return 0;
  }

  if (!farolconsent_campo_autorizado("v") ||
      !farolconsent_campo_autorizado("idioma") ||
      !farolconsent_campo_autorizado("frase") ||
      !farolconsent_campo_autorizado("perguntas") ||
      !farolconsent_campo_autorizado("gosto") ||
      !farolconsent_campo_autorizado("infantil") ||
      !farolconsent_campo_autorizado("consentiu")) {
    zerarMemoria(fraseNorm, sizeof fraseNorm);
    return 0;
  }

  idiStr = (idioma && idiomaValido(idioma)) ? idioma : "pt-BR";
  lim = cap < (size_t)(FAROL_ESCOLHA_CORPO_MAX + 1) ? cap : (size_t)(FAROL_ESCOLHA_CORPO_MAX + 1);
  e.buf = dst;
  e.n = 0;
  e.cap = lim;
  e.erro = 0;
  dst[0] = '\0';

  // ESTRITAMENTE as 7 chaves de CHAVES_ESCOLHA (rotaFarolEscolha).
  escLit(&e, "{\"v\":1,\"idioma\":");
  escStr(&e, idiStr);
  escLit(&e, ",\"frase\":");
  escStr(&e, fraseNorm);
  zerarMemoria(fraseNorm, sizeof fraseNorm);

  escLit(&e, ",\"perguntas\":[");
  for (i = 0; i < nP; i++) {
    const char *pid = farolescolha_pergunta_id(pergs[i]);
    if (!pid || !pid[0]) { zerarMemoria(dst, cap); return 0; }
    if (i > 0) escChar(&e, ',');
    escStr(&e, pid);
  }
  escLit(&e, "],\"gosto\":{");

  if (gosto) {
    long long agora = (long long)time(NULL);
    uint16_t dia = diaModeloAgora(agora);
    int nivel = gosto->titulos < 3u ? 0 : (gosto->titulos < 10u ? 1 : 2);
    int fimSem = ehFimDeSemana(agora);
    int primeiro = 1;

    if (farolconsent_campo_autorizado("gosto.nivel")) {
      escLit(&e, "\"nivel\":");
      escInt(&e, nivel);
      primeiro = 0;
    }
    if (farolconsent_campo_autorizado("gosto.fim_semana")) {
      if (!primeiro) escChar(&e, ',');
      escLit(&e, "\"fim_semana\":");
      escInt(&e, fimSem);
      primeiro = 0;
    }
    escGostoDim(&e, "generos", gosto, FG_DIM_GENERO, gosto->genero, FG_GENERO_N,
                dia, FAROL_ESCOLHA_GEN_MAX, 1, &primeiro);
    escGostoDim(&e, "temas", gosto, FG_DIM_TEMA, gosto->tema, FG_TEMA_N,
                dia, FAROL_ESCOLHA_TEMA_MAX, 0, &primeiro);
    escGostoDim(&e, "pessoas", gosto, FG_DIM_PESSOA, gosto->pessoa, FG_PESSOA_N,
                dia, FAROL_ESCOLHA_PESSOA_MAX, 0, &primeiro);
  }

  escLit(&e, "},\"infantil\":0,\"consentiu\":1}");
  if (e.erro || e.n > FAROL_ESCOLHA_CORPO_MAX) {
    zerarMemoria(dst, cap);
    return 0;
  }
  return e.n;
}

// ------------------------------------------------- validacao da resposta ----

static void pularWs(const char **pp, const char *fim) {
  while (*pp < fim && (**pp == ' ' || **pp == '\t' || **pp == '\n' || **pp == '\r'))
    (*pp)++;
}

static int hexVal(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

// Le string JSON estrita para ids curtos ASCII (chave/valor da resposta).
static int lerJsonStr(const char **pp, const char *fim, char *dst, size_t cap) {
  const char *p = *pp;
  size_t k = 0;
  if (p >= fim || *p != '"') return 0;
  p++;
  while (p < fim && *p != '"') {
    unsigned char ch = (unsigned char)*p++;
    if (ch < 0x20 || ch == 0x7f) return 0;
    if (ch == '\\') {
      char esc;
      if (p >= fim) return 0;
      esc = *p++;
      if (esc == '"' || esc == '\\' || esc == '/') {
        if (k + 1 >= cap) return 0;
        dst[k++] = esc;
      } else if (esc == 'u') {
        int h0, h1, h2, h3, cp;
        if (fim - p < 4) return 0;
        h0 = hexVal(p[0]); h1 = hexVal(p[1]); h2 = hexVal(p[2]); h3 = hexVal(p[3]);
        if (h0 < 0 || h1 < 0 || h2 < 0 || h3 < 0) return 0;
        p += 4;
        cp = (h0 << 12) | (h1 << 8) | (h2 << 4) | h3;
        if (cp < 0x20 || cp >= 0x7f) return 0;
        if (k + 1 >= cap) return 0;
        dst[k++] = (char)cp;
      } else {
        return 0;
      }
    } else {
      if (k + 1 >= cap) return 0;
      dst[k++] = (char)ch;
    }
  }
  if (p >= fim || *p != '"') return 0;
  p++;
  dst[k] = '\0';
  *pp = p;
  return 1;
}

static int perguntaNaLista(int perg, const int *perguntas, int nPerguntas) {
  int i;
  for (i = 0; i < nPerguntas; i++) if (perguntas[i] == perg) return 1;
  return 0;
}

int farolescolha_rede_validar_resposta(const char *json, size_t len,
                                       const int *perguntas, int nPerguntas,
                                       FarolEscRedeResp *out, int *ehRegrasOut) {
  const char *p, *fim;
  int viuOrigem = 0, viuRespostas = 0, viuFalta = 0;
  int ehLlm = 0, ehRegras = 0;
  int usadaResp[FESC_N];
  FarolEscRedeResp res;

  if (ehRegrasOut) *ehRegrasOut = 0;
  if (out) { memset(out, 0, sizeof *out); out->faltaPerg = -1; }
  if (!json || len == 0 || len > 16384u || !perguntas || nPerguntas <= 0) return 0;

  memset(&res, 0, sizeof res);
  res.faltaPerg = -1;
  memset(usadaResp, 0, sizeof usadaResp);

  p = json;
  fim = json + len;
  pularWs(&p, fim);
  if (p >= fim || *p != '{') return 0;
  p++;

  for (;;) {
    char chave[32];
    pularWs(&p, fim);
    if (p >= fim) return 0;
    if (*p == '}') { p++; break; }
    if (viuOrigem || viuRespostas || viuFalta) {
      if (*p != ',') return 0;
      p++;
      pularWs(&p, fim);
    }
    if (!lerJsonStr(&p, fim, chave, sizeof chave)) return 0;
    pularWs(&p, fim);
    if (p >= fim || *p != ':') return 0;
    p++;
    pularWs(&p, fim);

    if (!strcmp(chave, "origem")) {
      char val[32];
      if (viuOrigem || !lerJsonStr(&p, fim, val, sizeof val)) return 0;
      viuOrigem = 1;
      if (!strcmp(val, "llm")) ehLlm = 1;
      else if (!strcmp(val, "regras")) ehRegras = 1;
      else return 0;
    } else if (!strcmp(chave, "respostas")) {
      int primeiro = 1;
      if (viuRespostas || p >= fim || *p != '{') return 0;
      viuRespostas = 1;
      p++;
      for (;;) {
        char pKey[32], oVal[32];
        int perg, opc;
        pularWs(&p, fim);
        if (p >= fim) return 0;
        if (*p == '}') { p++; break; }
        if (!primeiro) {
          if (*p != ',') return 0;
          p++;
          pularWs(&p, fim);
        }
        primeiro = 0;
        if (!lerJsonStr(&p, fim, pKey, sizeof pKey)) return 0;
        pularWs(&p, fim);
        if (p >= fim || *p != ':') return 0;
        p++;
        pularWs(&p, fim);
        if (!lerJsonStr(&p, fim, oVal, sizeof oVal)) return 0;

        perg = farolescolha_pergunta_por_id(pKey);
        if (perg < 0 || !perguntaNaLista(perg, perguntas, nPerguntas)) return 0;
        if (usadaResp[perg]) return 0;
        opc = farolescolha_opcao_por_id(perg, oVal);
        // Compatibilidade com PERGUNTAS_ESCOLHA.continuar do servidor ("continuar" -> FESC_CONT_SIM).
        if (opc < 0 && perg == FESC_CONTINUAR && !strcmp(oVal, "continuar"))
          opc = FESC_CONT_SIM;
        if (opc < 0) return 0;
        if (res.nRespostas >= FESC_N) return 0;
        usadaResp[perg] = 1;
        res.respostas[res.nRespostas].perg = perg;
        res.respostas[res.nRespostas].opc = opc;
        snprintf(res.respostas[res.nRespostas].perguntaId,
                 sizeof res.respostas[res.nRespostas].perguntaId,
                 "%s", farolescolha_pergunta_id(perg));
        snprintf(res.respostas[res.nRespostas].opcaoId,
                 sizeof res.respostas[res.nRespostas].opcaoId,
                 "%s", farolescolha_opcao_id(perg, opc));
        res.nRespostas++;
      }
    } else if (!strcmp(chave, "falta")) {
      if (viuFalta) return 0;
      viuFalta = 1;
      if (fim - p >= 4 && !memcmp(p, "null", 4)) {
        p += 4;
        res.temFalta = 0;
        res.faltaPerg = -1;
        res.faltaId[0] = '\0';
      } else {
        char fVal[32];
        int fp;
        if (!lerJsonStr(&p, fim, fVal, sizeof fVal)) return 0;
        fp = farolescolha_pergunta_por_id(fVal);
        if (fp < 0 || !perguntaNaLista(fp, perguntas, nPerguntas)) return 0;
        res.temFalta = 1;
        res.faltaPerg = fp;
        snprintf(res.faltaId, sizeof res.faltaId, "%s", farolescolha_pergunta_id(fp));
      }
    } else {
      return 0; // chave desconhecida
    }
  }

  pularWs(&p, fim);
  if (p != fim) return 0;

  if (ehRegras) {
    if (ehRegrasOut) *ehRegrasOut = 1;
    return 0;
  }
  if (!ehLlm || !viuRespostas) return 0;
  if (res.temFalta && usadaResp[res.faltaPerg]) return 0; // falta repetida em respostas
  if (res.nRespostas == 0 && !res.temFalta) return 0;     // resposta vazia

  if (out) *out = res;
  return 1;
}

// ------------------------------------------------ guardas e fio de rede ---

static int suprimidoAgora(void) {
  if (player_aberto()) return 1;
  if (perfil_infantil_ativo()) return 1;
  if (seguro_perfil_ativo()) return 1;
  if (!farolsinal_recomendar()) return 1;
  if (!farolconsent_llm_ativo()) return 1;
  if (farolconsent_estado() != FER_CONSENT_SIM_VAL) return 1;
  return 0;
}

int farolescolha_rede_autorizado(void) {
  farolconsent_sincronizar_ativo();
  return !suprimidoAgora();
}

static void limparJob(EscolhaJob *j) {
  if (!j) return;
  if (j->corpo) {
    zerarMemoria(j->corpo, j->nCorpo);
    free(j->corpo);
  }
  zerarMemoria(j->aut, sizeof j->aut);
  zerarMemoria(j->via, sizeof j->via);
  zerarMemoria(j->perfilCab, sizeof j->perfilCab);
  zerarMemoria(j->url, sizeof j->url);
  free(j);
}

static int jobCancelado(EscolhaJob *j) {
  int sim;
  pthread_mutex_lock(&trava);
  sim = j->cancelar || j->gen != geracaoAtual;
  pthread_mutex_unlock(&trava);
  return sim;
}

static int contextoMudou(EscolhaJob *j) {
  if (suprimidoAgora() || perfilAtualValido() != j->perfil ||
      perfis_sessao_geracao() != j->sessao || contaAtualHash() != j->contaHash) {
    __atomic_store_n(&j->bloqueou, 1, __ATOMIC_RELEASE);
    return 1;
  }
  return 0;
}

static int jobBloqueou(EscolhaJob *j) {
  return __atomic_load_n(&j->bloqueou, __ATOMIC_ACQUIRE);
}

static int escolhaParar(void *u) {
  EscolhaJob *j = (EscolhaJob *)u;
  return jobCancelado(j) || agoraMs() >= j->fim || contextoMudou(j);
}

static int obterBaseUrl(char *dst, size_t cap) {
  dst[0] = '\0';
  pthread_mutex_lock(&trava);
  if (temUrlOverride) {
    snprintf(dst, cap, "%s", urlOverride);
    pthread_mutex_unlock(&trava);
    return dst[0] != '\0';
  }
  pthread_mutex_unlock(&trava);
#ifdef FAROL_ESCOLHA_WORKER
  if (FAROL_ESCOLHA_WORKER[0]) {
    snprintf(dst, cap, "%s", FAROL_ESCOLHA_WORKER);
    return 1;
  }
#endif
  nv_rec_url_copiar(dst, cap);
  return dst[0] != '\0';
}

static int obterAuth(char *aut, size_t na, char *via, size_t nv,
                     char *perfilCab, size_t np, int *usouTrakt) {
  const char *cab[4];
  aut[0] = '\0'; via[0] = '\0'; perfilCab[0] = '\0';
  *usouTrakt = 0;

  pthread_mutex_lock(&trava);
  if (temAuthOverride) {
    snprintf(aut, na, "%s", authOverrideAut);
    snprintf(via, nv, "%s", authOverrideVia);
    snprintf(perfilCab, np, "%s", authOverridePerfil);
    *usouTrakt = !strcmp(via, "X-Nuvio-Auth: trakt");
    pthread_mutex_unlock(&trava);
    return aut[0] != '\0';
  }
  pthread_mutex_unlock(&trava);

  if (!recomenda_cabecalhos(cab, aut, na, via, nv, perfilCab, np)) return 0;
  *usouTrakt = !strcmp(via, "X-Nuvio-Auth: trakt");
  return 1;
}

static void *executarEscolha(void *u) {
  EscolhaJob *j = (EscolhaJob *)u;
  const char *cab[5];
  int nc = 0, http = 0, authOk = 0, usouTrakt = 0;
  int cancelado = 0, prazoEstourou = 0, transporteFalhou = 0, bloqueado = 0;
  int valOk = 0, ehRegras = 0;
  char *respCorpo = NULL;
  size_t respLen = 0;
  long long restaMs;
  FarolEscRedeResp parsed;

  memset(&parsed, 0, sizeof parsed);
  parsed.faltaPerg = -1;

  // 1. Auth DENTRO do fio de trabalho, sob rede_parar_trocar e prazo de 6 s.
  if (jobCancelado(j)) {
    cancelado = 1;
  } else if (contextoMudou(j)) {
    bloqueado = 1;
  } else if (agoraMs() >= j->fim) {
    prazoEstourou = 1;
  } else {
    RedePararEstado prev = rede_parar_trocar(escolhaParar, j);
    authOk = obterAuth(j->aut, sizeof j->aut, j->via, sizeof j->via,
                       j->perfilCab, sizeof j->perfilCab, &usouTrakt);
    rede_parar_trocar(prev.parar, prev.usuario);
    j->usouTrakt = usouTrakt;
  }

  // 2. Revalida todos os bloqueios e o saldo do prazo (em long long, sem underflow).
  if (jobCancelado(j)) cancelado = 1;
  else if (jobBloqueou(j) || contextoMudou(j)) bloqueado = 1;
  else if ((restaMs = j->fim - agoraMs()) <= 0) prazoEstourou = 1;
  else if (!authOk) transporteFalhou = 1;
  else if (rede_saude_offline()) transporteFalhou = 1;
  else {
    unsigned restMs = restaMs > (long long)j->prazoMs ? j->prazoMs : (unsigned)restaMs;
    if (restMs < 1u) restMs = 1u;

    if (j->aut[0]) cab[nc++] = j->aut;
    if (j->via[0]) cab[nc++] = j->via;
    if (j->perfilCab[0]) cab[nc++] = j->perfilCab;
#ifndef __EMSCRIPTEN__
    cab[nc++] = "Content-Type: application/json";
#endif
    cab[nc] = NULL;

#ifdef __EMSCRIPTEN__
    {
      RedePararEstado prev = rede_parar_trocar(escolhaParar, j);
      int seg = (int)((restMs + 999u) / 1000u);
      if (seg < 1) seg = 1;
      respCorpo = rede_postar_st(j->url, seg, cab, j->corpo, &http);
      rede_parar_trocar(prev.parar, prev.usuario);
      if (jobCancelado(j)) {
        cancelado = 1;
      } else if (jobBloqueou(j) || contextoMudou(j)) {
        bloqueado = 1;
      } else if (agoraMs() >= j->fim && http == 0) {
        prazoEstourou = 1;
      } else if (!respCorpo || http == 0) {
        transporteFalhou = 1;
      } else {
        respLen = strlen(respCorpo);
      }
    }
#else
    {
      RedePedido req;
      RedeResposta res;
      memset(&req, 0, sizeof req);
      memset(&res, 0, sizeof res);
      req.metodo = "POST";
      req.url = j->url;
      req.cabecalhos = cab;
      req.corpo = j->corpo;
      req.n_corpo = j->nCorpo;
      req.prazo_ms = restMs;
      req.max_bytes = 16384u;
      req.max_cabecalhos = 8192u;
      req.seguir = 0;
      req.parar = escolhaParar;
      req.parar_usuario = j;

      rede_pedir(&req, &res);
      http = res.status;
      if (jobCancelado(j)) {
        cancelado = 1;
      } else if (jobBloqueou(j) || contextoMudou(j)) {
        bloqueado = 1;
      } else if (res.erro == REDE_PRAZO ||
                 (res.erro == REDE_CANCELADO && agoraMs() >= j->fim)) {
        prazoEstourou = 1;
      } else if (res.erro == REDE_LIMITE_CORPO) {
        valOk = 0;
      } else if (res.erro != REDE_OK || http == 0) {
        transporteFalhou = 1;
      } else if (res.corpo && res.n_corpo > 0) {
        respCorpo = res.corpo;
        respLen = res.n_corpo;
        res.corpo = NULL;
      }
      rede_resposta_limpar(&res);
    }
#endif
  }

  if (!cancelado && !bloqueado && !prazoEstourou && !transporteFalhou &&
      http == 200 && respCorpo && respLen > 0) {
    valOk = farolescolha_rede_validar_resposta(respCorpo, respLen,
                                               j->perguntas, j->nPerguntas,
                                               &parsed, &ehRegras);
  }
  if (respCorpo) {
    zerarMemoria(respCorpo, respLen);
    free(respCorpo);
  }

  if (!cancelado && !bloqueado && http == 401 && j->usouTrakt)
    trakt_http_recusada();

  pthread_mutex_lock(&trava);
  if (jobAtual == j) jobAtual = NULL;
  if (j->gen == geracaoAtual) {
    if (cancelado || j->cancelar) {
      memset(&respAtual, 0, sizeof respAtual);
      respAtual.faltaPerg = -1;
      estadoAtual.estado = FER_CANCELADO;
      estadoAtual.motivo = FER_MOT_CANCELADO;
      estadoAtual.http = 0;
    } else if (bloqueado) {
      memset(&respAtual, 0, sizeof respAtual);
      respAtual.faltaPerg = -1;
      estadoAtual.estado = FER_FALLBACK;
      estadoAtual.motivo = FER_MOT_SUPRIMIDO;
      estadoAtual.http = 0;
    } else if (valOk) {
      respAtual = parsed;
      estadoAtual.estado = FER_PRONTO;
      estadoAtual.motivo = FER_MOT_OK;
      estadoAtual.http = http;
    } else {
      FarolEscRedeMotivo mot;
      if (ehRegras) mot = FER_MOT_REGRAS;
      else if (prazoEstourou) mot = FER_MOT_PRAZO;
      else if (transporteFalhou) mot = FER_MOT_OFFLINE;
      else if (http == 429) mot = FER_MOT_LIMITE;
      else mot = FER_MOT_ERRO;

      memset(&respAtual, 0, sizeof respAtual);
      respAtual.faltaPerg = -1;
      estadoAtual.estado = FER_FALLBACK;
      estadoAtual.motivo = mot;
      estadoAtual.http = http;
    }
  }
  ativos--;
  if (ativos == 0) pthread_cond_broadcast(&condFim);
  pthread_mutex_unlock(&trava);

  limparJob(j);
  return NULL;
}

static void publicarFallbackSemRede(FarolEscRedeMotivo mot) {
  pthread_mutex_lock(&trava);
  if (jobAtual) { jobAtual->cancelar = 1; jobAtual = NULL; }
  geracaoAtual++;
  memset(&respAtual, 0, sizeof respAtual);
  respAtual.faltaPerg = -1;
  estadoAtual.estado = FER_FALLBACK;
  estadoAtual.motivo = mot;
  estadoAtual.http = 0;
  estadoAtual.geracao = geracaoAtual;
  pthread_mutex_unlock(&trava);
}

uint64_t farolescolha_rede_pedir_perguntas(const char *frase,
                                           const int *perguntas, int nPerguntas,
                                           const FarolGosto *gosto) {
  char corpoBuf[FAROL_ESCOLHA_CORPO_MAX + 1];
  char baseUrl[384];
  FarolGosto gLocal;
  const FarolGosto *gUso = gosto;
  size_t nCorpo;
  int pergsLimpos[FESC_N];
  int usadas[FESC_N];
  int nLimpos = 0, i;
  int perfil;
  uint64_t sessao, contaHash;

  farolconsent_sincronizar_ativo();

  // Guardas rigidas ANTES de abrir rede (0 POSTs!).
  if (suprimidoAgora()) {
    publicarFallbackSemRede(FER_MOT_SUPRIMIDO);
    return 0;
  }

  if (!frase || !perguntas || nPerguntas <= 0) {
    publicarFallbackSemRede(FER_MOT_FRASE_INVALIDA);
    return 0;
  }

  memset(usadas, 0, sizeof usadas);
  for (i = 0; i < nPerguntas && i < FESC_N; i++) {
    int p = perguntas[i];
    if (p < 0 || p >= FESC_N) {
      publicarFallbackSemRede(FER_MOT_FRASE_INVALIDA);
      return 0;
    }
    if (usadas[p]) continue;
    usadas[p] = 1;
    pergsLimpos[nLimpos++] = p;
  }
  if (nLimpos <= 0) {
    publicarFallbackSemRede(FER_MOT_FRASE_INVALIDA);
    return 0;
  }

  memset(&gLocal, 0, sizeof gLocal);
  if (!gUso && farolsinal_modelo(&gLocal)) gUso = &gLocal;

  nCorpo = farolescolha_rede_montar_corpo(frase, ajustes_tmdb_idioma(),
                                          pergsLimpos, nLimpos, gUso,
                                          corpoBuf, sizeof corpoBuf);
  zerarMemoria(&gLocal, sizeof gLocal);
  if (nCorpo == 0) {
    zerarMemoria(corpoBuf, sizeof corpoBuf);
    publicarFallbackSemRede(FER_MOT_FRASE_INVALIDA);
    return 0;
  }

  if (rede_saude_offline() || !obterBaseUrl(baseUrl, sizeof baseUrl)) {
    zerarMemoria(corpoBuf, sizeof corpoBuf);
    publicarFallbackSemRede(FER_MOT_OFFLINE);
    return 0;
  }

  perfil = perfilAtualValido();
  sessao = perfis_sessao_geracao();
  contaHash = contaAtualHash();

  {
    EscolhaJob *j = (EscolhaJob *)calloc(1, sizeof *j);
    size_t nBase;
    pthread_attr_t attr;
    pthread_t fio;
    int erro;
    uint64_t gen;

    if (!j) {
      zerarMemoria(corpoBuf, sizeof corpoBuf);
      publicarFallbackSemRede(FER_MOT_ERRO);
      return 0;
    }
    j->corpo = (char *)malloc(nCorpo + 1);
    if (!j->corpo) {
      zerarMemoria(corpoBuf, sizeof corpoBuf);
      limparJob(j);
      publicarFallbackSemRede(FER_MOT_ERRO);
      return 0;
    }
    memcpy(j->corpo, corpoBuf, nCorpo + 1);
    zerarMemoria(corpoBuf, sizeof corpoBuf);
    j->nCorpo = nCorpo;

    nBase = strlen(baseUrl);
    while (nBase > 0 && baseUrl[nBase - 1] == '/') baseUrl[--nBase] = '\0';
    snprintf(j->url, sizeof j->url, "%s/v1/farol/escolha", baseUrl);

    j->perfil = perfil;
    j->sessao = sessao;
    j->contaHash = contaHash;
    j->nPerguntas = nLimpos;
    memcpy(j->perguntas, pergsLimpos, (size_t)nLimpos * sizeof pergsLimpos[0]);
    j->prazoMs = FAROL_ESCOLHA_PRAZO_MS;
    j->fim = agoraMs() + (long long)j->prazoMs;

    if (pthread_attr_init(&attr) != 0) {
      limparJob(j);
      publicarFallbackSemRede(FER_MOT_ERRO);
      return 0;
    }
    erro = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    pthread_mutex_lock(&trava);
    if (jobAtual) { jobAtual->cancelar = 1; jobAtual = NULL; }
    geracaoAtual++;
    gen = geracaoAtual;
    j->gen = gen;
    memset(&respAtual, 0, sizeof respAtual);
    respAtual.faltaPerg = -1;
    if (!erro) {
      ativos++;
      jobAtual = j;
      estadoAtual.estado = FER_BUSCANDO;
      estadoAtual.motivo = FER_MOT_NENHUM;
      estadoAtual.http = 0;
      estadoAtual.geracao = gen;
      erro = pthread_create(&fio, &attr, executarEscolha, j);
      if (erro) {
        if (jobAtual == j) jobAtual = NULL;
        ativos--;
        if (ativos == 0) pthread_cond_broadcast(&condFim);
      }
    }
    if (erro) {
      estadoAtual.estado = FER_FALLBACK;
      estadoAtual.motivo = FER_MOT_OFFLINE;
      estadoAtual.http = 0;
      estadoAtual.geracao = gen;
    }
    pthread_mutex_unlock(&trava);
    pthread_attr_destroy(&attr);

    if (erro) {
      limparJob(j);
      return 0;
    }
    return gen;
  }
}

uint64_t farolescolha_rede_pedir(const char *frase, const FarolEsc *esc,
                                 const FarolGosto *gosto) {
  int pergs[FESC_N];
  int nPergs = 0, p;
  if (!esc) {
    publicarFallbackSemRede(FER_MOT_FRASE_INVALIDA);
    return 0;
  }
  for (p = 0; p < FESC_N; p++) {
    if (farolescolha_resposta(esc, p) == FESC_SEM_RESP)
      pergs[nPergs++] = p;
  }
  if (nPergs <= 0) {
    publicarFallbackSemRede(FER_MOT_FRASE_INVALIDA);
    return 0;
  }
  return farolescolha_rede_pedir_perguntas(frase, pergs, nPergs, gosto);
}

void farolescolha_rede_cancelar(void) {
  pthread_mutex_lock(&trava);
  if (jobAtual) {
    jobAtual->cancelar = 1;
    jobAtual = NULL;
  }
  geracaoAtual++;
  memset(&respAtual, 0, sizeof respAtual);
  respAtual.faltaPerg = -1;
  estadoAtual.estado = FER_CANCELADO;
  estadoAtual.motivo = FER_MOT_CANCELADO;
  estadoAtual.http = 0;
  estadoAtual.geracao = geracaoAtual;
  pthread_mutex_unlock(&trava);
}

void farolescolha_rede_encerrar(void) {
  pthread_mutex_lock(&trava);
  if (jobAtual) {
    jobAtual->cancelar = 1;
    jobAtual = NULL;
  }
  geracaoAtual++;
  while (ativos > 0) pthread_cond_wait(&condFim, &trava);
  zerarMemoria(&estadoAtual, sizeof estadoAtual);
  zerarMemoria(&respAtual, sizeof respAtual);
  respAtual.faltaPerg = -1;
  pthread_mutex_unlock(&trava);
}

FarolEscRedeEstado farolescolha_rede_estado(FarolEscRedeInfo *info) {
  FarolEscRedeEstado st;
  pthread_mutex_lock(&trava);
  if (info) *info = estadoAtual;
  st = estadoAtual.estado;
  pthread_mutex_unlock(&trava);
  return st;
}

int farolescolha_rede_resposta(uint64_t geracaoEsperada, FarolEscRedeResp *out) {
  int ok = 0;
  if (out) { memset(out, 0, sizeof *out); out->faltaPerg = -1; }
  pthread_mutex_lock(&trava);
  if (estadoAtual.estado == FER_PRONTO &&
      (geracaoEsperada == 0 || estadoAtual.geracao == geracaoEsperada)) {
    if (out) *out = respAtual;
    ok = 1;
  }
  pthread_mutex_unlock(&trava);
  return ok;
}

int farolescolha_rede_em_voo(void) {
  int n;
  pthread_mutex_lock(&trava);
  n = ativos;
  pthread_mutex_unlock(&trava);
  return n > 0;
}

// ------------------------------------------------------------ ganchos teste --

void farolescolha_rede_definir_url(const char *baseUrl) {
  pthread_mutex_lock(&trava);
  if (baseUrl && baseUrl[0]) {
    temUrlOverride = 1;
    snprintf(urlOverride, sizeof urlOverride, "%s", baseUrl);
  } else {
    temUrlOverride = 0;
    zerarMemoria(urlOverride, sizeof urlOverride);
  }
  pthread_mutex_unlock(&trava);
}

void farolescolha_rede_definir_auth(const char *autorizacao, const char *via,
                                    const char *perfilCab) {
  pthread_mutex_lock(&trava);
  if (autorizacao && autorizacao[0]) {
    temAuthOverride = 1;
    snprintf(authOverrideAut, sizeof authOverrideAut, "%s", autorizacao);
    snprintf(authOverrideVia, sizeof authOverrideVia, "%s",
             via ? via : "X-Nuvio-Auth: nuvio");
    snprintf(authOverridePerfil, sizeof authOverridePerfil, "%s",
             perfilCab ? perfilCab : "");
  } else {
    temAuthOverride = 0;
    zerarMemoria(authOverrideAut, sizeof authOverrideAut);
    zerarMemoria(authOverrideVia, sizeof authOverrideVia);
    zerarMemoria(authOverridePerfil, sizeof authOverridePerfil);
  }
  pthread_mutex_unlock(&trava);
}

void farolescolha_rede_teste_zera(void) {
  farolescolha_rede_encerrar();
  pthread_mutex_lock(&trava);
  temUrlOverride = 0;
  zerarMemoria(urlOverride, sizeof urlOverride);
  temAuthOverride = 0;
  zerarMemoria(authOverrideAut, sizeof authOverrideAut);
  zerarMemoria(authOverrideVia, sizeof authOverrideVia);
  zerarMemoria(authOverridePerfil, sizeof authOverridePerfil);
  geracaoAtual = 0;
  pthread_mutex_unlock(&trava);
}
